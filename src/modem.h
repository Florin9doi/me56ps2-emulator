#pragma once

#include <string>
#include <thread>
#include <arpa/inet.h>
#include "main_app.h"
#include "usb_raw_gadget.h"
#include "usb_raw_control_event.h"

class Modem {
public:
    virtual ~Modem() {}

    virtual const struct usb_device_descriptor &device_descriptor() const = 0;
    virtual const struct usb_config_descriptors &config_descriptors(const uint8_t id) const = 0;
    virtual const void * const *string_descriptors() const = 0;

    virtual bool handle_set_configuration(usb_raw_control_event *e, struct usb_packet_control *pkt);
    virtual bool handle_control_request(usb_raw_control_event *e, struct usb_packet_control *pkt);

    void process_at(std::string &line);
    virtual bool process_at_ext(std::string &line);
    void handle_disconnect();

    static bool parse_address(const std::string &addr, struct sockaddr_in *parsed_addr);
    static Modem *getInstance(const char *name);

protected:
    bool echo = false;
};
