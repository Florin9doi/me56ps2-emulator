#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <unistd.h>

#include "ring_buffer.h"
#include "tcp_sock.h"
#include "pty_dev.h"
#include "isp.h"
#include "modem.h"
#include "app_context.h"

AppContext ctx;

bool parse_address(const std::string addr, struct sockaddr_in *parsed_addr)
{
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

void ring_callback()
{
    const std::string ring = "RING\r\n";
    ctx.usb_tx_buffer.enqueue(ring.c_str(), ring.length());
    ctx.usb_tx_buffer.notify_one();

    printf("Client connected.\n");
}

void recv_callback(const char *buffer, size_t length)
{
    if (ctx.connected.load()) {
        const auto sent_length = ctx.usb_tx_buffer.enqueue(buffer, length);
        if (ctx.debug_level >= 2) {
            const auto buffer_size = ctx.usb_tx_buffer.get_buffer_size();
            const auto data_count = ctx.usb_tx_buffer.get_count();
            printf("usb_tx_buffer: used %ld bytes / %ld bytes (%.f%% used).\n", (long) data_count, (long) buffer_size, (float) data_count / buffer_size);
        }
        if (sent_length < length) {
            printf("Transmit buffer is full! (overflow %ld bytes.)\n", length - sent_length);
        }
        ctx.usb_tx_buffer.notify_one();
    }
}

void *usb_bulk_in_thread(usb_raw_gadget *usb, int ep_num)
{
    struct usb_packet_control pkt;
    auto timeout_at = std::chrono::steady_clock::now();

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        while (timeout_at <= now) {
            timeout_at += std::chrono::milliseconds(40);
        }
        ctx.usb_tx_buffer.wait(timeout_at);

        pkt.data[0] = 0x31;
        pkt.data[1] = 0x60;
        int payload_length = ctx.usb_tx_buffer.dequeue(&pkt.data[2], sizeof(pkt.data) - 2);

        pkt.header.ep = ep_num;
        pkt.header.flags = 0;
        pkt.header.length = 2 + payload_length;

        if (ctx.connected.load()) {pkt.data[0] |= 0x80;}

        usb->ep_write(reinterpret_cast<struct usb_raw_ep_io *>(&pkt));
    }

    return NULL;
}

