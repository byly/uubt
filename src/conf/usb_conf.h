#ifndef __USB_CONF_H
#define __USB_CONF_H

#include "stm32f4xx.h"		// __IO, *int*_t

// __packed is sparsely used by ST USB lib in a senseless way
#define __packed

// host mode conf: all descriptors should fit

#define USBH_MAX_NUM_ENDPOINTS  3
#define USBH_MAX_NUM_INTERFACES 2

#define USE_HOST_MODE

// low level conf:

#define USB_OTG_FS_CORE

// see [rm0090, 29.13.2 "FIFO RAM allocation"]
//  or Libraries/STM32_USB_OTG_Driver/inc/usb_conf_template.h
// for OTG_FS core: total size = 1.25kB, i.e. 320 words
// size is a misnomer: specify 32-bit word counts here

#define RX_FIFO_FS_SIZE   128
#define TXH_NP_FS_FIFOSIZ  96
#define TXH_P_FS_FIFOSIZ   96

//#define USB_OTG_FS_LOW_PWR_MGMT_SUPPORT
//#define USB_OTG_FS_SOF_OUTPUT_ENABLED

#endif	//__USB_CONF_H
