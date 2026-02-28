#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <unistd.h>

#include "usb_raw_gadget.h"
#include "usb_raw_control_event.h"
#include "ring_buffer.h"
#include "tcp_sock.h"
#include "pty_dev.h"
#include "isp.h"
#include "me56ps2.h"

std::thread *thread_bulk_in = nullptr;
std::thread *thread_bulk_out = nullptr;

ring_buffer<char> usb_tx_buffer(524288);
tcp_sock *sock;
pty_dev *pty;

int debug_level = 0;

std::atomic<bool> connected(false);

static const struct _usb_string_descriptor<1> str_lang = {
    .bLength = sizeof(str_lang),
    .bDescriptorType = USB_DT_STRING,
    .wData = {__constant_cpu_to_le16(0x0409)},
};

static const struct _usb_string_descriptor<5> me56ps2_str_manufacturer = {
    .bLength = sizeof(me56ps2_str_manufacturer),
    .bDescriptorType = USB_DT_STRING,
    .wData = {u'O', u'M', u'R', u'O', u'N'},
};
static const struct _usb_string_descriptor<7> me56ps2_str_product = {
    .bLength = sizeof(me56ps2_str_product),
    .bDescriptorType = USB_DT_STRING,
    .wData = {u'M', u'E', u'5', u'6', u'P', u'S', u'2'},
};
static const struct _usb_string_descriptor<3> me56ps2_str_serial = {
    .bLength = sizeof(me56ps2_str_serial),
    .bDescriptorType = USB_DT_STRING,
    .wData = {u'N', u'/', u'A'},
};

static const struct _usb_string_descriptor<6> ms56kps2_str_manufacturer = {
    .bLength = sizeof(ms56kps2_str_manufacturer),
    .bDescriptorType = USB_DT_STRING,
    .wData = {u'S', u'U', u'N', u'T', u'A', u'C'},
};
static const struct _usb_string_descriptor<8> ms56kps2_str_product = {
    .bLength = sizeof(ms56kps2_str_product),
    .bDescriptorType = USB_DT_STRING,
    .wData = {u'M', u'S', u'5', u'6', u'K', u'P', u'S', u'2'},
};
static const struct _usb_string_descriptor<3> ms56kps2_str_serial = {
    .bLength = sizeof(ms56kps2_str_serial),
    .bDescriptorType = USB_DT_STRING,
    .wData = {u'N', u'/', u'A'},
};

static const struct _usb_string_descriptor<22> p2gate_str_manufacturer = {
    .bLength = sizeof(p2gate_str_manufacturer),
    .bDescriptorType = USB_DT_STRING,
    .wData = {u'C', u'o', u'n', u'e', u'x', u'a', u'n', u't', u' ', u'S', u'y', u's', u't', u'e', u'm', u's', u',', u' ', u'I', u'n', u'c', u'.'},
};
static const struct _usb_string_descriptor<30> p2gate_str_product = {
    .bLength = sizeof(p2gate_str_product),
    .bDescriptorType = USB_DT_STRING,
    .wData = {u'V', u'.', u'9', u'0', u' ', u'M', u'o', u'd', u'e', u'm', u' ', u'w', u'i', u't', u'h', u' ', u'U', u'S', u'B', u' ', u'(', u'G', u'a', u'm', u'e', u' ', u'A', u'p', u'p', u')'},
};
static const struct _usb_string_descriptor<3> p2gate_str_serial = {
    .bLength = sizeof(p2gate_str_serial),
    .bDescriptorType = USB_DT_STRING,
    .wData = {u'N', u'/', u'A'},
};

static bool me56ps2_set_configuration(usb_raw_gadget *usb, struct usb_packet_control *pkt, const struct modem_config *cfg);
static bool ms56kps2_set_configuration(usb_raw_gadget *usb, struct usb_packet_control *pkt, const struct modem_config *cfg);
static bool p2gate_set_configuration(usb_raw_gadget *usb, struct usb_packet_control *pkt, const struct modem_config *cfg);
static bool me56ps2_vendor_request(usb_raw_gadget *usb, usb_raw_control_event *e, struct usb_packet_control *pkt);
static bool ms56kps2_vendor_request(usb_raw_gadget *usb, usb_raw_control_event *e, struct usb_packet_control *pkt);
static bool p2gate_vendor_request(usb_raw_gadget *usb, usb_raw_control_event *e, struct usb_packet_control *pkt);

