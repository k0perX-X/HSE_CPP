#include <string>
#include <filesystem>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <atomic>
#include <sys/types.h>
#include <cerrno>
#include <vector>
#include <cstdlib>
#include <netinet/in.h>
#include <sys/socket.h>
#include <mutex>
#include <thread>
#include <future>
#include <fstream>
#include <set>
#include <array>
#include <sys/wait.h>
#include "consts.h"
#include <arpa/inet.h>

#define PORT 10501
#define PID_FILE_NAME "/var/run/server.pid"

[[noreturn]] void close_program();

void sigint_handler([[maybe_unused]] int sig) {
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

void errno_abort(const char *error) {
    throw std::runtime_error(error);
}

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

pthread_t *get_prt() {
    std::lock_guard<std::mutex> lock(threads_id_mtx);
    if (threads_id.empty()) {
        delete_pid();
        exit(0);
    } else {
        return (pthread_t * ) & (*threads_id.begin());
    }
}

[[noreturn]] void close_program() {
    while (true) {
        pthread_t *ptr = get_prt();
        pthread_join(*ptr, nullptr);
    }
}

class Command {
public:
    int ExitStatus = 0;
    std::string Command;
    std::string StdIn;
    std::string StdOut;
    std::string StdErr;

    void execute() {
#define READ_END 0
#define WRITE_END 1

        int infd[2] = {0, 0};
        int outfd[2] = {0, 0};
        int errfd[2] = {0, 0};

        auto cleanup = [&]() {
            ::close(infd[READ_END]);
            ::close(infd[WRITE_END]);

            ::close(outfd[READ_END]);
            ::close(outfd[WRITE_END]);

            ::close(errfd[READ_END]);
            ::close(errfd[WRITE_END]);
        };

        auto rc = ::pipe(infd);
        if (rc < 0) {
            throw std::runtime_error(std::strerror(errno));
        }

        rc = ::pipe(outfd);
        if (rc < 0) {
            ::close(infd[READ_END]);
            ::close(infd[WRITE_END]);
            throw std::runtime_error(std::strerror(errno));
        }

        rc = ::pipe(errfd);
        if (rc < 0) {
            ::close(infd[READ_END]);
            ::close(infd[WRITE_END]);

            ::close(outfd[READ_END]);
            ::close(outfd[WRITE_END]);
            throw std::runtime_error(std::strerror(errno));
        }

        auto pid = fork();
        if (pid > 0) // PARENT
        {
            ::close(infd[READ_END]);    // Parent does not read from stdin
            ::close(outfd[WRITE_END]);  // Parent does not write to stdout
            ::close(errfd[WRITE_END]);  // Parent does not write to stderr

            if (::write(infd[WRITE_END], StdIn.data(), StdIn.size()) < 0) {
                throw std::runtime_error(std::strerror(errno));
            }
            ::close(infd[WRITE_END]); // Done writing
        } else if (pid == 0) // CHILD
        {
            ::dup2(infd[READ_END], STDIN_FILENO);
            ::dup2(outfd[WRITE_END], STDOUT_FILENO);
            ::dup2(errfd[WRITE_END], STDERR_FILENO);

            ::close(infd[WRITE_END]);   // Child does not write to stdin
            ::close(outfd[READ_END]);   // Child does not read from stdout
            ::close(errfd[READ_END]);   // Child does not read from stderr

            ::execl("/bin/bash", "bash", "-c", Command.c_str(), nullptr);
            ::exit(EXIT_SUCCESS);
        }

        // PARENT
        if (pid < 0) {
            cleanup();
            throw std::runtime_error("Failed to fork");
        }

        int status = 0;
        ::waitpid(pid, &status, 0);

        std::array<char, 256> buffer{};

        ssize_t bytes = 0;
        do {
            bytes = ::read(outfd[READ_END], buffer.data(), buffer.size());
            StdOut.append(buffer.data(), bytes);
        } while (bytes > 0);

        do {
            bytes = ::read(errfd[READ_END], buffer.data(), buffer.size());
            StdErr.append(buffer.data(), bytes);
        } while (bytes > 0);

        if (WIFEXITED(status)) {
            ExitStatus = WEXITSTATUS(status);
        }

        cleanup();
    }
};

void send(int fd, const char *letter, unsigned short size, struct sockaddr_in send_addr) {
    if (sendto(fd, letter, size, 0, (struct sockaddr *) &send_addr, sizeof send_addr) < 0)
        errno_abort("send");
}

void send_with_prefix(int fd, const std::string &message, struct sockaddr_in send_addr, const std::string &prefix) {
    SENT_TYPE sent = 0;
    while (sent <= message.size()) {
        std::vector<char> prefix_with_sent(sizeof(SIZE_TYPE) + prefix.size() + sizeof sent);
        memcpy(prefix_with_sent.data() + sizeof(SIZE_TYPE), prefix.c_str(), prefix.size());
        memcpy((void *) (prefix_with_sent.data() + prefix.size() + sizeof(SIZE_TYPE)), &sent, sizeof sent);


        // специально >=, чтобы последняя отправка всегда была через else
        if (prefix_with_sent.size() + message.size() - sent >= MAX_SIZE) {
            SIZE_TYPE size = MAX_SIZE;
            auto letter_mem = (char *)malloc(size);
            std::string letter(letter_mem, size);

            memcpy(letter.data(), prefix_with_sent.data(), prefix_with_sent.size());
            memcpy(letter.data(), &size, sizeof size);
            memcpy(letter.data() + prefix_with_sent.size(), message.c_str() + sent,
                   size - prefix_with_sent.size());

            send(fd, letter.c_str(), size, send_addr);
            free(letter_mem);
            sent += MAX_SIZE - prefix_with_sent.size();
        } else {
            SIZE_TYPE size = prefix_with_sent.size() + message.size() - sent;
            auto letter_mem = (char *)malloc(size);
            std::string letter(letter_mem, size);

            memcpy(letter.data(), prefix_with_sent.data(), prefix_with_sent.size());
            memcpy(letter.data(), &size, sizeof size);
            memcpy(letter.data() + prefix_with_sent.size(), message.c_str() + sent,
                   size - prefix_with_sent.size());

            send(fd, letter.c_str(), size, send_addr);
            free(letter_mem);
            sent += message.size() - sent + 1;
        }
    }
}

void send_reply(int exit, const std::string &out, const std::string &err, struct sockaddr_in from) {
    int trueflag = 1;
    int fd;
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        errno_abort("socket");

    if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST,
                   &trueflag, sizeof trueflag) < 0)
        errno_abort("setsockopt");

    SIZE_TYPE size = sizeof size + EXIT_PREFIX.size() + sizeof exit;
    auto exitcode_mem = (char *)malloc(size);
    std::string exitcode_message(exitcode_mem, size);
    memcpy(exitcode_message.data(), &size, sizeof size);
    memcpy(exitcode_message.data() + sizeof size, EXIT_PREFIX.c_str(), EXIT_PREFIX.size());
    memcpy(exitcode_message.data() + sizeof size + EXIT_PREFIX.size(), &exit, sizeof exit);
    send(fd, exitcode_message.c_str(), exitcode_message.size(), from);
    free(exitcode_mem);

    send_with_prefix(fd, out, from, STDOUT_PREFIX);

    send_with_prefix(fd, err, from, STDERR_PREFIX);

}

void cmd_handler(char *buffer, struct sockaddr_in from) {
    std::string message(buffer, strlen(buffer));
    free(buffer);
    size_t separator_position = message.find(SEPARATOR);

    std::cout << message << '\n';

    Command cmd;
    cmd.Command = message.substr(0, separator_position);
    if (separator_position != std::string::npos) {
        cmd.StdIn = message.substr(separator_position, std::string::npos);
    }

    cmd.execute();

    send_reply(cmd.ExitStatus, cmd.StdOut, cmd.StdErr, from);

    delete_thread(std::this_thread::get_id());
}

[[noreturn]] void daemon() {
    set_signals_handlers();

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

    while (true) {
        auto buffer = (char *) malloc(MAX_SIZE);
        struct sockaddr_in from{};
        socklen_t len = sizeof(from);
        if (recvfrom(fd, buffer, MAX_SIZE, 0, (struct sockaddr *) &from, &len) < 0) {
            std::cerr << "Error: " << "recv" << '\n';
            continue;
        }
        from.sin_port = (in_port_t) htons(CLIENT_PORT);
        std::thread th(cmd_handler, buffer, from);
        add_thread(th.get_id());
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
    daemon();

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