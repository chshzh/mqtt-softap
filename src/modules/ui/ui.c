/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/zbus/zbus.h>

#include "message_channel.h"
#include <net/softap_wifi_provision.h>
#include <zephyr/net/wifi_credentials.h>

LOG_MODULE_REGISTER(ui, LOG_LEVEL_INF);

/* Device tree nodes */
#define LED_1_NODE DT_ALIAS(led0)
#define LED_2_NODE DT_ALIAS(led1)
#define BUTTON_1_NODE DT_ALIAS(sw0)
#define BUTTON_2_NODE DT_ALIAS(sw1)

/* LED GPIO specifications */
#if DT_NODE_HAS_STATUS(LED_1_NODE, okay)
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED_1_NODE, gpios);
#endif

#if DT_NODE_HAS_STATUS(LED_2_NODE, okay) && IS_ENABLED(CONFIG_SOFTAP_WIFI_PROVISION_MODULE)
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED_2_NODE, gpios);
#endif

/* Button GPIO specifications */
#if DT_NODE_HAS_STATUS(BUTTON_1_NODE, okay)
static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET(BUTTON_1_NODE, gpios);
static struct gpio_callback button1_cb_data;
#endif

#if DT_NODE_HAS_STATUS(BUTTON_2_NODE, okay) && IS_ENABLED(CONFIG_SOFTAP_WIFI_PROVISION_MODULE)
static const struct gpio_dt_spec button2 = GPIO_DT_SPEC_GET(BUTTON_2_NODE, gpios);
static struct gpio_callback button2_cb_data;
#endif

/* Work queue for safe button handling */
static struct k_work_q ui_work_q;
static K_THREAD_STACK_DEFINE(ui_work_stack, 2048);

/* Work items for button handling */
#if DT_NODE_HAS_STATUS(BUTTON_1_NODE, okay)
static struct k_work button1_work;
#endif

#if DT_NODE_HAS_STATUS(BUTTON_2_NODE, okay) && IS_ENABLED(CONFIG_SOFTAP_WIFI_PROVISION_MODULE)
static struct k_work button2_work;
#endif

/* LED 2 blink work */
#if DT_NODE_HAS_STATUS(LED_2_NODE, okay) && IS_ENABLED(CONFIG_SOFTAP_WIFI_PROVISION_MODULE)
static struct k_work_delayable led2_blink_work;
static bool led2_blinking = false;
static uint32_t led2_blink_period_ms = 0;
static bool led2_state = false;
#endif

/* UI state tracking */
static enum network_status current_network_status = NETWORK_DISCONNECTED;
static enum provisioning_status current_provisioning_status = PROVISIONING_NOT_STARTED;
static bool mqtt_connected = false;

/* Forward declarations */
#if DT_NODE_HAS_STATUS(BUTTON_1_NODE, okay)
extern bool mqtt_helper_is_connected(void);
#endif

#if DT_NODE_HAS_STATUS(BUTTON_2_NODE, okay) && IS_ENABLED(CONFIG_SOFTAP_WIFI_PROVISION_MODULE)
extern int softap_wifi_provision_reset(void);
#endif

/* LED control functions */
#if DT_NODE_HAS_STATUS(LED_1_NODE, okay)
static void led1_set(bool on)
{
	gpio_pin_set_dt(&led1, on);
}
#endif

#if DT_NODE_HAS_STATUS(LED_2_NODE, okay) && IS_ENABLED(CONFIG_SOFTAP_WIFI_PROVISION_MODULE)
static void led2_set(bool on)
{
	gpio_pin_set_dt(&led2, on);
	led2_state = on;
}

static void led2_blink_work_fn(struct k_work *work)
{
	if (!led2_blinking) {
		return;
	}

	/* Toggle LED state */
	led2_set(!led2_state);

	/* Reschedule */
	k_work_reschedule(&led2_blink_work, K_MSEC(led2_blink_period_ms));
}

static void led2_start_blink(uint32_t period_ms)
{
	led2_blinking = true;
	led2_blink_period_ms = period_ms;
	k_work_reschedule(&led2_blink_work, K_MSEC(period_ms));
}

static void led2_stop_blink(bool final_state)
{
	led2_blinking = false;
	k_work_cancel_delayable(&led2_blink_work);
	led2_set(final_state);
}
#endif

