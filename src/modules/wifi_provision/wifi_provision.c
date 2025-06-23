/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/wifi_credentials.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_linkaddr.h>
#include <zephyr/zbus/zbus.h>
#include <net/softap_wifi_provision.h>

#include "wifi_provision.h"
#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(wifi_provision, CONFIG_MQTT_SAMPLE_WIFI_PROVISION_LOG_LEVEL);

/* WiFi provisioning state */
static bool provisioning_active = false;
static bool provisioning_completed = false;

/* Network management event callback structures */
static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback net_cb;

/* Forward declarations */
static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				    uint32_t mgmt_event, struct net_if *iface);
static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				   uint32_t mgmt_event, struct net_if *iface);

/* WiFi provisioning event handler */
static void provisioning_event_handler(const struct softap_wifi_provision_evt *evt)
{
	switch (evt->type) {
	case SOFTAP_WIFI_PROVISION_EVT_STARTED:
		LOG_INF("WiFi provisioning started - entering SoftAP mode");
		provisioning_active = true;
		provisioning_completed = false;
		break;

	case SOFTAP_WIFI_PROVISION_EVT_COMPLETED:
		LOG_INF("WiFi provisioning completed - switching from SoftAP to station mode");
		provisioning_active = false;
		provisioning_completed = true;
		
		/* Register NET management event handlers after provisioning completion */
		LOG_INF("Registering NET management event handlers after provisioning completion");
		net_mgmt_init_event_callback(&net_cb, net_mgmt_event_handler,
					     NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED);
		net_mgmt_add_event_callback(&net_cb);
		LOG_DBG("NET management event handlers registered successfully");
		break;

	case SOFTAP_WIFI_PROVISION_EVT_FATAL_ERROR:
		LOG_ERR("WiFi provisioning fatal error");
		provisioning_active = false;
		provisioning_completed = false;
		SEND_FATAL_ERROR();
		break;

	default:
		LOG_WRN("Unknown provisioning event: %d", evt->type);
		break;
	}

	LOG_DBG("Provisioning state: active=%d, completed=%d", 
		provisioning_active, provisioning_completed);
}

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				    uint32_t mgmt_event, struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		LOG_DBG("WiFi connect result event");
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		LOG_DBG("WiFi disconnect result event");
		break;
	default:
		break;
	}
}

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				   uint32_t mgmt_event, struct net_if *iface)
{
	int err;
	enum network_status status;

	/* Filter out events during active provisioning (SoftAP mode) */
	if (provisioning_active) {
		LOG_DBG("Ignoring network event during active provisioning: %u", mgmt_event);
		return;
	}

	/* Filter events from non-WiFi interfaces */
	if (iface && (net_if_get_link_addr(iface)->type != NET_LINK_ETHERNET)) {
		LOG_DBG("Ignoring event from non-WiFi interface");
		return;
	}

	switch (mgmt_event) {
	case NET_EVENT_L4_CONNECTED:
		LOG_INF("Network connected after provisioning");
		status = NETWORK_CONNECTED;
		break;
	case NET_EVENT_L4_DISCONNECTED:
		LOG_INF("Network disconnected");
		status = NETWORK_DISCONNECTED;
		break;
	default:
		/* Don't care about other events */
		return;
	}

	/* Notify the network module */
	err = zbus_chan_pub(&NETWORK_CHAN, &status, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	} else {
		LOG_INF("Network connected after provisioning - notified network module");
	}
}

int wifi_provision_init(void)
{
	int err;

	LOG_INF("WiFi provisioning initialized, event handlers prepared but not registered");

	/* Initialize WiFi management event callbacks (always register these) */
	net_mgmt_init_event_callback(&wifi_cb, wifi_mgmt_event_handler,
				     NET_EVENT_WIFI_CONNECT_RESULT |
				     NET_EVENT_WIFI_DISCONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_cb);

	/* Initialize SoftAP WiFi provisioning */
	err = softap_wifi_provision_init(provisioning_event_handler);
	if (err) {
		LOG_ERR("softap_wifi_provision_init failed: %d", err);
		return err;
	}

	return 0;
}

int wifi_provision_start(void)
{
	int err;
	bool credentials_empty;

	LOG_INF("Starting WiFi provisioning");

	/* Check if WiFi credentials already exist */
	credentials_empty = wifi_credentials_is_empty();

	if (!credentials_empty) {
		LOG_INF("Wi-Fi credentials found, skipping provisioning");
		provisioning_completed = true;
		
		/* Register NET management event handlers immediately */
		LOG_INF("Registering NET management event handlers (provisioning skipped)");
		net_mgmt_init_event_callback(&net_cb, net_mgmt_event_handler,
					     NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED);
		net_mgmt_add_event_callback(&net_cb);
		
		return 0; /* Credentials exist, no provisioning needed */
	}

	/* Start provisioning */
	err = softap_wifi_provision_start();
	if (err) {
		LOG_ERR("softap_wifi_provision_start failed: %d", err);
		return err;
	}

	return 1; /* Provisioning started */
}

int wifi_provision_reset(void)
{
	int err;

	LOG_INF("Resetting WiFi credentials");

	err = wifi_credentials_delete_all();
	if (err) {
		LOG_ERR("Failed to delete WiFi credentials: %d", err);
		return err;
	}

	provisioning_active = false;
	provisioning_completed = false;

	LOG_INF("WiFi credentials reset successfully");
	return 0;
}

bool wifi_provision_is_active(void)
{
	return provisioning_active;
}

bool wifi_provision_is_completed(void)
{
	return provisioning_completed;
} 