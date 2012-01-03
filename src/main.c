/* vi:set ts=4: */

		//*** Quick'n'dirty usb host bt class.  Byly Dec-2011.  ***//

// loosely based on stm32f4 audio demo and btstack msp-430 spp_counter.c example

//*****************************************************************************
//
// spp_counter demo - it provides a SPP and sends a counter every second
//
// it doesn't use the LCD to get down to a minimal memory footpring
//
//*****************************************************************************

#include "stm32f4xx.h"
#include "usbh_core.h"

#include "my_stdio.h"
#include <stdint.h>
#include <string.h>

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>
#include <btstack/sdp_util.h>

#include "hci.h"
#include "l2cap.h"
#include "btstack_memory.h"
#include "remote_device_db.h"
#include "rfcomm.h"
#include "sdp.h"
#include "config.h"

#define HEARTBEAT_PERIOD_MS 1000

static uint8_t   rfcomm_channel_nr = 1;
static uint16_t  rfcomm_channel_id;
static uint8_t   spp_service_buffer[150];

// Bluetooth logic
static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t event_addr;
    uint8_t   rfcomm_channel_nr;
    uint16_t  mtu;
    
	switch (packet_type) {
		case HCI_EVENT_PACKET:
			switch (packet[0]) {
					
				case BTSTACK_EVENT_STATE:
					// bt stack activated, get started - set local name
					if (packet[2] == HCI_STATE_WORKING) {
                        hci_send_cmd(&hci_write_local_name, "BlueMSP-Demo");
					}
					break;
				
				case HCI_EVENT_COMMAND_COMPLETE:
					if (COMMAND_COMPLETE_EVENT(packet, hci_read_bd_addr)){
                        bt_flip_addr(event_addr, &packet[6]);
                        printf("BD-ADDR: %s\n", bd_addr_to_str(event_addr));
                        break;
                    }
					if (COMMAND_COMPLETE_EVENT(packet, hci_write_local_name)){
                        hci_discoverable_control(1);
                        break;
                    }
                    break;

				case HCI_EVENT_LINK_KEY_REQUEST:
					// deny link key request
                    printf("Link key request\n");
                    bt_flip_addr(event_addr, &packet[2]);
					hci_send_cmd(&hci_link_key_request_negative_reply, &event_addr);
					break;
					
				case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    bt_flip_addr(event_addr, &packet[2]);
					hci_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
					break;
                
                case RFCOMM_EVENT_INCOMING_CONNECTION:
					// data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
					bt_flip_addr(event_addr, &packet[2]); 
					rfcomm_channel_nr = packet[8];
					rfcomm_channel_id = READ_BT_16(packet, 9);
					printf("RFCOMM channel %u requested for %s\n", rfcomm_channel_nr, bd_addr_to_str(event_addr));
                    rfcomm_accept_connection_internal(rfcomm_channel_id);
					break;
					
				case RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE:
					// data: event(8), len(8), status (8), address (48), server channel(8), rfcomm_cid(16), max frame size(16)
					if (packet[2]) {
						printf("RFCOMM channel open failed, status %u\n", packet[2]);
					} else {
						rfcomm_channel_id = READ_BT_16(packet, 12);
						mtu = READ_BT_16(packet, 14);
						printf("\n\rRFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n", rfcomm_channel_id, mtu);
					}
					break;
                    
                case RFCOMM_EVENT_CHANNEL_CLOSED:
                    rfcomm_channel_id = 0;
                    break;
                
                default:
                    break;
			}
            break;
                        
        default:
            break;
	}
}

static void  heartbeat_handler(struct timer *ts){

    if (rfcomm_channel_id){
        static int counter = 0;
        char lineBuffer[30];
        sprintf(lineBuffer, "BTstack counter %04u\r\n", ++counter);
        printf(lineBuffer);
        int err = rfcomm_send_internal(rfcomm_channel_id, (uint8_t*) lineBuffer, strlen(lineBuffer));
        if (err) {
            printf("rfcomm_send_internal -> error %d\n", err);
        }
    }
    run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    run_loop_add_timer(ts);
} 

// st lib (non-bts) globals:

USB_OTG_CORE_HANDLE USB_OTG_Core;
USBH_HOST USB_Host;
extern USBH_Class_cb_TypeDef USBH_BT_cb;
extern USBH_Usr_cb_TypeDef USR_Callbacks;

// bts ds wrapper for st lib USBH_Process():

static int stlib_usb_process(struct data_source *ds)
{
    USBH_Process(&USB_OTG_Core, &USB_Host);
    return 0;
}

static struct data_source stlib_usb_process_ds = { .process = stlib_usb_process };

// bts hal_{cpu,tick}* stuff inlined:

hci_transport_t * hci_transport_stm_instance(void);

void hal_cpu_disable_irqs(){ __disable_irq(); }
void hal_cpu_enable_irqs(){ __enable_irq(); }
void hal_cpu_enable_irqs_and_sleep(){ __enable_irq(); }

static void dummy_tick_handler(void){};
void (*tick_handler)(void) = &dummy_tick_handler;
void hal_tick_init(void){}
int  hal_tick_get_tick_period_in_ms(void){ return 10; }
void hal_tick_set_handler(void (*handler)(void)){ tick_handler = handler ? handler : dummy_tick_handler; }

int main(void)
{
    RCC_ClocksTypeDef RCC_Clocks;
    RCC_GetClocksFreq(&RCC_Clocks);
    SysTick_Config(RCC_Clocks.HCLK_Frequency * hal_tick_get_tick_period_in_ms() / 1000);

    init_shmem();
    led_pwm_config();

    /// GET STARTED with BTstack ///
    btstack_memory_init();
    run_loop_init(RUN_LOOP_EMBEDDED);

    hci_transport_t    * transport = hci_transport_stm_instance();
    bt_control_t       * control   = 0;
    hci_uart_config_t  * config    = 0;
    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
    hci_init(transport, config, control, remote_db);

    l2cap_init();
    l2cap_register_packet_handler(packet_handler);
    
    rfcomm_init();
    rfcomm_register_packet_handler(packet_handler);
    rfcomm_register_service_internal(NULL, rfcomm_channel_nr, 100);  // reserved channel, mtu=100

    // init SDP, create record for SPP and register with SDP
    sdp_init();
    memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    service_record_item_t * service_record_item = (service_record_item_t *) spp_service_buffer;
    sdp_create_spp_service( (uint8_t*) &service_record_item->service_record, 1, "SPP Counter");
    printf("SDP service buffer size: %u\n", (uint16_t) (sizeof(service_record_item_t) + de_get_len((uint8_t*) &service_record_item->service_record)));
    sdp_register_service_internal(NULL, service_record_item);
    
    // set one-shot timer
    timer_source_t heartbeat;
    heartbeat.process = &heartbeat_handler;
    run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    run_loop_add_timer(&heartbeat);
    
    // enough words...
    //  hci_power_control(HCI_POWER_ON) will be called from USBH_BT_ClassRequest()

    USBH_Init(&USB_OTG_Core, USB_OTG_FS_CORE_ID, &USB_Host, &USBH_BT_cb, &USR_Callbacks);
    run_loop_add_data_source(&stlib_usb_process_ds);
    run_loop_execute();
    return 0;
}
