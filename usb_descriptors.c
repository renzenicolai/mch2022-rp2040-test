/**
 * Copyright (c) 2022 Nicolai Electronics
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * SPDX-License-Identifier: MIT
 */

#include "tusb.h"
#include "bsp/board.h"
#include "pico/unique_id.h"
#include "usb_descriptors.h"

// String descriptors

char const* string_desc_arr [] = {
  (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
  "Badge.team",                  // 1: Manufacturer
  "MCH2022 badge",               // 2: Product
  "ESP32 console",               // 3: CDC Interface
  "FPGA console",                // 4: CDC Interface
  "RP2040 console",              // 5: CDC Interface
  "WebUSB ESP32 console",        // 6: WebUSB Interface: console
  "WebUSB control interface"     // 7: WebUSB Interface: control
  "HID",                         // 8: HID interface
//  "Mass storage"                 // 9: Mass storage interface
};

enum {
    STRING_DESC = 0,
    STRING_DESC_MANUFACTURER,
    STRING_DESC_PRODUCT,
    STRING_DESC_CDC_0,
    STRING_DESC_CDC_1,
    STRING_DESC_CDC_2,
    STRING_DESC_WEBUSB_UART,
    STRING_DESC_WEBUSB_FS,
    STRING_DESC_HID,
//    STRING_DESC_MSC,
    STRING_DESC_SERIAL // (Not in the string description array)
};

static uint16_t _desc_str[32];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    uint8_t chr_count;
    if (index == STRING_DESC) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else if (index == STRING_DESC_SERIAL) {
        pico_unique_board_id_t id;
        pico_get_unique_board_id(&id);
        const uint8_t *str = id.id;
        chr_count = 16;
        for (uint8_t len = 0; len < chr_count; ++len) {
            uint8_t c = str[len >> 1];
            c = ((c >> (((len & 1) ^ 1) << 2)) & 0x0F) + '0';
            if (c > '9') {
                c += 7;
            }
            _desc_str[1 + len] = c;
        }
    } else {
        // Convert ASCII string into UTF-16
        if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) {
            return NULL;
        }
        const char* str = string_desc_arr[index];
        // Cap at max char
        chr_count = strlen(str);
        if ( chr_count > 31 ) chr_count = 31;
        for(uint8_t i=0; i<chr_count; i++) {
            _desc_str[1+i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);

    return _desc_str;
}

// Device descriptors

tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,   // Endpoint 0 packet size
    .idVendor           = 0x16D0,                   // MCH2022 badge vendor identifier
    .idProduct          = 0x0F9A,                   // MCH2022 badge product identifier
    .bcdDevice          = 0x0100,                   // Protocol version
    .iManufacturer      = STRING_DESC_MANUFACTURER, // Index of manufacturer name string
    .iProduct           = STRING_DESC_PRODUCT,      // Index of product name string
    .iSerialNumber      = STRING_DESC_SERIAL,       // Index of serial number string
    .bNumConfigurations = 0x01                      // Number of configurations supported
};

uint8_t const * tud_descriptor_device_cb(void) {
  return (uint8_t const *) &desc_device;
}

// HID descriptor

uint8_t const desc_hid_report[] =
{
  TUD_HID_REPORT_DESC_KEYBOARD( HID_REPORT_ID(REPORT_ID_KEYBOARD         )),
  TUD_HID_REPORT_DESC_MOUSE   ( HID_REPORT_ID(REPORT_ID_MOUSE            )),
  TUD_HID_REPORT_DESC_CONSUMER( HID_REPORT_ID(REPORT_ID_CONSUMER_CONTROL )),
  TUD_HID_REPORT_DESC_GAMEPAD ( HID_REPORT_ID(REPORT_ID_GAMEPAD          ))
};

uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance) {
  (void) instance;
  return desc_hid_report;
}

// Configuration Descriptor

enum {
  ITF_NUM_CDC_0,
  ITF_NUM_CDC_0_DATA,
  ITF_NUM_VENDOR_0,
  ITF_NUM_VENDOR_1,
  ITF_NUM_CDC_1,
  ITF_NUM_CDC_1_DATA,
  ITF_NUM_HID,
  ITF_NUM_CDC_2,
  ITF_NUM_CDC_2_DATA,
  ITF_NUM_MSC,
  ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + CFG_TUD_CDC * TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN + CFG_TUD_VENDOR * TUD_VENDOR_DESC_LEN + CFG_TUD_MSC * TUD_MSC_DESC_LEN)

#define EPNUM_CDC_0_NOTIF  0x81 // Endpoint 1: CDC serial port for ESP32 console, control
#define EPNUM_CDC_0_OUT    0x02 // Endpoint 2: CDC serial port for ESP32 console, data
#define EPNUM_CDC_0_IN     0x82

#define EPNUM_VENDOR_0_OUT 0x03 // Endpoint 3: WebUSB for ESP32 console
#define EPNUM_VENDOR_0_IN  0x83

#define EPNUM_VENDOR_1_OUT 0x04 // Endpoint 4: WebUSB for control protocol
#define EPNUM_VENDOR_1_IN  0x84

#define EPNUM_CDC_1_NOTIF  0x85 // Endpoint 5: CDC serial port for FPGA console, control
#define EPNUM_CDC_1_OUT    0x06 // Endpoint 6: CDC serial port for FPGA console, data
#define EPNUM_CDC_1_IN     0x86

