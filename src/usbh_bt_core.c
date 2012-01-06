/*
   Copyright 2011, 2012 Vitaly Belostotsky.

   This file is part of Uubt.

   Uubt is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "usbh_core.h"
#include "usbh_ioreq.h"
#include "usbh_stdreq.h"
#include "usbh_hcs.h"
#include "hci.h"
#include <btstack/hci_cmds.h>
#include "hal_uubt.h"

#include <stdio.h>
#include <string.h>

#define WIRELESS_CLASS 0xe0
#define FRNUM_PERIOD 0x4000

// most of descriptors data is currently hardcoded:

#define EVT_EP  0x81
#define ACLO_EP 0x02
#define ACLI_EP 0x82

#define EVT_MPS 16
#define ACL_MPS 64

// ath3k fw: modelled after linux ath3k.c; reuses ACLO defines

#define ATHEROS_ID_VENDOR 0x0cf3
#define AR3011_ID_PRODUCT 0x3000
#define USB_REQ_DFU_DNLOAD 1
#define ATH_CHUNK 4096

#ifdef INCLUDE_FIRMWARE
#include "../firmware/ath3k_fw.c"
#endif

// buffer sizes: according to stm32f4xx ref. man. [rm0090, 29.16.3, OTG_FS_HCTSIZx.XFRSIZ]
//  IN buffers should not only divide 4, but also divide max packet size

#define CMD_ALIGNED_MAX_SIZE 260
#define EVT_ALIGNED_MAX_SIZE 272
#define ACL_ALIGNED_MAX_SIZE  64

typedef struct BT_State_s {
    // buffers should be 32 bit aligned
    uint8_t cmd_packet [ CMD_ALIGNED_MAX_SIZE ];
    uint8_t evt_packet [ EVT_ALIGNED_MAX_SIZE ];
    uint8_t aclo_packet[ ACL_ALIGNED_MAX_SIZE ];
    uint8_t acli_packet[ ACL_ALIGNED_MAX_SIZE ];

    USB_OTG_CORE_HANDLE *pdev;
    USBH_HOST *phost;

    int      loading_fw;
    int      fw_offset;

    int      cmd_busy;
    int      cmd_state;
    uint16_t cmd_len;

    int      evt_state;
    uint8_t  evt_hcn;
    uint16_t evt_tmo;

    int      aclo_busy;
    int      aclo_state;
    uint16_t aclo_len;
    uint8_t  aclo_hcn;

    int      acli_state;
    uint8_t  acli_hcn;
} BT_State_t;

BT_State_t __attribute__ ((aligned (4))) BT_State;

// btstack interface for rx/tx:

void hal_ub_reset(void){}

typedef void (* rx_handler_t)(uint8_t packet_type, uint8_t *packet, uint16_t size);
static void dummy_rx_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){}
static rx_handler_t rx_handler = dummy_rx_handler;

void hal_ub_reg_rx_packet(rx_handler_t rx_cb)
{
    rx_handler = rx_cb ? rx_cb : dummy_rx_handler;
}

typedef void (* tx_end_handler_t)(uint8_t packet_type);
static void dummy_tx_end_handler(uint8_t packet_type){}
static tx_end_handler_t tx_end_handler = dummy_tx_end_handler;

void hal_ub_reg_tx_end(tx_end_handler_t tx_end_cb)
{
    tx_end_handler = tx_end_cb ? tx_end_cb : dummy_tx_end_handler;
}

int hal_ub_tx_packet(uint8_t packet_type, const uint8_t *packet, uint16_t size)
{
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
	    if(BT_State.cmd_busy) return -1;
	    if(size > CMD_ALIGNED_MAX_SIZE) return -1;
	    BT_State.cmd_len = size;
	    memcpy(BT_State.cmd_packet, packet, size);
	    BT_State.cmd_state = 0;
	    BT_State.cmd_busy = 1;
	    break;
        case HCI_ACL_DATA_PACKET:
	    if(BT_State.aclo_busy) return -1;
	    if(size > ACL_ALIGNED_MAX_SIZE) return -1;
	    BT_State.aclo_len = size;
	    memcpy(BT_State.aclo_packet, packet, size);
	    BT_State.aclo_state = 0;
	    BT_State.aclo_busy = 1;
	    break;
	default:
	    return -1;
    }
    return 0;
}

// USB class callbacks:

static USBH_Status USBH_BT_InterfaceInit(USB_OTG_CORE_HANDLE *pdev, void *_phost)
{	 
    USBH_HOST *phost = _phost;
    
    if(phost->device_prop.Itf_Desc[0].bInterfaceClass == WIRELESS_CLASS){ // TODO: full matching
	BT_State.loading_fw = 0;
#ifdef INCLUDE_FIRMWARE
    } else if(phost->device_prop.Dev_Desc.idVendor == ATHEROS_ID_VENDOR
		&& phost->device_prop.Dev_Desc.idProduct == AR3011_ID_PRODUCT){
	BT_State.loading_fw = 1;
	BT_State.fw_offset = 0;
#endif
    } else {
        phost->usr_cb->USBH_USR_DeviceNotSupported(); 
	return USBH_NOT_SUPPORTED;
    }

    BT_State.pdev  = pdev;
    BT_State.phost = phost;

    BT_State.evt_hcn  = USBH_Alloc_Channel(pdev, EVT_EP);
    BT_State.aclo_hcn = USBH_Alloc_Channel(pdev, ACLO_EP);
    BT_State.acli_hcn = USBH_Alloc_Channel(pdev, ACLI_EP);

    if(!BT_State.loading_fw){
	USBH_Open_Channel(pdev, BT_State.evt_hcn,
	    phost->device_prop.address, phost->device_prop.speed, EP_TYPE_INTR, EVT_MPS);
	USBH_Open_Channel(pdev, BT_State.acli_hcn,
	    phost->device_prop.address, phost->device_prop.speed, EP_TYPE_BULK, ACL_MPS);
    }
    USBH_Open_Channel(pdev, BT_State.aclo_hcn,
	phost->device_prop.address, phost->device_prop.speed, EP_TYPE_BULK, ACL_MPS);

    BT_State.cmd_busy   = 0;
    BT_State.cmd_state  = 0;
    BT_State.evt_state  = 0;
    BT_State.aclo_busy  = 0;
    BT_State.aclo_state = 0;
    BT_State.acli_state = 0;

    return USBH_OK;
}

static void USBH_BT_InterfaceDeInit(USB_OTG_CORE_HANDLE *pdev, void *_phost)
{	
    if(!BT_State.loading_fw){
	hci_power_control(HCI_POWER_OFF);
    }
#define bt_free_ch(hcn) \
    if(BT_State.hcn){ \
        USB_OTG_HC_Halt(pdev, BT_State.hcn); \
        USBH_Free_Channel(pdev, BT_State.hcn); \
	BT_State.hcn = 0; \
    }
    bt_free_ch(evt_hcn);
    bt_free_ch(aclo_hcn);
    bt_free_ch(acli_hcn);
#undef bt_free_ch
}

static USBH_Status USBH_BT_ClassRequest(USB_OTG_CORE_HANDLE *pdev, void *_phost)
{   
    if(!BT_State.loading_fw){
	led_pwm_isr_init();
	hci_power_control(HCI_POWER_ON);		// executed after USB class init
    }
    printf("class-req%s\n", BT_State.loading_fw ? " (loading fw)" : "");
    return USBH_OK; 
}

// endpoints polling functions:

static int usb_bt_cmd_process(void)
{
    USBH_Status st;

    if(!BT_State.cmd_busy) return 0;

    if(BT_State.cmd_state == 0){
#define setup_packet (BT_State.phost->Control.setup.b)
	setup_packet.bmRequestType = USB_H2D | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE;
	setup_packet.bRequest = 0;
	setup_packet.wValue.w = 0;
	setup_packet.wIndex.w = 0;
	setup_packet.wLength.w = BT_State.cmd_len;           
#undef setup_packet
	BT_State.cmd_state = 1;
    }
    st = USBH_CtlReq(BT_State.pdev, BT_State.phost, BT_State.cmd_packet, BT_State.cmd_len);
    if(st == USBH_OK){
	(*tx_end_handler)(HCI_COMMAND_DATA_PACKET);
	BT_State.cmd_state = 0;
	BT_State.cmd_busy = 0;
    }
    else if(st == USBH_FAIL){
	BT_State.cmd_state = 0;
	BT_State.cmd_busy = 0;
	printf("cmd-fail\n");
    }
    return 0;
}

static int usb_bt_evt_process(void)
{
    int len, r;
    uint16_t cur_fn, fn_diff;

    cur_fn = HCD_GetCurrentFrame(BT_State.pdev);
    fn_diff = (FRNUM_PERIOD + cur_fn - BT_State.evt_tmo) % FRNUM_PERIOD;
    switch(BT_State.evt_state){
	case 0:
	    //while(!USB_OTG_IsEvenFrame(BT_State.pdev));	// hopefully rare and harmless

	    USBH_InterruptReceiveData(
		BT_State.pdev,
		BT_State.evt_packet,
		EVT_ALIGNED_MAX_SIZE,
		BT_State.evt_hcn
	    );
	    BT_State.evt_tmo = (2 + cur_fn) % FRNUM_PERIOD;
	    BT_State.evt_state = 1;
	    return 0;
	case 1:
	    if(fn_diff < FRNUM_PERIOD / 2){
		BT_State.evt_state = 0;
		break;
	    }
	    r = HCD_GetURB_State(BT_State.pdev, BT_State.evt_hcn);
	    switch(r){
		case URB_DONE:
		    len = HCD_GetXferCnt(BT_State.pdev, BT_State.evt_hcn);
		    (*rx_handler)(HCI_EVENT_PACKET, BT_State.evt_packet, len);
		    BT_State.evt_state = 2;
		    return 0;
		case URB_STALL:
		    printf("evt-stall\n");
		    if(USBH_OK == USBH_ClrFeature(BT_State.pdev, BT_State.phost, EVT_EP, BT_State.evt_hcn))
			BT_State.evt_state = 0;
		    break;
		default:
		    return 0;
	    }
	    break;
	case 2:
	    if(fn_diff < FRNUM_PERIOD / 2){
		BT_State.evt_state = 0;
	    }
	    break;
	default: return -1;
    }
    return 0;
}

static int usb_bt_aclo_process(const uint8_t * buf)	// tx_end_handler called only for NULL buf
{
    if(!BT_State.aclo_busy) return 0;

    switch(BT_State.aclo_state){
	case 0:
	    USBH_BulkSendData(
		BT_State.pdev,
		buf ? (uint8_t *)buf : BT_State.aclo_packet,
		BT_State.aclo_len,
		BT_State.aclo_hcn
	    );
	    BT_State.aclo_state = 1;
	    return 0;
	case 1:
	    switch(HCD_GetURB_State(BT_State.pdev, BT_State.aclo_hcn)){
		case URB_DONE:
		    if(!buf){
			(*tx_end_handler)(HCI_ACL_DATA_PACKET);
		    }
		    BT_State.aclo_state = 0;
		    BT_State.aclo_busy = 0;
		    return 0;
		case URB_STALL:
		    if(USBH_OK == USBH_ClrFeature(BT_State.pdev, BT_State.phost, ACLO_EP, BT_State.aclo_hcn))
			BT_State.aclo_state = 0;
		    break;
		default: return 0;
	    }
	    break;
	default: return -1;
    }
    return 0;
}

static int usb_bt_acli_process(void)
{
    int len;

    switch(BT_State.acli_state){
	case 0:
	    USBH_BulkReceiveData(
		BT_State.pdev,
		BT_State.acli_packet,
		ACL_ALIGNED_MAX_SIZE,
		BT_State.acli_hcn
	    );
	    BT_State.acli_state = 1;
	    return 0;
	case 1:
	    switch(HCD_GetURB_State(BT_State.pdev, BT_State.acli_hcn)){
		case URB_DONE:
		    len = HCD_GetXferCnt(BT_State.pdev, BT_State.acli_hcn);
		    (*rx_handler)(HCI_ACL_DATA_PACKET, BT_State.acli_packet, len);
		    BT_State.acli_state = 0;
		    return 0;
		case URB_STALL:
		    if(USBH_OK == USBH_ClrFeature(BT_State.pdev, BT_State.phost, ACLI_EP, BT_State.acli_hcn))
			BT_State.acli_state = 0;
		    break;
		default: return 0;
	    }
	    break;
	default: return -1;
    }
    return 0;
}

// USB class callbacks go on:

static USBH_Status USBH_BT_StMachine(USB_OTG_CORE_HANDLE *pdev, void *_phost)
{
#ifdef INCLUDE_FIRMWARE
    USBH_Status st;

    if(BT_State.loading_fw){
	switch(BT_State.loading_fw){
	    case 1:
		if(BT_State.cmd_state == 0){
#define setup_packet (BT_State.phost->Control.setup.b)
		    setup_packet.bmRequestType = USB_H2D | USB_REQ_TYPE_VENDOR | USB_REQ_RECIPIENT_DEVICE;
		    setup_packet.bRequest = USB_REQ_DFU_DNLOAD;
		    setup_packet.wValue.w = 0;
		    setup_packet.wIndex.w = 0;
		    setup_packet.wLength.w = BT_State.cmd_len = sizeof(ath3k_fw_cmd);
#undef setup_packet
		    memcpy(BT_State.cmd_packet, ath3k_fw_cmd, BT_State.cmd_len);
		    BT_State.cmd_state = 1;
		    led_pwm_ctl(LED_R, ENABLE);
		}
		st = USBH_CtlReq(BT_State.pdev, BT_State.phost, BT_State.cmd_packet, BT_State.cmd_len);
		if(st == USBH_OK){
		    BT_State.cmd_state = 0;
		    BT_State.loading_fw = 2;
		    led_pwm_ctl(LED_G, ENABLE);
		} else if(st == USBH_FAIL){
		    BT_State.cmd_state = 0;
		    printf("fw-setup-fail\n");
		}
		break;
	    case 2:
		while(1){
		    int n;
		    if(!BT_State.aclo_busy){
			n = sizeof(ath3k_fw_data) - BT_State.fw_offset;
			if(n > ACL_MPS) n = ACL_MPS;	// ATH_CHUNK hangs
			if(n <= 0){
			    BT_State.loading_fw = 3;
			    printf("fw-done\n");
			    led_pwm_ctl(LED_O, ENABLE);
			    break;
			}
			led_pwm_ctl(LED_B, ENABLE);
			BT_State.aclo_len = n;
			BT_State.aclo_state = 0;
			BT_State.aclo_busy = 1;
		    }
		    usb_bt_aclo_process(ath3k_fw_data + BT_State.fw_offset);
		    if(!BT_State.aclo_busy){
			BT_State.fw_offset += n;
		    }
		}
		break;
	    default:
		break;
	}
    } else
#endif
    {
	usb_bt_cmd_process();
	usb_bt_evt_process();
	usb_bt_aclo_process(0);
	usb_bt_acli_process();
    }
    return USBH_OK;
}

USBH_Class_cb_TypeDef USBH_BT_cb = {
    USBH_BT_InterfaceInit,
    USBH_BT_InterfaceDeInit,
    USBH_BT_ClassRequest,
    USBH_BT_StMachine,
};
