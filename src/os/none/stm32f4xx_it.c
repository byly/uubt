#include "usb_bsp.h"
#include "usb_hcd_int.h"
#include "my_usb.h"
#include "my_stm32f4_discovery.h"

void      NMI_Handler(void){}
void      SVC_Handler(void){}
void DebugMon_Handler(void){}
void   PendSV_Handler(void){}

void  TIM4_IRQHandler(void){}

void SysTick_Handler(void)
{
    (*tick_handler)();
    led_pwm_isr();
}

void TIM2_IRQHandler(void)
{
    USB_OTG_BSP_TimerIRQ();
}

void OTG_FS_IRQHandler(void)
{
    USBH_OTG_ISR_Handler(&USB_OTG_Core);
}
