//
// Created by Ben on 11/22/2018.
//

#ifndef _SMTP_SESSION_H_
#define _SMTP_SESSION_H_

#include <string.h>

struct smtp_session {
    int state;
    char* senderDomainName;
    char* sender;
    int recipientNum;
    char* recipients[0];
};

struct smtp_session* smtp_session_create();
struct smtp_session * addRecipient(struct smtp_session* session, char* recipient, int fd);

#endif


