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
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <stdlib.h>
#include <sys/types.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>

//TODO: optimize imports

struct command
{
    const char **argv;
};

int spawn_proc(int in_fd, int out_fd, struct command *cmd) {

    pid_t pid;

    if ((pid = fork ()) == 0) {
        if (in_fd != 0) {
            dup2 (in_fd, 0);
            close (in_fd);
        }

        if (out_fd != 1) {
            dup2 (out_fd, 1);
            close (out_fd);
        }

        return execvp (cmd->argv[0], (char * const *)cmd->argv);
    }

    return pid;
}

int fork_pipes(int n, int socket_fd, struct command *cmd) {

    int i;
    pid_t pid;
    int in, fd[2];

    in = 0;

    for (i = 0; i < n - 1; ++i) {
        pipe(fd);
        spawn_proc(in, fd[1], cmd + i);
        close(fd[1]);
        in = fd[0];
    }

    if (in != 0)
        dup2 (in, 0);
    dup2(socket_fd, 1);

    /* Execute the last stage with the current process. */
    return execvp(cmd[i].argv[0], (char * const *)cmd[i].argv);
}

std::vector<std::string> insplit(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    insplit(s, delim, elems);
    return elems;
}

// Error notification
void error_netsh(const char *error_str) {
    perror(error_str);
    exit(EXIT_FAILURE);
}

int create_and_bind(char* port) {
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

    return socket_fd;
}

//TODO: implement piping multiple commands (leave commented as a stub)
//    if (listen(socket_fd, 50) == -1) {
//        error_netsh("Failed to listen");
//    }
//
//    socklen_t client_len = sizeof(client_address);
//
//    while (1) {
//        int client_socket_fd = accept(socket_fd, (struct sockaddr *) &client_address, &client_len);
//        if (client_socket_fd == -1) {
//            error_netsh("Failed to accept");
//        }
//
//        char buffer[4096];
//        memset(&buffer, 0, sizeof(buffer));
//        size_t read_count = read(client_socket_fd, buffer, sizeof(buffer));
//        if (read_count == -1) {
//            error_netsh("Failed to read from socket");
//        }
//        if (read_count == sizeof(buffer)) {
//            //need to eat more input
//        }
//        std::string sequence(buffer, read_count - 1);
//        std::vector<std::string> subcommands = split(sequence, '|');
//        printf("%d commands\n", subcommands.size());
//        for (size_t i = 0; i != subcommands.size(); ++i) {
//            std::vector<std::string> command_args = split(subcommands[i], ' ');
//            for (std::vector<std::string>::iterator it = command_args.begin(); it != command_args.end();) {
//                if ((*it).size() == 0)
//                    it = command_args.erase(it);
//                else
//                    ++it;
//            }
//            printf("%d\n", command_args.size());
//            char* argv_to_pass[command_args.size() + 1];
//            for (size_t j = 0; j != command_args.size(); ++j) {
//                argv_to_pass[j] = const_cast<char*>(command_args[j].c_str());
//                printf("%s\n", command_args[j].c_str());
//            }
//            argv_to_pass[command_args.size()] = NULL;
//            char * pointer = (char*)&argv_to_pass;
//
//            pid_t pid = fork();
//
//            if (pid == -1) {
//                error_netsh("Failed to fork on input received");
//            } else if (pid > 0) {
//                exit(0);
//            } else {
//                printf("Executing %s %d \n", argv_to_pass[0], sizeof(argv_to_pass[0]));
//                execlp(argv_to_pass[0], pointer);
//            }
//        }
//    }

int make_socket_non_blocking (int socket_fd) {
    int flags, s;

    flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags == -1) {
        error_netsh("fcntl error");
    }

    flags |= O_NONBLOCK;
    s = fcntl(socket_fd, F_SETFL, flags);
    if (s == -1) {
        error_netsh("fcntl error");
    }

    return 0;
}

