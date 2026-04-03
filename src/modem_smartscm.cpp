#include "modem_smartscm.h"
#include "tcp_sock.h"
#include "pty_dev.h"
#include "app_context.h"

static const struct _usb_string_descriptor<1> str_lang = {
    .bLength = sizeof(str_lang),
    .bDescriptorType = USB_DT_STRING,
    .wData = {__constant_cpu_to_le16(0x0409)},
};
static const struct _usb_string_descriptor<22> str_manufacturer = {
    .bLength = sizeof(str_manufacturer),
    .bDescriptorType = USB_DT_STRING,
    .wData = {u'C', u'o', u'n', u'e', u'x', u'a', u'n', u't', u' ', u'S', u'y', u's', u't', u'e', u'm', u's', u',', u' ', u'I', u'n', u'c', u'.'},
};
static const struct _usb_string_descriptor<30> str_product = {
    .bLength = sizeof(str_product),
    .bDescriptorType = USB_DT_STRING,
    .wData = {u'V', u'.', u'9', u'0', u' ', u'M', u'o', u'd', u'e', u'm', u' ', u'w', u'i', u't', u'h', u' ', u'U', u'S', u'B', u' ', u'(', u'G', u'a', u'm', u'e', u' ', u'A', u'p', u'p', u')'},
};
static const struct _usb_string_descriptor<3> str_serial = {
    .bLength = sizeof(str_serial),
    .bDescriptorType = USB_DT_STRING,
    .wData = {u'N', u'/', u'A'},
};

static const void * const str_descs[STRING_DESCRIPTORS_NUM] = {
    &str_lang,
    &str_manufacturer,
    &str_product,
    &str_serial,
};

static const struct usb_device_descriptor dev_desc = {
    .bLength            = USB_DT_DEVICE_SIZE,
    .bDescriptorType    = USB_DT_DEVICE,
    .bcdUSB             = __constant_cpu_to_le16(0x0100U), // USB 1.0
    .bDeviceClass       = 0xff,
    .bDeviceSubClass    = 0xff,
    .bDeviceProtocol    = 0xff,
    .bMaxPacketSize0    = 0x40,
    .idVendor           = __constant_cpu_to_le16(0x0572U),
    .idProduct          = __constant_cpu_to_le16(0x1272U), // gaming modem
    .bcdDevice          = __constant_cpu_to_le16(0x0001U),
    .iManufacturer      = STRING_ID_MANUFACTURER,
    .iProduct           = STRING_ID_PRODUCT,
    .iSerialNumber      = STRING_ID_SERIAL,
    .bNumConfigurations = 1,
};