void *usb_bulk_out_thread(usb_raw_gadget *usb, int ep_num) {
    struct usb_packet_bulk pkt;
    std::string buffer;

    // modem echo flag
    bool echo = false;

    while (true) {
        pkt.header.ep = ep_num;
        pkt.header.flags = 0;
        pkt.header.length = sizeof(pkt.data);

        int ret = usb->ep_read(reinterpret_cast<struct usb_raw_ep_io *>(&pkt));
        int payload_length = pkt.data[0] >> 2;
        if (payload_length != ret - 1) {
            printf("Payload length mismatch! (payload length in header: %d, received payload: %d)\n", payload_length, ret - 1);
            payload_length = std::min(payload_length, ret - 1);
        }
        buffer.append(&pkt.data[1], payload_length);

        // Off-line mode loop
        while (!ctx.connected.load()) {
            bool enter_online = false;

            // Fetch one line from the receive buffer
            auto newline_pos = buffer.find('\x0d');
            if (newline_pos == std::string::npos) {break;}
            std::string line = buffer.substr(0, newline_pos);
            buffer.erase(0, newline_pos + 1);
            if (line.empty()) {break;}

            printf("AT command: %s\n", line.c_str());

            if (echo) {
                const auto s = line + "\r\n";
                ctx.usb_tx_buffer.enqueue(s.c_str(), s.length());
            }

            std::string reply = "OK\r\n";
            if (line == "AT&F") {echo = true;} // Restore factory default (turn on echo only in this emulator)
            if (line == "ATE0") {echo = false;} // Turn off echo
            if (line == "ATA") {
                // Answer an incoming call
                reply = "CONNECT 57600 V42\r\n";
                enter_online = true;
            }
            if (strncmp(line.c_str(), "ATD", 3) == 0) {
                // Dial. Ignore after "ATD"
                if (line.substr(3) == "100" || line.substr(3) == "T100" || line.substr(3) == "P100") {
                    // ATD100 / ATDT100 / ATDP100: use PTY
                    if (ctx.pty->connect()) {
                        reply = "CONNECT 57600 V42\r\n";
                        enter_online = true;
                        ISP::setupISP(ctx.pty->get_slave_name());
                    } else {
                        reply = "BUSY\r\n";
                    }
                } else {
                    if (ctx.sock == nullptr) {
                        reply = "BUSY\r\n";
                    } else {
                        struct sockaddr_in addr;
                        if (parse_address(line.substr(4), &addr)) {
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

        // On-line mode loop
        while (ctx.connected.load() && buffer.length() > 0) {
            if (ctx.pty->is_connected()) {
                ctx.pty->send(buffer.c_str(), buffer.length());
            } else if (ctx.sock != nullptr) {
                ctx.sock->send(buffer.c_str(), buffer.length());
            }
            buffer.clear();
        }
    }

    return NULL;
}


bool process_control_packet(usb_raw_gadget *usb, usb_raw_control_event *e, struct usb_packet_control *pkt)
{
    if (e->is_event(USB_TYPE_STANDARD, USB_REQ_GET_DESCRIPTOR)) {
        const auto descriptor_type = e->get_descriptor_type();
        if (descriptor_type == USB_DT_DEVICE) {
            memcpy(pkt->data, &ctx.current_modem->device_descriptor(), sizeof(ctx.current_modem->device_descriptor()));
            pkt->header.length = sizeof(ctx.current_modem->device_descriptor());
            return true;
        }
        if (descriptor_type == USB_DT_CONFIG) {
            const auto total_length = __le16_to_cpu(ctx.current_modem->config_descriptors().config.wTotalLength);
            memcpy(pkt->data, &ctx.current_modem->config_descriptors(), total_length);
            pkt->header.length = total_length;
            return true;
        }
        if (descriptor_type == USB_DT_STRING) {
            const auto id = e->ctrl.wValue & 0x00ff;
            if (id >= STRING_DESCRIPTORS_NUM) {return false;}
            const auto len = reinterpret_cast<const struct _usb_string_descriptor<1> *>(ctx.current_modem->string_descriptors()[id])->bLength;
            memcpy(pkt->data, ctx.current_modem->string_descriptors()[id], len);
            pkt->header.length = len;
            return true;
        }
    }
    if (e->is_event(USB_TYPE_STANDARD, USB_REQ_SET_CONFIGURATION)) {
        return ctx.current_modem->handle_set_configuration(usb, pkt);
    }
    if (e->is_event(USB_TYPE_STANDARD, USB_REQ_SET_INTERFACE)) {
        pkt->header.length = 0;
        return true;
    }
    if (e->is_event(USB_TYPE_VENDOR)) {
        return ctx.current_modem->handle_vendor_request(usb, e, pkt);
    }

    return false;
}

bool event_usb_control_loop(usb_raw_gadget *usb)
{
    usb_raw_control_event e;
    e.event.type = 0;
    e.event.length = sizeof(e.ctrl);

    struct usb_packet_control pkt;
    pkt.header.ep = 0;
    pkt.header.flags = 0;
    pkt.header.length = 0;

    usb->event_fetch(reinterpret_cast<struct usb_raw_event *>(&e.event));
    if (ctx.debug_level >= 1) {e.print_debug_log();}

    switch(e.event.type) {
        case USB_RAW_EVENT_CONNECT:
            break;
        case USB_RAW_EVENT_CONTROL:
            if (!process_control_packet(usb, &e, &pkt)) {
                usb->ep0_stall();
                break;
            }

            pkt.header.length = std::min(pkt.header.length, static_cast<unsigned int>(e.ctrl.wLength));
            if (e.ctrl.bRequestType & USB_DIR_IN) {
                usb->ep0_write(reinterpret_cast<struct usb_raw_ep_io *>(&pkt));
            } else {
                usb->ep0_read(reinterpret_cast<struct usb_raw_ep_io *>(&pkt));
            }
            break;
        default:
            break;
    }

    return true;
}

void show_usage(char *prog_name, bool verbose)
{
    printf("Usage: %s [-svh] [-m model] [ip_addr port] [usb_driver] [usb_device]\n", prog_name);
    if (!verbose) {return;}

    printf("\n");
    printf("Options:\n");
    printf("  -m    modem model (default: Omron)\n");
    printf("        Omron         Omron ME56PS2\n");
    printf("        OnlineStation Suntac OnlineStation MS56KPS2\n");
    printf("        SmartSCM      Conexant SmartSCM P2Gate\n");
    printf("  -s    run as server\n");
    printf("  -v    verbose. increment log level\n");
    printf("  -h    show this help message.\n");
    printf("\n");
    printf("Parameters:\n");
    printf("  ip_addr       server IPv4 address (required for socket mode)\n");
    printf("  port          port number (required for socket mode)\n");
    printf("  usb_driver    driver name (default: %s)\n", USB_RAW_GADGET_DRIVER_DEFAULT);
    printf("  usb_device    device name (default: %s)\n", USB_RAW_GADGET_DEVICE_DEFAULT);
    printf("\n");
    printf("PTY mode: dial ATD100 from the modem to open a PTY slave device.\n");
    return;
}

int main(int argc, char *argv[])
{
    ctx.current_modem = nullptr;
    const char *driver = USB_RAW_GADGET_DRIVER_DEFAULT;
    const char *device = USB_RAW_GADGET_DEVICE_DEFAULT;
    const char *ip_addr = nullptr;
    int port = -1;
    bool is_server = false;

    int opt;
    while((opt = getopt(argc, argv, "m:svh")) != -1) {
        switch(opt) {
            case 'm': {
                ctx.current_modem = Modem::getInstance(optarg);
                if (ctx.current_modem == nullptr) {
                    fprintf(stderr, "Unknown modem model: %s\n", optarg);
                    show_usage(argv[0], false);
                    exit(1);
                }
                break;
            }
            case 's':
                is_server = true;
                break;
            case 'v':
                ctx.debug_level++;
                break;
            case 'h':
                show_usage(argv[0], true);
                exit(0);
            default:
                show_usage(argv[0], false);
                exit(1);
        }
    }

    if (ctx.current_modem == nullptr) {
        ctx.current_modem = Modem::getInstance("Omron");
    }

    // Check if next two args look like ip_addr and port (socket mode)
    if (optind + 1 < argc) {
        const char *next_arg = argv[optind];
        struct sockaddr_in probe_addr;
        if (parse_address(next_arg, &probe_addr) || inet_addr(next_arg) != INADDR_NONE) {
            ip_addr = argv[optind++];
            port = atoi(argv[optind++]);
        }
    }
    if (optind < argc) {driver = argv[optind++];}
    if (optind < argc) {device = argv[optind++];}

    usb_raw_gadget *usb = new usb_raw_gadget("/dev/raw-gadget");
    usb->set_debug_level(ctx.debug_level);
    usb->init(USB_SPEED_HIGH, driver, device);
    usb->run();

    ctx.pty = new pty_dev();
    ctx.pty->set_debug_level(ctx.debug_level);
    ctx.pty->set_recv_callback(recv_callback);

    if (ip_addr != nullptr && port != -1) {
        ctx.sock = new tcp_sock(is_server, ip_addr, port);
        ctx.sock->set_debug_level(ctx.debug_level);
        ctx.sock->set_ring_callback(ring_callback);
        ctx.sock->set_recv_callback(recv_callback);
    }

    while(event_usb_control_loop(usb));

    delete usb;

    return 0;
}
