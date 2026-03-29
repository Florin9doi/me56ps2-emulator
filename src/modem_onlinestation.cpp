#include "modem_onlinestation.h"
#include "tcp_sock.h"
#include "pty_dev.h"
#include "app_context.h"

static const struct _usb_string_descriptor<1> str_lang = {
    .bLength = sizeof(str_lang),
    .bDescriptorType = USB_DT_STRING,
    .wData = {__constant_cpu_to_le16(0x0409)},
};
static const struct _usb_string_descriptor<6> str_manufacturer = {
    .bLength = sizeof(str_manufacturer),
    .bDescriptorType = USB_DT_STRING,
    .wData = {u'S', u'U', u'N', u'T', u'A', u'C'},
};
static const struct _usb_string_descriptor<8> str_product = {
    .bLength = sizeof(str_product),
    .bDescriptorType = USB_DT_STRING,
    .wData = {u'M', u'S', u'5', u'6', u'K', u'P', u'S', u'2'},
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
};

static const struct usb_config_descriptors cfg_descs = {
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
        .bEndpointAddress    = USB_DIR_IN | 3,
        .bmAttributes        = USB_ENDPOINT_XFER_INT,
        .wMaxPacketSize      = MAX_PACKET_SIZE_BULK,
        .bInterval           = 8,
    }
    }
};

const struct usb_device_descriptor &OnlineStationModem::device_descriptor() const { return dev_desc; }
const struct usb_config_descriptors &OnlineStationModem::config_descriptors(const uint8_t id) const {
    (void)id;
    return cfg_descs;
}
const void * const *OnlineStationModem::string_descriptors() const { return str_descs; }

bool OnlineStationModem::handle_set_configuration(usb_raw_control_event *e, struct usb_packet_control *pkt) {
    if (thread_bulk_in == nullptr) {
        const int ep_num = ctx.usb->ep_enable(reinterpret_cast<struct usb_endpoint_descriptor *>(
                const_cast<struct _usb_endpoint_descriptor *>(&config_descriptors(0).endpoints[0])));
        thread_bulk_in = new std::thread(&OnlineStationModem::bulk_in_thread, this, ep_num);
    }
    if (thread_bulk_out == nullptr) {
        const int ep_num = ctx.usb->ep_enable(reinterpret_cast<struct usb_endpoint_descriptor *>(
                const_cast<struct _usb_endpoint_descriptor *>(&config_descriptors(0).endpoints[1])));
        thread_bulk_out = new std::thread(&OnlineStationModem::bulk_out_thread, this, ep_num);
    }
    if (thread_intr_in == nullptr) {
        const int ep_num = ctx.usb->ep_enable(reinterpret_cast<struct usb_endpoint_descriptor *>(
                const_cast<struct _usb_endpoint_descriptor *>(&config_descriptors(0).endpoints[2])));
        thread_intr_in = new std::thread(&OnlineStationModem::intr_in_thread, this, ep_num);
    }
    return Modem::handle_set_configuration(e, pkt);
}

bool OnlineStationModem::handle_control_request(usb_raw_control_event *e, struct usb_packet_control *pkt) {
    if (e->is_event(USB_TYPE_VENDOR, 0x10)) { // set baud rate
        constexpr uint32_t rates[] = {110, 300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
        printf("os : vendor : set baud rate = 0x%02x (%d)\n", e->ctrl.wValue, rates[e->ctrl.wValue]);
        return true;
    }
    if (e->is_event(USB_TYPE_VENDOR, 0x11)) { // set line state
        printf("os : vendor : set line state = 0x%02x (DTR=%d, RTS=%d, Init=%d, Break=%d)\n",
            e->ctrl.wValue,
            e->ctrl.wValue & 0x01,
            e->ctrl.wValue & 0x02,
            e->ctrl.wValue & 0x08,
            e->ctrl.wValue & 0x40
        );
        if (!(e->ctrl.wValue & 0x01)) {
            if (ctx.debug_level >= 2) {printf("on-hook\n");}
            handle_disconnect();
        }
        return true;
    }
    if (e->is_event(USB_TYPE_VENDOR, 0x12)) { // set data bits
        printf("os : vendor : set data bits = 0x%02x (parity=%d, markparity=%d, even=%d, parity=%d, stop=%d, bits=%d)\n",
            e->ctrl.wValue,
            e->ctrl.wValue & 0x80,
            e->ctrl.wValue & 0x20,
            e->ctrl.wValue & 0x10,
            e->ctrl.wValue & 0x08,
            e->ctrl.wValue & 0x04,
            5 + (e->ctrl.wValue & 0x03)
        );
        return true;
    }
    if (e->is_event(USB_TYPE_VENDOR, 0xd0)) { // reset
        printf("os : vendor : reset\n");
        pkt->data[0] = 0x01; // ok
        pkt->data[1] = 0x00;
        pkt->header.length = 2;
        return true;
    }
    if (e->is_event(USB_TYPE_VENDOR, 0xe0)) { // shutdown
        printf("os : vendor : shutdown\n");
        return true;
    }
    if (e->is_event(USB_TYPE_VENDOR)) {
        pkt->header.length = 0;
        return true;
    }
    return false;
}

void *OnlineStationModem::intr_in_thread(int ep_num) {
    struct usb_packet_control pkt;
    auto timeout_at = std::chrono::steady_clock::now();

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        while (timeout_at <= now) {
            timeout_at += std::chrono::milliseconds(40);
        }
        ctx.usb_tx_buffer.wait(timeout_at);

        pkt.data[0] = 0x04;
        if (!ctx.usb_tx_buffer.is_empty())
            pkt.data[0] |= 0x01;

        pkt.data[1] = 0x03; // CTS/DTR
        if (ctx.connected.load())
            pkt.data[1] |= 0x08; // DCD

        pkt.header.ep = ep_num;
        pkt.header.flags = 0;
        pkt.header.length = 2;

        ctx.usb->ep_write(reinterpret_cast<struct usb_raw_ep_io *>(&pkt));
    }
    return nullptr;
}

void *OnlineStationModem::bulk_in_thread(int ep_num) {
    struct usb_packet_control pkt;
    auto timeout_at = std::chrono::steady_clock::now();

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        while (timeout_at <= now) {
            timeout_at += std::chrono::milliseconds(40);
        }
        ctx.usb_tx_buffer.wait(timeout_at);

        int payload_length = ctx.usb_tx_buffer.dequeue(&pkt.data[0], sizeof(pkt.data));

        pkt.header.ep = ep_num;
        pkt.header.flags = 0;
        pkt.header.length = payload_length;

        ctx.usb->ep_write(reinterpret_cast<struct usb_raw_ep_io *>(&pkt));
    }
    return nullptr;
}

void *OnlineStationModem::bulk_out_thread(int ep_num) {
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
