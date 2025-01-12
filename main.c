/*
 * enet_io
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

/******************************************************************************
 *
 * This example application demonstrates the operation of the Tiva
 * Ethernet controller using the lwIP TCP/IP Stack.  DHCP is used to obtain
 * an Ethernet address.  If DHCP times out without obtaining an address,
 * AutoIP will be used to obtain a link-local address.  The address that is
 * selected will be shown on the UART.
 *
 * UART0, connected to the ICDI virtual COM port and running at 115,200,
 * 8-N-1, is used to display messages from this application. Use the
 * following command to re-build the any file system files that change.
 *
 *     ../../../../tools/bin/makefsfile -i fs -o io_fsdata.h -r -h -q
 *
 * FreeRTOS is used to perform a variety of tasks in a concurrent fashion.
 * The following tasks are created:
 * * An Ethernet task to manage the Ethernet interface and its interrupt.
 * * A TCP/IP task to run the lwIP stack and manage all the TCP/IP packets.
 *   This task works very closely with the Ethernet task to server web pages,
 *   handle Telnet packets and Locator app packets.  Ethernet and TCP/IP tasks
 *   are managed by lwIP.
 * * An idle task (automatically created by FreeRTOS) that manages changes to
 *   the IP address and sends this information to the user via Debug UART.
 *
 * To build this application, install TivaWare for C Series v2.2.0.295
 * and copy the "enet_lwip_freertos" application folder into the EK-TM4C1294XL
 * board's folder at <TivaWare_Install_Folder>/examples/boards/ek-tm4c1294xl.
 *
 * The IP address and other debug messages are displayed on the Debug UART.
 * UART0, connected to the ICDI virtual COM port, is used as the Debug UART.
 * 115,200 baud with 8-N-1 settings is used to display debug messages from
 * this application.
 *
 * The finder application (in ../../../../tools/bin/) can also be used to
 * discover the IP address of the board.  The finder application will search
 * the network for all boards that respond to its requests and display
 * information about them.
 *
 * To access the webserver, enter the IP address in a web browser.
 *
 * The following command
 * can be used to re-build any file system files that change.
 *
 *   ../../../../tools/bin/makefsfile -i fs -o io_fsdata.h -r -h -q
 *
 * For additional details on FreeRTOS, refer to the FreeRTOS web page at:
 * http://www.freertos.org/
 *
 * For additional details on lwIP, refer to the lwIP web page at:
 * http://savannah.nongnu.org/projects/lwip/
 *
 */

/* Standard includes. */
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* Hardware includes. */
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_types.h"
#include "driverlib/flash.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "utils/swupdate.h"
#include "utils/ustdlib.h"
#include "utils/lwiplib.h"

#include "drivers/rtos_hw_drivers.h"
/*-----------------------------------------------------------*/

/* The system clock frequency. */
uint32_t g_ui32SysClock;

/* Set up the hardware. */
static void prvSetupHardware( void );

/* API to trigger the lwIP task. */
extern uint32_t lwIPTaskInit(void);

extern void time_task_init(void);
extern void lcd_task_init(void);
extern void alarm_task_init(void);
/*-----------------------------------------------------------*/

int main( void )
{

    prvSetupHardware();

    /* Initialize the Ethernet peripheral and create the lwIP tasks. */
    if(lwIPTaskInit() != 0)
    {
        for( ;; );
    }

    time_task_init();
    lcd_task_init();
    alarm_task_init();

    /* Start the scheduler.  This should not return. */
    vTaskStartScheduler();

    for( ;; );
}
/*-----------------------------------------------------------*/

static void prvSetupHardware( void )
{
    /* Run from the PLL at configCPU_CLOCK_HZ MHz. */
    g_ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
            SYSCTL_OSC_MAIN | SYSCTL_USE_PLL |
            SYSCTL_CFG_VCO_240), configCPU_CLOCK_HZ);

    /* Configure device hardware.*/
    PinoutSet();

    /* Enable global interrupts in the NVIC. */
    IntMasterEnable();
}
/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
    /* vApplicationMallocFailedHook() will only be called if
    configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
    function that will get called if a call to pvPortMalloc() fails.
    pvPortMalloc() is called internally by the kernel whenever a task, queue,
    timer or semaphore is created.  It is also called by various parts of the
    demo application.  If heap_1.c or heap_2.c are used, then the size of the
    heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
    FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
    to query the size of free heap space that remains (although it does not
    provide information on how the remaining heap might be fragmented). */
    IntMasterDisable();
    for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    ( void ) pcTaskName;
    ( void ) pxTask;

    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
    function is called if a stack overflow is detected. */
    IntMasterDisable();
    for( ;; );
}
/*-----------------------------------------------------------*/

void *malloc( size_t xSize )
{
    /* There should not be a heap defined, so trap any attempts to call
    malloc. */
    IntMasterDisable();
    for( ;; );
}
/*-----------------------------------------------------------*/
