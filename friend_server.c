#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// also include friends.h here
#include "friends.h"

#ifndef PORT
  #define PORT 569165
#endif
#define MAX_BACKLOG 5
#define BUF_SIZE 128

// from friendme.c
#define INPUT_BUFFER_SIZE 256
#define INPUT_ARG_MAX_NUM 12
#define DELIM " \n"

#define MAX_USERNAME_LEN 31
int find_network_newline(const char *buf, int n);

/*
 *  Write the message to the socket fd
 */

// Same error from friendme.c
void error(char *msg) {
    fprintf(stderr, "Error: %s", msg);
}

void write_client(int fd, char *msg) {
    if (write(fd, msg, strlen(msg)) < 0) {
        error("server: write");
        close(fd);
        exit(1);
    }
}



struct sockname {
    int sock_fd;
    char *username;
    char buf[BUF_SIZE];
    int inbuf;
    struct sockname *next;
};


/*
 *  Helper function that deletes a given user from users
 */
void delete_user(struct sockname *user, struct sockname **users) {
    // Find the user in the linked list while keeping track of previous
    struct sockname *prev = NULL;
    struct sockname *curr = *users;
    
    while (curr != NULL) {
        if (curr == user) {
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    // If the user is not in the list, return
    if (curr == NULL) {
        return;
    }

    // If the user is the first in the list, set the head to the next user
    if (prev == NULL) {
        *users = curr->next;
    } else {
        prev->next = curr->next;
    }

    // Free the user
    free(curr->username);
    free(curr);
}

int process_args(int cmd_argc, char **cmd_argv, User **user_list_ptr, char *curr_user, \
                int fd, struct sockname **users) {
    User *user_list = *user_list_ptr;

    if (cmd_argc <= 0) {
        return 0;
    } else if (strcmp(cmd_argv[0], "quit") == 0 && cmd_argc == 1) {
        close(fd);
        return -1;
    } else if (strcmp(cmd_argv[0], "list_users") == 0 && cmd_argc == 1) {
        char *list = list_users(user_list);
        write_client(fd, list);
        free(list);
    } else if (strcmp(cmd_argv[0], "make_friends") == 0 && cmd_argc == 2) {
        switch (make_friends(cmd_argv[1], curr_user, user_list)) {
            case 0:
                // Send a message to the friended user that they have been friended
                struct sockname *curr = *users;
                while (curr != NULL) {
                    if (strcmp(curr->username, cmd_argv[1]) == 0) {
                        char *msg = malloc(MAX_USERNAME_LEN + 32);
                        strcpy(msg, "You have been friended by ");
                        strncat(msg, curr_user, MAX_USERNAME_LEN);
                        strcat(msg, "\n");
                        write_client(curr->sock_fd, msg);
                    }
                    curr = curr->next;
                }
                /* for (int i = 0; i < MAX_CONNECTIONS; i++) {
                    if (users[i].sock_fd != -1 && strcmp(users[i].username, cmd_argv[1]) == 0) {
                        char *msg = malloc(MAX_USERNAME_LEN + 32);
                        strcpy(msg, "You have been friended by ");
                        strncat(msg, curr_user, MAX_USERNAME_LEN);
                        strcat(msg, "\n");
                        write_client(users[i].sock_fd, msg);
                    }   
                } */

                // Send a message to the friender that they have friended someone
                char *msg = malloc(MAX_USERNAME_LEN + 32);
                strcpy(msg, "You are now friends with ");
                strncat(msg, cmd_argv[1], MAX_USERNAME_LEN);
                strcat(msg, "\n");
                write_client(fd, msg);
                break;
            case 1:
                write_client(fd, "users are already friends\n");
                break;
            case 2:
                write_client(fd, "at least one user you entered has the max number of friends\n");
                break;
            case 3:
                write_client(fd, "you must enter two different users\n");
                break;
            case 4:
                write_client(fd, "at least one user you entered does not exist\n");
                break;
        }
    } else if (strcmp(cmd_argv[0], "post") == 0 && cmd_argc >= 3) {
        // first determine how long a string we need
        int space_needed = 0;
        for (int i = 2; i < cmd_argc; i++) {
            space_needed += strlen(cmd_argv[i]) + 1;
        }

        // allocate the space
        char *contents = malloc(space_needed);
        if (contents == NULL) {
            perror("malloc");
            exit(1);
        }

        // copy in the bits to make a single string
        strcpy(contents, cmd_argv[2]);
        for (int i = 3; i < cmd_argc; i++) {
            strcat(contents, " ");
            strcat(contents, cmd_argv[i]);
        }

        // Find the user using curr_user and user_list
        User *author = find_user(curr_user, user_list);
        User *target = find_user(cmd_argv[1], user_list);
        switch (make_post(author, target, contents)) {
            case 1:
                write_client(fd, "the users are not friends\n");
                break;
            case 2:
                write_client(fd, "at least one user you entered does not exist\n");
                break;
        }
    } else if (strcmp(cmd_argv[0], "profile") == 0 && cmd_argc == 2) {
        User *user = find_user(cmd_argv[1], user_list);
        char *printed_user = print_user(user);
        if (strcmp(printed_user, "") == 0) {
            write_client(fd, "user not found\n");
        }
        write_client(fd, printed_user);
        free(printed_user);
    } else {
        write_client(fd, "Incorrect syntax\n");
    }
    return 0;
}


/*
 * Tokenize the string stored in cmd.
 * Return the number of tokens, and store the tokens in cmd_argv.
 */
int tokenize(char *cmd, char **cmd_argv) {
    int cmd_argc = 0;
    char *next_token = strtok(cmd, DELIM);    
    while (next_token != NULL) {
        if (cmd_argc >= INPUT_ARG_MAX_NUM - 1) {
            error("Too many arguments!");
            cmd_argc = 0;
            break;
        }
        cmd_argv[cmd_argc] = next_token;
        cmd_argc++;
        next_token = strtok(NULL, DELIM);
    }

    return cmd_argc;
}

/*  
 *  Accept a connection on the given socket fd.
 * From Lab11
 */
int accept_connection(int fd, struct sockname **users, User **user_list_ptr) {
    /* while (user_index < MAX_CONNECTIONS && users[user_index].sock_fd != -1) {
        user_index++;
    } */

    /* if (user_index == MAX_CONNECTIONS) {
        fprintf(stderr, "server: max concurrent connections\n");
        return -1;
    } */
    // insert new user to users
    // if users is NULL, malloc space for it
    
    // Create new user
    struct sockname *new_user = malloc(sizeof(struct sockname));
    if (new_user == NULL) {
        perror("malloc");
        exit(1);
    }
    new_user->sock_fd = -1;
    new_user->username = NULL;
    new_user->inbuf = 0;
    new_user->next = NULL;


    int client_fd = accept(fd, NULL, NULL);
    if (client_fd < 0) {
        perror("server: accept");
        close(fd);
        exit(1);
    }

    // set user bufs and fds
    new_user->sock_fd = client_fd;
    
    // Send a msg asking for the user name to the client
    char *msg = "What is your user name?\n";
    if (write(client_fd, msg, strlen(msg)) < 0) {
        perror("server: write");
        close(client_fd);
        exit(1);
    }

    // append new user to users
    if (*users == NULL) {
        *users = new_user;
    } else {
        struct sockname *curr = *users;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = new_user;
    }
    return client_fd;
}

// Read from the client and return the number of bytes read
// Also parse the input and call process_args
int read_from(struct sockname **user, struct sockname **users, User **user_list_ptr) {
    // Check if the client has closed the connection
    /* In Lab 10, you focused on handling partial reads. For this lab, you do
     * not need handle partial reads. Because of that, this server program
     * does not check for "\r\n" when it reads from the client.
     */
    // remove newline
    char buf[BUF_SIZE] = {"\0"};
    int fd = (*user)->sock_fd;
    char *after = (*user)->buf + (*user)->inbuf;
    //print inbuf
    //printf("inbuf: %d\n", users[client_index].inbuf);
    //print address of user.buf
   /*  printf("user.buf: %p", user.buf);
    //print address of after
    printf("after: %p", after); */

    // partial read
    int nbytes = 0;
    if ((nbytes = read(fd, after, BUF_SIZE - (*user)->inbuf)) > 0) {
        // Step 1: update inbuf (how many bytes were just added?)
        (*user)->inbuf += nbytes;
        
        int where = 0;
        while ((where = find_network_newline((*user)->buf, (*user)->inbuf)) > 0) {
            // where is now the index into buf immediately after 
            // the first network newline
            // Step 3: Okay, we have a full line.
            // Output the full line, not including the "\r\n",
            // using print statement below.
            // Be sure to put a '\0' in the correct place first;
            // otherwise you'll get junk in the output.
            (*user)->buf[where-2] = '\0';

            //printf("[Server has read]: %s\n", users[client_index].buf);

            // Copy to buf after reading full line
            strncpy(buf, (*user)->buf, where - 2);
            // Note that we could have also used write to avoid having to
            // put the '\0' in the buffer. Try using write later!

            // if user has no username
            if ((*user)->username == NULL) {
                // check if username is too long
                if (strlen(buf) >= MAX_USERNAME_LEN) {
                    (*user)->username = malloc(MAX_USERNAME_LEN);
                    strncpy((*user)->username, buf, MAX_USERNAME_LEN - 1);
                    (*user)->username[MAX_USERNAME_LEN - 1] = '\0';
                }
                else{
                    (*user)->username = malloc(strlen(buf));
                    strcpy((*user)->username, buf);
                }
                
                // check if username alr exists,
                // if so send a welcome back msg
                switch (create_user((*user)->username, user_list_ptr)) {
                    case 0:
                        write_client((*user)->sock_fd, "Welcome.\n");
                        break;
                    case 1:
                        write_client((*user)->sock_fd, "Welcome back.\n");
                        break;
                }
                memmove((*user)->buf, (*user)->buf+where, (*user)->inbuf-where);
                (*user)->inbuf -= where;
                return 0;
            }

            // process the arguments
            char *cmd_argv[INPUT_ARG_MAX_NUM];
            int cmd_argc = tokenize(buf, cmd_argv);

            if (cmd_argc > 0 && process_args(cmd_argc, cmd_argv, user_list_ptr, \
                (*user)->username, (*user)->sock_fd, users) == -1) {
                // can only reach if quit command was entered
                int user_fd = (*user)->sock_fd;
                (*user)->sock_fd = -1;
                memmove((*user)->buf, (*user)->buf+where, (*user)->inbuf-where);
                (*user)->inbuf -= where;
                return user_fd;
            }
            // Step 4: update inbuf and remove the full line from the buffer
            // There might be stuff after the line, so don't just do inbuf = 0.

            // You want to move the stuff after the full line to the beginning
            // of the buffer.  A loop can do it, or you can use memmove.
            // memmove(destination, source, number_of_bytes)
            
            memmove((*user)->buf, (*user)->buf+where, (*user)->inbuf-where);
            (*user)->inbuf -= where;
        }
    }
    if (nbytes == 0) {
        int user_fd = (*user)->sock_fd;
        (*user)->sock_fd = -1;
        return user_fd;
    }
    // Error checking
    if (nbytes < 0) {
        perror("server: read");
        close(fd);
        exit(1);
    }
    // DEBUG PURPOSE
    //printf("%s: %s\n", users[client_index].username, buf);
    
    return 0;
}



int main(void) {
    // Create the heads of the empty data structure
    User *user_list = NULL;

    struct sockname *users = NULL;

    // Create the socket FD.
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("server: socket");
        exit(1);
    }

    // Set information about the port (and IP) we want to be connected to.
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;

    // This sets an option on the socket so that its port can be reused right
    // away. Since you are likely to run, stop, edit, compile and rerun your
    // server fairly quickly, this will mean you can reuse the same port.
    int on = 1;
    int status = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR,
                            (const char *) &on, sizeof(on));
    if (status == -1) {
        perror("setsockopt -- REUSEADDR");
    }

    // This should always be zero. On some systems, it won't error if you
    // forget, but on others, you'll get mysterious errors. So zero it.
    memset(&server.sin_zero, 0, 8);

    // Bind the selected port to the socket.
    if (bind(sock_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("server: bind");
        close(sock_fd);
        exit(1);
    }

    // Announce willingness to accept connections on this socket.
    if (listen(sock_fd, MAX_BACKLOG) < 0) {
        perror("server: listen");
        close(sock_fd);
        exit(1);
    }

    // The client accept - message accept loop. First, we prepare to listen to multiple
    // file descriptors by initializing a set of file descriptors.
    int max_fd = sock_fd;
    fd_set all_fds;
    FD_ZERO(&all_fds);
    FD_SET(sock_fd, &all_fds);

    while (1) {
        // select updates the fd_set it receives, so we always use a copy and retain the original.
        fd_set listen_fds = all_fds;
        if (select(max_fd + 1, &listen_fds, NULL, NULL, NULL) == -1) {
            perror("server: select");
            exit(1);
        }

        // Is it the original socket? Create a new connection ...
        if (FD_ISSET(sock_fd, &listen_fds)) {
            int client_fd = accept_connection(sock_fd, &users, &user_list);
            if (client_fd > max_fd) {
                max_fd = client_fd;
            }
            FD_SET(client_fd, &all_fds);
            printf("Accepted connection\n");
        }

        // Next, check the clients.
        struct sockname *curr = users;
        while (curr != NULL) {
            if (curr->sock_fd > -1 && FD_ISSET(curr->sock_fd, &listen_fds)) {
                int client_closed = read_from(&curr, &users, &user_list);
                if (client_closed > 0) {
                    FD_CLR(client_closed, &all_fds);
                    delete_user(curr, &users);
                    printf("Client %d disconnected\n", client_closed);
                } else {

                    // FOR DEBUG
                    //printf("Received from client %d\n", curr->sock_fd);
                }
            }
            curr = curr->next;
        }
        /* for (int index = 0; index < MAX_CONNECTIONS; index++) {
            if (users[index].sock_fd > -1 && FD_ISSET(users[index].sock_fd, &listen_fds)) {
                // Note: never reduces max_fd
                int client_closed = read_from(index, users, &user_list);
                if (client_closed > 0) {
                    FD_CLR(client_closed, &all_fds);
                    free(users[index].username);
                    printf("Client %d disconnected\n", client_closed);
                } else {
                    printf("Received from client %d\n", users[index].sock_fd);
                }
            }
        } */
    }

    // Should never get here.
    return 1;
}


/*
 * Search the first n characters of buf for a network newline (\r\n).
 * Return one plus the index of the '\n' of the first network newline,
 * or -1 if no network newline is found. The return value is the index into buf
 * where the current line ends.
 * Definitely do not use strchr or other string functions to search here. (Why not?)
 */
int find_network_newline(const char *buf, int n) {
    for (int i = 0; i < n; i++) {
        if (buf[i] == '\r' && buf[i+1] == '\n') {
            return i+2;
        }
    }
    return -1;
}