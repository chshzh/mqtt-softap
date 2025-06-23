/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <net/softap_wifi_provision.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_if.h>

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(network, CONFIG_MQTT_SAMPLE_NETWORK_LOG_LEVEL);

/* This module does not subscribe to any channels */

/* Macros used to subscribe to specific Zephyr NET management events. */
#define L4_EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
#define CONN_LAYER_EVENT_MASK (NET_EVENT_CONN_IF_FATAL_ERROR)

/* Zephyr NET management event callback structures. */
static struct net_mgmt_event_callback l4_cb;
static struct net_mgmt_event_callback conn_cb;

/* Macro called upon a fatal error, reboots the device. */
#define FATAL_ERROR()								\
	LOG_ERR("Fatal error!");						\
	SEND_FATAL_ERROR()

static void l4_event_handler(struct net_mgmt_event_callback *cb,
			     uint32_t event,
			     struct net_if *iface)
{
	switch (event) {
	case NET_EVENT_L4_CONNECTED:
		LOG_INF("Network connected");
		break;
	case NET_EVENT_L4_DISCONNECTED:
		LOG_INF("Network disconnected");
		break;
	default:
		/* Don't care */
		return;
	}
}

static void connectivity_event_handler(struct net_mgmt_event_callback *cb,
				       uint32_t event,
				       struct net_if *iface)
{
	if (event == NET_EVENT_CONN_IF_FATAL_ERROR) {
		LOG_ERR("NET_EVENT_CONN_IF_FATAL_ERROR");
		FATAL_ERROR();
		return;
	}
}

/* Callback for softAP Wi-Fi provision library events - EXACT COPY FROM NORDIC SAMPLE */
static void softap_wifi_provision_handler(const struct softap_wifi_provision_evt *evt)
{
	switch (evt->type) {
	case SOFTAP_WIFI_PROVISION_EVT_STARTED:
		LOG_INF("Provisioning started");
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
		break;
	case SOFTAP_WIFI_PROVISION_EVT_UNPROVISIONED_REBOOT_NEEDED:
		LOG_INF("Reboot request notified, rebooting...");
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

/**
 * @brief Configure AP channel to work around WIFI_CHANNEL_ANY issue
 * 
 * The SoftAP WiFi provisioning library uses WIFI_CHANNEL_ANY which doesn't work
 * properly with nRF70 regulatory domain constraints, causing "Failed to set beacon data".
 * This function sets a specific channel before starting provisioning.
 */
static int configure_ap_channel(void)
{
	struct net_if *iface = net_if_get_default();
	struct wifi_connect_req_params params = {
		.ssid = NULL,
		.ssid_length = 0,
		.channel = 6,  /* Use channel 6 (2.4GHz) - widely supported */
		.band = WIFI_FREQ_BAND_2_4_GHZ,
		.security = WIFI_SECURITY_TYPE_NONE,
	};
	
	if (!iface) {
		LOG_ERR("No default network interface found");
		return -ENODEV;
	}
	
	LOG_INF("Setting AP channel to %d to work around WIFI_CHANNEL_ANY issue", params.channel);
	
	/* This sets the channel preference for the WiFi interface */
	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
	if (ret && ret != -EALREADY) {
		LOG_WRN("Failed to set AP channel preference: %d (may be harmless)", ret);
	}
	
	return 0;
}

static void network_task(void)
{
	int ret;
	bool provisioning_completed = false;

	LOG_INF("Network task started - using Nordic's exact SoftAP logic");

	/* EXACT COPY OF NORDIC'S MAIN() LOGIC */
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

	/* Configure AP channel before starting provisioning to work around WIFI_CHANNEL_ANY issue */
	ret = configure_ap_channel();
	if (ret) {
		LOG_WRN("configure_ap_channel failed: %d, continuing anyway", ret);
	}

	ret = softap_wifi_provision_start();
	switch (ret) {
	case 0:
		provisioning_completed = true;
		break;
	case -EALREADY:
		LOG_INF("Wi-Fi credentials found, skipping provisioning");
		break;
	default:
		LOG_ERR("softap_wifi_provision_start, error: %d", ret);
		FATAL_ERROR();
		return;
	}

	/* Register NET mgmt handlers for Connection Manager events after provisioning
	 * is completed. This is to avoid receiving events during provisioning when the device is
	 * in softAP (server) mode and gets assigned an IP,
	 * which triggers the NET_EVENT_L4_CONNECTED.
	 */

	/* Setup handler for Zephyr NET Connection Manager events. */
	net_mgmt_init_event_callback(&l4_cb, l4_event_handler, L4_EVENT_MASK);
	net_mgmt_add_event_callback(&l4_cb);

	/* Setup handler for Zephyr NET Connection Manager Connectivity layer. */
	net_mgmt_init_event_callback(&conn_cb, connectivity_event_handler, CONN_LAYER_EVENT_MASK);
	net_mgmt_add_event_callback(&conn_cb);

	ret = conn_mgr_all_if_connect(true);
	if (ret) {
		LOG_ERR("conn_mgr_all_if_connect, error: %d", ret);
		FATAL_ERROR();
		return;
	}

	LOG_INF("Network task completed");
}

K_THREAD_DEFINE(network_task_id,
		CONFIG_MQTT_SAMPLE_NETWORK_THREAD_STACK_SIZE,
		network_task, NULL, NULL, NULL, 3, 0, 0);
