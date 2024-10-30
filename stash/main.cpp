#include <iostream>
#include <sys/stat.h>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>

void printHelp() {
    printf("stash [s/r] [filename]\n");
}

const char *key = "d24bf82cca5ef32953ed66aea9770fa8";

int stash(char *file) {
    int fd = open(file, O_RDWR);

    off_t fileStart = lseek(fd, 0, SEEK_SET);
    off_t fileEnd = lseek(fd, 0, SEEK_END);

    unsigned char buf[fileEnd - fileStart];
    pread(fd, buf, fileEnd - fileStart, 0);

    for (int i = 0; i < sizeof buf; i++)
        buf[i] -= key[i % strlen(key)];

    pwrite(fd, buf, fileEnd - fileStart, 0);
    return 0;
}

int restore(char *file) {
    int fd = open(file, O_RDWR);

    off_t fileStart = lseek(fd, 0, SEEK_SET);
    off_t fileEnd = lseek(fd, 0, SEEK_END);

    unsigned char buf[fileEnd - fileStart];
    pread(fd, buf, fileEnd - fileStart, 0);

    for (int i = 0; i < sizeof buf; i++) {
        buf[i] += key[i % strlen(key)];
    }

    pwrite(fd, buf, fileEnd - fileStart, 0);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printHelp();
        return 1;
    }

    if (argv[1][0] == 's') {
        return stash(argv[2]);
    } else if (argv[1][0] == 'r') {
        return restore(argv[2]);
    } else {
        printHelp();
        return 1;
    }
}
