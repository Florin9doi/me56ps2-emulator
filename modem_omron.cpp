#include <cstdio>

#include "modem_omron.h"
#include "tcp_sock.h"
#include "pty_dev.h"
#include "app_context.h"

static const struct _usb_string_descriptor<1> str_lang = {
    .bLength = sizeof(str_lang),
    .bDescriptorType = USB_DT_STRING,
    .wData = {__constant_cpu_to_le16(0x0409)},
};
static const struct _usb_string_descriptor<5> str_manufacturer = {
    .bLength = sizeof(str_manufacturer),
    .bDescriptorType = USB_DT_STRING,
    .wData = {u'O', u'M', u'R', u'O', u'N'},
};
static const struct _usb_string_descriptor<7> str_product = {
    .bLength = sizeof(str_product),
    .bDescriptorType = USB_DT_STRING,
    .wData = {u'M', u'E', u'5', u'6', u'P', u'S', u'2'},
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
    .idVendor           = __constant_cpu_to_le16(0x0590U), // Omron Corp.
    .idProduct          = __constant_cpu_to_le16(0x001aU), // ME56PS2
    .bcdDevice          = __constant_cpu_to_le16(0x0101U),
    .iManufacturer      = STRING_ID_MANUFACTURER,
    .iProduct           = STRING_ID_PRODUCT,
    .iSerialNumber      = STRING_ID_SERIAL,
    .bNumConfigurations = 1,
};

static const struct usb_config_descriptors cfg_descs = {
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
};

const char *OmronModem::model_name() const { return "Omron"; }
const struct usb_device_descriptor &OmronModem::device_descriptor() const { return dev_desc; }
const struct usb_config_descriptors &OmronModem::config_descriptors() const { return cfg_descs; }
const void * const *OmronModem::string_descriptors() const { return str_descs; }

bool OmronModem::handle_vendor_request(usb_raw_gadget *usb, usb_raw_control_event *e, struct usb_packet_control *pkt) {
    (void)usb;
    if (e->is_event(USB_TYPE_VENDOR, 0x01)) {
        pkt->header.length = 0;
        if ((e->ctrl.wValue & 0x0101) == 0x0100) {
            // set DTR to LOW for on-hook
            if (ctx.debug_level >= 2) {printf("on-hook\n");}
            // disconnect
            ctx.connected.store(false);
            if (ctx.sock != nullptr && ctx.sock->is_connected()) {
                ctx.sock->disconnect();
                printf("disconnected.\n");
            }
            if (ctx.pty != nullptr && ctx.pty->is_connected()) {
                ctx.pty->disconnect();
                printf("disconnected.\n");
            }
        } else if ((e->ctrl.wValue & 0x0101) == 0x0101) {
            // set DTR to HIGH for off-hook
            if (ctx.debug_level >= 2) {printf("off-hook\n");}
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
