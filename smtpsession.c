//
// Created by Ben on 11/22/2018.
//
#include "smtpsession.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

struct smtp_session* smtp_session_create(int recipientNum){
    struct smtp_session* session = malloc(sizeof(struct smtp_session));
    session->state = 0;
    session->senderDomainName = NULL;
    session->sender = NULL;
    session->recipientNum = 0;
    session->tempFileFD = -1;
    session->recipients = create_user_list();
    session->serverDomainName = NULL;
    char tmpFileName[] = "./mail.XXXXXX";
    memcpy(session->tempFileName, tmpFileName, strlen(tmpFileName));
    return session;
}

//Creates a new smtp_session with the new recipient tacked on
//struct smtp_session * addRecipient(struct smtp_session* session, char* recipient){
//    session->recipientNum++;
//    struct smtp_session* updated_session = realloc(session, sizeof(session) + sizeof(char*));
//    updated_session->recipients[updated_session->recipientNum - 1] = recipient;
//    return updated_session;
//}

struct smtp_session * addRecipient(struct smtp_session* session, char* recipient){
    session->recipientNum++;
    add_user_to_list(&(session->recipients), recipient);
    return session;
}



