/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef SER_COMMON_H_
#define SER_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

enum ser_command {
	SER_COMMAND_NUS_INIT = 0x01,
	SER_COMMAND_NUS_SEND = 0x02
};

enum ser_evt {
	SER_EVT_CONNECTED = 0x01,
	SER_EVT_DISCONNECTED = 0x02,
	SER_EVT_NUS_RECEIVED = 0x03
};


#ifdef __cplusplus
}
#endif

#endif /* SER_COMMON_H_ */