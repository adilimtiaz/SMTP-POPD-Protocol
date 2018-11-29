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
void handle_message(int fd, struct pop_session* session, char* buffer);
void handle_user_command(int fd, struct pop_session* session, char* username);
void handle_pass_command(int fd, struct pop_session* session, char* password);
void handle_stat_command(int fd, struct pop_session* session);
void handle_list_command(int fd, struct pop_session* session, char* mail_index);
void handle_dele_command(int fd, struct pop_session* session, char* dele_index);
void print_all_messages(int fd, mail_list_t messages);
void handle_rset_command(int fd, struct pop_session* session);
void handle_quit_command(int fd, struct pop_session* session);

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
        handle_message(fd, session, out);
    }
    free(session);
}

void handle_message(int fd, struct pop_session* session, char* buffer){
    char* command = strtok(buffer, " \n");

    if(strcasecmp(command, "USER") == 0){
        char* user_name = strtok(NULL, " \n");
        handle_user_command(fd, session, user_name);
    } else if(strcasecmp(command, "PASS") == 0){
        char* password = strtok(NULL, " \n");
        handle_pass_command(fd, session, password);
    } else if(strcasecmp(command, "STAT") == 0){
        //only if authorized
        if(session->is_authenticated > 0){
            handle_stat_command(fd, session);
        } else {
            send_string(fd, "-ERR must be authenticated to use STAT command\n");
        }
    }else if(strcasecmp(command, "LIST") == 0){
        if(session->is_authenticated > 0){
            char* mail_index = strtok(NULL, " \n");
            handle_list_command(fd, session, mail_index);
        } else {
            send_string(fd, "-ERR must be authenticated to use LIST command\n");
        }
    } else if(strcasecmp(command, "RETR") == 0){

    }else if(strcasecmp(command, "DELE") == 0){
        if(session->is_authenticated > 0){
            char* dele_index = strtok(NULL, " \n");
            handle_dele_command(fd, session, dele_index);
        } else {
            send_string(fd, "-ERR must be authenticated to use DELE command\n");
        }
    }else if(strcasecmp(command, "NOOP") == 0){
        if(session->is_authenticated){
            send_string(fd,"+OK");
        } else {
            send_string(fd, "-ERR must be authenticated to use NOOP command\n");
        }
    }else if(strcasecmp(command, "RSET") == 0){
        if(session->is_authenticated > 0){
            handle_rset_command(fd, session);
        } else {
            send_string(fd, "-ERR must be authenticated to use NOOP command\n");
        }
    }else if(strcasecmp(command, "QUIT") == 0){
        handle_quit_command(fd, session);
    } else {
        send_string(fd, "-ERR Invalid command\n");
    }
}

void handle_user_command(int fd, struct pop_session* session, char* username) {
    if(username == NULL){
        send_string(fd, "-ERR no username supplied\n");
    } else {
        if (is_valid_user(username, NULL)) {
            session->username = strdup(username);       //use strdup to avoid rewriting of these variables when using strtok
            send_string(fd, "+OK valid username: %s \n", username);
        } else {
            send_string(fd, "-ERR invalid username\n");
        }
    }
}

void handle_pass_command(int fd, struct pop_session* session, char* password){
    send_string(fd, "in handle_pass_command, password is : %s\n", password);
    if(password == NULL){
        send_string(fd, "-ERR no password supplied\n");
    } else if(session->username == NULL) {
        send_string(fd, "-ERR please specify a valid username first\n");
    } else {
        if(is_valid_user(session->username, password) > 0){
            session->is_authenticated = 1;
            session->messages = load_user_mail(session->username);
            send_string(fd, "+OK success, logged in as %s\n", session->username);
        } else {
            send_string(fd, "-ERR invalid password\n");
        }
    }
}

void handle_stat_command(int fd, struct pop_session* session){
    send_string(fd, "+OK %d message (%d octets)\n", get_mail_count(session->messages), (int)get_mail_list_size(session->messages));
}

void handle_list_command(int fd, struct pop_session* session, char* mail_index){
    int mail_count_total = get_mail_count(session->messages);
    if(mail_index == NULL){
        send_string(fd, "+OK %d message (%d octets)\n", mail_count_total, (int)get_mail_list_size(session->messages));
        print_all_messages(fd, session->messages);
    } else {
        int mail_index_int = atoi(mail_index) - 1;   //subtract 1 to revert back to 0 based indexing
        if(mail_index_int >= mail_count_total){
            send_string(fd, "-ERR no such message, only %d messages in maildrop\n", mail_count_total);
        } else if(mail_index_int + 1 <= 0) {
            send_string(fd, "-ERR mail index must be greater than 0\n");
        } else {
            mail_item_t mail_item = get_mail_item(session->messages, (unsigned int)mail_index_int);
            if(mail_item){
                send_string(fd, "%d %d\n", mail_index_int + 1, (int)get_mail_item_size(mail_item));
            } else {
                send_string(fd, "-ERR no such item\n");
            }
        }
    }
}

void print_all_messages(int fd, mail_list_t messages){
    int total_messages = get_mail_count(messages);
    int non_deleted_messages = 1;
    int current_index = 0;
    while(non_deleted_messages <= total_messages){
        mail_item_t mail_item = get_mail_item(messages, (unsigned int)current_index);
        if(mail_item){
            send_string(fd, "%d %d\n", current_index + 1, (int)get_mail_item_size(mail_item));
            non_deleted_messages++;
        }
        current_index++;
    }
}

void handle_dele_command(int fd, struct pop_session* session, char* dele_index){
    //TODO add checking if input is not a number
    if(dele_index == NULL){
        send_string(fd, "-ERR no mail index supplied to delete\n");
    } else {
        int dele_index_int = atoi(dele_index) - 1;  //subtract 1 to use 0 based indexing
        if(dele_index_int + 1 <= 0){
            send_string(fd, "-ERR mail index must be greater than 0\n");
        } else {
            mail_item_t mail_item = get_mail_item(session->messages, (unsigned int)dele_index_int);
            if(!mail_item) {
                send_string(fd, "-ERR no such item\n");
            } else {
                mark_mail_item_deleted(mail_item);
                send_string(fd, "+OK marked item %d for deletion\n", dele_index_int + 1);
            }
        }
    }
}

void handle_rset_command(int fd, struct pop_session* session){
    reset_mail_list_deleted_flag(session->messages);
    send_string(fd, "+OK maildrop has %d messages (%x octets)\n", get_mail_count(session->messages), (int)get_mail_list_size(session->messages));
}

void handle_quit_command(int fd, struct pop_session* session){
    if(session->is_authenticated > 0) {
        destroy_mail_list(session->messages);
    }
    session->has_quit = 1;
    send_string(fd, "+OK pop server signing off\n");
}