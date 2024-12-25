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
#include <arpa/inet.h>
#include <map>
#include <set>
#include <sys/param.h>

#define PORT 10501
#define MAX_KEYS_STORE 100
#define PID_FILE_NAME "/var/run/server.pid"
#define LOG_DIR "./keys/"

[[noreturn]] void close_program();

void sigint_handler(int sig) {
    std::cerr << "Stopping " << gettid() << '\n';
    close_program();
}

void set_signals_handlers() {
//    signal(SIGKILL, sigint_handler);
//    signal(SIGABRT, sigint_handler);
//    signal(SIGILL, sigint_handler);
//    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
}

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

void errno_abort(const char *header) {
    std::cerr << "Error on " << header << '\n';
    exit(1);
}

std::map<uint32_t, std::unique_ptr<std::mutex>> files_mtx = {};
std::set<std::thread::id> threads_id = {};
std::mutex threads_id_mtx;

void add_thread(std::thread::id id) {
    std::lock_guard<std::mutex> lock(threads_id_mtx);
    threads_id.insert(id);
}

void delete_thread(std::thread::id id) {
    std::lock_guard<std::mutex> lock(threads_id_mtx);
    threads_id.erase(id);
}

void save_keys(char *rbuf, struct sockaddr_in from) {
    auto id = std::this_thread::get_id();
    add_thread(id);

    if (files_mtx.count(from.sin_addr.s_addr) == 0)
        files_mtx[from.sin_addr.s_addr] = std::make_unique<std::mutex>();

    std::string addr(inet_ntoa(from.sin_addr));
    files_mtx[from.sin_addr.s_addr]->lock();
    std::fstream f;
    f.open(LOG_DIR + addr, std::ios::app);
    f << rbuf;
    free(rbuf);
    f.close();
    files_mtx[from.sin_addr.s_addr]->unlock();

    delete_thread(id);
}

pthread_t* get_prt() {
    std::lock_guard<std::mutex> lock(threads_id_mtx);
    if(threads_id.empty()) {
        delete_pid();
        exit(0);
    } else {
        return (pthread_t*)&(*threads_id.begin());
    }
}

[[noreturn]] void close_program() {
    while (true) {
        pthread_t* ptr = get_prt();
        pthread_join(*ptr, nullptr);
    }
}

[[noreturn]] void daemon() {
    set_signals_handlers();

    if (!std::filesystem::exists(LOG_DIR)) {
        if (!std::filesystem::create_directory(LOG_DIR))
            errno_abort("create LOG_DIR");
    } else {
        if (!std::filesystem::is_directory(LOG_DIR))
            errno_abort("LOG_DIR is not directory");
    }

    struct sockaddr_in recv_addr{};
    int trueflag = 1;
    int fd;
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        errno_abort("socket");

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                   &trueflag, sizeof trueflag) < 0)
        errno_abort("setsockopt");

    memset(&recv_addr, 0, sizeof recv_addr);
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = (in_port_t) htons(PORT);
    recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *) &recv_addr, sizeof recv_addr) < 0)
        errno_abort("bind");

    auto nbuf = (10 + 1) * MAX_KEYS_STORE + 1;
    while (true) {
        char *rbuf = (char *) malloc(nbuf);
        struct sockaddr_in from{};
        socklen_t len = sizeof(from);
        if (recvfrom(fd, rbuf, nbuf, 0, (struct sockaddr *) &from, &len) < 0) {
            std::cerr << "Error: " << "recv" << '\n';
            continue;
        }
        std::thread th(save_keys, rbuf, from);
        th.detach();
    }
}

void print_help() {
    std::cout << "server [start/stop/status]" << '\n';
}

void start_daemon() {
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
    daemon();
}

void stop_daemon() {
    kill(read_pid(), SIGTERM);
}

int main(int argc, char *argv[]) {
//    daemon();

    if (argc != 2) {
        print_help();
        exit(1);
    }

    std::string argument(argv[1]);
    if (argument == "start") {
        if (!check_daemon()) {
            start_daemon();
            return 0;
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