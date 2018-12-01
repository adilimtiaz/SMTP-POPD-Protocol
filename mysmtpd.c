#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"
#include "smtpsession.h"
#include "helpers.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>

#define MAX_LINE_LENGTH 1024

static void handle_client(int fd);
void handle_incoming_message(int fd, struct smtp_session* session, char *buffer);
void handle_state_zero(int fd, struct smtp_session* session, char* buffer);
void handle_state_one(int fd, struct smtp_session* session, char* buffer);
void handle_state_two(int fd, struct smtp_session* session, char* buffer);
void handle_state_three(int fd, struct smtp_session* session, char* buffer);
void handle_state_four(int fd, struct smtp_session* session, char* buffer);

char file_name_template[] = "../mail.XXXXXX";
int sendStringFailed = 0;

int main(int argc, char *argv[]) {
  
  if (argc != 2) {
    fprintf(stderr, "Invalid arguments. Expected: %s <port>\n", argv[0]);
    return 1;
  }
  
  run_server(argv[1], handle_client);
  
  return 0;
}

int robust_send_string(int fd, char* str, ...){
    va_list args;
    static char *buf = NULL;
    static int bufsize = 0;
    int strsize;

    // Start with string length, increase later if needed
    if (bufsize < strlen(str) + 1) {
        bufsize = strlen(str) + 1;
        buf = realloc(buf, bufsize);
    }
    while(1){
        va_start(args, str);
        strsize = vsnprintf(buf, bufsize, str, args);
        va_end(args);

        // If buffer was enough to fit entire string, send it
        if (strsize <= bufsize) {
            if (send_all(fd, buf, strsize) < 0) {
                sendStringFailed = 1;
            }
            return 0;
        }

        // Try again with more space
        bufsize = strsize + 1;
        buf = realloc(buf, bufsize);
    }
}

void handle_client(int fd) {
    struct smtp_session* session = smtp_session_create();

    net_buffer_t buffer = nb_create(fd, 10000);

    struct utsname system;
    if(uname(&system) != 0){
        // TODO figure out what error code should be
        robust_send_string(fd, "Bad uname bro");
    }
    session->serverDomainName = system.nodename;
    robust_send_string(fd, "220 %s Simple Mail Transfer Service Ready\n", session->serverDomainName);

    while(session->state >= 0  && sendStringFailed == 0){
        robust_send_string(fd, "state is: %d\n", session->state);
        char out[MAX_LINE_LENGTH + 1];
        if(nb_read_line(buffer, out) > 0) {
            if(endsWithNewline(out) == 0) {
                robust_send_string(fd, "500 - Line Too Long");
                session->state = -1;
            } else {
                handle_incoming_message(fd, session, out);
            }
        } else {
            session->state = -1;
        }
    }
    close(session->tempFileFD);
    unlink(session->tempFileName);
    close(fd);
    destroy_user_list(session->recipients);
    free(session);
}

void handle_incoming_message(int fd, struct smtp_session* session, char *buffer){
    switch(session->state) {
        case 0:
            //HELO
            handle_state_zero(fd, session, buffer);
            break;
        case 1:
            //Receiving MAIL FROM:<sender>
            handle_state_one(fd, session, buffer);
            break;
        case 2:
            //RCPT TO:<recipient>
            //DATA -> moves to state 3
            handle_state_two(fd, session, buffer);
            break;
        case 3:
            //Receiving e-mail contents
            // "." -> moves to state 4
            handle_state_three(fd, session, buffer);
            break;
        case 4:
            //Can either quit or restart on MAIL FROM:<sender>
            handle_state_four(fd, session, buffer);
            break;
        default:
            robust_send_string(fd, "Went to default case in handle_incoming_message\n");
            break;
    }
}

