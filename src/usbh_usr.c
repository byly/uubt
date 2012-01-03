
// User callbacks for ST USB libs.

#include "usbh_core.h"

void USBH_USR_Init(void){}
void USBH_USR_DeInit(void){}
void USBH_USR_DeviceAttached(void){}
void USBH_USR_ResetDevice(void){}
void USBH_USR_DeviceDisconnected (void){}
void USBH_USR_OverCurrentDetected (void){}
void USBH_USR_DeviceSpeedDetected(uint8_t DeviceSpeed){}
void USBH_USR_Device_DescAvailable(void *DeviceDesc){}
void USBH_USR_DeviceAddressAssigned(void){}
void USBH_USR_Configuration_DescAvailable(USBH_CfgDesc_TypeDef * cfgDesc,
	USBH_InterfaceDesc_TypeDef *itfDesc, USBH_EpDesc_TypeDef *epDesc){}
void USBH_USR_Manufacturer_String(void *ManufacturerString){}
void USBH_USR_Product_String(void *ProductString){}
void USBH_USR_SerialNum_String(void *SerialNumString){}

void USBH_USR_EnumerationDone(void)
{
    USB_OTG_BSP_mDelay(500);		// 1000 is possibly (not certain) better for ath3k
} 

USBH_USR_Status USBH_USR_UserInput(void){ return USBH_USR_RESP_OK; }
int  USBH_USR_BT_Application(void){}
void USBH_USR_DeviceNotSupported(void){}
void USBH_USR_UnrecoveredError (void){}

USBH_Usr_cb_TypeDef USR_Callbacks =
{
    USBH_USR_Init,
    USBH_USR_DeInit,
    USBH_USR_DeviceAttached,
    USBH_USR_ResetDevice,
    USBH_USR_DeviceDisconnected,
    USBH_USR_OverCurrentDetected,
    USBH_USR_DeviceSpeedDetected,
    USBH_USR_Device_DescAvailable,
    USBH_USR_DeviceAddressAssigned,
    USBH_USR_Configuration_DescAvailable,
    USBH_USR_Manufacturer_String,
    USBH_USR_Product_String,
    USBH_USR_SerialNum_String,
    USBH_USR_EnumerationDone,
    USBH_USR_UserInput,
    USBH_USR_BT_Application,
    USBH_USR_DeviceNotSupported,
    USBH_USR_UnrecoveredError
};
