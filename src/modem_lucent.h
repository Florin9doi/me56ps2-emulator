#pragma once

#include "modem.h"

class LucentModem : public Modem {
public:
    const struct usb_device_descriptor &device_descriptor() const override;
    const struct usb_config_descriptors &config_descriptors(const uint8_t id) const override;
    const void * const *string_descriptors() const override;

    bool handle_set_configuration(usb_raw_control_event *e, struct usb_packet_control *pkt) override;
    bool handle_control_request(usb_raw_control_event *e, struct usb_packet_control *pkt) override;
    bool process_at_ext(std::string &line) override;

protected:
    void *intr_in_thread(int ep_num);
    void *bulk_out_thread(int ep_num);
    void *bulk_in_thread(int ep_num);

    std::thread *thread_intr_in;
    std::thread *thread_bulk_out;
    std::thread *thread_bulk_in;
};