static const struct modem_config modem_configs[] = {
    {
        .model_name = "me56ps2",
        .device_descriptor = {
            .bLength            = USB_DT_DEVICE_SIZE,
            .bDescriptorType    = USB_DT_DEVICE,
            .bcdUSB             = __constant_cpu_to_le16(0x0110U),
            .bDeviceClass       = 0,
            .bDeviceSubClass    = 0,
            .bDeviceProtocol    = 0,
            .bMaxPacketSize0    = 0x40,
            .idVendor           = __constant_cpu_to_le16(0x0590U), // Omron Corp.
            .idProduct          = __constant_cpu_to_le16(0x001aU), // ME56PS2
            .bcdDevice          = __constant_cpu_to_le16(0x0101U),
            .iManufacturer      = STRING_ID_MANUFACTURER,
            .iProduct           = STRING_ID_PRODUCT,
            .iSerialNumber      = STRING_ID_SERIAL,
            .bNumConfigurations = 1,
        },
        .config_descriptors = {
            .config = {
                .bLength             = USB_DT_CONFIG_SIZE,
                .bDescriptorType     = USB_DT_CONFIG,
                .wTotalLength        = __cpu_to_le16(USB_DT_CONFIG_SIZE + USB_DT_INTERFACE_SIZE + 2 * USB_DT_ENDPOINT_SIZE),
                .bNumInterfaces      = 1,
                .bConfigurationValue = 1,
                .iConfiguration      = 2,
                .bmAttributes        = USB_CONFIG_ATT_WAKEUP,
                .bMaxPower           = 0x1e,
            },
            .interface = {
                .bLength             = USB_DT_INTERFACE_SIZE,
                .bDescriptorType     = USB_DT_INTERFACE,
                .bInterfaceNumber    = 0,
                .bAlternateSetting   = 0,
                .bNumEndpoints       = 2,
                .bInterfaceClass     = 0xff,
                .bInterfaceSubClass  = 0xff,
                .bInterfaceProtocol  = 0xff,
                .iInterface          = 2,
            },
            .endpoints = {
            {
                .bLength             = USB_DT_ENDPOINT_SIZE,
                .bDescriptorType     = USB_DT_ENDPOINT,
                .bEndpointAddress    = USB_DIR_IN | 2,
                .bmAttributes        = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize      = MAX_PACKET_SIZE_BULK,
                .bInterval           = 0,
            },
            {
                .bLength             = USB_DT_ENDPOINT_SIZE,
                .bDescriptorType     = USB_DT_ENDPOINT,
                .bEndpointAddress    = USB_DIR_OUT | 2,
                .bmAttributes        = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize      = MAX_PACKET_SIZE_BULK,
                .bInterval           = 0,
            }
            }
        },
        .string_descriptors = {
            &str_lang,
            &me56ps2_str_manufacturer,
            &me56ps2_str_product,
            &me56ps2_str_serial,
        },
        .handle_set_configuration = me56ps2_set_configuration,
        .handle_vendor_request = me56ps2_vendor_request,
    },
    {
        .model_name = "p2gate",
        .device_descriptor = {
            .bLength            = USB_DT_DEVICE_SIZE,
            .bDescriptorType    = USB_DT_DEVICE,
            .bcdUSB             = __constant_cpu_to_le16(0x0100U), // USB 1.0
            .bDeviceClass       = 0xff,
            .bDeviceSubClass    = 0xff,
            .bDeviceProtocol    = 0xff,
            .bMaxPacketSize0    = 0x40,
            .idVendor           = __constant_cpu_to_le16(0x0572U), // Conexant Systems
            .idProduct          = __constant_cpu_to_le16(0x1272U), // SmartSCM P2Gate
            .bcdDevice          = __constant_cpu_to_le16(0x0001U), // 0.01
            .iManufacturer      = STRING_ID_MANUFACTURER,
            .iProduct           = STRING_ID_PRODUCT,
            .iSerialNumber      = STRING_ID_SERIAL,
            .bNumConfigurations = 1,
        },
        .config_descriptors = {
            .config = {
                .bLength             = USB_DT_CONFIG_SIZE,
                .bDescriptorType     = USB_DT_CONFIG,
                .wTotalLength        = __cpu_to_le16(USB_DT_CONFIG_SIZE + USB_DT_INTERFACE_SIZE + 8 * USB_DT_ENDPOINT_SIZE),
                .bNumInterfaces      = 1,
                .bConfigurationValue = 1,
                .iConfiguration      = 4,
                .bmAttributes        = 0xe0,
                .bMaxPower           = 0x5a,
            },
            .interface = {
                .bLength             = USB_DT_INTERFACE_SIZE,
                .bDescriptorType     = USB_DT_INTERFACE,
                .bInterfaceNumber    = 0,
                .bAlternateSetting   = 0,
                .bNumEndpoints       = 8,
                .bInterfaceClass     = 0xff,
                .bInterfaceSubClass  = 0xff,
                .bInterfaceProtocol  = 0xff,
                .iInterface          = 5,
            },
            .endpoints = {
            {
                .bLength             = USB_DT_ENDPOINT_SIZE,
                .bDescriptorType     = USB_DT_ENDPOINT,
                .bEndpointAddress    = USB_DIR_OUT | 1,
                .bmAttributes        = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize      = MAX_PACKET_SIZE_BULK,
                .bInterval           = 0,
            },
            {
                .bLength             = USB_DT_ENDPOINT_SIZE,
                .bDescriptorType     = USB_DT_ENDPOINT,
                .bEndpointAddress    = USB_DIR_IN | 1,
                .bmAttributes        = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize      = MAX_PACKET_SIZE_BULK,
                .bInterval           = 0,
            },
            {
                .bLength             = USB_DT_ENDPOINT_SIZE,
                .bDescriptorType     = USB_DT_ENDPOINT,
                .bEndpointAddress    = USB_DIR_OUT | 2,
                .bmAttributes        = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize      = MAX_PACKET_SIZE_BULK,
                .bInterval           = 0,
            },
            {
                .bLength             = USB_DT_ENDPOINT_SIZE,
                .bDescriptorType     = USB_DT_ENDPOINT,
                .bEndpointAddress    = USB_DIR_IN | 2,
                .bmAttributes        = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize      = MAX_PACKET_SIZE_BULK,
                .bInterval           = 0,
            },
            {
                .bLength             = USB_DT_ENDPOINT_SIZE,
                .bDescriptorType     = USB_DT_ENDPOINT,
                .bEndpointAddress    = USB_DIR_OUT | 3,
                .bmAttributes        = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize      = MAX_PACKET_SIZE_BULK,
                .bInterval           = 0,
            },
            {
                .bLength             = USB_DT_ENDPOINT_SIZE,
                .bDescriptorType     = USB_DT_ENDPOINT,
                .bEndpointAddress    = USB_DIR_IN | 3,
                .bmAttributes        = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize      = MAX_PACKET_SIZE_BULK,
                .bInterval           = 0,
            },
            {
                .bLength             = USB_DT_ENDPOINT_SIZE,
                .bDescriptorType     = USB_DT_ENDPOINT,
                .bEndpointAddress    = USB_DIR_OUT | 4,
                .bmAttributes        = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize      = MAX_PACKET_SIZE_BULK,
                .bInterval           = 0,
            },
            {
                .bLength             = USB_DT_ENDPOINT_SIZE,
                .bDescriptorType     = USB_DT_ENDPOINT,
                .bEndpointAddress    = USB_DIR_IN | 4,
                .bmAttributes        = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize      = MAX_PACKET_SIZE_BULK,
                .bInterval           = 0,
            }
            }
        },
        .string_descriptors = {
            &str_lang,
            &p2gate_str_manufacturer,
            &p2gate_str_product,
            &p2gate_str_serial,
        },
        .handle_set_configuration = p2gate_set_configuration,
        .handle_vendor_request = p2gate_vendor_request,
    },
    {
        .model_name = "ms56kps2",
        .device_descriptor = {
            .bLength            = USB_DT_DEVICE_SIZE,
            .bDescriptorType    = USB_DT_DEVICE,
            .bcdUSB             = __constant_cpu_to_le16(0x0110U),
            .bDeviceClass       = 0,
            .bDeviceSubClass    = 0,
            .bDeviceProtocol    = 0,
            .bMaxPacketSize0    = 0x40,
            .idVendor           = __constant_cpu_to_le16(0x05dbU),
            .idProduct          = __constant_cpu_to_le16(0x0006U),
            .bcdDevice          = __constant_cpu_to_le16(0x0100U),
            .iManufacturer      = STRING_ID_MANUFACTURER,
            .iProduct           = STRING_ID_PRODUCT,
            .iSerialNumber      = STRING_ID_SERIAL,
            .bNumConfigurations = 1,
        },
        .config_descriptors = {
            .config = {
                .bLength             = USB_DT_CONFIG_SIZE,
                .bDescriptorType     = USB_DT_CONFIG,
                .wTotalLength        = __cpu_to_le16(USB_DT_CONFIG_SIZE + USB_DT_INTERFACE_SIZE + 3 * USB_DT_ENDPOINT_SIZE),
                .bNumInterfaces      = 1,
                .bConfigurationValue = 1,
                .iConfiguration      = 0,
                .bmAttributes        = USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER | USB_CONFIG_ATT_WAKEUP,
                .bMaxPower           = 0x32,
            },
            .interface = {
                .bLength             = USB_DT_INTERFACE_SIZE,
                .bDescriptorType     = USB_DT_INTERFACE,
                .bInterfaceNumber    = 0,
                .bAlternateSetting   = 0,
                .bNumEndpoints       = 3,
                .bInterfaceClass     = 0xff,
                .bInterfaceSubClass  = 0xff,
                .bInterfaceProtocol  = 0xff,
                .iInterface          = 0,
            },
            .endpoints = {
            {
                .bLength             = USB_DT_ENDPOINT_SIZE,
                .bDescriptorType     = USB_DT_ENDPOINT,
                .bEndpointAddress    = USB_DIR_OUT | 1,
                .bmAttributes        = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize      = MAX_PACKET_SIZE_BULK,
                .bInterval           = 0,
            },
            {
                .bLength             = USB_DT_ENDPOINT_SIZE,
                .bDescriptorType     = USB_DT_ENDPOINT,
                .bEndpointAddress    = USB_DIR_IN | 2,
                .bmAttributes        = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize      = MAX_PACKET_SIZE_BULK,
                .bInterval           = 0,
            },
            {
                .bLength             = USB_DT_ENDPOINT_SIZE,
                .bDescriptorType     = USB_DT_ENDPOINT,
                .bEndpointAddress    = USB_DIR_IN | 3,
                .bmAttributes        = USB_ENDPOINT_XFER_INT,
                .wMaxPacketSize      = MAX_PACKET_SIZE_BULK,
                .bInterval           = 8,
            }
            }
        },
        .string_descriptors = {
            &str_lang,
            &ms56kps2_str_manufacturer,
            &ms56kps2_str_product,
            &ms56kps2_str_serial,
        },
        .handle_set_configuration = ms56kps2_set_configuration,
        .handle_vendor_request = ms56kps2_vendor_request,
    }
};

