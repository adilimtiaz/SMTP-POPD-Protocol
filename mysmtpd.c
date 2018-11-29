#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"
#include "smtpsession.h"

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

    send_string(fd, "220 foo.com Simple Mail Transfer Service Ready\n");
    net_buffer_t buffer = nb_create(fd, 10000);

    while(session->state >= 0){
        send_string(fd, "state is: %d\n", session->state);
        char out[MAX_LINE_LENGTH];
        nb_read_line(buffer, out);
        handle_incoming_message(fd, session, out);
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
        // TODO FIX BUG WHERE YOU CAN HELO NOONE. I.E. COMMAND IS HELO .
        char* word = strtok(buffer, " "); // Ignore first word
        send_string(fd, "Word is %s", word);
        char* domainName = strtok(NULL, " "); // Domain name is second word
        send_string(fd, "Word is %s", domainName);
        send_string(fd, "bird is %s", strtok(NULL, " "));
        if(domainName == NULL) { //indicates no domain provided
            send_string(fd, "500-Invalid Syntax No Domain Name provided\n");
        } else {
            struct utsname system;
            if(uname(&system) != 0){
                // TODO figure out what error code should be
                send_string(fd, "Bad uname bro");
            }
            session->senderDomainName = domainName;
            session->state = 1; //transition to next state
            send_string(fd, "%s greets %s", system.sysname, domainName);
        }
    } else if(strcasecmp(buffer,"MAIL") == 0){
        send_string(fd, "503 Bad Sequence of Commands\n");
    } else if(strcasecmp(buffer, "RCPT") == 0){
        send_string(fd, "503 Bad Sequence of Commands\n");
    } else if(strcasecmp(buffer, "DATA") == 0){
        send_string(fd, "503 Bad Sequence of Commands\n");
    } else if(strcasecmp(buffer, "QUIT") == 0){
        session->state = -1;
        send_string(fd, "221 OK\n");
    } else if(strcasecmp(buffer, "NOOP") == 0){
        send_string(fd, "250 OK\n");
    } else if(strcasecmp(buffer, "RSET") == 0){
        send_string(fd, "502-Command not Implemented\n");
    } else if(strcasecmp(buffer, "VRFY") == 0){
        send_string(fd, "502-Command not Implemented\n");
    } else if(strcasecmp(buffer, "EXPN") == 0){
        send_string(fd, "502-Command not Implemented\n");
    } else if(strcasecmp(buffer, "HELP") == 0){
        send_string(fd,"502-Command not Implemented\n");
    } else if(strcasecmp(buffer, "EHLO") == 0) {
        send_string(fd,"502-Command not Implemented\n");
    }else {
        send_string(fd, "500-Invalid Syntax Coming From Here\n");
    }
}

// Taken from: https://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
// Note: This function removes all LEADING and TRAILING whitespaces, newlines etc. from a string.
char* trimwhitespace(char *str)
{
    char *end;

    // Trim leading space
    while(isspace((unsigned char)*str)) str++;

    if(*str == 0)  // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator character
    end[1] = '\0';

    return str;
}

// Cuts trailing whitespace and then checks if a newline char is present at the end of the line.
// If anything else is present in str, then it will return false
// Modifies input String
int isLineEndingValid(char *str){
    if(str == NULL){
        // No newline
        return 0;
    }

    str = trimwhitespace(str);
    return strlen(str) > 0 ? 0 : 1;
}

// Adapted from: https://cboard.cprogramming.com/c-programming/81565-substr.html
/*  substr("some string", 5, 0, NULL)
    returns "string"
    substr("some string", -5, 3, NULL)
    returns "str"
    substr("some string", 4, 0, "thing")
    returns "something"
 *
 */

