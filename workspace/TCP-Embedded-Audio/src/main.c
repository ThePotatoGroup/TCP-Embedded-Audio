/*
 * Copyright (c) 2007-2009 Xilinx, Inc.  All rights reserved.
 *
 * Xilinx, Inc.
 * XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A
 * COURTESY TO YOU.  BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS
 * ONE POSSIBLE   IMPLEMENTATION OF THIS FEATURE, APPLICATION OR
 * STANDARD, XILINX IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION
 * IS FREE FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE
 * FOR OBTAINING ANY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.
 * XILINX EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO
 * THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO
 * ANY WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE
 * FROM CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *@file main.c
 *
 *@brief
 *  - Test audio loopback
 *
 * 1. Cofigure I2C controller to communicate with codec
 * 2. reads file via JTAG
 * 3. Playback the audio data
 * 4. transfer data using DMA ch.4
 *
 * Target:   TLL6537v1-1
 * Compiler: VDSP++     Output format: VDSP++ "*.dxe"
 *
 * @author    Rohan Kangralkar, ECE, Northeastern University  (03/11/09)
 * @date 03/15/2009
 *
 * LastChange:
 * $Id: main.c 984 2016-03-15 14:29:21Z schirner $
 *
 *******************************************************************************/


// socketapps includes
#define SOCKETS_DEBUG                   LWIP_DBG_ON
#include <stdio.h>
#if __MICROBLAZE__ || __PPC__
#include "xmk.h"
#include "sys/timer.h"
#include "xenv_standalone.h"
#endif
#include "xparameters.h"
#include "lwipopts_custom.h"
#define SOCKETS_DEBUG (LWIP_DBG_LEVEL_SEVERE | LWIP_DBG_ON)
//#define SOCKETS_DEBUG                   LWIP_DBG_OFF
#include "platform_config.h"
#include "platform.h"

#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/init.h"
#include "netif/xadapter.h"
#include "lwip/dhcp.h"
#include "config_apps.h"

#include "lwip/inet.h"
#include "lwip/ip_addr.h"

// Audio Lab includes
#include "audioSample.h"
#include "audioPlayer.h"
#include "bufferPool_d.h"
#include "zedboard_freertos.h"
#include "adau1761.h"



// Audio Lab #define's
#define VOLUME_MIN (0x2F)
#define numChunks 561
#define chunkSize 511


// Coniditoinal socketapps includes and socketapps function declarations
#if __arm__
#include "task.h"
#include "portmacro.h"
#include "xil_printf.h"
int main_thread();
#endif
//void print_headers();
void launch_app_threads();
void tcp_client(void* pThisArg);

/******************************************************************************
 *                     GLOBALS
 *****************************************************************************/
 
audioPlayer_t            audioPlayer;

struct netif server_netif;

/**************************************************************
 *                        MAIN                                *
**************************************************************/
int main(void)
{
	// Start up audio stuff
	printf("[MAIN]: Starting Audio Player\r\n");

//    audioPlayer_init(&audioPlayer);
//    audioPlayer_start(&audioPlayer);


	if (init_platform() < 0) {
        xil_printf("ERROR initializing platform.\r\n");
        return -1;
    }

    sys_thread_new("main_thrd", (void(*)(void*))main_thread, 0,
                120,
                DEFAULT_THREAD_PRIO);
	vTaskStartScheduler();
    while(1);

    return 0;
}


// Function Defintions from socketapps

void print_ip(char *msg, struct ip_addr *ip)
{
    print(msg);
    xil_printf("%d.%d.%d.%d\r\n", ip4_addr1(ip), ip4_addr2(ip),
            ip4_addr3(ip), ip4_addr4(ip));
}

void print_ip_settings(struct ip_addr *ip, struct ip_addr *mask, struct ip_addr *gw)
{

    print_ip("Board IP: ", ip);
    print_ip("Netmask : ", mask);
    print_ip("Gateway : ", gw);
}


void network_thread(void *p)
{
    struct netif *netif;
    struct ip_addr ipaddr, netmask, gw;

    /* the mac address of the board. this should be unique per board */
    unsigned char mac_ethernet_address[] = { 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

    netif = &server_netif;

    /* initliaze IP addresses to be used */
    IP4_ADDR(&ipaddr,  192, 168,   1, 10);
    IP4_ADDR(&netmask, 255, 255, 255,  0);
    IP4_ADDR(&gw,      192, 168,   1,  1);

    /* print out IP settings of the board */
    print("\r\n\r\n");
    print("-----lwIP Socket Mode Demo Application ------\r\n");

    print_ip_settings(&ipaddr, &netmask, &gw);
    /* print all application headers */

    /* Add network interface to the netif_list, and set it as default */
    if (!xemac_add(netif, &ipaddr, &netmask, &gw, mac_ethernet_address, PLATFORM_EMAC_BASEADDR)) {
        xil_printf("Error adding N/W interface\r\n");
        return;
    }
    netif_set_default(netif);

    /* specify that the network if is up */
    netif_set_up(netif);

    /* start packet receive thread - required for lwIP operation */
    sys_thread_new("xemacif_input_thread", (void(*)(void*))xemacif_input_thread, netif,
            THREAD_STACKSIZE,
            DEFAULT_THREAD_PRIO);

	// print_headers();
    
    sys_thread_new("TCP_CLIENT", tcp_client, (void*)&(audioPlayer.codec),
    		THREAD_STACKSIZE,
            DEFAULT_THREAD_PRIO);
			
	vTaskDelete(NULL);
    return;
}

int main_thread()
{
	/* initialize lwIP before calling sys_thread_new */
    lwip_init();

    /* any thread using lwIP should be created using sys_thread_new */
    sys_thread_new("NW_THRD", network_thread, NULL,
            THREAD_STACKSIZE,
            DEFAULT_THREAD_PRIO);
			
    return 0;
}

// Function definitions from audio lab (also represented in socketapps)

void vApplicationMallocFailedHook( void )
{
	/* vApplicationMallocFailedHook() will only be called if
	configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
	function that will get called if a call to pvPortMalloc() fails.
	pvPortMalloc() is called internally by the kernel whenever a task, queue or
	semaphore is created.  It is also called by various parts of the demo
	application.  If heap_1.c or heap_2.c are used, then the size of the heap
	available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
	FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
	to query the size of free heap space that remains (although it does not
	provide information on how the remaining heap might be fragmented). */
	xil_printf("Memory Allocation Error\r\n");
	taskDISABLE_INTERRUPTS();
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( xTaskHandle *pxTask, signed char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* vApplicationStackOverflowHook() will only be called if
	configCHECK_FOR_STACK_OVERFLOW is set to either 1 or 2.  The handle and name
	of the offending task will be passed into the hook function via its
	parameters.  However, when a stack has overflowed, it is possible that the
	parameters will have been corrupted, in which case the pxCurrentTCB variable
	can be inspected directly. */
	xil_printf("Stack Overflow in %s\r\n", pcTaskName);
	taskDISABLE_INTERRUPTS();
	for( ;; );
}
void vApplicationSetupHardware( void )
{

}

