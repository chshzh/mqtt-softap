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
#include <zephyr/net/dhcpv4.h>

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(network, CONFIG_MQTT_SAMPLE_NETWORK_LOG_LEVEL);

/* Subscribe to provisioning channel if wifi provisioning is enabled */
#if IS_ENABLED(CONFIG_SOFTAP_WIFI_PROVISION_MODULE)
static bool provisioning_completed = false;
static bool provisioning_in_progress = false;
ZBUS_SUBSCRIBER_DEFINE(network, 4);

static void provisioning_status_handler(const struct zbus_channel *chan)
{
	int err;
	enum provisioning_status status;

	err = zbus_chan_read(chan, &status, K_MSEC(500));
	if (err) {
		LOG_ERR("Failed to read provisioning status: %d", err);
		return;
	}

	switch (status) {
	case PROVISIONING_IN_PROGRESS:
		LOG_INF("Provisioning in progress, blocking network events");
		provisioning_in_progress = true;
		break;
	case PROVISIONING_COMPLETED:
		LOG_INF("Provisioning completed, starting network connection");
		provisioning_completed = true;
		provisioning_in_progress = false;
		break;
	default:
		break;
	}
}
#endif

/* Macros used to subscribe to specific Zephyr NET management events. */
#define L4_EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
#define CONN_LAYER_EVENT_MASK (NET_EVENT_CONN_IF_FATAL_ERROR)

/* Zephyr NET management event callback structures. */
static struct net_mgmt_event_callback l4_cb;
static struct net_mgmt_event_callback conn_cb;

static void l4_event_handler(struct net_mgmt_event_callback *cb,
			     uint32_t event,
			     struct net_if *iface)
{
	int err;
	enum network_status status;

	switch (event) {
	case NET_EVENT_L4_CONNECTED:
#if IS_ENABLED(CONFIG_SOFTAP_WIFI_PROVISION_MODULE)
		if (provisioning_in_progress) {
			LOG_INF("L4 connected during provisioning (SoftAP mode) - not publishing network event");
			return;
		}
#endif
		LOG_INF("Network connectivity established");
		status = NETWORK_CONNECTED;
		
		/* Start the DHCPv4 client after connecting to the network.
		 * This is needed to get a dynamic IPv4 address from the AP's DHCPv4 server.
		 */
		net_dhcpv4_start(iface);
		break;
	case NET_EVENT_L4_DISCONNECTED:
#if IS_ENABLED(CONFIG_SOFTAP_WIFI_PROVISION_MODULE)
		if (provisioning_in_progress) {
			LOG_INF("L4 disconnected during provisioning - not publishing network event");
			return;
		}
#endif
		LOG_INF("Network connectivity lost");
		status = NETWORK_DISCONNECTED;
		break;
	default:
		/* Don't care */
		return;
	}

	err = zbus_chan_pub(&NETWORK_CHAN, &status, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	}
}

static void connectivity_event_handler(struct net_mgmt_event_callback *cb,
				       uint32_t event,
				       struct net_if *iface)
{
	if (event == NET_EVENT_CONN_IF_FATAL_ERROR) {
		LOG_ERR("NET_EVENT_CONN_IF_FATAL_ERROR");
		SEND_FATAL_ERROR();
		return;
	}
}

static void network_task(void)
{
	int err;

	/* Setup handler for Zephyr NET Connection Manager events. */
	net_mgmt_init_event_callback(&l4_cb, l4_event_handler, L4_EVENT_MASK);
	net_mgmt_add_event_callback(&l4_cb);

	/* Setup handler for Zephyr NET Connection Manager Connectivity layer. */
	net_mgmt_init_event_callback(&conn_cb, connectivity_event_handler, CONN_LAYER_EVENT_MASK);
	net_mgmt_add_event_callback(&conn_cb);

#if IS_ENABLED(CONFIG_SOFTAP_WIFI_PROVISION_MODULE)
	/* If wifi provisioning is enabled, wait for it to complete before connecting */
	LOG_INF("Waiting for WiFi provisioning to complete");
	
	while (!provisioning_completed) {
		/* Check for provisioning channel messages */
		const struct zbus_channel *chan;
		if (zbus_sub_wait(&network, &chan, K_MSEC(1000)) == 0) {
			provisioning_status_handler(chan);
		}
	}
	
	/* After provisioning is complete, connect to the network */
	err = conn_mgr_all_if_connect(true);
	if (err) {
		LOG_ERR("conn_mgr_all_if_connect, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
#endif

	/* Connecting to the configured connectivity layer.
	 * Wi-Fi or LTE depending on the board that the sample was built for.
	 */
	LOG_INF("Bringing network interface up and connecting to the network");

	err = conn_mgr_all_if_up(true);
	if (err) {
		LOG_ERR("conn_mgr_all_if_up, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

#if !IS_ENABLED(CONFIG_SOFTAP_WIFI_PROVISION_MODULE)
	/* Only connect immediately if wifi provisioning is not enabled */
	err = conn_mgr_all_if_connect(true);
	if (err) {
		LOG_ERR("conn_mgr_all_if_connect, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
#endif

	/* Resend connection status if the sample is built for Native Sim.
	 * This is necessary because the network interface is automatically brought up
	 * at SYS_INIT() before main() is called.
	 * This means that NET_EVENT_L4_CONNECTED fires before the
	 * appropriate handler l4_event_handler() is registered.
	 */
	if (IS_ENABLED(CONFIG_BOARD_NATIVE_SIM)) {
		conn_mgr_mon_resend_status();
	}
}

K_THREAD_DEFINE(network_task_id,
		CONFIG_MQTT_SAMPLE_NETWORK_THREAD_STACK_SIZE,
		network_task, NULL, NULL, NULL, 3, 0, 0);
