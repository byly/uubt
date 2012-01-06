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

#include "ch.h"
#include "hal.h"
#include "usb_bsp.h"
#include "usb_core.h"
#include "usb_hcd_int.h"
#include "my_usb.h"

#define GPIO_AF_OTG_FS 10
#define OTG_FS_Vector  Vector14C

CH_IRQ_HANDLER(OTG_FS_Vector)
{
    CH_IRQ_PROLOGUE();
    chSysLockFromIsr();

    USBH_OTG_ISR_Handler(&USB_OTG_Core);

    chSysUnlockFromIsr();
    CH_IRQ_EPILOGUE();
}

// ISR initiated USB reset:

static Thread *usb_reset_tp;

void USB_OTG_ResetPort_I(USB_OTG_CORE_HANDLE *pdev)
{
    if(usb_reset_tp){
	usb_reset_tp->p_u.rdymsg = (msg_t)pdev;
	chSchReadyI(usb_reset_tp);
	usb_reset_tp = 0;
    }
}

static WORKING_AREA(wa_usb_reset_thread, 128);

static msg_t usb_reset_thread(void *p)
{
    (void)p;
    chRegSetThreadName("usb_reset_thread");
    while(1){
	msg_t msg;
	chSysLock();
	usb_reset_tp = chThdSelf();
	chSchGoSleepS(THD_STATE_SUSPENDED);
	msg = chThdSelf()->p_u.rdymsg;
	chSysUnlock();

	USB_OTG_ResetPort((USB_OTG_CORE_HANDLE *)msg);
    }
    return 0;
}

// timing:

void my_usb_bsp_init(void)
{
    chThdCreateStatic(wa_usb_reset_thread, sizeof(wa_usb_reset_thread), NORMALPRIO, usb_reset_thread, 0);
}

void USB_OTG_BSP_uDelay(const uint32_t usec)	// artifact
{
    volatile int c = usec * 17;
    while(c--);
}

void USB_OTG_BSP_mDelay(const uint32_t msec){ chThdSleepMilliseconds(msec); }


// stm32f4-discovery usb pins, see schematics:
//  A9, A10, A11, A12  = OTG_VBUS_FS, OTG_FS_ID, OTG_FS_DM, OTG_FS_DP
//  C0 = !EN_VBUS, D5 = !OVER_DRAIN
//  notes:
//   OTG_VBUS_FS is probably forgotten to be listed in ds8625, table 6.
//   OVER_DRAIN interrupt currently absent

void USB_OTG_BSP_DriveVBUS(USB_OTG_CORE_HANDLE *pdev, uint8_t state)
{
    state ? palClearPad(GPIOC, 0) : palSetPad(GPIOC, 0);
}

void USB_OTG_BSP_ConfigVBUS(USB_OTG_CORE_HANDLE *pdev)
{
    rccEnableAHB1(RCC_AHB1ENR_GPIOCEN, 0);
    palSetPadMode(GPIOC, 0, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUDR_FLOATING);
    USB_OTG_BSP_DriveVBUS(pdev, 0);
    chThdSleepMilliseconds(200);	// stabilise low during reset
}

void USB_OTG_BSP_EnableInterrupt(USB_OTG_CORE_HANDLE *pdev)
{
    NVICEnableVector(OTG_FS_IRQn, CORTEX_PRIORITY_MASK(STM32_OTG_FS_IRQ_PRIORITY));
}

void USB_OTG_BSP_Init(USB_OTG_CORE_HANDLE *pdev)
{
    rccEnableAHB1(RCC_AHB1ENR_GPIOAEN, 0);

    // follow ST lib USB_OTG_BSP_Init() pin types although it looks a bit irrational:
    palSetGroupMode(GPIOA, PAL_PORT_BIT(9) | PAL_PORT_BIT(11) | PAL_PORT_BIT(12),
	PAL_MODE_ALTERNATE(GPIO_AF_OTG_FS) | PAL_STM32_OTYPE_PUSHPULL  | PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUDR_FLOATING
    );
    palSetPadMode(GPIOA, 10,
	PAL_MODE_ALTERNATE(GPIO_AF_OTG_FS) | PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUDR_PULLUP
    );

    rccEnableAPB2(RCC_APB2ENR_SYSCFGEN, 0);
    rccEnableAHB2(RCC_AHB2ENR_OTGFSEN, 0);

    rccResetAHB2(RCC_AHB2RSTR_OTGFSRST);
}
