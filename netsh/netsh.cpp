//
// Created by heat_wave on 02/04/16.
//
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <cstdlib>

//error notification
void error_netsh(const char *error_str)
{
    fprintf(stderr, "%s\n", error_str);
    exit(1);
}

//a routine used to write pid to file correctly
size_t int_to_string(char *s, int x)
{
    //  Set pointer to current position.
    char *p = s;

    //  Set t to absolute value of x.
    unsigned t = x;
    if (x < 0) t = -t;

    //  Write digits.
    do {
        *p++ = '0' + t % 10;
        t /= 10;
    } while (t);

    //  If x is negative, write sign.
    if (x < 0)
        *p++ = '-';

    //  Remember the return value, the number of characters written.
    size_t r = p-s;

    //  Since we wrote the characters in reverse order, reverse them.
    while (s < --p) {
        char t = *s;
        *s++ = *p;
        *p = t;
    }

    return r;
}

//daemon?
int main(int argc, char *argv[]) {
    pid_t first_fork_pid = fork();

    if (first_fork_pid == -1) {
        error_netsh("Failed to fork initially");
    } else if (first_fork_pid > 0) {
        // do nothing
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
                //do nothing
            } else {
                //daemonized successfully
                pid_t daemon_pid = getpid();
                int tmp_fd = open("/tmp/netsh.pid", O_WRONLY | O_CREAT);
                char* buf = new char[64];
                size_t digits = int_to_string(buf, (int)daemon_pid);
                ssize_t written_count = write(tmp_fd, buf, digits);
                if (written_count > 0) {
                    close(tmp_fd);
                    //proceed with TCP listening
                } else {
                    close(tmp_fd);
                    error_netsh("Failed to write pid to file");
                }
            }
        }
    }
}