/* Update LED states based on current status */
static void update_led_states(void)
{
#if DT_NODE_HAS_STATUS(LED_1_NODE, okay)
	/* LED 1: Network status - ON when internet connectivity established */
	led1_set(mqtt_connected);
#endif

#if DT_NODE_HAS_STATUS(LED_2_NODE, okay) && IS_ENABLED(CONFIG_SOFTAP_WIFI_PROVISION_MODULE)
	/* LED 2: Provisioning status */
	switch (current_provisioning_status) {
	case PROVISIONING_NOT_STARTED:
		/* Fast blink: Waiting for WiFi connection */
		led2_start_blink(200);
		break;
	
	case PROVISIONING_IN_PROGRESS:
		/* Slow blink: Provisioning in progress (SoftAP mode) */
		led2_start_blink(1000);
		break;
	
	case PROVISIONING_COMPLETED:
		if (current_network_status == NETWORK_CONNECTED) {
			/* Solid ON: Connected to provisioned WiFi */
			led2_stop_blink(true);
		} else {
			/* Fast blink: Provisioning done but not connected yet */
			led2_start_blink(200);
		}
		break;
	}
#endif
}

/* Button work handlers */
#if DT_NODE_HAS_STATUS(BUTTON_1_NODE, okay)
static void button1_work_fn(struct k_work *work)
{
	if (!mqtt_connected) {
		LOG_WRN("Button 1 pressed but MQTT not connected");
		return;
	}

	/* Create and publish button press message */
	struct payload button_payload;
	snprintk(button_payload.string, sizeof(button_payload.string), 
		 "Button 1 pressed at %lld", k_uptime_get());

	LOG_INF("Button 1 pressed - publishing MQTT message");
	
	/* Publish via payload channel to transport module */
	int ret = zbus_chan_pub(&PAYLOAD_CHAN, &button_payload, K_SECONDS(1));
	if (ret) {
		LOG_ERR("Failed to publish button payload: %d", ret);
	}
}

static void button1_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	/* Schedule work to handle button press safely */
	k_work_submit_to_queue(&ui_work_q, &button1_work);
}
#endif

#if DT_NODE_HAS_STATUS(BUTTON_2_NODE, okay) && IS_ENABLED(CONFIG_SOFTAP_WIFI_PROVISION_MODULE)
static void button2_work_fn(struct k_work *work)
{
	LOG_INF("Button 2 pressed - resetting WiFi credentials and restarting provisioning");
	
	/* Check if credentials exist before reset */
	bool credentials_before = !wifi_credentials_is_empty();
	LOG_INF("WiFi credentials exist before reset: %s", credentials_before ? "YES" : "NO");
	
	/* Reset WiFi credentials directly - this is more reliable than using the async API */
	int ret = wifi_credentials_delete_all();
	if (ret) {
		LOG_ERR("Failed to delete WiFi credentials directly: %d", ret);
		return;
	}
	
	LOG_INF("WiFi credentials deleted directly");
	
	/* Check if credentials exist after reset */
	bool credentials_after = !wifi_credentials_is_empty();
	LOG_INF("WiFi credentials exist after reset: %s", credentials_after ? "YES" : "NO");
	
	LOG_INF("WiFi credentials reset, rebooting for reprovisioning...");
	/* Reboot to restart provisioning */
	sys_reboot(SYS_REBOOT_COLD);
}

static void button2_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	/* Schedule work to handle button press safely */
	k_work_submit_to_queue(&ui_work_q, &button2_work);
}
#endif