void handle_state_zero(int fd, struct smtp_session* session, char* buffer) {
    if(strncasecmp(buffer,"HELO ", 5) == 0){
        char* bufCopy = strdup(buffer);
        strtok(bufCopy, " "); // Ignore first word
        char* domainName = strtok(NULL, " "); // Domain name is second word
        if(isWord(domainName) == 0) { //indicates no domain provided
            robust_send_string(fd, "501-Invalid Syntax No Domain Name provided\n");
        } else {
            char* lineEnding = substr(strdup(buffer), (int)(5 + strlen(domainName)), 0);
            if(isLineEndingValid(lineEnding) == 1){
                trimwhitespace(domainName);
                session->senderDomainName = domainName;
                session->state = 1; //transition to next state
                robust_send_string(fd, "%s greets %s\n", session->serverDomainName, domainName);
            }
            else
                robust_send_string(fd, "501-Invalid Syntax: Invalid Line Ending\n");
        }
    } else if(strncasecmp(buffer,"MAIL FROM:<", 10) == 0){
        robust_send_string(fd, "503 Bad Sequence of Commands\n");
    } else if(strncasecmp(buffer, "RCPT TO:<",7) == 0){
        robust_send_string(fd, "503 Bad Sequence of Commands\n");
    } else if(strncasecmp(buffer, "DATA", 4) == 0){
        robust_send_string(fd, "503 Bad Sequence of Commands\n");
    } else if(strncasecmp(buffer, "QUIT", 4) == 0){
        session->state = -1;
        robust_send_string(fd, "221 OK\n");
    } else if(strncasecmp(buffer, "NOOP", 4) == 0){
        robust_send_string(fd, "250 OK\n");
    } else if(strncasecmp(buffer, "RSET",4) == 0){
        robust_send_string(fd, "502-Command not Implemented\n");
    } else if(strncasecmp(buffer, "VRFY ", 5) == 0){
        robust_send_string(fd, "502-Command not Implemented\n");
    } else if(strncasecmp(buffer, "EXPN ", 5) == 0){
        robust_send_string(fd, "502-Command not Implemented\n");
    } else if(strncasecmp(buffer, "HELP", 4) == 0){
        robust_send_string(fd,"502-Command not Implemented\n");
    } else if(strncasecmp(buffer, "EHLO ", 5) == 0) {
        robust_send_string(fd,"502-Command not Implemented\n");
    }else {
        robust_send_string(fd, "500-Invalid Syntax Coming From Here\n");
    }
}

void handle_state_one(int fd, struct smtp_session* session, char* buffer){
    if(strncasecmp(buffer,"HELO ", 5) == 0){
        char* bufCopy = strdup(buffer);
        strtok(bufCopy, " "); // Ignore first word
        char* domainName = strtok(NULL, " "); // Domain name is second word
        trimwhitespace(domainName);
        robust_send_string(fd, "%s 421 Service Not Available\n", domainName);
    } else if(strncasecmp(buffer,"MAIL ", 5) == 0){
        if(strncasecmp(buffer, "MAIL FROM:<", 11) == 0 && strchr(buffer, '>') != NULL){
            char* bufCopy = strdup(buffer);
            strtok(bufCopy, "<");
            char* recipient = strtok(NULL, ">");
            if(isWord(recipient) == 0){
                robust_send_string(fd, "501-Invalid Argument: <Address> is empty\n");
                return;
            }
            char* strInlcudingRightArrow = strchr(buffer, '>');
            char* strAfterRightArrow = substr(strInlcudingRightArrow, 1, 0);
            if(isLineEndingValid(strAfterRightArrow) == 1){
                session->sender = recipient;
                robust_send_string(fd, "250 OK\n");
                session->state = 2;
            }
            else
                robust_send_string(fd, "501-Invalid Syntax: Invalid Line Ending\n");
        } else {
            // Mail provided without proper <Address>
            robust_send_string(fd, "501-Invalid Argument: <Address> not formatted correctly\n");
        }
    } else if(strncasecmp(buffer, "RCPT TO:<", 7) == 0){
        robust_send_string(fd, "503 Bad Sequence of Commands\n");
    } else if(strncasecmp(buffer, "DATA", 4) == 0){
        robust_send_string(fd, "503 Bad Sequence of Commands\n");
    } else if(strncasecmp(buffer, "QUIT", 4) == 0){
        session->state = -1;
        robust_send_string(fd, "221 OK\n");
    } else if(strncasecmp(buffer, "NOOP", 4) == 0){
        robust_send_string(fd, "250 OK\n");
    } else if(strncasecmp(buffer, "RSET", 4) == 0){
        robust_send_string(fd, "502-Command not Implemented\n");
    } else if(strncasecmp(buffer, "VRFY ", 5) == 0){
        robust_send_string(fd, "502-Command not Implemented\n");
    } else if(strncasecmp(buffer, "EXPN ", 5) == 0){
        robust_send_string(fd, "502-Command not Implemented\n");
    } else if(strncasecmp(buffer, "HELP", 4) == 0){
        robust_send_string(fd, "502-Command not Implemented\n");
    } else if(strncasecmp(buffer, "EHLO ",5) == 0) {
        robust_send_string(fd,"502-Command not Implemented\n");
    } else {
        robust_send_string(fd, "500-Invalid Syntax\n");
    }
}

