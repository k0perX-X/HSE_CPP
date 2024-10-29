#include <iostream>
#include <sys/stat.h>
#include <cstring>
#include <unistd.h>

struct stat statFile;
const char *dirName = ".shadow/";

void moveFile(const char *file, const char *newDir) {
    char newFileName[sizeof(newDir) + sizeof(file)];
    strcpy(newFileName, newDir);

    int i = sizeof(file) - 1;
    for (; i > 0 and file[i] != '/'; i--);
    for (int j = i; j < sizeof(file); j++)
        newFileName[sizeof(file) + j - i] = file[j];

    if (stat(file, &statFile) == 0) {
        if (rename(file, newFileName) == 0)
            printf("Файл %s перемещен\n", file);
        else
            printf("Файл %s не перемещен\n", file);
    } else
        printf("Файл %s не найден\n", file);
}

int main(int argc, char *argv[]) {
    if (argc <= 1) {
        printf("Не указаны аргументы\n");
        return 1;
    }

    if (stat(dirName, &statFile) != 0) {
        if (mkdir(dirName, 0700) != 0) {
            printf("Не удалось создать директорию %s\n", dirName);
            return 1;
        } else
            printf("Директория %s создана\n", dirName);
    } else if (chmod(dirName, 0700) != 0) {
        printf("Не удалось использовать директорию %s\n", dirName);
        return 1;
    } else
        printf("Используется директория %s\n", dirName);

//    system((std::string("ls -la ") + dirName).c_str());

    for (int i = 1; i < argc; i++) {
        moveFile(argv[i], dirName);
    }

    chmod(dirName, 0000);
    return 0;
}
