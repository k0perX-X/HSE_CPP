#include <string>
#include <filesystem>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <atomic>
#include <vector>
#include <cstdlib>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <mutex>
#include <thread>
#include <future>
#include <fstream>
#include <random>
#include <sys/poll.h>
#include <map>
#include "consts.h"

const unsigned char STDOUT_PREFIX_WITH_SENT_SIZE = STDOUT_PREFIX.size() + sizeof(SENT_TYPE);
const unsigned char STDERR_PREFIX_WITH_SENT_SIZE = STDERR_PREFIX.size() + sizeof(SENT_TYPE);

//void set_random_port(unsigned short server_port) {
//    do {
//        std::random_device rd;
//        std::mt19937 gen(rd());
//        std::uniform_int_distribution<> distrib(PORT_MIN, PORT_MAX);
//        CLIENT_PORT = distrib(gen);
//    } while (CLIENT_PORT == server_port);
//}

void send_message(const std::string &address, unsigned short port, const std::string &cmd_args_in) {
    struct sockaddr_in send_addr{};
    int trueflag = 1;
    int fd;
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        throw std::runtime_error("send socket");

    if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST,
                   &trueflag, sizeof trueflag) < 0)
        throw std::runtime_error("send setsockopt");

    memset(&send_addr, 0, sizeof send_addr);
    send_addr.sin_family = AF_INET;
    send_addr.sin_port = (in_port_t) htons(port);
    // broadcasting address for unix (?)
    inet_aton(address.c_str(), &send_addr.sin_addr);

    auto letter = cmd_args_in.c_str();
    if (strlen(letter) > MAX_SIZE)
        throw std::runtime_error("import too size");

    if (sendto(fd, letter, strlen(letter), 0,
               (struct sockaddr *) &send_addr, sizeof send_addr) < 0)
        throw std::runtime_error("send");
}

class Receive {
public:
    int exit_code{};
    std::string out;
    std::string err;
};

Receive recv_answer() {
    struct sockaddr_in recv_addr{};
    int trueflag = 1;
    int fd;
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        throw std::runtime_error("recv socket");

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                   &trueflag, sizeof trueflag) < 0)
        throw std::runtime_error("recv setsockopt");

    memset(&recv_addr, 0, sizeof recv_addr);
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = (in_port_t) htons(CLIENT_PORT);
    recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *) &recv_addr, sizeof recv_addr) < 0)
        throw std::runtime_error("bind");

    Receive receive;
    std::map<SENT_TYPE, std::string> errs, outs;

    bool full_out_received = false, full_err_received = false, exit_code_received = false;
    auto buffer = (char *) malloc(MAX_SIZE);
    while (not(full_out_received & full_err_received & exit_code_received)) {
        struct sockaddr_in from{};
        socklen_t len = sizeof(from);

        if (recvfrom(fd, buffer, MAX_SIZE, 0, (struct sockaddr *) &from, &len) < 0) {
            std::cerr << "Error: " << "recv" << '\n';
            continue;
        }
        SIZE_TYPE size;
        memcpy(&size, buffer, sizeof size);
        std::string letter(buffer + sizeof size, size - sizeof size);

        if (letter.starts_with(STDOUT_PREFIX)) {
            if (size != MAX_SIZE)
                full_out_received = true;

            SENT_TYPE sent;
            memcpy(&sent, (letter.substr(STDOUT_PREFIX.size()).c_str()), sizeof sent);

            outs[sent] = letter.substr(STDOUT_PREFIX_WITH_SENT_SIZE);

        } else if (letter.starts_with(STDERR_PREFIX)) {
            if (size != MAX_SIZE)
                full_err_received = true;

            SENT_TYPE sent;
            memcpy(&sent, (letter.substr(STDERR_PREFIX.size()).c_str()), sizeof sent);

            errs[sent] = letter.substr(STDERR_PREFIX_WITH_SENT_SIZE);

        } else if (letter.starts_with(EXIT_PREFIX)) {
            exit_code_received = true;
            auto exit_code_str = letter.substr(EXIT_PREFIX.size());
            memcpy(&receive.exit_code, exit_code_str.c_str(), exit_code_str.size());
        }
    }
    free(buffer);

    SENT_TYPE point = 0;
    while (not outs.empty()) {
        if (!outs[point].empty())
            receive.out += outs[point];
        auto size = outs[point].size();
        outs.erase(point);
        point += size + STDOUT_PREFIX_WITH_SENT_SIZE;
    }

    point = 0;
    while (not errs.empty()) {
        if (!errs[point].empty())
            receive.out += errs[point];
        auto size = errs[point].size();
        errs.erase(point);
        point += size + STDERR_PREFIX_WITH_SENT_SIZE;
    }

    return receive;
}

void print_help() {
    std::cout << "client_runner address port command [args...]\nFor stdin use output redirection" << '\n';
}

int main(int argc, char *argv[]) {

    if (argc < 4) {
        print_help();
        exit(1);
    }

    std::string address(argv[1]);
    unsigned short port = std::stoi(argv[2]);

    std::string cmd_args_in(argv[3]);
    for (int i = 4; i < argc; i++)
        cmd_args_in += ' ' + std::string(argv[i]);

    if (cmd_args_in.find(SEPARATOR) != std::string::npos)
        throw std::runtime_error("separator (" + SEPARATOR + ") in command or args");

    struct pollfd fds{};
    int ret;
    fds.fd = 0; // this is STDIN
    fds.events = POLLIN;
    ret = poll(&fds, 1, 0);
    if (ret)
        cmd_args_in += SEPARATOR + std::string(std::istreambuf_iterator<char>(std::cin),
                                               std::istreambuf_iterator<char>());

//    set_random_port(port);

    send_message(address, port, cmd_args_in);

    auto recv = recv_answer();

    std::cout << recv.out;
    std::cerr << recv.err;

    return recv.exit_code;
}