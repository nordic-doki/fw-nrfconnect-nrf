/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RP_LOG_H_
#define RP_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <logging/log.h>
#define _NRF_RPC_LOG_REGISTER2(module) LOG_MODULE_REGISTER(module, CONFIG_ ## module ## _LOG_LEVEL)
#define _NRF_RPC_LOG_REGISTER1(module) _NRF_RPC_LOG_REGISTER2(module)
_NRF_RPC_LOG_REGISTER1(NRF_RPC_LOG_MODULE);

/**
 * @brief Macro for logging a message with the severity level ERR.
 *
 * @param ... printf-style format string, optionally followed by arguments
 *            to be formatted and inserted in the resulting string.
 */
#define NRF_RPC_ERR(...)  LOG_ERR(__VA_ARGS__)

/**
 * @brief Macro for logging a message with the severity level WRN.
 *
 * @param ... printf-style format string, optionally followed by arguments
 *            to be formatted and inserted in the resulting string.
 */
#define NRF_RPC_WRN(...)  LOG_WRN(__VA_ARGS__)

/**
 * @brief Macro for logging a message with the severity level INF.
 *
 * @param ... printf-style format string, optionally followed by arguments
 *            to be formatted and inserted in the resulting string.
 */
#define NRF_RPC_INF(...)  LOG_INF(__VA_ARGS__)

/**
 * @brief Macro for logging a message with the severity level DBG.
 *
 * @param ... printf-style format string, optionally followed by arguments
 *            to be formatted and inserted in the resulting string.
 */
#define NRF_RPC_DBG(...)  LOG_DBG(__VA_ARGS__)

/**
 * @brief Macro for logging a memory dump with the severity level ERR.
 *
 * @param[in] memory Pointer to the memory region to be dumped.
 * @param[in] length Length of the memory region in bytes.
 * @param[in] text   Text describing the dump.
 */
#define NRF_RPC_DUMP_ERR(memory, length, text) \
	LOG_HEXDUMP_ERR(memory, length, text)

/**
 * @brief Macro for logging a memory dump with the severity level WRN.
 *
 * @param[in] memory Pointer to the memory region to be dumped.
 * @param[in] length Length of the memory region in bytes.
 * @param[in] text   Text describing the dump.
 */
#define NRF_RPC_DUMP_WRN(memory, length, text) \
	LOG_HEXDUMP_WRN(memory, length, text)

/**
 * @brief Macro for logging a memory dump with the severity level INF.
 *
 * @param[in] memory Pointer to the memory region to be dumped.
 * @param[in] length Length of the memory region in bytes.
 * @param[in] text   Text describing the dump.
 */
#define NRF_RPC_DUMP_INF(memory, length, text) \
	LOG_HEXDUMP_INF(memory, length, text)

/**
 * @brief Macro for logging a memory dump with the severity level DBG.
 *
 * @param[in] memory Pointer to the memory region to be dumped.
 * @param[in] length Length of the memory region in bytes.
 * @param[in] text   Text describing the dump.
 */
#define NRF_RPC_DUMP_DBG(memory, length, text) \
	LOG_HEXDUMP_DBG(memory, length, text)

/**
 *@}
 */

#ifdef __cplusplus
}
#endif

#endif /* RP_LOG_H_ */