static const struct usb_config_descriptors cfg_descs = {
    .config = {
        .bLength             = USB_DT_CONFIG_SIZE,
        .bDescriptorType     = USB_DT_CONFIG,
        .wTotalLength        = __cpu_to_le16(USB_DT_CONFIG_SIZE + USB_DT_INTERFACE_SIZE + 8 * USB_DT_ENDPOINT_SIZE),
        .bNumInterfaces      = 1,
        .bConfigurationValue = 1,
        .iConfiguration      = 4,
        .bmAttributes        = USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER | USB_CONFIG_ATT_WAKEUP,
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
};

const struct usb_device_descriptor &SmartSCMModem::device_descriptor() const { return dev_desc; }
const struct usb_config_descriptors &SmartSCMModem::config_descriptors(const uint8_t id) const {
    (void)id;
    return cfg_descs;
}
const void * const *SmartSCMModem::string_descriptors() const { return str_descs; }

bool SmartSCMModem::handle_set_configuration(usb_raw_control_event *e, struct usb_packet_control *pkt) {
    if (thread_control_out == nullptr) {
        const int ep_num = ctx.usb->ep_enable(reinterpret_cast<struct usb_endpoint_descriptor *>(
                const_cast<struct _usb_endpoint_descriptor *>(&config_descriptors(0).endpoints[0])));
        thread_control_out = new std::thread(&SmartSCMModem::control_out_thread, this, ep_num);
    }
    if (thread_control_in == nullptr) {
        const int ep_num = ctx.usb->ep_enable(reinterpret_cast<struct usb_endpoint_descriptor *>(
                const_cast<struct _usb_endpoint_descriptor *>(&config_descriptors(0).endpoints[1])));
        thread_control_in = new std::thread(&SmartSCMModem::control_in_thread, this, ep_num);
    }
    if (thread_data_out == nullptr) {
        const int ep_num = ctx.usb->ep_enable(reinterpret_cast<struct usb_endpoint_descriptor *>(
                const_cast<struct _usb_endpoint_descriptor *>(&config_descriptors(0).endpoints[4])));
        thread_data_out = new std::thread(&SmartSCMModem::data_out_thread, this, ep_num);
    }
    if (thread_data_in == nullptr) {
        const int ep_num = ctx.usb->ep_enable(reinterpret_cast<struct usb_endpoint_descriptor *>(
                const_cast<struct _usb_endpoint_descriptor *>(&config_descriptors(0).endpoints[5])));
        thread_data_in = new std::thread(&SmartSCMModem::data_in_thread, this, ep_num);
    }
    if (thread_gpio_out == nullptr) {
        const int ep_num = ctx.usb->ep_enable(reinterpret_cast<struct usb_endpoint_descriptor *>(
                const_cast<struct _usb_endpoint_descriptor *>(&config_descriptors(0).endpoints[6])));
        thread_gpio_out = new std::thread(&SmartSCMModem::gpio_out_thread, this, ep_num);
    }
    if (thread_gpio_in == nullptr) {
        const int ep_num = ctx.usb->ep_enable(reinterpret_cast<struct usb_endpoint_descriptor *>(
                const_cast<struct _usb_endpoint_descriptor *>(&config_descriptors(0).endpoints[7])));
        thread_gpio_in = new std::thread(&SmartSCMModem::gpio_in_thread, this, ep_num);
    }
    return Modem::handle_set_configuration(e, pkt);
}

bool SmartSCMModem::process_at_ext(std::string &line) {
    std::string reply;
    if (line == "ATI" || line == "ATI0") {
        reply = "56000";
    } else if (line == "ATI1") {
        reply = "042";
    } else if (line == "ati3") {
        reply = "P2109-V90";
    } else if (line == "ATI4") {
        reply = "a007080284C6002F\r\n\r\n"
                "bC60000000\r\n\r\n"
                "r1005111151012004\r\n\r\n"
                "r3000111170000000";
    } else if (line == "ATI5") {
        reply = "B5";
    } else if (line == "ATI6") {
        reply = "RCV56DPF-PLL L8773A Rev 14.00/34.00";
    }
    if (!reply.empty()) {
        reply += "\r\n\r\nOK\r\n";
        ctx.usb_tx_buffer.enqueue(reply.c_str(), reply.length());
        ctx.usb_tx_buffer.notify_one();
        return true;
    }
    return false;
}

// ep1 out
void *SmartSCMModem::control_out_thread(int ep_num) {
    struct usb_packet_bulk pkt;
    while (true) {
        pkt.header.ep = ep_num;
        pkt.header.flags = 0;
        pkt.header.length = sizeof(pkt.data);

        int length = ctx.usb->ep_read(reinterpret_cast<struct usb_raw_ep_io *>(&pkt));
        for (int i = 0; i < length && pkt.data[i] == 0x40; i += 3) {
            uint8_t cmd = pkt.data[i + 1];
            uint8_t val = pkt.data[i + 2];
            printf("control_out_thread: cmd=0x%02x; val=0x%02x\n", cmd, val);
            if (cmd == 0x04 && !(val & 0x01)) {
                if (ctx.debug_level >= 2) {printf("on-hook\n");}
                printf("on-hook\n");
                handle_disconnect();
            }
        }
    }
    return nullptr;
}

// ep1 in
void *SmartSCMModem::control_in_thread(int ep_num) {
    struct usb_packet_control pkt;
    auto timeout_at = std::chrono::steady_clock::now();

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        while (timeout_at <= now) {
            timeout_at += std::chrono::milliseconds(5000);
        }
        std::this_thread::sleep_until(timeout_at);

        pkt.header.ep = ep_num;
        pkt.header.flags = 0;
        pkt.header.length = 0;

        ctx.usb->ep_write(reinterpret_cast<struct usb_raw_ep_io *>(&pkt));
    }
    return nullptr;
}

