#include <cstdio>
#include <cstring>
#include <thread>

#include "modem.h"
#include "modem_omron.h"
#include "modem_onlinestation.h"
#include "modem_smartscm.h"

std::thread *Modem::thread_bulk_in = nullptr;
std::thread *Modem::thread_bulk_out = nullptr;

bool Modem::handle_set_configuration(usb_raw_gadget *usb, struct usb_packet_control *pkt) {
    if (thread_bulk_in == nullptr) {
        const int ep_num_bulk_in = usb->ep_enable(
            reinterpret_cast<struct usb_endpoint_descriptor *>(
                const_cast<struct _usb_endpoint_descriptor *>(&config_descriptors().endpoints[1])));
        thread_bulk_in = new std::thread(usb_bulk_in_thread, usb, ep_num_bulk_in);
    }
    if (thread_bulk_out == nullptr) {
        const int ep_num_bulk_out = usb->ep_enable(
            reinterpret_cast<struct usb_endpoint_descriptor *>(
                const_cast<struct _usb_endpoint_descriptor *>(&config_descriptors().endpoints[0])));
        thread_bulk_out = new std::thread(usb_bulk_out_thread, usb, ep_num_bulk_out);
    }
    usb->vbus_draw(config_descriptors().config.bMaxPower);
    usb->configure();
    printf("USB configured.\n");
    pkt->header.length = 0;
    return true;
}

bool Modem::handle_vendor_request(usb_raw_gadget *usb, usb_raw_control_event *e, struct usb_packet_control *pkt) {
    (void)usb;
    (void)e;
    (void)pkt;
    return true;
}

struct modem_entry {
    const char *name;
    Modem *(*factory)();
} static const registry[] = {
    { "Omron",         []() -> Modem * { return new OmronModem();         } },
    { "OnlineStation", []() -> Modem * { return new OnlineStationModem(); } },
    { "SmartSCM",      []() -> Modem * { return new SmartSCMModem();      } },
};

Modem *Modem::getInstance(const char *name) {
    static Modem *instance = nullptr;
    static const char *instance_name = nullptr;
    if (instance != nullptr && strcmp(instance_name, name) == 0) {
        return instance;
    }
    for (const auto &entry : registry) {
        if (strcmp(entry.name, name) == 0) {
            delete instance;
            instance = entry.factory();
            instance_name = entry.name;
            return instance;
        }
    }
    return nullptr;
}
