#include "modem_smartscm.h"

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
    .idVendor           = __constant_cpu_to_le16(0x0572U), // Conexant Systems
    .idProduct          = __constant_cpu_to_le16(0x1272U), // SmartSCM P2Gate
    .bcdDevice          = __constant_cpu_to_le16(0x0001U), // 0.01
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

const char *SmartSCMModem::model_name() const { return "SmartSCM"; }
const struct usb_device_descriptor &SmartSCMModem::device_descriptor() const { return dev_desc; }
const struct usb_config_descriptors &SmartSCMModem::config_descriptors() const { return cfg_descs; }
const void * const *SmartSCMModem::string_descriptors() const { return str_descs; }