const struct modem_config *find_modem_config(const char *name)
{
    for (const auto& cfg : modem_configs) {
        if (strcmp(cfg.model_name, name) == 0) {
            return &cfg;
        }
    }
    return nullptr;
}

static const struct modem_config *current_config = &modem_configs[0];

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
    usb_tx_buffer.enqueue(ring.c_str(), ring.length());
    usb_tx_buffer.notify_one();

    printf("Client connected.\n");
}

void recv_callback(const char *buffer, size_t length)
{
    if (connected.load()) {
        const auto sent_length = usb_tx_buffer.enqueue(buffer, length);
        if (debug_level >= 2) {
            const auto buffer_size = usb_tx_buffer.get_buffer_size();
            const auto data_count = usb_tx_buffer.get_count();
            printf("usb_tx_buffer: used %ld bytes / %ld bytes (%.f%% used).\n", (long) data_count, (long) buffer_size, (float) data_count / buffer_size);
        }
        if (sent_length < length) {
            printf("Transmit buffer is full! (overflow %ld bytes.)\n", length - sent_length);
        }
        usb_tx_buffer.notify_one();
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
        usb_tx_buffer.wait(timeout_at);

        pkt.data[0] = 0x31;
        pkt.data[1] = 0x60;
        int payload_length = usb_tx_buffer.dequeue(&pkt.data[2], sizeof(pkt.data) - 2);

        pkt.header.ep = ep_num;
        pkt.header.flags = 0;
        pkt.header.length = 2 + payload_length;

        if (connected.load()) {pkt.data[0] |= 0x80;}

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
        while (!connected.load()) {
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
                usb_tx_buffer.enqueue(s.c_str(), s.length());
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
                    if (pty->connect()) {
                        reply = "CONNECT 57600 V42\r\n";
                        enter_online = true;
                        ISP::setupISP(pty->get_slave_name());
                    } else {
                        reply = "BUSY\r\n";
                    }
                } else {
                    if (sock == nullptr) {
                        reply = "BUSY\r\n";
                    } else {
                        struct sockaddr_in addr;
                        if (parse_address(line.substr(4), &addr)) {
                            sock->set_addr(&addr);
                        }
                        if (sock->connect()) {
                            reply = "CONNECT 57600 V42\r\n";
                            enter_online = true;
                        } else {
                            reply = "BUSY\r\n";
                        }
                    }
                }
            }

            usb_tx_buffer.enqueue(reply.c_str(), reply.length());
            usb_tx_buffer.notify_one();

            if (enter_online) {
                printf("Enter on-line mode.\n");
                connected.store(true);
            }
        }

        // On-line mode loop
        while (connected.load() && buffer.length() > 0) {
            if (pty->is_connected()) {
                pty->send(buffer.c_str(), buffer.length());
            } else if (sock != nullptr) {
                sock->send(buffer.c_str(), buffer.length());
            }
            buffer.clear();
        }
    }

    return NULL;
}

