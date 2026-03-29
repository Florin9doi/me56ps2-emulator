#pragma once

#include <cstdint>
#include <linux/usb/ch9.h>

constexpr auto TCP_DEFAULT_PORT = 10023;

constexpr auto MAX_PACKET_SIZE_BULK = 64U;

constexpr auto STRING_ID_MANUFACTURER = 1U;
constexpr auto STRING_ID_PRODUCT = 2U;
constexpr auto STRING_ID_SERIAL = 3U;
constexpr auto STRING_DESCRIPTORS_NUM = 4;

constexpr auto CONTROL_DATA_BUFFER_SIZE = 256U;

struct usb_packet_control {
    struct {
        uint16_t ep;
        uint16_t flags;
        uint32_t length;
    } header;
    char data[CONTROL_DATA_BUFFER_SIZE];
};

struct usb_packet_bulk {
    struct {
        uint16_t ep;
        uint16_t flags;
        uint32_t length;
    } header;
    char data[MAX_PACKET_SIZE_BULK];
};

struct _usb_endpoint_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} __attribute__ ((packed));

template<int N>
struct _usb_string_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    char16_t wData[N];
} __attribute__ ((packed));

struct usb_config_descriptors {
    struct usb_config_descriptor config;
    struct usb_interface_descriptor interface;
    struct _usb_endpoint_descriptor endpoints[8];
};
