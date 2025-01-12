/*
 * lwip_task
 *
 * Copyright (C) 2022 Texas Instruments Incorporated
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/

/* Standard includes. */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Hardware includes. */
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/timer.h"
#include "driverlib/flash.h"
#include "driverlib/interrupt.h"
#include "utils/lwiplib.h"
#include "priorities.h"
/*-----------------------------------------------------------*/

/*
 * The system clock frequency.
 */
extern uint32_t g_ui32SysClock;

/*
 * A flag indicating the current link status.
 */
volatile bool g_bLinkStatusUp = false;

/*
 * Required by lwIP library to support any host-related timer functions.  This
 * function is called periodically, from the lwIP (TCP/IP) timer task context,
 * every "HOST_TMR_INTERVAL" ms (defined in lwipopts.h file).
 */
void
lwIPHostTimerHandler(void)
{
    bool bLinkStatusUp;

    /* Get the current link status and see if it has changed. */
    bLinkStatusUp = GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_0) ? true : false;
    if(bLinkStatusUp != g_bLinkStatusUp)
    {
        /* Save the new link status. */
        g_bLinkStatusUp = bLinkStatusUp;

    }

}

/*
 * Initializes the lwIP tasks.
 */
uint32_t
lwIPTaskInit(void)
{
    uint32_t ui32User0, ui32User1;
    uint8_t pui8MAC[6];

    /* Get the MAC address from the user registers. */
    MAP_FlashUserGet(&ui32User0, &ui32User1);
    if((ui32User0 == 0xffffffff) || (ui32User1 == 0xffffffff))
    {
        return(1);
    }

    /* Convert the 24/24 split MAC address from NV ram into a 32/16 split MAC
     * address needed to program the hardware registers, then program the MAC
     * address into the Ethernet Controller registers. */
    pui8MAC[0] = ((ui32User0 >>  0) & 0xff);
    pui8MAC[1] = ((ui32User0 >>  8) & 0xff);
    pui8MAC[2] = ((ui32User0 >> 16) & 0xff);
    pui8MAC[3] = ((ui32User1 >>  0) & 0xff);
    pui8MAC[4] = ((ui32User1 >>  8) & 0xff);
    pui8MAC[5] = ((ui32User1 >> 16) & 0xff);

    /* Lower the priority of the Ethernet interrupt handler to less than
     * configMAX_SYSCALL_INTERRUPT_PRIORITY.  This is required so that the
     * interrupt handler can safely call the interrupt-safe FreeRTOS functions
     * if any. */
    MAP_IntPrioritySet(INT_EMAC0, ETHERNET_INT_PRIORITY);

    /* Set the link status based on the LED0 signal (which defaults to link
     * status in the PHY). */
    g_bLinkStatusUp = GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_3) ? false : true;

    /* Initialize lwIP. */
    lwIPInit(g_ui32SysClock, pui8MAC, 0, 0, 0, IPADDR_USE_DHCP);

    /* Success. */
    return(0);
}
