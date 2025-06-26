/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/drivers/gpio.h>
#include <net/softap_wifi_provision.h>
#include <zephyr/zbus/zbus.h>
#include "message_channel.h"

LOG_MODULE_REGISTER(wifi_provision, CONFIG_SOFTAP_WIFI_PROVISION_MODULE_LOG_LEVEL);



/* This module does not subscribe to any channels */

/* WiFi provisioning module - network events handled by network module */

/* Macro called upon a fatal error, reboots the device. */
#define FATAL_ERROR()								\
	LOG_ERR("Fatal error!%s", IS_ENABLED(CONFIG_RESET_ON_FATAL_ERROR) ?	\
				  " Rebooting the device" : "");		\
	LOG_PANIC();								\
	IF_ENABLED(CONFIG_REBOOT, (sys_reboot(0)))

static bool wifi_provisioned = false;

/* Network event handlers removed - handled by network module */

/* Callback for softAP Wi-Fi provision library events. */
static void softap_wifi_provision_handler(const struct softap_wifi_provision_evt *evt)
{
	int ret;
	enum provisioning_status status;

	switch (evt->type) {
	case SOFTAP_WIFI_PROVISION_EVT_STARTED:
		LOG_INF("Provisioning started");
		status = PROVISIONING_IN_PROGRESS;
		ret = zbus_chan_pub(&PROVISIONING_CHAN, &status, K_SECONDS(1));
		if (ret) {
			LOG_ERR("Failed to publish provisioning status: %d", ret);
		}
		break;

	case SOFTAP_WIFI_PROVISION_EVT_CLIENT_CONNECTED:
		LOG_INF("Client connected");
		break;

	case SOFTAP_WIFI_PROVISION_EVT_CLIENT_DISCONNECTED:
		LOG_INF("Client disconnected");
		break;

	case SOFTAP_WIFI_PROVISION_EVT_CREDENTIALS_RECEIVED:
		LOG_INF("Wi-Fi credentials received");
		break;

	case SOFTAP_WIFI_PROVISION_EVT_COMPLETED:
		LOG_INF("Provisioning completed");
		wifi_provisioned = true;
		status = PROVISIONING_COMPLETED;
		ret = zbus_chan_pub(&PROVISIONING_CHAN, &status, K_SECONDS(1));
		if (ret) {
			LOG_ERR("Failed to publish provisioning completion: %d", ret);
		}
		LOG_INF("Network event callbacks registered");
		break;

	case SOFTAP_WIFI_PROVISION_EVT_UNPROVISIONED_REBOOT_NEEDED:
		LOG_INF("Reboot request notified, rebooting...");
		LOG_PANIC();
		IF_ENABLED(CONFIG_REBOOT, (sys_reboot(0)));
		break;

	case SOFTAP_WIFI_PROVISION_EVT_FATAL_ERROR:
		LOG_ERR("Provisioning failed, fatal error!");
		FATAL_ERROR();
		break;

	default:
		/* Don't care */
		return;
	}
}

static int wifi_power_saving_disable(void)
{
	int ret;
	struct net_if *iface = net_if_get_first_wifi();
	struct wifi_ps_params params = {
		.enabled = WIFI_PS_DISABLED
	};

	ret = net_mgmt(NET_REQUEST_WIFI_PS, iface, &params, sizeof(params));
	if (ret) {
		LOG_ERR("Failed to disable PSM, error: %d", ret);
		FATAL_ERROR();
		return ret;
	}

	return 0;
}

static int wifi_power_saving_enable(void)
{
	int ret;
	struct net_if *iface = net_if_get_first_wifi();
	struct wifi_ps_params params = {
		.enabled = WIFI_PS_ENABLED
	};

	ret = net_mgmt(NET_REQUEST_WIFI_PS, iface, &params, sizeof(params));
	if (ret) {
		LOG_ERR("Failed to enable PSM, error: %d", ret);
		FATAL_ERROR();
		return ret;
	}

	return 0;
}

/* Function used to disable and re-enable PSM after a configured amount of time post provisioning.
 * This is to ensure that the device is discoverable via mDNS so that clients can
 * confirm that provisioning succeeded. This is needed due to mDNS SD being unstable
 * in Power Save Mode.
 */
static void psm_set(void)
{
	int ret;

	ret = wifi_power_saving_disable();
	if (ret) {
		LOG_ERR("wifi_power_saving_disable, error: %d", ret);
		FATAL_ERROR();
		return;
	}

	LOG_INF("PSM disabled");

	k_sleep(K_SECONDS(CONFIG_SOFTAP_WIFI_PROVISION_MODULE_PSM_DISABLED_SECONDS));

	ret = wifi_power_saving_enable();
	if (ret) {
		LOG_ERR("wifi_power_saving_enable, error: %d", ret);
		FATAL_ERROR();		
		return;
	}

	LOG_INF("PSM enabled");
}

static void wifi_provision_task(void)
{
	int ret;
	bool provisioning_completed = false;

	LOG_INF("SoftAP Wi-Fi provision sample started");

	/* Publish initial provisioning status for LED indication */
	enum provisioning_status initial_status = PROVISIONING_NOT_STARTED;
	ret = zbus_chan_pub(&PROVISIONING_CHAN, &initial_status, K_SECONDS(1));
	if (ret) {
		LOG_ERR("Failed to publish initial provisioning status: %d", ret);
	}



	ret = softap_wifi_provision_init(softap_wifi_provision_handler);
	if (ret) {
		LOG_ERR("softap_wifi_provision_init, error: %d", ret);
		FATAL_ERROR();
		return;
	}

	ret = conn_mgr_all_if_up(true);
	if (ret) {
		LOG_ERR("conn_mgr_all_if_up, error: %d", ret);
		FATAL_ERROR();
		return;
	}

	LOG_INF("Network interface brought up");

	ret = softap_wifi_provision_start();
	switch (ret) {
	case 0:
		provisioning_completed = true;
		break;
	case -EALREADY:
		LOG_INF("Wi-Fi credentials found, skipping provisioning");
		provisioning_completed = true;
		/* Notify network module that provisioning is complete */
		enum provisioning_status status = PROVISIONING_COMPLETED;
		ret = zbus_chan_pub(&PROVISIONING_CHAN, &status, K_SECONDS(1));
		if (ret) {
			LOG_ERR("Failed to publish provisioning completion: %d", ret);
		}
		break;
	default:
		LOG_ERR("softap_wifi_provision_start, error: %d", ret);
		FATAL_ERROR();
		return;
	}

	/* Network connection will be handled by the network module after
	 * it receives the provisioning completion notification */

	if (provisioning_completed) {
		psm_set();
	}
    return;
}

// Set higher priority than network module 
K_THREAD_DEFINE(wifi_provision_thread, 8192, wifi_provision_task, NULL, NULL, NULL,
		2, 0, 0);
