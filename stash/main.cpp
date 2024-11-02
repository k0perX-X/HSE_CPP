#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>

void printHelp() {
    printf("stash [s/r] [filename]\n");
}

const char *key = "d24bf82cca5ef32953ed66aea9770fa8";

const long logFrequency = 100000;

int stash(char *file) {
    int fd = open(file, O_RDWR);

    off_t fileStart = lseek(fd, 0, SEEK_SET);
    off_t fileEnd = lseek(fd, fileStart, SEEK_END);

    long *buf = (long *) malloc(sizeof(long));
    off_t fileHole = lseek(fd, fileStart, SEEK_HOLE);
    long cursor = fileStart;

    while (fileHole != fileEnd) {
        for (; cursor <= fileHole - sizeof(long); cursor += sizeof(long)) {
            pread(fd, buf, sizeof(long), cursor);
            *buf -= *(long *) (key + ((cursor - fileStart) % (strlen(key) - sizeof(long))));
            pwrite(fd, buf, sizeof(long), cursor);
            if (cursor % logFrequency == 0) {
                printf("\r%li%%", cursor * 100 / fileEnd);
                fflush(stdout);
            }
        }

        cursor = lseek(fd, fileHole, SEEK_DATA);
        fileHole = lseek(fd, cursor, SEEK_HOLE);
    }

    for (; cursor <= fileHole - sizeof(long); cursor += sizeof(long)) {
        pread(fd, buf, sizeof(long), cursor);
        *buf -= *(long *) (key + ((cursor - fileStart) % (strlen(key) - sizeof(long))));
        pwrite(fd, buf, sizeof(long), cursor);
        if (cursor % logFrequency == 0) {
            printf("\r%li%%", cursor * 100 / fileEnd);
            fflush(stdout);
        }
    }

    free(buf);
    printf("\r100%%\n");
    return 0;
}

int restore(char *file) {
    int fd = open(file, O_RDWR);

    off_t fileStart = lseek(fd, 0, SEEK_SET);
    off_t fileEnd = lseek(fd, fileStart, SEEK_END);

    long *buf = (long *) malloc(sizeof(long));
    off_t fileHole = lseek(fd, fileStart, SEEK_HOLE);
    long cursor = fileStart;

    while (fileHole != fileEnd) {
        for (; cursor <= fileHole - sizeof(long); cursor += sizeof(long)) {
            pread(fd, buf, sizeof(long), cursor);
            *buf += *(long *) (key + ((cursor - fileStart) % (strlen(key) - sizeof(long))));
            pwrite(fd, buf, sizeof(long), cursor);
            if (cursor % logFrequency == 0) {
                printf("\r%li%%", cursor * 100 / fileEnd);
                fflush(stdout);
            }
        }

        cursor = lseek(fd, fileHole, SEEK_DATA);
        fileHole = lseek(fd, cursor, SEEK_HOLE);
    }

    for (; cursor <= fileHole - sizeof(long); cursor += sizeof(long)) {
        pread(fd, buf, sizeof(long), cursor);
        *buf += *(long *) (key + ((cursor - fileStart) % (strlen(key) - sizeof(long))));
        pwrite(fd, buf, sizeof(long), cursor);
        if (cursor % logFrequency == 0) {
            printf("\r%li%%", cursor * 100 / fileEnd);
            fflush(stdout);
        }
    }

    free(buf);
    printf("\r100%%\n");
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
