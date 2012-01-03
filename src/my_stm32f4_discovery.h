
		// *** My convenience defs for stm32f4 discovery board. Byly Nov-2011.  ***//

#ifndef __MY_STM32F4_DISCOVERY_H
#define __MY_STM32F4_DISCOVERY_H

#ifdef __SHMEM_BOARD_SIDE
  #include "shmem.h"
  #include "printf.h"
#else
  #include <stdio.h>
  #define init_shmem()
#endif

#ifdef BLINK_LEDS

#include "stm32f4_discovery.h"
#define LED_R LED5
#define LED_G LED4
#define LED_B LED6
#define LED_O LED3

void led_pwm_config(void);
void led_pwm_ctl(int led, int en);
void led_pwm_isr_init(void);
void led_pwm_isr(void);
void hal_arm_blinker(int led);

#else

#define led_pwm_config()
#define led_pwm_ctl(led, en)
#define led_pwm_isr_init()
#define led_pwm_isr()
#define hal_arm_blinker(led)

#endif

#endif	//__MY_STM32F4_DISCOVERY_H
