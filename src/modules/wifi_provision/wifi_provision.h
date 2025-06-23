/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _WIFI_PROVISION_H_
#define _WIFI_PROVISION_H_

#include <zephyr/kernel.h>
#include <stdbool.h>

/**
 * @brief WiFi provision events
 */
enum wifi_provision_event {
	WIFI_PROVISION_EVT_STARTED,
	WIFI_PROVISION_EVT_COMPLETED,
	WIFI_PROVISION_EVT_FAILED,
};

/**
 * @brief Initialize WiFi provisioning
 *
 * @return 0 on success, negative error code on failure
 */
int wifi_provision_init(void);

/**
 * @brief Start WiFi provisioning process
 *
 * This function will check if WiFi credentials are already stored.
 * If not, it will start the SoftAP provisioning process.
 *
 * @return 0 if credentials exist and device is ready to connect
 * @return 1 if provisioning was started
 * @return negative error code on failure
 */
int wifi_provision_start(void);

/**
 * @brief Reset stored WiFi credentials
 *
 * @return 0 on success, negative error code on failure
 */
int wifi_provision_reset(void);

/**
 * @brief Check if WiFi provisioning is currently active
 *
 * @return true if provisioning is active, false otherwise
 */
bool wifi_provision_is_active(void);

/**
 * @brief Check if WiFi provisioning has completed
 *
 * @return true if provisioning is completed, false otherwise
 */
bool wifi_provision_is_completed(void);

#endif /* _WIFI_PROVISION_H_ */ 