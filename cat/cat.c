//
// Created by heat_wave on 17/02/16.
//
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    char buf[256];

    for (int i = 1; i < argc; ++i) {
        int fd = open(argv[i], O_RDONLY);
        int count;
        while (1) {
            count = read(fd, buf, 256);
            if (count >= 0) {
                int res = write(STDOUT_FILENO, buf, count);
                if (res == -1) {
                    int errsv = errno;
                    printf("Write returned error: %d", errsv);
                    break;
                }
            } else {
                int errsv = errno;
                printf("Read returned error: %d", errsv);
                break;
            }
            if (count == 0) {
                break;
            }
        }

        close(fd);
    }
}