void handle_state_two(int fd, struct smtp_session* session, char* buffer) {
    if(strncasecmp(buffer,"HELO ", 5) == 0){
        char* bufCopy = strdup(buffer);
        strtok(bufCopy, " "); // Ignore first word
        char* domainName = strtok(NULL, " "); // Domain name is second word
        trimwhitespace(domainName);
        robust_send_string(fd, "%s 421 Service Not Available\n", domainName);
    } else if (strncasecmp(buffer, "MAIL FROM:<", 11) == 0) {
        robust_send_string(fd, "503 Bad Sequence of Commands\n");
    } else if (strncasecmp(buffer, "RCPT ", 5) == 0) {
        if(strncasecmp(buffer, "RCPT TO:<", 9) == 0 && strchr(buffer, '>') != NULL) {
            char *bufCopy = strdup(buffer);
            strtok(bufCopy, "<");
            char *recipient = strtok(NULL, ">");
            char* strInlcudingRightArrow = strchr(buffer, '>');
            char* strAfterRightArrow = substr(strInlcudingRightArrow, 1, 0);
            if(isLineEndingValid(strAfterRightArrow) == 1){
                robust_send_string(fd, "Recipient: %s \n", recipient);
                if (is_valid_user(recipient, NULL) > 0) {
                    addRecipient(session, recipient);
                    robust_send_string(fd, "250 OK\n");
                } else {
                    robust_send_string(fd, "550 No such user here\n");
                }
            } else {
                robust_send_string(fd, "501-Invalid Syntax: <Bad Line Ending>\n");
            }
        } else {
            robust_send_string(fd, "501-Invalid Syntax: <RECIPIENT not formatted correctly>\n");
        }
    } else if (strncasecmp(buffer, "DATA", 4) == 0) {
        //  TODO IF THERE ARE MORE SPACES
        if (session->recipients == 0) {
            robust_send_string(fd, "554 No valid recipients\n");
        }
        else{
            session->state++;
            session->tempFileFD = mkstemp(session->tempFileName);
            robust_send_string(fd, "354 Start mail input; end with <CRLF>.<CRLF>\n");
        }
    } else if (strncasecmp(buffer, "QUIT", 4) == 0) {
        session->state = -1;
        robust_send_string(fd, "221 OK\n");
    } else if (strncasecmp(buffer, "NOOP ", 5) == 0) {
        robust_send_string(fd, "250 OK\n");
    } else if (strncasecmp(buffer, "RSET ",5) == 0) {
        robust_send_string(fd, "502-Command not Implemented\n");
    } else if (strncasecmp(buffer, "VRFY ",5) == 0) {
        robust_send_string(fd, "502-Command not Implemented\n");
    } else if (strncasecmp(buffer, "EXPN ",5) == 0) {
        robust_send_string(fd, "502-Command not Implemented\n");
    } else if (strncasecmp(buffer, "HELP ",5) == 0) {
        robust_send_string(fd, "502-Command not Implemented\n");
    } else if(strncasecmp(buffer, "EHLO ", 5) == 0) {
        robust_send_string(fd,"502-Command not Implemented\n");
    } else {
        robust_send_string(fd, "500-Invalid Syntax\n");
    }
}

void handle_state_three(int fd, struct smtp_session* session, char* buffer) {
    //Data collection state
    if(strncasecmp(buffer, ".", 1) == 0){
        save_user_mail(session->tempFileName, session->recipients);
        close(session->tempFileFD);
        unlink(session->tempFileName);
        session->state++;
    } else {
        write(session->tempFileFD, buffer, strlen(buffer));
    }
}

void handle_state_four(int fd, struct smtp_session* session, char* buffer) {
    if (strncasecmp(buffer, "HELO ", 5) == 0) {
        destroy_user_list(session->recipients);
        session->recipients = create_user_list();
        session->recipientNum = 0;
        session->senderDomainName = NULL;
        strncpy(session->tempFileName, file_name_template, strlen(file_name_template));    //reset value back to end with .XXXXXX
        session->state = 0;
        handle_state_zero(fd, session, buffer);
    } else if (strncasecmp(buffer, "MAIL FROM:<", 11) == 0) {
        session->state = 1;
        destroy_user_list(session->recipients);
        session->recipients = create_user_list();
        session->recipientNum = 0;
        strncpy(session->tempFileName, file_name_template, strlen(file_name_template));    //reset value back to end with .XXXXXX
        handle_state_one(fd, session, buffer);
    } else if (strncasecmp(buffer, "RCPT TO:<", 9) == 0) {
        robust_send_string(fd, "503 Bad Sequence of Commands\n");
    } else if (strncasecmp(buffer, "DATA", 4) == 0) {
        robust_send_string(fd, "503 Bad Sequence of Commands\n");
    } else if (strncasecmp(buffer, "QUIT", 4) == 0) {
        session->state = -1;
        robust_send_string(fd, "221 OK\n");
    } else if (strncasecmp(buffer, "NOOP", 4) == 0) {
        robust_send_string(fd, "250 OK\n");
    } else if (strncasecmp(buffer, "RSET", 4) == 0) {
        robust_send_string(fd, "502-Command not Implemented\n");
    } else if (strncasecmp(buffer, "VRFY", 4) == 0) {
        robust_send_string(fd, "502-Command not Implemented\n");
    } else if (strncasecmp(buffer, "EXPN",4) == 0) {
        robust_send_string(fd, "502-Command not Implemented\n");
    } else if (strncasecmp(buffer, "HELP", 4) == 0) {
        robust_send_string(fd, "502-Command not Implemented\n");
    } else if(strncasecmp(buffer, "EHLO", 4) == 0) {
        robust_send_string(fd,"502-Command not Implemented\n");
    } else {
        robust_send_string(fd, "500-Invalid Syntax\n");
    }
}