static bool default_set_configuration(usb_raw_gadget *usb, struct usb_packet_control *pkt, const struct modem_config *cfg)
{
    if (thread_bulk_in == nullptr) {
        const int ep_num_bulk_in = usb->ep_enable(
            reinterpret_cast<struct usb_endpoint_descriptor *>(
                const_cast<struct _usb_endpoint_descriptor *>(&cfg->config_descriptors.endpoints[1])));
        thread_bulk_in = new std::thread(usb_bulk_in_thread, usb, ep_num_bulk_in);
    }
    if (thread_bulk_out == nullptr) {
        const int ep_num_bulk_out = usb->ep_enable(
            reinterpret_cast<struct usb_endpoint_descriptor *>(
                const_cast<struct _usb_endpoint_descriptor *>(&cfg->config_descriptors.endpoints[0])));
        thread_bulk_out = new std::thread(usb_bulk_out_thread, usb, ep_num_bulk_out);
    }
    usb->vbus_draw(cfg->config_descriptors.config.bMaxPower);
    usb->configure();
    printf("USB configured.\n");
    pkt->header.length = 0;
    return true;
}

static bool me56ps2_set_configuration(usb_raw_gadget *usb, struct usb_packet_control *pkt, const struct modem_config *cfg)
{
    return default_set_configuration(usb, pkt, cfg);
}

