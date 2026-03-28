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

bool process_control_packet(usb_raw_control_event *e, struct usb_packet_control *pkt)
{
    if (e->is_event(USB_TYPE_STANDARD, USB_REQ_GET_DESCRIPTOR)) {
        const auto descriptor_type = e->get_descriptor_type();
        if (descriptor_type == USB_DT_DEVICE) {
            memcpy(pkt->data, &ctx.current_modem->device_descriptor(), sizeof(ctx.current_modem->device_descriptor()));
            pkt->header.length = sizeof(ctx.current_modem->device_descriptor());
            return true;
        }
        if (descriptor_type == USB_DT_CONFIG) {
            const auto id = e->ctrl.wValue & 0x00ff;
            const auto total_length = __le16_to_cpu(ctx.current_modem->config_descriptors(id).config.wTotalLength);
            memcpy(pkt->data, &ctx.current_modem->config_descriptors(id), total_length);
            pkt->header.length = total_length;
            return true;
        }
        if (descriptor_type == USB_DT_STRING) {
            const auto id = e->ctrl.wValue & 0x00ff;
            if (id >= STRING_DESCRIPTORS_NUM) {return false;}
            const auto len = std::min<uint16_t>(e->ctrl.wLength,
                reinterpret_cast<const struct _usb_string_descriptor<1> *>(ctx.current_modem->string_descriptors()[id])->bLength);
            memcpy(pkt->data, ctx.current_modem->string_descriptors()[id], len);
            pkt->data[0] = len;
            pkt->header.length = len;
            return true;
        }
    }
    if (e->is_event(USB_TYPE_STANDARD, USB_REQ_SET_CONFIGURATION)) {
        return ctx.current_modem->handle_set_configuration(e, pkt);
    }
    if (e->is_event(USB_TYPE_STANDARD, USB_REQ_SET_INTERFACE)) {
        pkt->header.length = 0;
        return true;
    }
    if (e->is_event(USB_TYPE_VENDOR) || e->is_event(USB_TYPE_CLASS)) {
        return ctx.current_modem->handle_control_request(e, pkt);
    }

    return false;
}

bool event_usb_control_loop()
{
    usb_raw_control_event e;
    e.event.type = 0;
    e.event.length = sizeof(e.ctrl);

    struct usb_packet_control pkt;
    pkt.header.ep = 0;
    pkt.header.flags = 0;
    pkt.header.length = 0;

    ctx.usb->event_fetch(reinterpret_cast<struct usb_raw_event *>(&e.event));
    if (ctx.debug_level >= 1) {e.print_debug_log();}

    switch(e.event.type) {
        case USB_RAW_EVENT_CONNECT:
            break;
        case USB_RAW_EVENT_CONTROL:
            if ((e.ctrl.bRequestType & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT) {
                pkt.header.length = e.ctrl.wLength;
                ctx.usb->ep0_read(reinterpret_cast<struct usb_raw_ep_io *>(&pkt));
            }

            if (!process_control_packet(&e, &pkt)) {
                ctx.usb->ep0_stall();
                break;
            }

            pkt.header.length = std::min(pkt.header.length, static_cast<unsigned int>(e.ctrl.wLength));
            if (e.ctrl.bRequestType & USB_DIR_IN) {
                ctx.usb->ep0_write(reinterpret_cast<struct usb_raw_ep_io *>(&pkt));
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
    printf("        Omron         Omron Viaggio (ME56PS2)\n");
    printf("        OnlineStation Suntac OnlineStation (MS56KPS2)\n");
    printf("        SmartSCM      Conexant SmartSCM (P2GATE)\n");
    printf("        Lucent        Multi-Tech MultiMobile (MT5634MU)\n");
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
        if (Modem::parse_address(next_arg, &probe_addr) || inet_addr(next_arg) != INADDR_NONE) {
            ip_addr = argv[optind++];
            port = atoi(argv[optind++]);
        }
    }
    if (optind < argc) {driver = argv[optind++];}
    if (optind < argc) {device = argv[optind++];}

    ctx.usb = new usb_raw_gadget("/dev/raw-gadget");
    ctx.usb->set_debug_level(ctx.debug_level);
    ctx.usb->init(USB_SPEED_FULL, driver, device);
    ctx.usb->run();

    ctx.pty = new pty_dev();
    ctx.pty->set_debug_level(ctx.debug_level);
    ctx.pty->set_recv_callback(recv_callback);

    if (ip_addr != nullptr && port != -1) {
        ctx.sock = new tcp_sock(is_server, ip_addr, port);
        ctx.sock->set_debug_level(ctx.debug_level);
        ctx.sock->set_ring_callback(ring_callback);
        ctx.sock->set_recv_callback(recv_callback);
    }

    while(event_usb_control_loop());

    delete ctx.usb;

    return 0;
}
