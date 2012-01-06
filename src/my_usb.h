#ifndef __MY_USB_H
#define __MY_USB_H

#include "usb_core.h"
#include "usbh_core.h"
#include "hci_transport.h"

hci_transport_t * hci_transport_stm_instance(void);

void my_usb_bsp_init(void);

extern void (*tick_handler)(void);

extern USB_OTG_CORE_HANDLE USB_OTG_Core;
extern USBH_HOST USB_Host;
extern USBH_Class_cb_TypeDef USBH_BT_cb;
extern USBH_Usr_cb_TypeDef USR_Callbacks;

#endif	//__MY_USB_H
