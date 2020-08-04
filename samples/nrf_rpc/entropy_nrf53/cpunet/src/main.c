/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>

#if 1
#include <SEGGER_RTT.h>
uint8_t rtt_buffer[8192];
static SEGGER_RTT_BUFFER_UP *up;
static void initialize(void)
{
	int i;

	up = &_SEGGER_RTT.aUp[1];
	up->sName = "test_rtt";
	up->pBuffer = (char *)(&rtt_buffer[0]);
	up->SizeOfBuffer = sizeof(rtt_buffer);
	//up->RdOff = 0u;
	//up->WrOff = 0u;
	up->Flags = SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL;
	for (i = 0; i < sizeof(rtt_buffer); i++)
	{
		rtt_buffer[i] = i;
	}
}
#endif

void main(void)
{
	/* The only activity of this application is interaction with the APP
	 * core using serialized communication through the nRF RPC library.
	 * The necessary handlers are registered through nRF RPC interface
	 * and start at system boot.
	 */
	printk("Entropy sample started[NET Core].\n");
	uint32_t p;

	printk("Remote init send\n");

	initialize();
	while (1) {
		p = up->RdOff;
		up->WrOff = (p > 0) ? p - 1 : sizeof(rtt_buffer) - 1;
		__asm__ volatile ("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n");
	}

}
