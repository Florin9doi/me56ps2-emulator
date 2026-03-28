#pragma once

#include "modem.h"

class SmartSCMModem : public Modem {
public:
    const struct usb_device_descriptor &device_descriptor() const override;
    const struct usb_config_descriptors &config_descriptors(const uint8_t id) const override;
    const void * const *string_descriptors() const override;

    bool handle_set_configuration(usb_raw_control_event *e, struct usb_packet_control *pkt) override;

    bool process_at_ext(std::string &line) override;

protected:
    void *control_out_thread(int ep_num);
    void *control_in_thread(int ep_num); // empty
    void *data_out_thread(int ep_num);
    void *data_in_thread(int ep_num);
    void *gpio_out_thread(int ep_num);
    void *gpio_in_thread(int ep_num); // empty

    std::thread *thread_control_out;
    std::thread *thread_control_in;
    std::thread *thread_data_out;
    std::thread *thread_data_in;
    std::thread *thread_gpio_out;
    std::thread *thread_gpio_in;
};
