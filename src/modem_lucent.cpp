#include "modem_lucent.h"
#include "tcp_sock.h"
#include "pty_dev.h"
#include "app_context.h"

static const struct _usb_string_descriptor<1> str_lang = {
    .bLength = sizeof(str_lang),
    .bDescriptorType = USB_DT_STRING,
    .wData = {__constant_cpu_to_le16(0x0409)},
};
static const struct _usb_string_descriptor<24> str_manufacturer = {
    .bLength = sizeof(str_manufacturer),
    .bDescriptorType = USB_DT_STRING,
    .wData = {u'M', u'u', u'l', u't', u'i', u'-', u'T', u'e', u'c', u'h', u' ',
        u'S', u'y', u's', u't', u'e', u'm', u's', u',', u' ',
        u'I', u'n', u'c', u'.'},
};
static const struct _usb_string_descriptor<13> str_product = {
    .bLength = sizeof(str_product),
    .bDescriptorType = USB_DT_STRING,
    .wData = {u'M', u'u', u'l', u't', u'i', u'M', u'o', u'd', u'e', u'm', u'U', u'S', u'B'},
};
static const struct _usb_string_descriptor<23> str_configuration = {
    .bLength = sizeof(str_configuration),
    .bDescriptorType = USB_DT_STRING,
    .wData = {u'M', u'u', u'l', u't', u'i', u'-', u'T', u'e', u'c', u'h', u' ',
        u'N', u'T', u' ', u'M', u'o', u'd', u'e', u'm', u' ',
        u'U', u'S', u'B'},
};

static const void * const str_descs[STRING_DESCRIPTORS_NUM] = {
    &str_lang,
    &str_manufacturer,
    &str_product,
    &str_configuration,
};

static const struct usb_device_descriptor dev_desc = {
    .bLength            = USB_DT_DEVICE_SIZE,
    .bDescriptorType    = USB_DT_DEVICE,
    .bcdUSB             = __constant_cpu_to_le16(0x0100U),
    .bDeviceClass       = 0x02,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = 0x40,
    .idVendor           = __constant_cpu_to_le16(0x06e0U),
    .idProduct          = __constant_cpu_to_le16(0xf103U),
    .bcdDevice          = __constant_cpu_to_le16(0x0100U),
    .iManufacturer      = STRING_ID_MANUFACTURER,
    .iProduct           = STRING_ID_PRODUCT,
    .iSerialNumber      = 0,
    .bNumConfigurations = 2,
};

struct config1_descriptors {
    struct usb_config_descriptor config;
    struct usb_interface_descriptor interfaces[2];
    struct _usb_endpoint_descriptor endpoints[2];
} static const cfg1_descs = {
    .config = {
        .bLength             = USB_DT_CONFIG_SIZE,
        .bDescriptorType     = USB_DT_CONFIG,
        .wTotalLength        = __cpu_to_le16(USB_DT_CONFIG_SIZE + 2 * USB_DT_INTERFACE_SIZE + 2 * USB_DT_ENDPOINT_SIZE),
        .bNumInterfaces      = 0x02,
        .bConfigurationValue = 0x01,
        .iConfiguration      = 0x00,
        .bmAttributes        = USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_WAKEUP,
        .bMaxPower           = 0xc8,
    },
    .interfaces = {
    {
        .bLength             = USB_DT_INTERFACE_SIZE,
        .bDescriptorType     = USB_DT_INTERFACE,
        .bInterfaceNumber    = 0x00,
        .bAlternateSetting   = 0x00,
        .bNumEndpoints       = 0x00,
        .bInterfaceClass     = 0xff,
        .bInterfaceSubClass  = 0xff,
        .bInterfaceProtocol  = 0xff,
        .iInterface          = 0x00,
    },
    {
        .bLength             = USB_DT_INTERFACE_SIZE,
        .bDescriptorType     = USB_DT_INTERFACE,
        .bInterfaceNumber    = 0x01,
        .bAlternateSetting   = 0x00,
        .bNumEndpoints       = 0x02,
        .bInterfaceClass     = 0xff,
        .bInterfaceSubClass  = 0xff,
        .bInterfaceProtocol  = 0xff,
        .iInterface          = 0x00,
    }
    },
    .endpoints = {
    {
        .bLength             = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType     = USB_DT_ENDPOINT,
        .bEndpointAddress    = USB_DIR_OUT | 2,
        .bmAttributes        = USB_ENDPOINT_XFER_BULK,
        .wMaxPacketSize      = MAX_PACKET_SIZE_BULK,
        .bInterval           = 0x00,
    },
    {
        .bLength             = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType     = USB_DT_ENDPOINT,
        .bEndpointAddress    = USB_DIR_IN | 4,
        .bmAttributes        = USB_ENDPOINT_XFER_INT,
        .wMaxPacketSize      = __constant_cpu_to_le16(0x003f),
        .bInterval           = 0x02,
    }
    }
};

