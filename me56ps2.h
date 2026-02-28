#if defined(HW_NANOPI_NEO2) // for NanoPi NEO2
constexpr char USB_RAW_GADGET_DRIVER_DEFAULT[] = "musb-hdrc";
constexpr char USB_RAW_GADGET_DEVICE_DEFAULT[] = "musb-hdrc.2.auto";
#elif defined(HW_RPI_ZERO) // for Raspberry Pi Zero W
constexpr char USB_RAW_GADGET_DRIVER_DEFAULT[] = "20980000.usb";
constexpr char USB_RAW_GADGET_DEVICE_DEFAULT[] = "20980000.usb";
#elif defined(HW_RPI_ZERO2) // for Raspberry Pi Zero 2 W
constexpr char USB_RAW_GADGET_DRIVER_DEFAULT[] = "3f980000.usb";
constexpr char USB_RAW_GADGET_DEVICE_DEFAULT[] = "3f980000.usb";
#else // for Raspberry Pi 4 Model B
constexpr char USB_RAW_GADGET_DRIVER_DEFAULT[] = "fe980000.usb";
constexpr char USB_RAW_GADGET_DEVICE_DEFAULT[] = "fe980000.usb";
#endif

constexpr auto TCP_DEFAULT_PORT = 10023;

constexpr auto BCD_USB = 0x0110U; // USB 1.1
constexpr auto BCD_DEVICE = 0x0101U;

constexpr auto ENDPOINT_ADDR_BULK = 2U;
constexpr auto MAX_PACKET_SIZE_CONTROL = 64U; // 8 in original ME56PS2
constexpr auto MAX_PACKET_SIZE_BULK = 64U;

constexpr auto STRING_ID_MANUFACTURER = 1U;
constexpr auto STRING_ID_PRODUCT = 2U;
constexpr auto STRING_ID_SERIAL = 3U;

constexpr auto STRING_DESCRIPTORS_NUM = 6;

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
    struct _usb_endpoint_descriptor endpoint_bulk_out;
    struct _usb_endpoint_descriptor endpoint_bulk_in;
    struct _usb_endpoint_descriptor endpoint_extra[6];
};

struct modem_config {
    const char *model_name;
    struct usb_device_descriptor device_descriptor;
    struct usb_config_descriptors config_descriptors;
    int string_descriptors_num;
    const void *string_descriptors[STRING_DESCRIPTORS_NUM];
};

extern const struct modem_config modem_me56ps2;   // Omron ME56PS2
extern const struct modem_config modem_ms56kps2;  // Suntac OnlineStation MS56KPS2
extern const struct modem_config modem_p2gate;    // Conexant SmartSCM P2Gate

const struct modem_config *find_modem_config(const char *name);
