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
#include <random>
#include <sys/poll.h>
#include "consts.h"

#define PORT 10501
#define PID_FILE_NAME "/var/run/server.pid"

int main(int argc, char *argv[]) {
    std::string letter = STDERR_PREFIX;

    std::cout << letter << ' ' << letter.substr(STDERR_PREFIX.size()) << ' ' << (letter.starts_with(STDERR_PREFIX));
}