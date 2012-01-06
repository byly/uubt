#ifndef __HAL_UB_H
#define __HAL_UB_H

#include <stdint.h>

void hal_ub_reset(void);

void hal_ub_reg_tx_end(void (*tx_end_cb)(uint8_t packet_type));
int  hal_ub_tx_packet(uint8_t packet_type, const uint8_t *packet, uint16_t size);

void hal_ub_reg_rx_packet(void (*rx_cb)(uint8_t packet_type, uint8_t *packet, uint16_t size));

#endif	//__HAL_UB_H
