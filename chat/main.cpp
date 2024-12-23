#include <cstdlib>
#include <iostream>
#include <netinet/in.h>
#include <cstdio>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <csignal>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

#define default_port 5010
#define nBuf 1024
#define separator ':'

//std::atomic<pid_t> recv_loop_pid(0);

void errno_abort(const char *header) {
    std::cerr << "Error on " << header << '\n';
    exit(EXIT_FAILURE);
}

[[noreturn]] void recv_loop(int fd, const char *name) {
//    recv_loop_pid.store((pid_t)syscall(SYS_gettid));
    char *rbuf = (char *) malloc(nBuf);
    while (true) {
        if (recv(fd, rbuf, nBuf, 0) < 0) {
            std::cerr << "Error: " << "recv" << '\n';
            continue;
        }
        bool out = false;
        int i;
        for (i = 0; i < strlen(name); i++)
            if (rbuf[i] != name[i]) {
                out = true;
                break;
            }
        if (rbuf[i] != separator)
            out = true;
        if (out){
            std::cout << rbuf << '\n';
        }
    }
    free(rbuf);
}

void send_loop(const std::string &name, int fd, struct sockaddr_in send_addr) {
    while (true) {
        std::string message;
        std::cin >> message;
//        message = "test";
        if (message == "/q")
            break;
        std::string letter;
        letter += name;
        letter += separator;
        letter += ' ';
        letter += message;
        const char *cletter = letter.c_str();
        if (sendto(fd, cletter, strlen(cletter) + 1, 0,
                   (struct sockaddr *) &send_addr, sizeof send_addr) < 0)
            errno_abort("send");
    }
}

void loop(unsigned short port) {
    std::cout << "Port: " << port << '\n';

    std::string name;
    std::cout << "For exit enter /q\nEnter name: ";
    std::cin >> name;

    struct sockaddr_in send_addr{}, recv_addr{};
    int trueflag = 1;
    int fd;
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        errno_abort("socket");

    if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST,
                   &trueflag, sizeof trueflag) < 0)
        errno_abort("setsockopt");

    memset(&send_addr, 0, sizeof send_addr);
    send_addr.sin_family = AF_INET;
    send_addr.sin_port = (in_port_t) htons(port);
    // broadcasting address for unix (?)
    inet_aton("127.255.255.255", &send_addr.sin_addr);
//    send_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);


    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                   &trueflag, sizeof trueflag) < 0)
        errno_abort("setsockopt");

    memset(&recv_addr, 0, sizeof recv_addr);
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = (in_port_t) htons(port);
    recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *) &recv_addr, sizeof recv_addr) < 0)
        errno_abort("bind");

    std::thread thread_obj(recv_loop, fd, name.c_str());
    thread_obj.detach(); //чтобы код выхода треда не влиял на основой код выхода

    send_loop(name, fd, send_addr);

//    std::cout << recv_loop_pid.load() << '\n';
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        loop(default_port);
    } else if (argc == 2) {
        char *endptr;
        auto port = strtol(argv[1], &endptr, 10);
        if (endptr == argv[1]) {
            std::cerr << "Invalid number: " << argv[1] << '\n';
        } else if (*endptr) {
            std::cerr << "Trailing characters after number: " << argv[1] << '\n';
        } else if (errno == ERANGE or port > 65535) {
            std::cerr << "Number out of range: " << argv[1] << '\n';
        } else {
            loop(port);
        }
    }
    return 0;
}
