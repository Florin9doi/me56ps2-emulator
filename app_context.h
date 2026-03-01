#pragma once

#include <atomic>
#include "ring_buffer.h"

class tcp_sock;
class pty_dev;
class Modem;

struct AppContext {
    ring_buffer<char> usb_tx_buffer{524288};
    tcp_sock *sock = nullptr;
    pty_dev *pty = nullptr;
    int debug_level = 0;
    std::atomic<bool> connected{false};
    Modem *current_modem = nullptr;
};

extern AppContext ctx;
