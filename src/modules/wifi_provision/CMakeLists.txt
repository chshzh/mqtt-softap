#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

# Include common headers
target_include_directories(app PRIVATE ../../common)

target_sources(app PRIVATE wifi_provision.c) 