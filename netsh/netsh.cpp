//
// Created by heat_wave on 02/04/16.
//
#include <sys/types.h>
#include <sys/wait.h>
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
    struct sockaddr_in server_address, client_address;
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        error_netsh("Failed to create socket");
    }
    memset(&server_address, 0, sizeof(struct sockaddr_in));
    int portno = atoi(port);
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(portno);

    if (bind(socket_fd, (struct sockaddr *) &server_address,
             sizeof(struct sockaddr)) == -1) {
        error_netsh("Failed to bind socket");
    }

    if (listen(socket_fd, 50) == -1) {
        error_netsh("Failed to listen");
    }

    socklen_t client_len = sizeof(client_address);
    int client_socket_fd = accept(socket_fd, (struct sockaddr *) &client_address, &client_len);
    if (client_socket_fd == -1) {
        error_netsh("Failed to accept");
    }

    char buffer [4096];
    memset(&buffer, 0, sizeof(buffer));
    size_t read_count = read(client_socket_fd, buffer, sizeof(buffer));
    shutdown(client_socket_fd, SHUT_RD);
    if (read_count == -1) {
        error_netsh("Failed to read from socket");
    }

    pid_t parent = getpid();
    pid_t pid = fork();

    if (pid == -1) {
        error_netsh("Failed to fork on input received");
    } else if (pid > 0) {
        //
    } else {
        //execlp(buffer, buffer);
        //int test_fd = open("/tmp/test.out", O_WRONLY | O_CREAT);
        size_t write_count = write(client_socket_fd, buffer, read_count);
        //write_count = write(test_fd, buffer, read_count);

        if (write_count == -1) {
            error_netsh("Failed to write to socket");
        }
        close(client_socket_fd);
        close(socket_fd);
        //close(test_fd);
        exit(0);
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
        }
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