//
// Created by Ben on 11/22/2018.
//

#ifndef _SMTP_SESSION_H_
#define _SMTP_SESSION_H_

#include <string.h>
#include "mailuser.h"

struct smtp_session {
    int state;
    char* senderDomainName;
    char* serverDomainName;
    char* sender;
    int recipientNum;
    int tempFileFD;
    user_list_t recipients;
    char tempFileName[13];
};

struct smtp_session* smtp_session_create();
struct smtp_session * addRecipient(struct smtp_session* session, char* recipient);

#endif


