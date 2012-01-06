
// my_usb and btstack hal_{cpu,tick}* stuff:

#include "my_usb.h"
#include "usb_bsp.h"
#include <btstack/hal_cpu.h>
#include <btstack/hal_tick.h>
#include "stm32f4xx.h"
#include "stm32f4xx_rcc.h"

// hal_{cpu,tick}*

void hal_cpu_disable_irqs(){ __disable_irq(); }
void hal_cpu_enable_irqs(){ __enable_irq(); }
void hal_cpu_enable_irqs_and_sleep(){ __enable_irq(); }

static void dummy_tick_handler(void){};
void (*tick_handler)(void) = &dummy_tick_handler;
void hal_tick_set_handler(void (*handler)(void)){ tick_handler = handler ? handler : dummy_tick_handler; }
void hal_tick_init(void){}
int  hal_tick_get_tick_period_in_ms(void){ return 10; }

// my_usb

void my_usb_bsp_init()
{
    RCC_ClocksTypeDef RCC_Clocks;
    RCC_GetClocksFreq(&RCC_Clocks);
    SysTick_Config(RCC_Clocks.HCLK_Frequency * hal_tick_get_tick_period_in_ms() / 1000);
}

void USB_OTG_ResetPort_I(USB_OTG_CORE_HANDLE *pdev){ USB_OTG_ResetPort(pdev); }