// ep3 out
void *SmartSCMModem::data_out_thread(int ep_num) {
    struct usb_packet_bulk pkt;
    std::string buffer;

    while (true) {
        pkt.header.ep = ep_num;
        pkt.header.flags = 0;
        pkt.header.length = sizeof(pkt.data);

        int length = ctx.usb->ep_read(reinterpret_cast<struct usb_raw_ep_io *>(&pkt));
        buffer.append(&pkt.data[0], length);

        // Off-line mode loop
        while (!ctx.connected.load()) {
            auto newline_pos = buffer.find('\x0d');
            if (newline_pos == std::string::npos) { break; }
            std::string line = buffer.substr(0, newline_pos);
            buffer.erase(0, newline_pos + 1);
            if (line.empty()) { continue; }
            process_at(line);
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
    return nullptr;
}

// ep3 in
void *SmartSCMModem::data_in_thread(int ep_num) {
    struct usb_packet_control pkt;
    auto timeout_at = std::chrono::steady_clock::now();

    bool last_dcd = true;

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        while (timeout_at <= now) {
            timeout_at += std::chrono::milliseconds(40);
        }
        ctx.usb_tx_buffer.wait(timeout_at);

        char data[15] = {0};
        int payload_length = ctx.usb_tx_buffer.dequeue(data, sizeof(data));

        bool dcd = ctx.connected.load();
        if (!payload_length && last_dcd == dcd)
            continue;
        last_dcd = dcd;

        pkt.data[0] = 0x30; // MSR
        if (dcd)
            pkt.data[0] |= 0x80; // DCD

        for (int i = 0; i < payload_length; ++i) {
             pkt.data[1 + 2*i]     = 0x61; // LSR
             pkt.data[1 + 2*i + 1] = data[i];
        }
        bool is_empty = ctx.usb_tx_buffer.is_empty();
        if (is_empty)
            pkt.data[1 + 2*payload_length] = 0x60; // LSR

        pkt.header.ep = ep_num;
        pkt.header.flags = 0;
        pkt.header.length = 1 + 2 * payload_length + (is_empty ? 1 : 0);

        ctx.usb->ep_write(reinterpret_cast<struct usb_raw_ep_io *>(&pkt));
    }
    return nullptr;
}

// ep4 out
void *SmartSCMModem::gpio_out_thread(int ep_num) {
    struct usb_packet_bulk pkt;
    while (true) {
        pkt.header.ep = ep_num;
        pkt.header.flags = 0;
        pkt.header.length = sizeof(pkt.data);

        int length = ctx.usb->ep_read(reinterpret_cast<struct usb_raw_ep_io *>(&pkt));
        (void) length;
    }
    return nullptr;
}

// ep4 in
void *SmartSCMModem::gpio_in_thread(int ep_num) {
    struct usb_packet_control pkt;
    auto timeout_at = std::chrono::steady_clock::now();

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        while (timeout_at <= now) {
            timeout_at += std::chrono::milliseconds(5000);
        }
        std::this_thread::sleep_until(timeout_at);

        pkt.header.ep = ep_num;
        pkt.header.flags = 0;
        pkt.header.length = 0;

        ctx.usb->ep_write(reinterpret_cast<struct usb_raw_ep_io *>(&pkt));
    }
    return nullptr;
}
