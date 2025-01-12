//*****************************************************************************
//
// priorities.h - Priorities for the various FreeRTOS tasks.
//
// Copyright (c) 2014-2015 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 2.1.1.71 of the EK-TM4C1294XL Firmware Package.
//
//*****************************************************************************

#ifndef __PRIORITIES_H__
#define __PRIORITIES_H__

//*****************************************************************************
//
// Interrupt priorities.  Since the interrupt handlers (of these interrupts)
// use FreeRTOS APIs, the interrupt priorities should be between
// configKERNEL_INTERRUPT_PRIORITY and configMAX_SYSCALL_INTERRUPT_PRIORITY.
//
//*****************************************************************************
#define ETHERNET_INT_PRIORITY   0xC0
#define GPIO_PK_INT_PRIORITY    0xA0

//*****************************************************************************
//
// The priorities of the various tasks.  Note that PRIORITY_ETH_INT_TASK and
// PRIORITY_TCPIP_TASK are not used in this project but are placed here to
// indicate the priorities of all tasks in one place.  The Ethernet task and
// TCP/IP task are created by lwIP.
//
//*****************************************************************************
#define PRIORITY_TIME_TASK      4
#define PRIORITY_LCD_TASK       5
#define PRIORITY_ALARM_TASK     6
#define PRIORITY_ETH_INT_TASK   1
#define PRIORITY_TCPIP_TASK     3

#endif // __PRIORITIES_H__
