/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 ave oezkal (ave.zone)
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "tusb.h"
#include "SwitchDescriptors.h"
#include "usb_mode.h"

/* The device presents one of two personalities depending on g_usb_mode:
 *   - USB_MODE_CONTROLLER: the HORIPAD HID gamepad the console expects.
 *   - USB_MODE_CONFIG:     a USB CDC serial port for the Web Serial config tool.
 * The controller-mode descriptors are returned byte-for-byte unchanged so the
 * console-validated behavior is never affected.
 */

#define _PID_MAP(itf, n) ((CFG_TUD_##itf) << (n))

//--------------------------------------------------------------------+
// Config-mode (CDC) descriptors
//--------------------------------------------------------------------+

// Distinct VID/PID so the web tool can filter for this device.
#define CONFIG_USB_VID 0xCAFE
#define CONFIG_USB_PID 0x4011

static const tusb_desc_device_t desc_device_cdc = {
	.bLength = sizeof(tusb_desc_device_t),
	.bDescriptorType = TUSB_DESC_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = TUSB_CLASS_MISC,
	.bDeviceSubClass = MISC_SUBCLASS_COMMON,
	.bDeviceProtocol = MISC_PROTOCOL_IAD,
	.bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
	.idVendor = CONFIG_USB_VID,
	.idProduct = CONFIG_USB_PID,
	.bcdDevice = 0x0100,
	.iManufacturer = 0x01,
	.iProduct = 0x02,
	.iSerialNumber = 0x03,
	.bNumConfigurations = 0x01,
};

enum { ITF_NUM_CDC = 0, ITF_NUM_CDC_DATA, ITF_NUM_CDC_TOTAL };

#define CONFIG_TOTAL_LEN_CDC (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)

#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT 0x02
#define EPNUM_CDC_IN 0x82

static const uint8_t desc_configuration_cdc[] = {
	TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_CDC_TOTAL, 0, CONFIG_TOTAL_LEN_CDC, 0x00, 100),
	TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 0, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

static const uint8_t config_string_manufacturer[] = "SwitchKMAdapter";
static const uint8_t config_string_product[] = "SwitchKMAdapter Config";
static const uint8_t config_string_serial[] = "000001";

static const uint8_t *config_string_descriptors[] = {
	switch_string_language,
	config_string_manufacturer,
	config_string_product,
	config_string_serial,
};

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+

uint8_t const *
tud_descriptor_device_cb(void)
{
	if (g_usb_mode == USB_MODE_CONFIG)
		return (uint8_t const *) &desc_device_cdc;
	return switch_device_descriptor;
}

//--------------------------------------------------------------------+
// HID Report Descriptor (controller mode only)
//--------------------------------------------------------------------+

uint8_t const *
tud_hid_descriptor_report_cb(uint8_t instance)
{
	(void) instance;
	return switch_report_descriptor;
}

//--------------------------------------------------------------------+
// Configuration Descriptor (controller mode)
//--------------------------------------------------------------------+

enum { ITF_NUM_HID1, ITF_NUM_HID2, ITF_NUM_HID3, ITF_NUM_HID4, ITF_NUM_TOTAL };

#define CONFIG_TOTAL_LEN                                                       \
	(TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN +           \
	 TUD_HID_DESC_LEN + TUD_HID_DESC_LEN)

#define EPNUM_HID1 0x81
#define EPNUM_HID2 0x82
#define EPNUM_HID3 0x83
#define EPNUM_HID4 0x84

uint8_t const desc_configuration[] = {
	// Config number, interface count, string index, total length, attribute, power in mA
	TUD_CONFIG_DESCRIPTOR(1,
	                      ITF_NUM_TOTAL,
	                      0,
	                      CONFIG_TOTAL_LEN,
	                      TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,
	                      500),

	// Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
	TUD_HID_DESCRIPTOR(ITF_NUM_HID1,
	                   0,
	                   HID_ITF_PROTOCOL_NONE,
	                   sizeof(switch_report_descriptor),
	                   EPNUM_HID1,
	                   CFG_TUD_HID_EP_BUFSIZE,
	                   1),
	TUD_HID_DESCRIPTOR(ITF_NUM_HID2,
	                   0,
	                   HID_ITF_PROTOCOL_NONE,
	                   sizeof(switch_report_descriptor),
	                   EPNUM_HID2,
	                   CFG_TUD_HID_EP_BUFSIZE,
	                   1),
	TUD_HID_DESCRIPTOR(ITF_NUM_HID3,
	                   0,
	                   HID_ITF_PROTOCOL_NONE,
	                   sizeof(switch_report_descriptor),
	                   EPNUM_HID3,
	                   CFG_TUD_HID_EP_BUFSIZE,
	                   1),
	TUD_HID_DESCRIPTOR(ITF_NUM_HID4,
	                   0,
	                   HID_ITF_PROTOCOL_NONE,
	                   sizeof(switch_report_descriptor),
	                   EPNUM_HID4,
	                   CFG_TUD_HID_EP_BUFSIZE,
	                   1)
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
uint8_t const *
tud_descriptor_configuration_cb(uint8_t index)
{
	(void) index;
	if (g_usb_mode == USB_MODE_CONFIG)
		return desc_configuration_cdc;
	return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

static uint16_t _desc_str[32];

uint16_t const *
tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
	(void) langid;

	const uint8_t **descs;
	uint8_t desc_count;
	if (g_usb_mode == USB_MODE_CONFIG) {
		descs = config_string_descriptors;
		desc_count = sizeof(config_string_descriptors) /
		             sizeof(config_string_descriptors[0]);
	} else {
		descs = switch_string_descriptors;
		desc_count = sizeof(switch_string_descriptors) /
		             sizeof(switch_string_descriptors[0]);
	}

	uint8_t chr_count;

	if (index == 0) {
		memcpy(&_desc_str[1], descs[0], 2);
		chr_count = 1;
	} else {
		if (index >= desc_count)
			return NULL;

		const char *str = (const char *) descs[index];

		// Cap at max char
		chr_count = strlen(str);
		if (chr_count > 31)
			chr_count = 31;

		// Convert ASCII string into UTF-16
		for (uint8_t i = 0; i < chr_count; i++) {
			_desc_str[1 + i] = str[i];
		}
	}

	// first byte is length (including header), second byte is string type
	_desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

	return _desc_str;
}

//--------------------------------------------------------------------+
// HID callbacks (controller mode)
//--------------------------------------------------------------------+

uint16_t
tud_hid_get_report_cb(uint8_t instance,
                      uint8_t report_id,
                      hid_report_type_t report_type,
                      uint8_t *buffer,
                      uint16_t reqlen)
{
	return 0;
}

void
tud_hid_set_report_cb(uint8_t itf,
                      uint8_t report_id,
                      hid_report_type_t report_type,
                      uint8_t const *buffer,
                      uint16_t bufsize)
{
	return;
}
