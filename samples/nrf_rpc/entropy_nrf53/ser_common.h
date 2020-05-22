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

enum rpc_command {
	RPC_COMMAND_ENTROPY_INIT = 0x01,
	RPC_COMMAND_ENTROPY_GET = 0x02,
};

enum rpc_event {
	RPC_EVENT_ENTROPY_GET_ASYNC = 0x03,
	RPC_EVENT_ENTROPY_GET_ASYNC_RESULT = 0x04,
};

#ifdef __cplusplus
}
#endif

#endif /* SER_COMMON_H_ */
