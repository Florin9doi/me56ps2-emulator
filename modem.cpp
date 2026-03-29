#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include "modem.h"
#include "modem_lucent.h"
#include "modem_omron.h"
#include "modem_onlinestation.h"
#include "modem_smartscm.h"
#include "tcp_sock.h"
#include "pty_dev.h"
#include "app_context.h"
#include "isp.h"

bool Modem::parse_address(const std::string &addr, struct sockaddr_in *parsed_addr) {
    // Input format: "000-000-000-000#00000"
    int d[4] = {0, 0, 0, 0};
    int port = TCP_DEFAULT_PORT;

    auto has_port = addr.find('#') != std::string::npos;

    // Parse IPv4 address
    if (has_port) {
        auto ret = sscanf(addr.c_str(), "%u-%u-%u-%u#%u", &d[0], &d[1], &d[2], &d[3], &port);
        if (ret != 5) {return false;}
    } else {
        auto ret = sscanf(addr.c_str(), "%u-%u-%u-%u", &d[0], &d[1], &d[2], &d[3]);
        if (ret != 4) {return false;}
    }

    // Check each digit range
    for (int i = 0; i < 4; i++) {
        if (d[i] < 0 || d[i] > 255) {return false;}
    }

    // Check port range (1 - 65535)
    if (port < 1 || port > 65535) {return false;}

    char ip_addr[16];
    sprintf(ip_addr, "%d.%d.%d.%d", d[0], d[1], d[2], d[3]);

    memset(parsed_addr, 0, sizeof(*parsed_addr));
    parsed_addr->sin_family = AF_INET;
    parsed_addr->sin_port = htons(port);
    parsed_addr->sin_addr.s_addr = inet_addr(ip_addr);

    return true;
}

struct modem_entry {
    const char *name;
    Modem *(*factory)();
} static const registry[] = {
    { "Lucent",        []() -> Modem * { return new LucentModem();        } },
    { "Omron",         []() -> Modem * { return new OmronModem();         } },
    { "OnlineStation", []() -> Modem * { return new OnlineStationModem(); } },
    { "SmartSCM",      []() -> Modem * { return new SmartSCMModem();      } },
};

Modem *Modem::getInstance(const char *name) {
    static Modem *instance = nullptr;
    static const char *instance_name = nullptr;
    if (instance != nullptr && strcmp(instance_name, name) == 0) {
        return instance;
    }
    for (const auto &entry : registry) {
        if (strcmp(entry.name, name) == 0) {
            delete instance;
            instance = entry.factory();
            instance_name = entry.name;
            return instance;
        }
    }
    return nullptr;
}

bool Modem::handle_set_configuration(usb_raw_control_event *e, struct usb_packet_control *pkt) {
    const auto id = e->ctrl.wValue & 0x00ff;
    ctx.usb->vbus_draw(config_descriptors(id).config.bMaxPower);
    ctx.usb->configure();
    printf("USB configured.\n");
    pkt->header.length = 0;
    return true;
}

bool Modem::handle_control_request(usb_raw_control_event *e, struct usb_packet_control *pkt) {
    (void)e;
    (void)pkt;
    return true;
}

void Modem::process_at(std::string &line) {
    bool enter_online = false;
    printf("AT command: %s\n", line.c_str());

    if (echo) {
        const auto s = line + "\r\r\n";
        ctx.usb_tx_buffer.enqueue(s.c_str(), s.length());
    }

    if (process_at_ext(line)) {
        return;
    }

    std::string reply = "OK\r\n";
    if (line == "ATZ" || line == "AT&F") {echo = true;}
    if (line == "ATE0") {echo = false;}
    if (line == "ATA") {
        reply = "CONNECT 57600 V42\r\n";
        enter_online = true;
    }
    if (strncmp(line.c_str(), "ATD", 3) == 0) {
        // PPP
        if (line.substr(3) == "100" || line.substr(3) == "T100" || line.substr(3) == "P100"
         || line.substr(3) == "T186,005363121201" || line.substr(3) == "P186,005363121201") {
            if (ctx.pty->connect()) {
                reply = "CONNECT 57600 V42\r\n";
                enter_online = true;
                ISP::setupISP(ctx.pty->get_slave_name());
            } else {
                reply = "BUSY\r\n";
            }

        // P2P
        } else {
            if (ctx.sock == nullptr) {
                reply = "BUSY\r\n";
            } else {
                struct sockaddr_in addr;
                if (Modem::parse_address(line.substr(4), &addr)) {
                    ctx.sock->set_addr(&addr);
                }
                if (ctx.sock->connect()) {
                    reply = "CONNECT 57600 V42\r\n";
                    enter_online = true;
                } else {
                    reply = "BUSY\r\n";
                }
            }
        }
    }

    ctx.usb_tx_buffer.enqueue(reply.c_str(), reply.length());
    ctx.usb_tx_buffer.notify_one();

    if (enter_online) {
        printf("Enter on-line mode.\n");
        ctx.connected.store(true);
    }
}

bool Modem::process_at_ext(std::string&) {
    return false;
}

void Modem::handle_disconnect() {
    ctx.connected.store(false);
    if (ctx.sock != nullptr && ctx.sock->is_connected()) {
        ctx.sock->disconnect();
        printf("disconnected.\n");
    }
    if (ctx.pty != nullptr && ctx.pty->is_connected()) {
        ctx.pty->disconnect();
        printf("disconnected.\n");
    }
}
