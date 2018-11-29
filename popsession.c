//
// Created by Ben on 11/26/2018.
//

#include <stdlib.h>
#include "popsession.h"

struct pop_session * pop_session_create(){
    struct pop_session * session = malloc(sizeof(struct pop_session));
    session->has_quit = 0;
    session->is_authenticated = 0;
    session->username = NULL;
    session->messages = NULL;
    return session;
}
