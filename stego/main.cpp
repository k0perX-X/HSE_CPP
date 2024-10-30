#include <iostream>
#include <sys/stat.h>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>

const char *key = "d24bf82cca5ef32953ed66aea9770fa8";
const unsigned char keySize = 32;

const unsigned char commentMarker[] = {0xFF, 0xFE};
const unsigned char commentMarkerSize = 2;
const unsigned char dataEndMarker[] = {(unsigned char)(0x00 ^ key[0]), (unsigned char)(0x00 ^ key[1])}; // не может быть в строке, так как невозможно передать аргументом
const unsigned char dataEndMarkerSize = 2;

void printHelp() {
    printf("stego [s/r/d] [filename.jpg] [data]\n");
}

unsigned long findMarker(const unsigned char *buf, unsigned long bufSize,
                         const unsigned char *marker = commentMarker, unsigned long markerSize = commentMarkerSize,
                         unsigned long offset = 0) {
    unsigned long i = offset;
    for (; i < bufSize; i++) {
        int j = 0;
        for (; buf[i + j] == marker[j] and j < markerSize; j++);
        if (j == markerSize)
            break;
    }
    return i;
}

bool checkData(const char *data, unsigned long dataSize) {
    for (int i = 0; i < dataSize; i++) {
        if (data[i] == (char) 0xFF)
            return false;
    }
    return true;
}

int save(const char *file, const char *data) {
    int fd = open(file, O_RDWR);

    off_t fileStart = lseek(fd, 0, SEEK_SET);
    off_t fileEnd = lseek(fd, 0, SEEK_END);

    unsigned long bufSize = fileEnd - fileStart;
    unsigned char buf[bufSize];
    pread(fd, buf, fileEnd - fileStart, 0);

    unsigned long position = findMarker(buf, bufSize);
    bool notFound = position == bufSize;
    unsigned long dataSize = strlen(data);
    unsigned long newBufSize = dataSize + bufSize + dataEndMarkerSize;
    if (notFound) {
        position -= commentMarkerSize;
        newBufSize += commentMarkerSize;
    }
    unsigned char newBuf[newBufSize];
    unsigned long i = 0;
    for (; i < position; i++)
        newBuf[i] = buf[i];
    for (; i < position + commentMarkerSize; i++)
        newBuf[i] = commentMarker[i - (position)];
    for (; i < dataSize + position + commentMarkerSize; i++)
        newBuf[i] = data[i - (position + commentMarkerSize)] ^ key[(i - (position + commentMarkerSize)) % keySize];
    for (; i < position + commentMarkerSize + dataSize + dataEndMarkerSize; i++)
        newBuf[i] = dataEndMarker[i - (position + commentMarkerSize + dataSize)];
    for (; i < newBufSize; i++)
        newBuf[i] = buf[i - (newBufSize - bufSize)];

    pwrite(fd, newBuf, newBufSize, 0);
    close(fd);
    return 0;
}

int read(char *file) {
    int fd = open(file, O_RDWR);

    off_t fileStart = lseek(fd, 0, SEEK_SET);
    off_t fileEnd = lseek(fd, 0, SEEK_END);

    unsigned long bufSize = fileEnd - fileStart;
    unsigned char buf[bufSize];
    pread(fd, buf, fileEnd - fileStart, 0);
    close(fd);

    unsigned long position = findMarker(buf, bufSize);
    unsigned long endPosition = findMarker(buf, bufSize, dataEndMarker, dataEndMarkerSize, position);

    if (position == bufSize or endPosition == bufSize) {
        printf("Данные не найдены");
        return 2;
    }

    while (endPosition != bufSize){
        for (unsigned long i = position + commentMarkerSize; i < endPosition; i++) {
            printf("%c", buf[i] ^ key[(i - (position + commentMarkerSize)) % keySize]);
        }
        printf("\n");
        position = endPosition;
        endPosition = findMarker(buf, bufSize, dataEndMarker, dataEndMarkerSize, endPosition + dataEndMarkerSize);
    }
    return 0;
}

int del(char *file) {
    int fd = open(file, O_RDWR);

    off_t fileStart = lseek(fd, 0, SEEK_SET);
    off_t fileEnd = lseek(fd, 0, SEEK_END);

    unsigned long bufSize = fileEnd - fileStart;
    unsigned char buf[bufSize];
    pread(fd, buf, fileEnd - fileStart, 0);

    unsigned long position = findMarker(buf, bufSize);
    unsigned long endPosition = findMarker(buf, bufSize, dataEndMarker, dataEndMarkerSize, position);

    if (position == bufSize or endPosition == bufSize) {
        printf("Данные не найдены");
        return 2;
    }

    unsigned long startPosition = position;
    while (endPosition != bufSize) {
        position = endPosition;
        endPosition = findMarker(buf, bufSize, dataEndMarker, dataEndMarkerSize, endPosition + dataEndMarkerSize);
    }
    endPosition = position + dataEndMarkerSize;

    bool anotherComments = buf[endPosition] != 0xFF;
    if (anotherComments) {
        startPosition += commentMarkerSize;
    }

    unsigned long delDataSize = endPosition - startPosition;
    unsigned long newBufSize = bufSize - delDataSize;
    unsigned char newBuf[newBufSize];
    for (unsigned long i = 0; i < startPosition; i++)
        newBuf[i] = buf[i];
    for (unsigned long i = endPosition; i < bufSize; i++)
        newBuf[i - delDataSize] = buf[i];

    pwrite(fd, newBuf, newBufSize, 0);
    ftruncate(fd, (long)newBufSize);
    close(fd);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argv[1][0] == 's' and argc == 4) {
        if (!checkData(argv[3], strlen(argv[3])))
            return 127;
        return save(argv[2], argv[3]);
    } else if (argv[1][0] == 'r' and argc == 3) {
        return read(argv[2]);
    } else if (argv[1][0] == 'd' and argc == 3) {
        return del(argv[2]);
    } else {
        printHelp();
        return 1;
    }
}
