#pragma once

#include "modem.h"

class OnlineStationModem : public Modem {
public:
    const char *model_name() const override;
    const struct usb_device_descriptor &device_descriptor() const override;
    const struct usb_config_descriptors &config_descriptors() const override;
    const void * const *string_descriptors() const override;
};
