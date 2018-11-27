//
// Created by Ben on 11/26/2018.
//

#ifndef A3_POPSESSION_H
#define A3_POPSESSION_H

#include "mailuser.h"

struct pop_session * pop_session_create();

struct pop_session {
    int is_authenticated;
    int has_quit;
    char* username;
    char* password;
    mail_list_t messages;
};

#endif //A3_POPSESSION_H
