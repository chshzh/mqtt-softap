#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mqtt_softapwifiprov)

# Include files that are common for all modules
add_subdirectory(src/common)

# Include mandatory module source folders
add_subdirectory(src/modules/trigger)
add_subdirectory(src/modules/sampler)
add_subdirectory(src/modules/network)
add_subdirectory(src/modules/transport)
add_subdirectory(src/modules/error)

# Optional modules
add_subdirectory_ifdef(CONFIG_MQTT_SAMPLE_LED src/modules/ui)

# WiFi provisioning module (conditional)
add_subdirectory_ifdef(CONFIG_SOFTAP_WIFI_PROVISION_MODULE src/modules/wifi_provision)
