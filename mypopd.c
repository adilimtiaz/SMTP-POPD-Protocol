#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"
#include "popsession.h"
#include "helpers.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024

static void handle_client(int fd);
void handle_message(int fd, struct pop_session* session, char* buffer);
void handle_user_command(int fd, struct pop_session* session, char* buffer);
void handle_pass_command(int fd, struct pop_session* session, char* buffer);
void handle_stat_command(int fd, struct pop_session* session, char* buffer);
void handle_list_command(int fd, struct pop_session* session, char* buffer);
void handle_dele_command(int fd, struct pop_session* session, char* dele_index);
void print_all_messages(int fd, mail_list_t messages);
void handle_rset_command(int fd, struct pop_session* session, char* buffer);
void handle_quit_command(int fd, struct pop_session* session, char* buffer);
void handle_retr_command(int fd, struct pop_session* session, char* buffer);
void read_mail_file(int fd, struct pop_session* session, const char* file_name);
void handle_noop_command(int fd, char* buffer);

int sendStringFailed = 0;

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Invalid arguments. Expected: %s <port>\n", argv[0]);
        return 1;
    }

    run_server(argv[1], handle_client);

    return 0;
}

int robust_send_string(int fd, char* str, ...){
    va_list args;
    static char *buf = NULL;
    static int bufsize = 0;
    int strsize;

    // Start with string length, increase later if needed
    if (bufsize < strlen(str) + 1) {
        bufsize = strlen(str) + 1;
        buf = realloc(buf, bufsize);
    }
    while(1){
        va_start(args, str);
        strsize = vsnprintf(buf, bufsize, str, args);
        va_end(args);

        // If buffer was enough to fit entire string, send it
        if (strsize <= bufsize) {
            if (send_all(fd, buf, strsize) < 0) {
                sendStringFailed = 1;
            }
            return 0;
        }

        // Try again with more space
        bufsize = strsize + 1;
        buf = realloc(buf, bufsize);
    }
}

void handle_client(int fd) {
    struct pop_session* session = pop_session_create();
    robust_send_string(fd, "+OK POP3 server\n");
    net_buffer_t buffer = nb_create(fd, 10000);
    while(session->has_quit == 0 && sendStringFailed == 0){
        char out[MAX_LINE_LENGTH];
        nb_read_line(buffer, out);
        handle_message(fd, session, out);
    }
    free(session);
}

void handle_message(int fd, struct pop_session* session, char* buffer){
    if(strncasecmp(buffer, "USER ", 5) == 0){
        handle_user_command(fd, session, buffer);
    } else if(strncasecmp(buffer, "PASS ", 5) == 0){
        handle_pass_command(fd, session, buffer);
    } else if(strncasecmp(buffer, "STAT", 4) == 0){
        if(session->is_authenticated > 0){
            handle_stat_command(fd, session, buffer);
        } else {
            robust_send_string(fd, "-ERR must be authenticated to use STAT command\n");
        }
    }else if(strncasecmp(buffer, "LIST", 4) == 0){
        if(session->is_authenticated > 0){
            handle_list_command(fd, session, buffer);
        } else {
            robust_send_string(fd, "-ERR must be authenticated to use LIST command\n");
        }
    } else if(strncasecmp(buffer, "RETR ", 5) == 0){
        if(session->is_authenticated > 0) {
            handle_retr_command(fd, session, buffer);
        } else {
            robust_send_string(fd, "-ERR must be authenticated to use RETR command\n");
        }
    }else if(strncasecmp(buffer, "DELE ", 5) == 0){
        if(session->is_authenticated > 0){
            handle_dele_command(fd, session, buffer);
        } else {
            robust_send_string(fd, "-ERR must be authenticated to use DELE command\n");
        }
    }else if(strncasecmp(buffer, "NOOP", 4) == 0){
        if(session->is_authenticated){
            handle_noop_command(fd, buffer);
        } else {
            robust_send_string(fd, "-ERR must be authenticated to use NOOP command\n");
        }
    }else if(strncasecmp(buffer, "RSET", 4) == 0){
        if(session->is_authenticated > 0){
            handle_rset_command(fd, session, buffer);
        } else {
            robust_send_string(fd, "-ERR must be authenticated to use RSET command\n");
        }
    }else if(strncasecmp(buffer, "QUIT", 4) == 0){
        handle_quit_command(fd, session, buffer);
    } else {
        robust_send_string(fd, "-ERR Invalid command\n");
    }
}

