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

// stm32 usb hci transport adapted from:

/*
 *  hci_h4_transport_dma.c
 *
 *  HCI Transport implementation for basic H4 protocol for blocking UART write and IRQ-driven blockwise RX
 *
 *  Created by Matthias Ringwald on 4/29/09.
 */

// This is a thin interface between btstack and stm32 usb.

#include <stdio.h>
#include <string.h>

#include "hci.h"
#include <btstack/run_loop.h>

#include "hal_uubt.h"

static void arm_blinker(int rx, uint8_t packet_type)
{
#ifdef BLINK_LEDS
    static int colormap[9] = {
	[ 0+HCI_COMMAND_DATA_PACKET ] = LED_G,
	[ 4+HCI_EVENT_PACKET ]        = LED_R,
	[ 0+HCI_ACL_DATA_PACKET ]     = LED_O,
	[ 4+HCI_ACL_DATA_PACKET ]     = LED_B,
    };
    hal_arm_blinker(colormap[(rx ? 4 : 0) + packet_type]);
#endif
}

static void dump(int rx, uint8_t packet_type, uint8_t *data, uint16_t len)
{
#ifdef PRINT_PACKETS
    int i;
    printf("%s: (%02x) ", rx ? "rx" : "tx", packet_type);
    for (i=0; i<len; i++){
        printf("%02x ", data[i]);
    }
    printf("\n");
#endif
}

static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){}

static void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = dummy_handler;

static int tx_busy = 0;

static int h2_can_send_packet_now(uint8_t packet_type){ return !tx_busy; } 

static int h2_send_packet(uint8_t packet_type, uint8_t *packet, int size)
{
    if(!h2_can_send_packet_now(packet_type)){
        return -1;
    }
    tx_busy = 1;
    dump(0, packet_type, packet, size);
    return hal_ub_tx_packet(packet_type, packet, size);
}

static void h2_tx_end_cb(uint8_t packet_type)
{
    uint8_t event = DAEMON_EVENT_HCI_PACKET_SENT;
    packet_handler(HCI_EVENT_PACKET, &event, 1);
    tx_busy = 0;
    embedded_trigger();
    arm_blinker(0, packet_type);
}

static void h2_rx_cb(uint8_t packet_type, uint8_t *packet, uint16_t size)
{
    dump(1, packet_type, packet, size);
    packet_handler(packet_type, packet, size);
    embedded_trigger();
    arm_blinker(1, packet_type);
}

static int h2_open(void *transport_config){
    hal_ub_reset();
    tx_busy = 0;
    hal_ub_reg_tx_end(h2_tx_end_cb);
    hal_ub_reg_rx_packet(h2_rx_cb);
    return 0;
}

static int h2_close(void *transport_config){ return 0; }

static void h2_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    packet_handler = handler;
}

static const char * h2_get_transport_name(void){ return "UUBT"; }

static hci_transport_t hci_transport_stm = {
    .open                          = h2_open,
    .close                         = h2_close,
    .send_packet                   = h2_send_packet,
    .register_packet_handler       = h2_register_packet_handler,
    .get_transport_name            = h2_get_transport_name,
    .can_send_packet_now           = h2_can_send_packet_now
};

hci_transport_t * hci_transport_stm_instance(void){ return &hci_transport_stm; }