static bool ms56kps2_set_configuration(usb_raw_gadget *usb, struct usb_packet_control *pkt, const struct modem_config *cfg)
{
    return default_set_configuration(usb, pkt, cfg);
}

static bool p2gate_set_configuration(usb_raw_gadget *usb, struct usb_packet_control *pkt, const struct modem_config *cfg)
{
    return default_set_configuration(usb, pkt, cfg);
}

static bool dtr_vendor_request(usb_raw_gadget *usb __attribute__((unused)), usb_raw_control_event *e, struct usb_packet_control *pkt)
{
    if (e->is_event(USB_TYPE_VENDOR, 0x01)) {
        pkt->header.length = 0;

        if ((e->ctrl.wValue & 0x0101) == 0x0100) {
            // set DTR to LOW for on-hook
            if (debug_level >= 2) {printf("on-hook\n");};
            // disconnect
            connected.store(false);
            if (sock != nullptr && sock->is_connected()) {
                sock->disconnect();
                printf("disconnected.\n");
            }
            if (pty != nullptr && pty->is_connected()) {
                pty->disconnect();
                printf("disconnected.\n");
            }
        } else if ((e->ctrl.wValue & 0x0101) == 0x0101) {
            // set DTR to HIGH for off-hook
            if (debug_level >= 2) {printf("off-hook\n");};
        }

        return true;
    }
    if (e->is_event(USB_TYPE_VENDOR, 0x05)) {
        pkt->data[0] = 0x31;
        pkt->header.length = 1;
        return true;
    }
    if (e->is_event(USB_TYPE_VENDOR)) {
        pkt->header.length = 0;
        return true;
    }
    return false;
}

