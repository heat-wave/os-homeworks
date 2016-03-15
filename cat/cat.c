//
// Created by heat_wave on 17/02/16.
//
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void error_cat(const char *error_str)
{
    fprintf(stderr, "%s\n", error_str);
    exit(1);
}

int main(int argc, char* argv[]) {

    char buf[4096];
    size_t read_count;

    int fd;
    fd = open(argv[1], O_RDONLY);

    do {
        read_count = (size_t) read(fd, buf, sizeof(buf));
        if (read_count == -1 && EINTR != errno) {
            error_cat(strerror(errno));
        }

        int to_write = read_count;
        int total_written = 0;
        while (to_write > 0) {
            size_t written_count = (size_t) write(STDOUT_FILENO, buf + total_written, to_write);
            if (written_count == -1) {
                error_cat("IO error");
            }
            total_written += written_count;
            to_write -= written_count;
        }
    } while (read_count > 0 && EINTR == errno);

    close(fd);
    return 0;
}

