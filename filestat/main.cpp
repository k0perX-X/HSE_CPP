#include <iostream>
#include <dirent.h>
#include <unistd.h>

const char *getTypeName(int typeNumber) {
    switch (typeNumber) {
        case DT_REG: return "Regular file";
        case DT_DIR: return "Directory";
        case DT_LNK: return "Symbolic link";
        case DT_SOCK: return "Socket";
        case DT_WHT: return "Link";
        case DT_FIFO: return "Pipe";
        case DT_CHR: return "Character device";
        case DT_BLK: return "Block device";
        default: return nullptr;
    }
}

int main() {
    // Получаем путь
    char path[256];
    getcwd(path, 256);

    // Открываем директорию
    DIR* dir = opendir(path);
    printf("Текущая директория: %s\n", path);
    // DIR* dir = opendir("."); // альтернативный способ

    // Проверка открытия директории
    if (dir == nullptr) {
        printf("Ну тут я умываю руки\n");
        return 1;
    }

    printf("Имя файла              Тип\n");
    dirent *file;
    while ((file = readdir(dir)) != nullptr) {
        // Заполняем имя файла для выравнивания
        char name[24] = { ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','\0' };
        // Переносим имя
        for (int i = 0; i < 23 and file->d_name[i] != '\0';i++)
                name[i] = file->d_name[i];
        // Выводим результат
        printf("%s%i:%s\n", name, file->d_type, getTypeName(file->d_type));
    }

    return 0;
}
