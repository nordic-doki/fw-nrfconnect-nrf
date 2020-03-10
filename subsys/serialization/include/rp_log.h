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

#define RP_MODULE_PREFIX  _CONCAT(RP_, RP_LOG_MODULE)
/*
 * The following macros from nrfx_config control the log messages coming from
 * a given module:
 * - RP <module>_CONFIG_LOG_ENABLED enables the messages (when set to 1)
 * - RP <module>_CONFIG_LOG_LEVEL specifies the severity level of the messages
 *   that are to be output.
 */
#if !IS_ENABLED(_CONCAT(RP_MODULE_PREFIX, _CONFIG_LOG_ENABLED))
#define RP_MODULE_CONFIG_LOG_LEVEL 0
#else
#define RP_MODULE_CONFIG_LOG_LEVEL \
	_CONCAT(RP_MODULE_PREFIX, _CONFIG_LOG_LEVEL)
#endif

#if	RP_MODULE_CONFIG_LOG_LEVEL == 0
#define RP_MODULE_LOG_LEVEL		LOG_LEVEL_NONE
#elif	RP_MODULE_CONFIG_LOG_LEVEL == 1
#define RP_MODULE_LOG_LEVEL		LOG_LEVEL_ERR
#elif	RP_MODULE_CONFIG_LOG_LEVEL == 2
#define RP_MODULE_LOG_LEVEL		LOG_LEVEL_WRN
#elif	RP_MODULE_CONFIG_LOG_LEVEL == 3
#define RP_MODULE_LOG_LEVEL		LOG_LEVEL_INF
#elif	RP_MODULE_CONFIG_LOG_LEVEL == 4
#define RP_MODULE_LOG_LEVEL		LOG_LEVEL_DBG
#endif
#include <logging/log.h>
LOG_MODULE_REGISTER(RP_MODULE_PREFIX, RP_MODULE_LOG_LEVEL);

/**
 * @brief Macro for logging a message with the severity level ERR.
 *
 * @param ... printf-style format string, optionally followed by arguments
 *            to be formatted and inserted in the resulting string.
 */
#define RP_LOG_ERR(...)  LOG_ERR(__VA_ARGS__)

/**
 * @brief Macro for logging a message with the severity level WRN.
 *
 * @param ... printf-style format string, optionally followed by arguments
 *            to be formatted and inserted in the resulting string.
 */
#define RP_LOG_WRN(...)  LOG_WRN(__VA_ARGS__)

/**
 * @brief Macro for logging a message with the severity level INF.
 *
 * @param ... printf-style format string, optionally followed by arguments
 *            to be formatted and inserted in the resulting string.
 */
#define RP_LOG_INF(...)  LOG_INF(__VA_ARGS__)

/**
 * @brief Macro for logging a message with the severity level DBG.
 *
 * @param ... printf-style format string, optionally followed by arguments
 *            to be formatted and inserted in the resulting string.
 */
#define RP_LOG_DBG(...)  LOG_DBG(__VA_ARGS__)

/**
 * @brief Macro for logging a memory dump with the severity level ERR.
 *
 * @param[in] p_memory Pointer to the memory region to be dumped.
 * @param[in] length   Length of the memory region in bytes.
 */
#define RP_LOG_HEXDUMP_ERR(p_memory, length) \
	LOG_HEXDUMP_ERR(p_memory, length, "")

/**
 * @brief Macro for logging a memory dump with the severity level WRN.
 *
 * @param[in] p_memory Pointer to the memory region to be dumped.
 * @param[in] length   Length of the memory region in bytes.
 */
#define RP_LOG_HEXDUMP_WRN(p_memory, length) \
	LOG_HEXDUMP_WRN(p_memory, length, "")

/**
 * @brief Macro for logging a memory dump with the severity level INF.
 *
 * @param[in] p_memory Pointer to the memory region to be dumped.
 * @param[in] length   Length of the memory region in bytes.
 */
#define RP_LOG_HEXDUMP_INF(p_memory, length) \
	LOG_HEXDUMP_INF(p_memory, length, "")

/**
 * @brief Macro for logging a memory dump with the severity level DBG.
 *
 * @param[in] p_memory Pointer to the memory region to be dumped.
 * @param[in] length   Length of the memory region in bytes.
 */
#define RP_LOG_HEXDUMP_DBG(p_memory, length) \
	LOG_HEXDUMP_DBG(p_memory, length, "")

/**
 *@}
 */

#ifdef __cplusplus
}
#endif

#endif /* RP_LOG_H_ */