void handle_user_command(int fd, struct pop_session* session, char* buffer) {
    if(session->is_authenticated > 0){
        robust_send_string(fd, "-ERR already logged in, please quit before issuing USER command\n");
    } else {
        char* bufCopy = strdup(buffer);
        strtok(bufCopy," ");     //ignore first word
        char* user_name = strtok(NULL, " ");
        if (isWord(user_name) == 0) {
            robust_send_string(fd, "-ERR no username supplied\n");
        } else {
            char* lineEnding = substr(buffer, (int)(5 + strlen(user_name)), 0);
            if(isLineEndingValid(lineEnding) == 1){
                if (is_valid_user(user_name, NULL)) {
                    session->username = strdup(
                            user_name);       //use strdup to avoid rewriting of these variables when using strtok
                    robust_send_string(fd, "+OK valid username: %s \n", user_name);
                } else {
                    robust_send_string(fd, "-ERR invalid username\n");
                }
            } else {
                robust_send_string(fd, "-ERR invalid input\n");
            }
        }
    }
}

void handle_pass_command(int fd, struct pop_session* session, char* buffer) {
    if (session->is_authenticated > 0) {
        robust_send_string(fd, "-ERR already logged in, please quit before issuing PASS command\n");
    } else {
        if (session->username == NULL) {
            robust_send_string(fd, "-ERR please specify a valid username first\n");
        }
        else {
            char* bufCopy = strdup(buffer);
            strtok(bufCopy, " "); // Ignore first word
            char* password = strtok(NULL, " "); // Domain name is second word
            if(isWord(password) == 0) { //indicates no domain provided
                robust_send_string(fd, "500-Invalid syntax no password provided\n");
            } else {
                char* lineEnding = substr(strdup(buffer), (int)(5 + strlen(password)), 0);
                if(isLineEndingValid(lineEnding) == 1) {
                    if(is_valid_user(session->username, password) > 0) {
                        session->is_authenticated = 1;
                        session->messages = load_user_mail(session->username);
                        robust_send_string(fd, "+OK success, logged in as %s\n", session->username);
                    } else {
                        robust_send_string(fd, "-ERR invalid password\n");
                    }
                } else {
                    robust_send_string(fd, "500-Invalid Syntax Invalid Line Ending\n");
                }
            }
        }
    }
}

void handle_stat_command(int fd, struct pop_session* session, char* buffer){
    char* lineEnding = substr(strdup(buffer), 4, 0);
    if(isLineEndingValid(lineEnding) == 1) {
        robust_send_string(fd, "+OK %d message (%d octets)\n", get_mail_count(session->messages),
                    (int) get_mail_list_size(session->messages));
    } else {
        robust_send_string(fd, "-ERR STAT command requries no additional parameters\n");
    }
}