struct config2_descriptors {
    struct usb_config_descriptor config;
    struct usb_interface_descriptor interface1;
    uint8_t cdc_desc[12];
    struct _usb_endpoint_descriptor endpoint1;
    struct usb_interface_descriptor interface2;
    struct _usb_endpoint_descriptor endpoints2[2];
} static const cfg2_descs = {
    .config = {
        .bLength             = USB_DT_CONFIG_SIZE,
        .bDescriptorType     = USB_DT_CONFIG,
        .wTotalLength        = __cpu_to_le16(0x3c),
        .bNumInterfaces      = 0x02,
        .bConfigurationValue = 0x02,
        .iConfiguration      = 0x00,
        .bmAttributes        = USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_WAKEUP,
        .bMaxPower           = 0xc8,
    },
    .interface1 = {
        .bLength             = USB_DT_INTERFACE_SIZE,
        .bDescriptorType     = USB_DT_INTERFACE,
        .bInterfaceNumber    = 0,
        .bAlternateSetting   = 0,
        .bNumEndpoints       = 1,
        .bInterfaceClass     = 0x02,
        .bInterfaceSubClass  = 0x02,
        .bInterfaceProtocol  = 0x01,
        .iInterface          = 0,
    },
    .cdc_desc = {
        // header
        0x03, // bFunctionLength
        0x24, // bDescriptorType = CS_INTERFACE
        0x00, // bDescriptorSubtype = Header Functional Descriptor

        // call mgm
        0x05, // bFunctionLength
        0x24, // bDescriptorType = CS_INTERFACE
        0x01, // bDescriptorSubtype = Call Management Functional Descriptor
        0x03, // bmCapabilities = Call mgm over Data Class if
        0x01, // bDataInterface

        // acm
        0x04, // bFunctionLength
        0x24, // bDescriptorType = CS_INTERFACE
        0x02, // bDescriptorSubtype = Abstract Control Management Functional Descriptor.
        0x07  // bmCapabilities = Break + Line state + Comm feature
    },
    .endpoint1 = {
        .bLength             = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType     = USB_DT_ENDPOINT,
        .bEndpointAddress    = USB_DIR_IN | 4,
        .bmAttributes        = USB_ENDPOINT_XFER_INT,
        .wMaxPacketSize      = 0x32,
        .bInterval           = 128,
    },
    .interface2 = {
        .bLength             = USB_DT_INTERFACE_SIZE,
        .bDescriptorType     = USB_DT_INTERFACE,
        .bInterfaceNumber    = 1,
        .bAlternateSetting   = 0,
        .bNumEndpoints       = 2,
        .bInterfaceClass     = 0x0a,
        .bInterfaceSubClass  = 0x02,
        .bInterfaceProtocol  = 0x01,
        .iInterface          = 0,
    },
    .endpoints2 = {
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
        .bEndpointAddress    = USB_DIR_IN | 6,
        .bmAttributes        = USB_ENDPOINT_XFER_BULK,
        .wMaxPacketSize      = MAX_PACKET_SIZE_BULK,
        .bInterval           = 0,
    }
    }
};

const struct usb_device_descriptor &LucentModem::device_descriptor() const { return dev_desc; }
const struct usb_config_descriptors &LucentModem::config_descriptors(uint8_t id) const {
    if (id == 1) {
        return (const usb_config_descriptors&)cfg2_descs;
    }
    return (const usb_config_descriptors&)cfg1_descs;
}
const void * const *LucentModem::string_descriptors() const { return str_descs; }

bool LucentModem::handle_set_configuration(usb_raw_control_event *e, struct usb_packet_control *pkt) {
    const auto id = e->ctrl.wValue & 0x00ff;
    if (id == 2) {
        if (thread_intr_in == nullptr) {
            const int ep_num = ctx.usb->ep_enable(reinterpret_cast<struct usb_endpoint_descriptor *>(
                    const_cast<struct _usb_endpoint_descriptor *>(&cfg2_descs.endpoint1)));
            thread_intr_in = new std::thread(&LucentModem::intr_in_thread, this, ep_num);
        }
        if (thread_bulk_out == nullptr) {
            const int ep_num = ctx.usb->ep_enable(reinterpret_cast<struct usb_endpoint_descriptor *>(
                    const_cast<struct _usb_endpoint_descriptor *>(&cfg2_descs.endpoints2[0])));
            thread_bulk_out = new std::thread(&LucentModem::bulk_out_thread, this, ep_num);
        }
        if (thread_bulk_in == nullptr) {
            const int ep_num = ctx.usb->ep_enable(reinterpret_cast<struct usb_endpoint_descriptor *>(
                    const_cast<struct _usb_endpoint_descriptor *>(&cfg2_descs.endpoints2[1])));
            thread_bulk_in = new std::thread(&LucentModem::bulk_in_thread, this, ep_num);
        }
    }
    return Modem::handle_set_configuration(e, pkt);
}

enum {
    SET_LINE_CODING        = 0x20,
    SET_CONTROL_LINE_STATE = 0x22
};

