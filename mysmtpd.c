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
        char out[1025];
        nb_read_line(buffer, out);
        handle_incoming_message(fd, session, out);
    }
}

void handle_incoming_message(int fd, struct smtp_session* session, char *buffer){
    switch(session->state) {
        case 0:
            //initialization state
            handle_state_zero(fd, session, buffer);
            break;
        case 1:
            //client has been identified, should be receiving MAIL code
            handle_state_one(fd, session, buffer);
            break;
        case 2:
            //Client has requested to send mail, should be receiving Receipient
            handle_state_two(fd, session, buffer);
            break;
        case 3:
            //Client has specified recipeint, expecting DATA now
            handle_state_three(fd, session, buffer);
            break;
        case 4:
            //Client is sending that sweet sweet data
//            handle_state_four(fd, state, buffer);
            break;
        case 5:
            //Client has finished sending data, might quit now or send more stuff
//            handle_state_five(fd, state, buffer);
            break;
        default:
            send_string(fd, "Went to default case in handle_incoming_message");
            break;
    }
}

void handle_state_zero(int fd, struct smtp_session* session, char* buffer) {
    char* code = strtok(buffer, " \n");           //Gets the code submitted by the client ex: HELO, MAIL ect...
    if(strcmp(code, "EHLO") == 0 || strcmp(code,"HELO") == 0){
        char* domainName = strtok(NULL, " ");
        if(strtok(NULL, " ") != NULL) { //indicates that there is extra text after the domain name
            send_string(fd, "500-Invalid Syntax\n");
        } else {
            session->senderDomainName = domainName;
            session->state = 1; //transition to next state
            send_string(fd, "250-foo.com greets %s", domainName);
        }
    } else if(strcmp(code,"MAIL") == 0){

    } else if(strcmp(code, "RCPT") == 0){

    } else if(strcmp(code, "DATA") == 0){

    } else if(strcmp(code, "QUIT") == 0){

    } else if(strcmp(code, "NOOP") == 0){

    } else if(strcmp(code, "RSET") == 0){
        send_string(fd, "502-Command not Implemented");
    } else if(strcmp(code, "VRFY") == 0){
        send_string(fd, "502-Command not Implemented");
    } else if(strcmp(code, "EXPN") == 0){
        send_string(fd, "502-Command not Implemented");
    } else if(strcmp(code, "HELP") == 0){
        send_string(fd,"502-Command not Implemented");
    } else {
        send_string(fd, "500-Invalid Syntax");
    }
}

void handle_state_one(int fd, struct smtp_session* session, char* buffer){
    char* code = strtok(buffer, " \n");           //Gets the code submitted by the client ex: HELO, MAIL ect...

    if(strcmp(code, "EHLO") == 0 || strcmp(code,"HELO") == 0){
    } else if(strcmp(code,"MAIL") == 0){
        if(strcmp(strtok(NULL, "<"), "FROM:") != 0){
            send_string(fd, "500-Invalid Syntax");
        } else {
            char *recipient = strtok(NULL, ">");
            session->sender = recipient;
            send_string(fd, "250 OK\n");
            session->state = 2;
        }
    } else if(strcmp(code, "RCPT") == 0){

    } else if(strcmp(code, "DATA") == 0){

    } else if(strcmp(code, "QUIT") == 0){

    } else if(strcmp(code, "NOOP") == 0){

    } else if(strcmp(code, "RSET") == 0){
        send_string(fd, "502-Command not Implemented");
    } else if(strcmp(code, "VRFY") == 0){
        send_string(fd, "502-Command not Implemented");
    } else if(strcmp(code, "EXPN") == 0){
        send_string(fd, "502-Command not Implemented");
    } else if(strcmp(code, "HELP") == 0){
        send_string(fd,"502-Command not Implemented");
    } else {
        send_string(fd, "500-Invalid Syntax");
    }
}

void handle_state_two(int fd, struct smtp_session* session, char* buffer) {
    send_string(fd, "In state two\n");
    char *code = strtok(buffer, " \n");           //Gets the code submitted by the client ex: HELO, MAIL ect...
    send_string(fd, "Code is: %s\n", code);

    if (strcmp(code, "EHLO") == 0 || strcmp(code, "HELO") == 0) {
    } else if (strcmp(code, "MAIL") == 0) {

    } else if (strcmp(code, "RCPT") == 0) {
        send_string(fd, "In the if for RCPT\n");
        if (strcmp(strtok(NULL, "<"), "TO:") != 0) {
            send_string(fd, "500-Invalid Syntax");
        } else {
            char *recipient = strtok(NULL, ">");
            if(is_valid_user(recipient,NULL) > 0){
                addRecipient(session, recipient);
                send_string(fd, "did it update userList: %d\n", session->recipientNum);
                send_string(fd, "250 OK\n");
            } else {
                send_string(fd, "550 No such user here\n");
            }
        }
    } else if (strcmp(code, "DATA") == 0) {
        //if(session->recipients == 0){//TODO}
        session->state++;
        send_string(fd, "%s\n", session->tempFileName);
        session->tempFileFD = mkstemp(session->tempFileName);
        send_string(fd, "session template: %s\n", session->tempFileName);
        send_string(fd, "354 Start mail input; end with <CRLF>.<CRLF>\n");
    } else if (strcmp(code, "QUIT") == 0) {

    } else if (strcmp(code, "NOOP") == 0) {

    } else if (strcmp(code, "RSET") == 0) {
        send_string(fd, "502-Command not Implemented");
    } else if (strcmp(code, "VRFY") == 0) {
        send_string(fd, "502-Command not Implemented");
    } else if (strcmp(code, "EXPN") == 0) {
        send_string(fd, "502-Command not Implemented");
    } else if (strcmp(code, "HELP") == 0) {
        send_string(fd, "502-Command not Implemented");
    } else {
        send_string(fd, "500-Invalid Syntax");
    }
}

void handle_state_three(int fd, struct smtp_session* session, char* buffer) {
    //Data collection state
    if(strcmp(buffer, ".\n") == 0){
        send_string(fd, "got terminating character in data read phase\n");
        send_string(fd, "the tempFIleName in the session is: %s\n", session->tempFileName);
        save_user_mail(session->tempFileName, session->recipients);
        close(session->tempFileFD);
        unlink(session->tempFileName);
        session->state++;
    } else {
        write(session->tempFileFD, buffer, strlen(buffer));
    }
}

