/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>

#include <nrf_rpc.h>

#include "entropy_ser.h"

#define BUFFER_LENGTH 10

/*static u8_t buffer[BUFFER_LENGTH];

static void async_callback(u8_t* buffer, size_t length)
{
	size_t i;

	printk("async");

	for (i = 0; i < length; i++) {
		printk("  0x%02x", buffer[i]);
	}

	printk("\n");
}*/

void main(void)
{
	int err;

	printk("Entropy sample started[APP Core].\n");

	err = entropy_remote_init();
	if (err) {
		printk("Remote entropy driver initialization failed\n");
		return;
	}

	while (true) {
		/*k_sleep(K_MSEC(1000));
		
		err = entropy_remote_get(buffer, sizeof(buffer));
		if (err) {
			printk("Entropy remote get failed: %d\n", err);
			continue;
		}

		printk("sync ");

		for (int i = 0; i < BUFFER_LENGTH; i++) {
			printk("  0x%02x", buffer[i]);
		}

		printk("\n");

		k_sleep(K_MSEC(1000));

		err = entropy_remote_get_async(sizeof(buffer), async_callback);
		if (err) {
			printk("Entropy remote get async failed: %d\n", err);
			continue;
		}*/
	}
}
