//
// Created by heat_wave on 17/02/16.
//
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

void error_cat(const char *error_str)
{
    fprintf(stderr, "%s\n", error_str);
    exit(1);
}

int main() {

    char buf[4096];
    size_t read_count;

    do {
        read_count = (size_t) read(STDIN_FILENO, buf, sizeof(buf));
        if (read_count == -1) {
            error_cat(strerror(errno));
        }

        size_t written_count = (size_t) write(STDOUT_FILENO, buf, read_count);

        if (written_count < read_count) {
            error_cat("Unexpected EOF");
        }
    } while (read_count > 0);

    return 0;
}

