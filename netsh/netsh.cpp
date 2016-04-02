//
// Created by heat_wave on 02/04/16.
//
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <cstdlib>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

//error notification
void error_netsh(const char *error_str) {
    fprintf(stderr, "%s\n", error_str);
    exit(EXIT_FAILURE);
}

void handle_socket(char* port) {
    struct sockaddr_in address;
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        error_netsh("Failed to create socket");
    }
    memset(&address, 0, sizeof(struct sockaddr_in));
    int portno = atoi(port);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(portno);

    if (bind(socket_fd, (struct sockaddr *) &address,
             sizeof(struct sockaddr)) == -1) {
        error_netsh("Failed to bind socket");
    }

    if (listen(socket_fd, 50) == -1) {
        error_netsh("Failed to listen");
    }
    else {
        printf("Listening");
    }
}

//daemon?
int main(int argc, char *argv[]) {
    if (argc != 2) {
        error_netsh("Wrong argument count!");
    }

    pid_t first_fork_pid = fork();

    if (first_fork_pid == -1) {
        error_netsh("Failed to fork initially");
    } else if (first_fork_pid > 0) {
        exit(0);
    } else {
        //this is the child that can proceed with daemonization
        pid_t new_session_pid = setsid();

        if (new_session_pid == -1) {
            error_netsh("Failed to setsid");
        } else {
            //successfully created new session
            pid_t second_fork_pid = fork();

            if (second_fork_pid == -1) {
                error_netsh("Failed to fork after creating a new session");
            } else if (second_fork_pid > 0) {
                exit(0);
            } else {
                //daemonized successfully
                pid_t daemon_pid = getpid();
                int tmp_fd = open("/tmp/netsh.pid", O_WRONLY | O_CREAT);
                char* buf = new char[64];
                int digits = sprintf(buf, "%d\n", daemon_pid);
                ssize_t written_count = write(tmp_fd, buf, digits);
                if (written_count > 0) {
                    close(tmp_fd);
                    handle_socket(argv[1]);
                    do {
                        //just wait?
                    } while (1);
                } else {
                    close(tmp_fd);
                    error_netsh("Failed to write pid to file");
                }
            }
        }
    }
}