#pragma once

#include "modem.h"

class OnlineStationModem : public Modem {
public:
    const struct usb_device_descriptor &device_descriptor() const override;
    const struct usb_config_descriptors &config_descriptors(const uint8_t id) const override;
    const void * const *string_descriptors() const override;

    bool handle_set_configuration(usb_raw_control_event *e, struct usb_packet_control *pkt) override;
    bool handle_control_request(usb_raw_control_event *e, struct usb_packet_control *pkt) override;

protected:
    void *bulk_in_thread(int ep_num);
    void *bulk_out_thread(int ep_num);
    void *intr_in_thread(int ep_num);

    std::thread *thread_bulk_in;
    std::thread *thread_bulk_out;
    std::thread *thread_intr_in;
};
