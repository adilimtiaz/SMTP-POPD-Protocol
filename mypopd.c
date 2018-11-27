#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"
#include "popsession.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024

static void handle_client(int fd);
void handle_message(int fd, struct pop_session* session);

int main(int argc, char *argv[]) {
  
  if (argc != 2) {
    fprintf(stderr, "Invalid arguments. Expected: %s <port>\n", argv[0]);
    return 1;
  }
  
  run_server(argv[1], handle_client);
  
  return 0;
}

void handle_client(int fd) {
    struct pop_session* session = pop_session_create();
    send_string(fd, "+OK POP3 server\n");
    net_buffer_t buffer = nb_create(fd, 10000);

    while(session->has_quit == 0){
        char out[MAX_LINE_LENGTH];
        nb_read_line(buffer, out);
        handle_message(fd, session);
    }
}

void handle_message(int fd, struct pop_session* session){

}