// Daemon
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
        // This is the child that can proceed with daemonization
        pid_t new_session_pid = setsid();

        if (new_session_pid == -1) {
            error_netsh("Failed to setsid");
        }
        // Successfully created new session
        pid_t second_fork_pid = fork();

        if (second_fork_pid == -1) {
            error_netsh("Failed to fork after creating a new session");
        } else if (second_fork_pid > 0) {
            exit(0);
        } else {
            // Daemonized successfully
            pid_t daemon_pid = getpid();
            int tmp_fd = open("/tmp/netsh.pid", O_WRONLY | O_CREAT);
            char* buf = new char[64];
            int digits = sprintf(buf, "%d\n", daemon_pid);
            ssize_t written_count = write(tmp_fd, buf, digits);
            if (written_count == -1) {
                close(tmp_fd);
                error_netsh("Failed to write pid to file");
            }
            close(tmp_fd);

            const int MAXEVENTS = 64;
            struct epoll_event event;
            struct epoll_event *events;

            int sfd = create_and_bind(argv[1]);
            make_socket_non_blocking(sfd);
            if (listen(sfd, SOMAXCONN) == -1) {
                error_netsh("Failed to listen");
            }
            int efd = epoll_create1(0);
            if (efd == -1) {
                error_netsh("Epoll_create error");
            }
            event.data.fd = sfd;
            event.events = EPOLLIN | EPOLLET;
            if (epoll_ctl (efd, EPOLL_CTL_ADD, sfd, &event) == -1) {
                error_netsh("Epoll_ctl error");
            }

            events = (struct epoll_event *)calloc(MAXEVENTS, sizeof(event));

            // Main loop learnt from an example on the internets
            while (1) {
                int n, i;

                n = epoll_wait (efd, events, MAXEVENTS, -1);
                for (i = 0; i < n; i++) {
                    if ((events[i].events & EPOLLERR) ||
                        (events[i].events & EPOLLHUP) ||
                        (!(events[i].events & EPOLLIN)))
                    {
                        /* An error has occured on this fd, or the socket is not
                           ready for reading (why were we notified then?) */
                        fprintf (stderr, "epoll error\n");
                        close (events[i].data.fd);
                        continue;
                    }

                    else if (sfd == events[i].data.fd) {
                        /* We have a notification on the listening socket, which
                           means one or more incoming connections. */
                        while (1) {
                            struct sockaddr in_addr;
                            socklen_t in_len;
                            int infd;
                            char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

                            in_len = sizeof in_addr;
                            infd = accept (sfd, &in_addr, &in_len);
                            if (infd == -1) {
                                if ((errno == EAGAIN) ||
                                    (errno == EWOULDBLOCK)) {
                                    /* We have processed all incoming
                                       connections. */
                                    break;
                                }
                                else {
                                    perror ("Failed to accept");
                                    break;
                                }
                            }

                            /* Make the incoming socket non-blocking and add it to the
                               list of fds to monitor. */
                            if (make_socket_non_blocking (infd) == -1) {
                                error_netsh("Failed to make socket nonblocking");
                            }

                            event.data.fd = infd;
                            event.events = EPOLLIN | EPOLLET;
                            if (epoll_ctl (efd, EPOLL_CTL_ADD, infd, &event) == -1) {
                                error_netsh("Epoll_ctl error");
                            }
                        }
                        continue;
                    }
                    else {
                        /* We have data on the fd waiting to be read. Read and
                           display it. We must read whatever data is available
                           completely, as we are running in edge-triggered mode
                           and won't get a notification again for the same
                           data. */
                        int done = 0;

                        while (1) {
                            ssize_t count;
                            char buf[512];

                            count = read (events[i].data.fd, buf, sizeof buf);
                            if (count == -1) {
                                /* If errno == EAGAIN, that means we have read all
                                   data. So go back to the main loop. */
                                if (errno != EAGAIN) {
                                    perror ("read");
                                    done = 1;
                                }
                                break;
                            } else if (count == 0) {
                                /* End of file. The remote has closed the
                                   connection. */
                                done = 1;
                                break;
                            }

                            pid_t pid = fork();

                            if (pid == -1) {
                                error_netsh("Failed to fork on input received");
                            } else if (pid > 0) {
                                // Do nothing
                            } else {
                                //TODO: parse a complex command
                                // this is for testing purposes
                                const char *ls[] = { "ls", "-l", 0 };
                                const char *awk[] = { "awk", "{print $1}", 0 };
                                const char *sort[] = { "sort", 0 };
                                const char *uniq[] = { "uniq", 0 };

                                struct command cmd [] = { {ls}, {awk}, {sort}, {uniq} };

                                fork_pipes (1, events[i].data.fd, cmd);
                            }
                        }

                        if (done) {
                            /* Closing the descriptor will make epoll remove it
                               from the set of descriptors which are monitored. */
                            close (events[i].data.fd);
                        }
                    }
                }
            }

            //TODO: probably never reaching this line
            free (events);
            close (sfd);
            return EXIT_SUCCESS;
        }
    }
}