#define EPNUM_HID          0x87 // Endpoint 7: Composite HID device for emulating keyboard, mouse, joystick and media keys

#define EPNUM_CDC_2_NOTIF  0x88 // Endpoint 8: CDC serial port for RP2040 console, control
#define EPNUM_CDC_2_OUT    0x09 // Endpoint 9: CDC serial port for RP2040 console, data
#define EPNUM_CDC_2_IN     0x89

#define EPNUM_MSC_OUT      0x0A // Endpoint 10: Mass storage device
#define EPNUM_MSC_IN       0x8A

uint8_t const desc_fs_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

    // 1st CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_0, STRING_DESC_CDC_0, EPNUM_CDC_0_NOTIF, 8, EPNUM_CDC_0_OUT, EPNUM_CDC_0_IN, 64),

    // 2nd CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_1, STRING_DESC_CDC_1, EPNUM_CDC_1_NOTIF, 8, EPNUM_CDC_1_OUT, EPNUM_CDC_1_IN, 64),
    
    // 3rd CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_2, STRING_DESC_CDC_2, EPNUM_CDC_2_NOTIF, 8, EPNUM_CDC_2_OUT, EPNUM_CDC_2_IN, 64),

    // HID: Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, STRING_DESC_HID, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 5),
    
    // WebUSB: Interface number, string index, EP Out & IN address, EP size
    TUD_VENDOR_DESCRIPTOR(ITF_NUM_VENDOR_0, STRING_DESC_WEBUSB_UART, EPNUM_VENDOR_0_OUT, EPNUM_VENDOR_0_IN, 32),
    TUD_VENDOR_DESCRIPTOR(ITF_NUM_VENDOR_1, STRING_DESC_WEBUSB_FS, EPNUM_VENDOR_1_OUT, EPNUM_VENDOR_1_IN, 32),
    
    // MSC: Interface number, string index, EP Out & EP In address, EP size
    //TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, STRING_DESC_MSC, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    return desc_fs_configuration;
}

// WebUSB

#define BOS_TOTAL_LEN      (TUD_BOS_DESC_LEN + TUD_BOS_WEBUSB_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)
#define MS_OS_20_DESC_LEN  0xB2

// BOS Descriptor is required for webUSB
uint8_t const desc_bos[] = {
  // total length, number of device caps
  TUD_BOS_DESCRIPTOR(BOS_TOTAL_LEN, 2),

  // Vendor Code, iLandingPage
  TUD_BOS_WEBUSB_DESCRIPTOR(VENDOR_REQUEST_WEBUSB, 1),

  // Microsoft OS 2.0 descriptor
  TUD_BOS_MS_OS_20_DESCRIPTOR(MS_OS_20_DESC_LEN, VENDOR_REQUEST_MICROSOFT)
};

uint8_t const * tud_descriptor_bos_cb(void) {
  return desc_bos;
}

uint8_t const desc_ms_os_20[] = {
    // Set header: length, type, windows version, total length
    U16_TO_U8S_LE(0x000A), U16_TO_U8S_LE(MS_OS_20_SET_HEADER_DESCRIPTOR), U32_TO_U8S_LE(0x06030000), U16_TO_U8S_LE(MS_OS_20_DESC_LEN),

    // Configuration subset header: length, type, configuration index, reserved, configuration total length
    U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_CONFIGURATION), 0, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN-0x0A),

    // Function Subset header: length, type, first interface, reserved, subset length
    U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_FUNCTION), ITF_NUM_VENDOR_0, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN-0x0A-0x08),

    // MS OS 2.0 Compatible ID descriptor: length, type, compatible ID, sub compatible ID
    U16_TO_U8S_LE(0x0014), U16_TO_U8S_LE(MS_OS_20_FEATURE_COMPATBLE_ID), 'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // sub-compatible

    // MS OS 2.0 Registry property descriptor: length, type
    U16_TO_U8S_LE(MS_OS_20_DESC_LEN-0x0A-0x08-0x08-0x14), U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY),
    U16_TO_U8S_LE(0x0007), U16_TO_U8S_LE(0x002A), // wPropertyDataType, wPropertyNameLength and PropertyName "DeviceInterfaceGUIDs\0" in UTF-16
    'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00,
    'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00, 0x00, 0x00,
    U16_TO_U8S_LE(0x0050), // wPropertyDataLength
    //bPropertyData: “{975F44D9-0D08-43FD-8B3E-127CA8AFFF9D}”.
    '{', 0x00, '9', 0x00, '7', 0x00, '5', 0x00, 'F', 0x00, '4', 0x00, '4', 0x00, 'D', 0x00, '9', 0x00, '-', 0x00,
    '0', 0x00, 'D', 0x00, '0', 0x00, '8', 0x00, '-', 0x00, '4', 0x00, '3', 0x00, 'F', 0x00, 'D', 0x00, '-', 0x00,
    '8', 0x00, 'B', 0x00, '3', 0x00, 'E', 0x00, '-', 0x00, '1', 0x00, '2', 0x00, '7', 0x00, 'C', 0x00, 'A', 0x00,
    '8', 0x00, 'A', 0x00, 'F', 0x00, 'F', 0x00, 'F', 0x00, '9', 0x00, 'D', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00
};

TU_VERIFY_STATIC(sizeof(desc_ms_os_20) == MS_OS_20_DESC_LEN, "Incorrect size");
