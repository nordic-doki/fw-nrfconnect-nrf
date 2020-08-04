/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>

#include <nrf_rpc.h>

#include "entropy_ser.h"

#define BUFFER_LENGTH 10

static u8_t buffer[BUFFER_LENGTH];

static void result_callback(int result, u8_t *buffer, size_t length)
{
	size_t i;

	if (result) {
		printk("Entropy remote get failed: %d\n", result);
		return;
	}

	for (i = 0; i < length; i++) {
		printk("  0x%02x", buffer[i]);
	}

	printk("\n");
}

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
	up->RdOff = 0u;
	up->WrOff = 0u;
	up->Flags = SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL;
	for (i = 0; i < sizeof(rtt_buffer); i++)
	{
		rtt_buffer[i] = i;
	}
}


void main(void)
{
	int err;
	uint32_t p;

	printk("Entropy sample started[APP Core].\n");

	/*err = entropy_remote_init();
	if (err) {
		printk("Remote entropy driver initialization failed\n");
		return;
	}*/

	printk("Remote init send\n");

	initialize();
	while (1) {
		p = up->RdOff;
		up->WrOff = (p > 0) ? p - 1 : sizeof(rtt_buffer) - 1;
		//k_sleep(K_MSEC(1));
		__asm__ volatile ("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n");
	}

	while (true) {
		k_sleep(K_MSEC(2000));

		err = entropy_remote_get(buffer, sizeof(buffer));
		if (err) {
			printk("Entropy remote get failed: %d\n", err);
			continue;
		}

		for (int i = 0; i < BUFFER_LENGTH; i++) {
			printk("  0x%02x", buffer[i]);
		}

		printk("\n");

		k_sleep(K_MSEC(2000));

		err = entropy_remote_get_inline(buffer, sizeof(buffer));
		if (err) {
			printk("Entropy remote get failed: %d\n", err);
			continue;
		}

		for (int i = 0; i < BUFFER_LENGTH; i++) {
			printk("  0x%02x", buffer[i]);
		}

		printk("\n");

		k_sleep(K_MSEC(2000));

		err = entropy_remote_get_async(sizeof(buffer), result_callback);
		if (err) {
			printk("Entropy remote get async failed: %d\n", err);
			continue;
		}

		k_sleep(K_MSEC(2000));

		err = entropy_remote_get_cbk(sizeof(buffer), result_callback);
		if (err) {
			printk("Entropy remote get callback failed: %d\n", err);
			continue;
		}
	}
}
