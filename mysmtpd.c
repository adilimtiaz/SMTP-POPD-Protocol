#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"
#include "smtpsession.h"
#include "helpers.h"

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

char file_name_template[] = "./mail.XXXXXX";

int main(int argc, char *argv[]) {
  
  if (argc != 2) {
    fprintf(stderr, "Invalid arguments. Expected: %s <port>\n", argv[0]);
    return 1;
  }
  
  run_server(argv[1], handle_client);
  
  return 0;
}

void handle_client(int fd) {
    struct smtp_session* session = smtp_session_create();

    net_buffer_t buffer = nb_create(fd, 10000);

    struct utsname system;
    if(uname(&system) != 0){
        // TODO figure out what error code should be
        send_string(fd, "Bad uname bro");
    }
    session->serverDomainName = system.nodename;
    send_string(fd, "220 %s Simple Mail Transfer Service Ready\n", session->serverDomainName);

    while(session->state >= 0){
        send_string(fd, "state is: %d\n", session->state);
        char out[MAX_LINE_LENGTH + 1];
        if(nb_read_line(buffer, out) > 0) {
            if(endsWithNewline(out) == 0) {
                send_string(fd, "500 - Line Too Long");
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
            send_string(fd, "Went to default case in handle_incoming_message\n");
            break;
    }
}

void handle_state_zero(int fd, struct smtp_session* session, char* buffer) {
    if(strncasecmp(buffer,"HELO ", 5) == 0){
        char* bufCopy = strdup(buffer);
        strtok(bufCopy, " "); // Ignore first word
        char* domainName = strtok(NULL, " "); // Domain name is second word
        if(isWord(domainName) == 0) { //indicates no domain provided
            send_string(fd, "500-Invalid Syntax No Domain Name provided\n");
        } else {
            char* lineEnding = substr(strdup(buffer), (int)(5 + strlen(domainName)), 0);
            if(isLineEndingValid(lineEnding) == 1){
                session->senderDomainName = domainName;
                session->state = 1; //transition to next state
                send_string(fd, "%s greets %s\n", session->serverDomainName, domainName);
            }
            else
                send_string(fd, "500-Invalid Syntax: Invalid Line Ending\n");
        }
    } else if(strncasecmp(buffer,"MAIL FROM:<", 10) == 0){
        send_string(fd, "503 Bad Sequence of Commands\n");
    } else if(strncasecmp(buffer, "RCPT TO:<",7) == 0){
        send_string(fd, "503 Bad Sequence of Commands\n");
    } else if(strncasecmp(buffer, "DATA", 4) == 0){
        send_string(fd, "503 Bad Sequence of Commands\n");
    } else if(strncasecmp(buffer, "QUIT", 4) == 0){
        session->state = -1;
        send_string(fd, "221 OK\n");
    } else if(strncasecmp(buffer, "NOOP", 4) == 0){
        send_string(fd, "250 OK\n");
    } else if(strncasecmp(buffer, "RSET",4) == 0){
        send_string(fd, "502-Command not Implemented\n");
    } else if(strncasecmp(buffer, "VRFY ", 5) == 0){
        send_string(fd, "502-Command not Implemented\n");
    } else if(strncasecmp(buffer, "EXPN ", 5) == 0){
        send_string(fd, "502-Command not Implemented\n");
    } else if(strncasecmp(buffer, "HELP", 4) == 0){
        send_string(fd,"502-Command not Implemented\n");
    } else if(strncasecmp(buffer, "EHLO ", 5) == 0) {
        send_string(fd,"502-Command not Implemented\n");
    }else {
        send_string(fd, "500-Invalid Syntax Coming From Here\n");
    }
}

void handle_state_one(int fd, struct smtp_session* session, char* buffer){
    if(strncasecmp(buffer,"HELO ", 5) == 0){
        send_string(fd, "503 Bad Sequence of Commands\n");
    } else if(strncasecmp(buffer,"MAIL FROM:<", 10) == 0){
        if(strchr(buffer, '>') != NULL){
            char* bufCopy = strdup(buffer);
            strtok(bufCopy, "<");
            char* recipient = strtok(NULL, ">");
            if(isWord(recipient) == 0){
                // TODO: Check if 500 or 501
                send_string(fd, "501-Invalid Argument: <Address> is empty\n");
                return;
            }
            char* strInlcudingRightArrow = strchr(buffer, '>');
            char* strAfterRightArrow = substr(strInlcudingRightArrow, 1, 0);
            if(isLineEndingValid(strAfterRightArrow) == 1){
                session->sender = recipient;
                send_string(fd, "250 OK\n");
                session->state = 2;
            }
            else
                send_string(fd, "500-Invalid Syntax: Invalid Line Ending\n");
        } else {
            // Mail provided without proper <Address>
            send_string(fd, "501-Invalid Argument: <Address> not formatted correctly\n");
        }
    } else if(strncasecmp(buffer, "RCPT TO:<", 7) == 0){
        send_string(fd, "503 Bad Sequence of Commands\n");
    } else if(strncasecmp(buffer, "DATA", 4) == 0){
        send_string(fd, "503 Bad Sequence of Commands\n");
    } else if(strncasecmp(buffer, "QUIT", 4) == 0){
        session->state = -1;
        send_string(fd, "221 OK\n");
    } else if(strncasecmp(buffer, "NOOP", 4) == 0){
        send_string(fd, "250 OK\n");
    } else if(strncasecmp(buffer, "RSET", 4) == 0){
        send_string(fd, "502-Command not Implemented\n");
    } else if(strncasecmp(buffer, "VRFY ", 5) == 0){
        send_string(fd, "502-Command not Implemented\n");
    } else if(strncasecmp(buffer, "EXPN ", 5) == 0){
        send_string(fd, "502-Command not Implemented\n");
    } else if(strncasecmp(buffer, "HELP", 4) == 0){
        send_string(fd, "502-Command not Implemented\n");
    } else if(strncasecmp(buffer, "EHLO ",5) == 0) {
        send_string(fd,"502-Command not Implemented\n");
    } else {
        send_string(fd, "500-Invalid Syntax\n");
    }
}

void handle_state_two(int fd, struct smtp_session* session, char* buffer) {
    if (strncasecmp(buffer, "HELO ", 5) == 0) {

    } else if (strncasecmp(buffer, "MAIL FROM:<", 10) == 0) {
        send_string(fd, "503 Bad Sequence of Commands\n");
    } else if (strncasecmp(buffer, "RCPT TO:<", 7) == 0) {
        if(strchr(buffer, '>') != NULL) {
            char *bufCopy = strdup(buffer);
            strtok(bufCopy, "<");
            char *recipient = strtok(NULL, ">");
            char* strInlcudingRightArrow = strchr(buffer, '>');
            char* strAfterRightArrow = substr(strInlcudingRightArrow, 1, 0);
            if(isLineEndingValid(strAfterRightArrow) == 1){
                if (is_valid_user(recipient, NULL) > 0) {
                    addRecipient(session, recipient);
                    send_string(fd, "250 OK\n");
                } else {
                    send_string(fd, "550 No such user here\n");
                }
            }
        } else {
            send_string(fd, "500-Invalid Syntax: <RECIPIENT not formatted correctly>\n");
        }
    } else if (strcasecmp(buffer, "DATA") == 0) {
        if (session->recipients == 0) {
            send_string(fd, "554 No valid recipients\n");
        }
        else{
            session->state++;
            session->tempFileFD = mkstemp(session->tempFileName);
            send_string(fd, "354 Start mail input; end with <CRLF>.<CRLF>\n");
        }
    } else if (strncasecmp(buffer, "QUIT", 4) == 0) {
        session->state = -1;
        send_string(fd, "221 OK\n");
    } else if (strncasecmp(buffer, "NOOP ", 5) == 0) {
        send_string(fd, "250 OK\n");
    } else if (strncasecmp(buffer, "RSET ",5) == 0) {
        send_string(fd, "502-Command not Implemented\n");
    } else if (strncasecmp(buffer, "VRFY ",5) == 0) {
        send_string(fd, "502-Command not Implemented\n");
    } else if (strncasecmp(buffer, "EXPN ",5) == 0) {
        send_string(fd, "502-Command not Implemented\n");
    } else if (strncasecmp(buffer, "HELP ",5) == 0) {
        send_string(fd, "502-Command not Implemented\n");
    } else if(strncasecmp(buffer, "EHLO ", 5) == 0) {
        send_string(fd,"502-Command not Implemented\n");
    } else {
        send_string(fd, "500-Invalid Syntax\n");
    }
}

void handle_state_three(int fd, struct smtp_session* session, char* buffer) {
    //Data collection state
    if(strcasecmp(buffer, ".\n") == 0){
        save_user_mail(session->tempFileName, session->recipients);
        close(session->tempFileFD);
        unlink(session->tempFileName);
        session->state++;
    } else {
        write(session->tempFileFD, buffer, strlen(buffer));
    }
}

void handle_state_four(int fd, struct smtp_session* session, char* buffer) {
    char* copyBuff = strdup(buffer);    //Use this in case the client decides to send another message and move the state back to state 1
    char *code = strtok(buffer, " \n");
    if (strcasecmp(code, "HELO") == 0) {
        send_string(fd, "503 Bad Sequence of Commands\n");
    } else if (strcasecmp(code, "MAIL") == 0) {
        session->state = 1;
        destroy_user_list(session->recipients);
        session->recipients = create_user_list();
        session->recipientNum = 0;
        strncpy(session->tempFileName, file_name_template, strlen(file_name_template));    //reset value back to end with .XXXXXX
        handle_state_one(fd, session, copyBuff);
    } else if (strcasecmp(code, "RCPT") == 0) {
        send_string(fd, "503 Bad Sequence of Commands\n");
    } else if (strcasecmp(code, "DATA") == 0) {
        send_string(fd, "503 Bad Sequence of Commands\n");
    } else if (strcasecmp(code, "QUIT") == 0) {
        session->state = -1;
        send_string(fd, "221 OK\n");
    } else if (strcasecmp(code, "NOOP") == 0) {
        send_string(fd, "250 OK\n");
    } else if (strcasecmp(code, "RSET") == 0) {
        send_string(fd, "502-Command not Implemented\n");
    } else if (strcasecmp(code, "VRFY") == 0) {
        send_string(fd, "502-Command not Implemented\n");
    } else if (strcasecmp(code, "EXPN") == 0) {
        send_string(fd, "502-Command not Implemented\n");
    } else if (strcasecmp(code, "HELP") == 0) {
        send_string(fd, "502-Command not Implemented\n");
    } else if(strcasecmp(code, "EHLO") == 0) {
        send_string(fd,"502-Command not Implemented\n");
    } else {
        send_string(fd, "500-Invalid Syntax\n");
    }
    free(copyBuff);
}