bool LucentModem::handle_control_request(usb_raw_control_event *e, struct usb_packet_control *pkt) {
    if (e->is_event(USB_TYPE_CLASS, SET_LINE_CODING)) {
        const uint32_t baud = pkt->data[0]
                            | pkt->data[1] <<  8
                            | pkt->data[2] << 16
                            | pkt->data[3] << 24;
        const uint8_t stop   = pkt->data[4];
        const uint8_t parity = pkt->data[5];
        const uint8_t bits   = pkt->data[6];
        printf("SET_LINE_CODING : baud=%d, bits=%d, parity=%d, stop=%d\n", baud, bits, parity, stop);
        return true;
    }
    if (e->is_event(USB_TYPE_CLASS, SET_CONTROL_LINE_STATE)) {
        printf("SET_CONTROL_LINE_STATE : DTR=%d, RTS=%d\n",
            !!(e->ctrl.wValue & 0x01),
            !!(e->ctrl.wValue & 0x02));
        if (!(e->ctrl.wValue & 0x01)) {
            if (ctx.debug_level >= 2) {printf("on-hook\n");}
            handle_disconnect();
        }
        return true;
    }
    if (e->is_event(USB_TYPE_CLASS)) {
        printf("handle_control_request: type=%x, req=%x\n", e->ctrl.bRequestType, e->ctrl.bRequest);
        return true;
    }
    return false;
}

bool LucentModem::process_at_ext(std::string &line) {
    std::string reply;
    if (line == "AT#CLS=?" || line == "AT+GCI?" || line == "AT+GCI=?") {
        reply += "\r\nERROR\r\n";
        usb_tx_buffer.enqueue(reply.c_str(), reply.length());
        usb_tx_buffer.notify_one();
        return true;
    }
    if (line == "AT+GMM") {
        reply = "\r\nH.324 video-ready rev. 1.0\r\n";
    } else if (line == "AT+FCLASS=?") {
        reply = "\r\n0,1,2";
    } else if (line == "ATI" || line == "ATI0" || line == "ATI3") {
        reply = "\r\nLT V.90 1.0 MT5634MU USB Data/Fax Modem Version 8.18j\r\n";
    } else if (line == "ATI1") {
        reply = "\r\nD092\r\n";
    } else if (line == "ATI4") {
        reply = "\r\n17\r\n";
    } else if (line == "ATI5") {
        reply = "\r\nU052099f,0,34\r\n";
    } else if (line == "ATI7") {
        reply = "\r\nGlobal2 Build\r\n";
    } else if (line == "ATI9") {
        reply = "\r\n52\r\n";
    }
    if (!reply.empty()) {
        reply += "\r\nOK\r\n";
        ctx.usb_tx_buffer.enqueue(reply.c_str(), reply.length());
        ctx.usb_tx_buffer.notify_one();
        return true;
    }
    return false;
}

void *LucentModem::intr_in_thread(int ep_num) {
    struct usb_packet_control pkt;
    auto timeout_at = std::chrono::steady_clock::now();

    bool last_dcd = true;

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        while (timeout_at <= now) {
            timeout_at += std::chrono::milliseconds(40);
        }
        ctx.usb_tx_buffer.wait(timeout_at);

        bool dcd = ctx.connected.load();
        if (last_dcd == dcd)
            continue;
        last_dcd = dcd;

        pkt.data[0] = 0xa1; // bmRequestType
        pkt.data[1] = 0x20; // bNotification
        pkt.data[2] = 0x00; // wValue LSB
        pkt.data[3] = 0x00; // wValue MSB
        pkt.data[4] = 0x00; // wIndex LSB
        pkt.data[5] = 0x00; // wIndex MSB
        pkt.data[6] = 0x02; // wLength LSB
        pkt.data[7] = 0x00; // wLength MSB

        pkt.data[8] = 0x02; // DSR
        if (dcd)
            pkt.data[8] |= 0x01; // DCD
        pkt.data[9] = 0x00;

        pkt.header.ep = ep_num;
        pkt.header.flags = 0;
        pkt.header.length = 10;

        ctx.usb->ep_write(reinterpret_cast<struct usb_raw_ep_io *>(&pkt));
    }
    return nullptr;
}

void *LucentModem::bulk_out_thread(int ep_num) {
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
            while (!buffer.empty() && (buffer[0] == '\0')) {
                buffer.erase(0, 1);
            }
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

void *LucentModem::bulk_in_thread(int ep_num) {
    struct usb_packet_control pkt;
    auto timeout_at = std::chrono::steady_clock::now();

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        while (timeout_at <= now) {
            timeout_at += std::chrono::milliseconds(40);
        }
        ctx.usb_tx_buffer.wait(timeout_at);

        int payload_length = ctx.usb_tx_buffer.dequeue(&pkt.data[0], sizeof(pkt.data));
        if (!payload_length)
            continue;

        pkt.header.ep = ep_num;
        pkt.header.flags = 0;
        pkt.header.length = payload_length;

        ctx.usb->ep_write(reinterpret_cast<struct usb_raw_ep_io *>(&pkt));
    }
    return nullptr;
}
