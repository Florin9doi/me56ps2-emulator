#pragma once

#include "modem.h"

class OmronModem : public Modem {
public:
    const char *model_name() const override;
    const struct usb_device_descriptor &device_descriptor() const override;
    const struct usb_config_descriptors &config_descriptors() const override;
    const void * const *string_descriptors() const override;
    bool handle_vendor_request(usb_raw_gadget *usb, usb_raw_control_event *e, struct usb_packet_control *pkt) override;
};
