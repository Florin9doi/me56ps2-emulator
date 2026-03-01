#pragma once

#include <thread>
#include "me56ps2.h"
#include "usb_raw_gadget.h"
#include "usb_raw_control_event.h"

// Forward declarations for bulk-thread functions defined in me56ps2.cpp
void *usb_bulk_in_thread(usb_raw_gadget *usb, int ep_num);
void *usb_bulk_out_thread(usb_raw_gadget *usb, int ep_num);

class Modem {
public:
    virtual ~Modem() {}

    virtual const char *model_name() const = 0;
    virtual const struct usb_device_descriptor &device_descriptor() const = 0;
    virtual const struct usb_config_descriptors &config_descriptors() const = 0;
    virtual const void * const *string_descriptors() const = 0;

    virtual bool handle_set_configuration(usb_raw_gadget *usb, struct usb_packet_control *pkt);
    virtual bool handle_vendor_request(usb_raw_gadget *usb, usb_raw_control_event *e, struct usb_packet_control *pkt);

    static Modem *getInstance(const char *name);

protected:
    static std::thread *thread_bulk_in;
    static std::thread *thread_bulk_out;
};
