
// chibios btstack hal_{cpu,tick}* stuff:

#include "ch.h"
#include "hal.h"
#include <btstack/hal_cpu.h>
#include <btstack/hal_tick.h>

void hal_cpu_disable_irqs(){}
void hal_cpu_enable_irqs(){}
void hal_cpu_enable_irqs_and_sleep(){}

static void dummy_tick_handler(void){};
void (*tick_handler)(void) = &dummy_tick_handler;
void hal_tick_set_handler(void (*handler)(void)){ tick_handler = handler ? handler : dummy_tick_handler; }
int  hal_tick_get_tick_period_in_ms(void){ return 1000 / CH_FREQUENCY; }	// 1 (F=1000) is suspected of instability

VirtualTimer vt;

static void tick_wrapper(void *p)
{
    chVTSetI(&vt, 1, tick_wrapper, p);
    tick_handler();
}

void hal_tick_init(void)
{
    chSysLock();
    chVTSetI(&vt, 1, tick_wrapper, 0);
    chSysUnlock();
}