char* substr (const char* string, int pos, int len)
{
    char* substring;
    int   i;
    int   length;

    if (string == NULL)
        return NULL;
    length = strlen(string);
    if (pos < 0) {
        pos = length + pos;
        if (pos < 0) pos = 0;
    }
    else if (pos > length) pos = length;
    if (len <= 0) {
        len = length - pos + len;
        if (len < 0) len = length - pos;
    }
    if (pos + len > length) len = length - pos;

    if ((substring = malloc(sizeof(*substring)*(len+1))) == NULL)
        return NULL;
    len += pos;
    for (i = 0; pos != len; i++, pos++)
        substring[i] = string[pos];
    substring[i] = '\0';

    return substring;
}

void handle_state_one(int fd, struct smtp_session* session, char* buffer){
    if(strcasecmp(buffer,"HELO") == 0){
        send_string(fd, "503 Bad Sequence of Commands\n");
    } else if(strncasecmp(buffer,"MAIL FROM:<", 10) == 0){
        // 62 is '>'
        if(strchr(buffer, 62) != NULL){
            char* bufCopy = strdup(buffer);
            strtok(bufCopy, "<");
            char* recipient = strtok(NULL, ">");
            char* strInlcudingRightArrow = strchr(buffer, 62);
            char* strAfterRightArrow = substr(strInlcudingRightArrow, 1, 0); //
            if(isLineEndingValid(strAfterRightArrow) == 1){
                session->sender = recipient;
                send_string(fd, "250 OK\n");
                session->state = 2;
            }
            else
                send_string(fd, "500-Invalid Syntax: Extra characters found after recipient mail\n");
        } else {
            // Mail provided without proper <Address>
            send_string(fd, "500-Invalid Syntax: <Address> not formatted correctly\n");
        }
    } else if(strcasecmp(buffer, "RCPT") == 0){
        send_string(fd, "503 Bad Sequence of Commands\n");
    } else if(strcasecmp(buffer, "DATA") == 0){
        send_string(fd, "503 Bad Sequence of Commands\n");
    } else if(strcasecmp(buffer, "QUIT") == 0){
        session->state = -1;
        send_string(fd, "221 OK\n");
    } else if(strcasecmp(buffer, "NOOP") == 0){
        send_string(fd, "250 OK\n");
    } else if(strcasecmp(buffer, "RSET") == 0){
        send_string(fd, "502-Command not Implemented\n");
    } else if(strcasecmp(buffer, "VRFY") == 0){
        send_string(fd, "502-Command not Implemented\n");
    } else if(strcasecmp(buffer, "EXPN") == 0){
        send_string(fd, "502-Command not Implemented\n");
    } else if(strcasecmp(buffer, "HELP") == 0){
        send_string(fd, "502-Command not Implemented\n");
    } else if(strcasecmp(buffer, "EHLO") == 0) {
        send_string(fd,"502-Command not Implemented\n");
    } else {
        send_string(fd, "500-Invalid Syntax\n");
    }
}

void handle_state_two(int fd, struct smtp_session* session, char* buffer) {
    char *code = strtok(buffer, " \n");
    if (strcasecmp(code, "HELO") == 0) {
    } else if (strcasecmp(code, "MAIL") == 0) {
        send_string(fd, "503 Bad Sequence of Commands\n");
    } else if (strcasecmp(code, "RCPT") == 0) {
        if (strcasecmp(strtok(NULL, "<"), "TO:") != 0) {
                send_string(fd, "500-Invalid Syntax");
            } else {
            char *recipient = strtok(NULL, ">");
            if (is_valid_user(recipient, NULL) > 0) {
                addRecipient(session, recipient);
                send_string(fd, "250 OK\n");
            } else {
                send_string(fd, "550 No such user here\n");
            }
        }
    } else if (strcasecmp(code, "DATA") == 0) {
        if (session->recipients == 0) {
            send_string(fd, "554 No valid recipients\n");
        }
        else{
            session->state++;
            session->tempFileFD = mkstemp(session->tempFileName);
            send_string(fd, "354 Start mail input; end with <CRLF>.<CRLF>\n");
        }
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

