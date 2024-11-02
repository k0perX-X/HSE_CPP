#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>

const unsigned char marker[] = {0xFF, 0xD9};
const int markerSize = 2;
const size_t nBuf = 16777216; // задает размер буфера для записи и чтения файла

off_t getMarkerPoint(unsigned char *buf, int fd, off_t fileHole, off_t fileData, off_t fileEnd) {
    off_t markerPoint;
    off_t bufSize;
    off_t cursor = fileData;
    do {
        if ((bufSize = fileHole - cursor) > nBuf)
            bufSize = nBuf;
        if (bufSize == 0) {
            if (cursor == fileEnd)
                return cursor;
            fileData = lseek(fd, cursor, SEEK_DATA);
            fileHole = lseek(fd, fileData, SEEK_HOLE);
            continue;
        }
        pread(fd, buf, bufSize, cursor);
        off_t i = 0;
        for (; i < bufSize; i++) {
            int j = 0;
            for (; buf[i + j] == marker[j] and j < markerSize; j++);
            if (j == markerSize)
                break;
        }
        markerPoint = i + cursor;
        cursor += bufSize;
    } while (markerPoint == cursor);
    return markerPoint + markerSize;
}

int save_from_stdin(char *filename) {
    int fd;
    if ((fd = open(filename, O_RDWR)) == -1) {
        fprintf(stderr, "Ошибка открытия файла\n");
        return 2;
    }
    auto *buf = (unsigned char *) malloc(nBuf);

    off_t fileData = lseek(fd, 0, SEEK_DATA);
    off_t fileEnd = lseek(fd, fileData, SEEK_END);
    off_t fileHole = lseek(fd, fileData, SEEK_HOLE);
    off_t pointer = getMarkerPoint(buf, fd, fileHole, fileData, fileEnd);

    if (pointer != fileEnd) {
        fprintf(stderr, "Данные уже записаны\n");
        return 3;
    }

    size_t n;
    while ((n = fread(buf, 1, nBuf, stdin)) > 0) {
        ssize_t wrote = pwrite(fd, buf, n, lseek(fd, 0, SEEK_END));
        while (wrote != n) {
            n -= wrote;
            wrote = pwrite(fd, buf + nBuf - n, n, lseek(fd, 0, SEEK_END));
        }
        if (wrote == -1) {
            fprintf(stderr, "Ошибка записи\n");
            free(buf);
            close(fd);
            return 3;
        }
    }

    if (n == -1) {
        fprintf(stderr, "Данные не найдены\n");
        free(buf);
        return 1;
    }

    free(buf);
    close(fd);
    return 0;
}

int save_from_argv(char *filename, int argc, char *argv[]) {
    int fd;
    if ((fd = open(filename, O_RDWR)) == -1) {
        fprintf(stderr, "Ошибка открытия файла\n");
        return 2;
    }
    auto *buf = (unsigned char *) malloc(nBuf);

    off_t fileData = lseek(fd, 0, SEEK_DATA);
    off_t fileEnd = lseek(fd, fileData, SEEK_END);
    off_t fileHole = lseek(fd, fileData, SEEK_HOLE);
    off_t pointer = getMarkerPoint(buf, fd, fileHole, fileData, fileEnd);
    free(buf);

    if (pointer != fileEnd) {
        fprintf(stderr, "Данные уже записаны\n");
        return 1;
    }

    for (int i = 3; i < argc; i++) {
        size_t n = strlen(argv[i]);
        ssize_t wrote = pwrite(fd, argv[i], n, lseek(fd, 0, SEEK_END));
        while (wrote != n) {
            n -= wrote;
            wrote = pwrite(fd, argv[i] + strlen(argv[i]) - n, n, lseek(fd, 0, SEEK_END));
        }
        if (wrote == -1) {
            fprintf(stderr, "Ошибка записи\n");
            close(fd);
            return 3;
        }
    }

    close(fd);
    return 0;
}

int read_data(char *filename) {
    int fd;
    if ((fd = open(filename, O_RDONLY)) == -1) {
        fprintf(stderr, "Ошибка открытия файла\n");
        return 2;
    };

    off_t fileData = lseek(fd, 0, SEEK_DATA);
    off_t fileEnd = lseek(fd, fileData, SEEK_END);
    off_t fileHole = lseek(fd, fileData, SEEK_HOLE);

    auto *buf = (unsigned char *) malloc(nBuf);
    off_t cursor = getMarkerPoint(buf, fd, fileHole, fileData, fileEnd);

    if (cursor == fileEnd) {
        fprintf(stderr, "Данные не найдены\n");
        free(buf);
        return 1;
    }

    off_t bufSize;
    while (cursor != fileEnd) {
        if ((bufSize = fileHole - cursor) > nBuf)
            bufSize = nBuf;
        if (bufSize == 0) {
            fileData = lseek(fd, cursor, SEEK_DATA);
            fileHole = lseek(fd, fileData, SEEK_HOLE);
            continue;
        }
        pread(fd, buf, bufSize, cursor);
        fwrite(buf, bufSize, 1, stdout);
        cursor += bufSize;
    }

    close(fd);
    free(buf);
    return 0;
}

int delete_data(char *filename) {

    int fd;
    if ((fd = open(filename, O_RDWR)) == -1) {
        fprintf(stderr, "Ошибка открытия файла\n");
        return 2;
    }
    auto *buf = (unsigned char *) malloc(nBuf);

    off_t fileData = lseek(fd, 0, SEEK_DATA);
    off_t fileEnd = lseek(fd, fileData, SEEK_END);
    off_t fileHole = lseek(fd, fileData, SEEK_HOLE);
    off_t cursor = getMarkerPoint(buf, fd, fileHole, fileData, fileEnd);

    if (cursor == fileEnd) {
        fprintf(stderr, "Данные не найдены\n");
        free(buf);
        return 1;
    }

    ftruncate(fd, cursor);

    close(fd);
    free(buf);
    return 0;
}

void printHelp() {
    fprintf(stderr, "stego [s/r/d] [filename.jpg] [data ...]\n");
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printHelp();
        return 1;
    }

    if (argv[1][0] == 's' and argc == 3) {
        return save_from_stdin(argv[2]);
    } else if (argv[1][0] == 's') {
        return save_from_argv(argv[2], argc, argv);
    } else if (argv[1][0] == 'r') {
        return read_data(argv[2]);
    } else if (argv[1][0] == 'd') {
        return delete_data(argv[2]);
    } else {
        printHelp();
        return 1;
    }
}
