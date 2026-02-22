#ifndef PTY_DEV_H
#define PTY_DEV_H

#include <atomic>
#include <thread>

class pty_dev {
    private:
        std::atomic<int> master_fd;
        int debug_level = 0;
        std::thread *recv_thread_ptr = nullptr;
        void (*recv_callback)(const char *, size_t) = nullptr;
        void* recv_thread(void);
    public:
        pty_dev();
        ~pty_dev();
        void set_debug_level(const int level);
        void set_recv_callback(void (*func)(const char *, size_t));
        bool is_connected();
        bool connect();
        void disconnect();
        void send(const char *buffer, size_t length);
};

#endif // PTY_DEV_H
