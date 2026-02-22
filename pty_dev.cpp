#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <thread>

#include "pty_dev.h"

void* pty_dev::recv_thread(void)
{
    fd_set readfds;
    timeval recv_timeout = {.tv_sec = 0, .tv_usec = 100 * 1000}; // 100ms
    auto fd = master_fd.load();
    char buf[64];

    if (debug_level >= 1) {printf("pty_dev: start recv_thread.\n");}
    while (true) {
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        auto ret = select(fd + 1, &readfds, nullptr, nullptr, &recv_timeout);
        if (ret < 0) {
            printf("pty_dev: select(): %s\n", std::strerror(errno));
            break;
        }
        if (ret == 0) {
            if (!is_connected()) {
                break;
            }
            continue;
        }

        auto len = read(fd, buf, sizeof(buf));
        if (len < 0) {
            if (errno == EIO) {
                // slave side closed (no process has the slave open)
                if (debug_level >= 1) {printf("pty_dev: slave closed.\n");}
            } else {
                printf("pty_dev: read(): %s\n", std::strerror(errno));
            }
            break;
        }
        if (len == 0) {
            printf("pty_dev: connection closed.\n");
            break;
        }
        if (debug_level >= 2) {printf("pty_dev: received %ld bytes.\n", len);}
        (*recv_callback)(buf, len);
    }

    return nullptr;
}

pty_dev::pty_dev()
{
    master_fd.store(0);
}

pty_dev::~pty_dev()
{
    disconnect();
}

void pty_dev::set_debug_level(const int level)
{
    debug_level = level;
}

void pty_dev::set_recv_callback(void (*func)(const char *, size_t))
{
    recv_callback = func;
}

bool pty_dev::is_connected()
{
    return master_fd.load() != 0;
}

bool pty_dev::connect()
{
    int fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (fd < 0) {
        printf("pty_dev: posix_openpt(): %s\n", std::strerror(errno));
        return false;
    }

    if (grantpt(fd) < 0) {
        printf("pty_dev: grantpt(): %s\n", std::strerror(errno));
        close(fd);
        return false;
    }

    if (unlockpt(fd) < 0) {
        printf("pty_dev: unlockpt(): %s\n", std::strerror(errno));
        close(fd);
        return false;
    }

    // Set raw mode so binary PPP frames pass through without echo or
    // line-discipline processing (required for pppd compatibility)
    struct termios tios;
    if (tcgetattr(fd, &tios) < 0) {
        printf("pty_dev: tcgetattr(): %s\n", std::strerror(errno));
        close(fd);
        return false;
    }
    cfmakeraw(&tios);
    if (tcsetattr(fd, TCSANOW, &tios) < 0) {
        printf("pty_dev: tcsetattr(): %s\n", std::strerror(errno));
        close(fd);
        return false;
    }

    char slave_name[256];
    if (ptsname_r(fd, slave_name, sizeof(slave_name)) != 0) {
        printf("pty_dev: ptsname_r(): %s\n", std::strerror(errno));
        close(fd);
        return false;
    }

    printf("pty_dev: PTY slave device: %s\n", slave_name);

    master_fd.store(fd);
    recv_thread_ptr = new std::thread([this]{pty_dev::recv_thread();});
    return true;
}

void pty_dev::disconnect()
{
    auto fd = master_fd.load();
    if (fd != 0) {
        close(fd);
        master_fd.store(0);
    }
    if (recv_thread_ptr != nullptr) {
        if (recv_thread_ptr->joinable()) {
            recv_thread_ptr->join();
        }
        delete recv_thread_ptr;
        recv_thread_ptr = nullptr;
    }
}

void pty_dev::send(const char *buffer, size_t length)
{
    size_t ptr = 0;
    auto fd = master_fd.load();
    if (fd == 0) {
        printf("pty_dev: not connected.\n");
        return;
    }
    while (ptr < length) {
        auto ret = write(fd, buffer + ptr, length - ptr);
        if (ret < 0) {
            printf("pty_dev: write(): %s\n", std::strerror(errno));
            break;
        }
        ptr += ret;
    }
}