void handle_list_command(int fd, struct pop_session* session, char* buffer){
    char* bufCopy = strdup(buffer);
    char* hasNoParam = substr(bufCopy, 4, 0);
    if(isLineEndingValid(hasNoParam)){
        robust_send_string(fd, "+OK %d message (%d octets)\n", get_mail_count(session->messages), (int)get_mail_list_size(session->messages));
        print_all_messages(fd, session->messages);
    } else {
        strtok(bufCopy, " "); // Ignore first word
        char* mail_index = strtok(NULL, " "); // mail_index is second word
        int mail_index_int = (int)(strtol(mail_index, NULL, 10) - 1);   //subtract 1 to revert back to 0 based indexing
        if(mail_index_int < 0){
            robust_send_string(fd, "-ERR mail index must be a positive number\n");
        } else {
            if (mail_index_int + 1 <= 0) {
                robust_send_string(fd, "-ERR mail index must be greater than 0\n");
            } else {
                mail_item_t mail_item = get_mail_item(session->messages, (unsigned int) mail_index_int);
                if (mail_item) {
                    robust_send_string(fd, "+OK %d %d\n", mail_index_int + 1, (int) get_mail_item_size(mail_item));
                } else {
                    robust_send_string(fd, "-ERR no such item\n");
                }
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
            robust_send_string(fd, "%d %d\n", current_index + 1, (int)get_mail_item_size(mail_item));
            non_deleted_messages++;
        }
        current_index++;
    }
    robust_send_string(fd, ".\n");
}

void handle_dele_command(int fd, struct pop_session* session, char* buffer){
    char* bufCopy = strdup(buffer);
    strtok(bufCopy, " "); // Ignore first word
    char* dele_index = strtok(NULL, " "); // delete index is second word
    if(isWord(dele_index) == 0){
        robust_send_string(fd, "-ERR no mail index supplied to delete\n");
    } else {
        char* lineEnding = substr(buffer, (int)(5 + strlen(dele_index)), 0);
        if(isLineEndingValid(lineEnding) == 1) {
            int dele_index_int = atoi(dele_index) - 1;  //subtract 1 to use 0 based indexing
            if (dele_index_int + 1 <= 0) {
                robust_send_string(fd, "-ERR mail index must be greater than 0\n");
            } else {
                mail_item_t mail_item = get_mail_item(session->messages, (unsigned int) dele_index_int);
                if (!mail_item) {
                    robust_send_string(fd, "-ERR no such item\n");
                } else {
                    mark_mail_item_deleted(mail_item);
                    robust_send_string(fd, "+OK marked item %d for deletion\n", dele_index_int + 1);
                }
            }
        } else {
            robust_send_string(fd, "-ERR DELE command requires no additional parameters\n");
        }
    }
}

void handle_rset_command(int fd, struct pop_session* session, char* buffer){
    char* lineEnding = substr(strdup(buffer), 4, 0);
    if(isLineEndingValid(lineEnding) == 1){
        reset_mail_list_deleted_flag(session->messages);
        robust_send_string(fd, "+OK maildrop has %d messages (%d octets)\n",
                    get_mail_count(session->messages),
                    (int)get_mail_list_size(session->messages));
    } else {
        robust_send_string(fd, "-ERR RSET command requires no additional parameters\n");
    }
}

void handle_noop_command(int fd, char* buffer){
    char* lineEnding = substr(strdup(buffer), 4, 0);
    if(isLineEndingValid(lineEnding) == 1){
        robust_send_string(fd, "+OK\n");
    } else {
        robust_send_string(fd, "-ERR NOOP command requires no additional parameters\n");
    }
}

void handle_quit_command(int fd, struct pop_session* session,  char* buffer){
    char* lineEnding = substr(strdup(buffer), 4, 0);
    if(isLineEndingValid(lineEnding) == 1){
        if(session->is_authenticated > 0) {
            destroy_mail_list(session->messages);
        }
        session->has_quit = 1;
        robust_send_string(fd, "+OK pop server signing off\n");
    } else {
        robust_send_string(fd, "-ERR QUIT command requires no additional parameters\n");
    }
}

void handle_retr_command(int fd, struct pop_session* session, char* buffer){
    char* bufCopy = strdup(buffer);
    strtok(bufCopy, " "); // Ignore first word
    char* mail_index = strtok(NULL, " "); // Domain name is second word
    if(isWord(mail_index) == 0){
        robust_send_string(fd, "-ERR no mail index supplied to retrieve\n");
    } else {
        char* lineEnding = substr(buffer, (int)(5 + strlen(mail_index)), 0);
        if(isLineEndingValid(lineEnding) == 1) {
            int retr_index_int = atoi(mail_index) - 1;  //subtract 1 to use 0 based indexing
            if (retr_index_int + 1 <= 0) {
                robust_send_string(fd, "-ERR mail index must be a number greater than 0\n");
            } else {
                mail_item_t mail_item = get_mail_item(session->messages, (unsigned int) retr_index_int);
                if (!mail_item) {
                    robust_send_string(fd, "-ERR no such item\n");
                } else {
                    const char *file_name = get_mail_item_filename(mail_item);
                    robust_send_string(fd, "+OK %d octets\n", (int) get_mail_item_size(mail_item));
                    read_mail_file(fd, session, file_name);
                }
            }
        } else {
            robust_send_string(fd, "-ERR RETR command requires no additional parameters\n");
        }
    }
}

void read_mail_file(int fd, struct pop_session* session, const char* file_name){
    //SRC: https://stackoverflow.com/questions/3463426/in-c-how-should-i-read-a-text-file-and-print-all-strings
    char * buf = malloc(MAX_LINE_LENGTH);
    FILE *file;
    size_t nread;
    file = fopen(file_name, "r");
    if(file) {
        while((nread = fread(buf, 1, sizeof(buf), file)) > 0){
            send_all(fd, buf, nread);
        }
        if(ferror(file)){
            robust_send_string(fd, "-ERR unable to read file\n");
        }
    }
    fclose(file);
    robust_send_string(fd, "\n.\n");
}