/* Initialize GPIOs */
static int leds_init(void)
{
	int ret = 0;

#if DT_NODE_HAS_STATUS(LED_1_NODE, okay)
	if (!gpio_is_ready_dt(&led1)) {
		LOG_ERR("LED 1 device not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		LOG_ERR("Failed to configure LED 1: %d", ret);
		return ret;
	}
	LOG_INF("LED 1 initialized (network status)");
#endif

#if DT_NODE_HAS_STATUS(LED_2_NODE, okay) && IS_ENABLED(CONFIG_SOFTAP_WIFI_PROVISION_MODULE)
	if (!gpio_is_ready_dt(&led2)) {
		LOG_ERR("LED 2 device not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		LOG_ERR("Failed to configure LED 2: %d", ret);
		return ret;
	}

	/* Initialize LED 2 blink work */
	k_work_init_delayable(&led2_blink_work, led2_blink_work_fn);
	LOG_INF("LED 2 initialized (provisioning status)");
#endif

	return ret;
}

static int buttons_init(void)
{
	int ret = 0;

#if DT_NODE_HAS_STATUS(BUTTON_1_NODE, okay)
	if (!gpio_is_ready_dt(&button1)) {
		LOG_ERR("Button 1 device not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&button1, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("Failed to configure button 1: %d", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&button1, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		LOG_ERR("Failed to configure button 1 interrupt: %d", ret);
		return ret;
	}

	gpio_init_callback(&button1_cb_data, button1_pressed, BIT(button1.pin));
	gpio_add_callback(button1.port, &button1_cb_data);

	/* Initialize button 1 work */
	k_work_init(&button1_work, button1_work_fn);
	LOG_INF("Button 1 initialized (MQTT publish)");
#endif

#if DT_NODE_HAS_STATUS(BUTTON_2_NODE, okay) && IS_ENABLED(CONFIG_SOFTAP_WIFI_PROVISION_MODULE)
	if (!gpio_is_ready_dt(&button2)) {
		LOG_ERR("Button 2 device not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&button2, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("Failed to configure button 2: %d", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&button2, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		LOG_ERR("Failed to configure button 2 interrupt: %d", ret);
		return ret;
	}

	gpio_init_callback(&button2_cb_data, button2_pressed, BIT(button2.pin));
	gpio_add_callback(button2.port, &button2_cb_data);

	/* Initialize button 2 work */
	k_work_init(&button2_work, button2_work_fn);
	LOG_INF("Button 2 initialized (credential reset)");
#endif

	return ret;
}

/* ZBus message handlers */
static void network_status_handler(const struct zbus_channel *chan)
{
	enum network_status status;
	
	int ret = zbus_chan_read(chan, &status, K_MSEC(100));
	if (ret) {
		LOG_ERR("Failed to read network status: %d", ret);
		return;
	}

	if (current_network_status != status) {
		current_network_status = status;
		LOG_INF("Network status changed to: %d", status);
		update_led_states();
	}
}

static void provisioning_status_handler(const struct zbus_channel *chan)
{
	enum provisioning_status status;
	
	int ret = zbus_chan_read(chan, &status, K_MSEC(100));
	if (ret) {
		LOG_ERR("Failed to read provisioning status: %d", ret);
		return;
	}

	if (current_provisioning_status != status) {
		current_provisioning_status = status;
		LOG_INF("Provisioning status changed to: %d", status);
		update_led_states();
	}
}

static void transport_status_handler(const struct zbus_channel *chan)
{
	enum transport_status status;
	
	int ret = zbus_chan_read(chan, &status, K_MSEC(100));
	if (ret) {
		LOG_ERR("Failed to read transport status: %d", ret);
		return;
	}

	bool new_mqtt_state = (status == TRANSPORT_CONNECTED);
	if (mqtt_connected != new_mqtt_state) {
		mqtt_connected = new_mqtt_state;
		LOG_INF("MQTT connection status changed to: %s", mqtt_connected ? "connected" : "disconnected");
		update_led_states();
	}
}

/* ZBus subscribers */
ZBUS_SUBSCRIBER_DEFINE(ui, 4);

static void ui_task(void)
{
	int ret;
	const struct zbus_channel *chan;

	LOG_INF("UI module started");

	/* Initialize work queue */
	k_work_queue_init(&ui_work_q);
	k_work_queue_start(&ui_work_q, ui_work_stack,
			   K_THREAD_STACK_SIZEOF(ui_work_stack),
			   K_HIGHEST_APPLICATION_THREAD_PRIO, NULL);

	/* Initialize GPIOs */
	ret = leds_init();
	if (ret) {
		LOG_ERR("Failed to initialize LEDs: %d", ret);
		return;
	}

	ret = buttons_init();
	if (ret) {
		LOG_ERR("Failed to initialize buttons: %d", ret);
		/* Continue without buttons */
	}

	/* Set initial LED states */
	update_led_states();

	/* Main event loop - wait for messages on the UI subscriber */
	while (!zbus_sub_wait(&ui, &chan, K_FOREVER)) {
		
		if (chan == &NETWORK_CHAN) {
			network_status_handler(chan);
		} else if (chan == &PROVISIONING_CHAN) {
			provisioning_status_handler(chan);
		} else if (chan == &TRANSPORT_CHAN) {
			transport_status_handler(chan);
		}
	}
}

/* Start UI module thread */
K_THREAD_DEFINE(ui_thread, 4096, ui_task, NULL, NULL, NULL, 5, 0, 0); 