static bool me56ps2_vendor_request(usb_raw_gadget *usb, usb_raw_control_event *e, struct usb_packet_control *pkt)
{
    return dtr_vendor_request(usb, e, pkt);
}

static bool ms56kps2_vendor_request(usb_raw_gadget *usb, usb_raw_control_event *e, struct usb_packet_control *pkt)
{
    return dtr_vendor_request(usb, e, pkt);
}

static bool p2gate_vendor_request(usb_raw_gadget *usb __attribute__((unused)), usb_raw_control_event *e, struct usb_packet_control *pkt)
{
    if (e->is_event(USB_TYPE_VENDOR)) {
        pkt->header.length = 0;
        return true;
    }
    return false;
}

bool process_control_packet(usb_raw_gadget *usb, usb_raw_control_event *e, struct usb_packet_control *pkt)
{
    if (e->is_event(USB_TYPE_STANDARD, USB_REQ_GET_DESCRIPTOR)) {
        const auto descriptor_type = e->get_descriptor_type();
        if (descriptor_type == USB_DT_DEVICE) {
            memcpy(pkt->data, &current_config->device_descriptor, sizeof(current_config->device_descriptor));
            pkt->header.length = sizeof(current_config->device_descriptor);
            return true;
        }
        if (descriptor_type == USB_DT_CONFIG) {
            const auto total_length = __le16_to_cpu(current_config->config_descriptors.config.wTotalLength);
            memcpy(pkt->data, &current_config->config_descriptors, total_length);
            pkt->header.length = total_length;
            return true;
        }
        if (descriptor_type == USB_DT_STRING) {
            const auto id = e->ctrl.wValue & 0x00ff;
            if (id >= STRING_DESCRIPTORS_NUM) {return false;}
            const auto len = reinterpret_cast<const struct _usb_string_descriptor<1> *>(current_config->string_descriptors[id])->bLength;
            memcpy(pkt->data, current_config->string_descriptors[id], len);
            pkt->header.length = len;
            return true;
        }
    }
    if (e->is_event(USB_TYPE_STANDARD, USB_REQ_SET_CONFIGURATION)) {
        return current_config->handle_set_configuration(usb, pkt, current_config);
    }
    if (e->is_event(USB_TYPE_STANDARD, USB_REQ_SET_INTERFACE)) {
        pkt->header.length = 0;
        return true;
    }
    return current_config->handle_vendor_request(usb, e, pkt);
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
    if (debug_level >= 1) {e.print_debug_log();}

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
    printf("  -m    modem model (default: me56ps2)\n");
    printf("        me56ps2   Omron ME56PS2\n");
    printf("        ms56kps2  Suntac OnlineStation MS56KPS2\n");
    printf("        p2gate    Conexant SmartSCM P2Gate\n");
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
    const char *driver = USB_RAW_GADGET_DRIVER_DEFAULT;
    const char *device = USB_RAW_GADGET_DEVICE_DEFAULT;
    const char *ip_addr = nullptr;
    int port = -1;
    bool is_server = false;

    int opt;
    while((opt = getopt(argc, argv, "m:svh")) != -1) {
        switch(opt) {
            case 'm': {
                const struct modem_config *cfg = find_modem_config(optarg);
                if (cfg == nullptr) {
                    fprintf(stderr, "Unknown modem model: %s\n", optarg);
                    show_usage(argv[0], false);
                    exit(1);
                }
                current_config = cfg;
                break;
            }
            case 's':
                is_server = true;
                break;
            case 'v':
                debug_level++;
                break;
            case 'h':
                show_usage(argv[0], true);
                exit(0);
            default:
                show_usage(argv[0], false);
                exit(1);
        }
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
    usb->set_debug_level(debug_level);
    usb->init(USB_SPEED_HIGH, driver, device);
    usb->run();

    pty = new pty_dev();
    pty->set_debug_level(debug_level);
    pty->set_recv_callback(recv_callback);

    if (ip_addr != nullptr && port != -1) {
        sock = new tcp_sock(is_server, ip_addr, port);
        sock->set_debug_level(debug_level);
        sock->set_ring_callback(ring_callback);
        sock->set_recv_callback(recv_callback);
    }

    while(event_usb_control_loop(usb));

    delete usb;

    return 0;
}
