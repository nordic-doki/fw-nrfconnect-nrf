#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_include_directories(include)

zephyr_sources(cbkproxy.c)
zephyr_sources(serialize.c)
zephyr_sources(bt_rpc_common.c)
