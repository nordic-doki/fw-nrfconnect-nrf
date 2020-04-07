#
# Copyright (c) 2020 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

zephyr_include_directories(include)
zephyr_include_directories(transport)

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_NRF_RPC_TR_RPMSG transport/nrf_rpc_rpmsg.c)
zephyr_library_sources_ifdef(CONFIG_NRF_RPC_TR_RPMSG transport/rp_ll.c)

