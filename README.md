# MQTT with SoftAP WiFi Provisioning

This sample demonstrates how to combine MQTT connectivity with SoftAP-based WiFi provisioning on nRF70 Series devices. The sample starts with WiFi provisioning using SoftAP mode, allowing mobile clients to connect and provide WiFi credentials. Once provisioned, the device connects to the configured WiFi network and starts MQTT operations.

## Table of Contents

- [Requirements](#requirements)
- [Overview](#overview)
- [Architecture](#architecture)
- [User Interface](#user-interface)
- [Building and Running](#building-and-running)
- [Testing](#testing)
- [Configuration](#configuration)
- [WiFi Provisioning Details](#wifi-provisioning-details)
- [MQTT Operations](#mqtt-operations)
- [Dependencies](#dependencies)
- [Troubleshooting](#troubleshooting)

## Requirements

The sample supports the following development kits:

- **nRF7002 DK** (nrf7002dk/nrf5340/cpuapp and nrf7002dk/nrf5340/cpuapp/ns)

### Prerequisites

- Nordic Connect SDK v3.0.2 or later
- nRF Wi-Fi Provisioner mobile app ([Android](https://play.google.com/store/apps/details?id=no.nordicsemi.android.wifi.provisioner) | [iOS](https://apps.apple.com/app/nrf-wi-fi-provisioner/id1638948698))
- WiFi network with internet access
- MQTT broker (default: `test.mosquitto.org`)

## Overview

The sample combines two main functionalities:

1. **WiFi Provisioning**: Uses SoftAP mode to allow mobile clients to provision WiFi credentials
2. **MQTT Communication**: Connects to an MQTT broker to publish and receive messages

After boot, the device starts advertising in SoftAP mode with the SSID name set by the `CONFIG_SOFTAP_WIFI_PROVISION_MODULE_SSID` option (default is *nrf-wifiprov*), allowing clients (STA) to connect to it and provide Wi-Fi credentials. Once provisioned, the device automatically connects to the configured WiFi network and begins MQTT operations.

### Key Features

- **Automatic WiFi Provisioning**: Device creates a SoftAP for credential provisioning
- **MQTT Operations**: Publishes and subscribes to MQTT topics
- **TLS Support**: Optional encrypted MQTT communication
- **Modular Architecture**: Clean separation of concerns with dedicated modules
- **LED Indicators**: Visual feedback for device state
- **Button Controls**: Manual credential reset functionality
- **Persistent Storage**: WiFi credentials stored securely in flash

## Architecture

The sample is organized into several modules:

```
src/
├── common/           # Shared utilities and message channels
└── modules/
    ├── trigger/      # Event triggering and timing
    ├── sampler/      # Data sampling and collection
    ├── network/      # Network connectivity management
    ├── transport/    # MQTT transport layer
    ├── error/        # Error handling and reporting
    ├── led/          # LED status indicators (optional)
    └── wifi_provision/  # WiFi provisioning via SoftAP
        ├── certs/    # TLS certificates
        └── scripts/  # Provisioning scripts
```

### Module Communication

Modules communicate through a ZBus-based message channel system, providing:
- **Decoupled Architecture**: Modules are independent and testable
- **Event-Driven Design**: Reactive system based on message passing
- **Scalability**: Easy to add new modules or modify existing ones

## User Interface

### LEDs

- **LED 1**: Network connectivity status. Turns **on** when connected to internet (MQTT-ready), **off** when disconnected.
- **LED 2**: WiFi provisioning status:
  - **Fast blink** (200ms): Waiting for WiFi connection (initial state)
  - **Slow blink** (1000ms): Provisioning in progress (SoftAP mode active)
  - **Solid on**: Connected to provisioned WiFi network

### Buttons
- **Button 1** (sw0): Publish test MQTT message when MQTT broker is connected.
- **Button 2** (sw1): Reset stored WiFi credentials and restart provisioning (device reboots).

## Building and Running

### Basic Build Commands

For **nRF7002 DK (secure)** - ✅ **Tested and Working**:
```bash
west build -p -b nrf7002dk/nrf5340/cpuapp/ns -- -DEXTRA_CONF_FILE=overlay-softap-wifiprov-nrf70.conf
```

For **nRF7002 DK (non-secure)** - ✅ **Tested and Working**:
```bash
west build -p -b nrf7002dk/nrf5340/cpuapp -- -DEXTRA_CONF_FILE=overlay-softap-wifiprov-nrf70.conf
```

### With TLS Support

For TLS-enabled builds:
```bash
# Non-secure with TLS
west build -p -b nrf7002dk/nrf5340/cpuapp/ns -- -DEXTRA_CONF_FILE="overlay-softap-wifiprov-nrf70.conf;overlay-tls-nrf70.conf"

# Secure with TLS  
west build -p -b nrf7002dk/nrf5340/cpuapp -- -DEXTRA_CONF_FILE="overlay-softap-wifiprov-nrf70.conf;overlay-tls-nrf70.conf"
```

### Flashing

```bash
west flash --erase
```

## Testing

### Method 1: Using Mobile App (Recommended)

1. **Flash the sample** to your nRF7002 DK
2. **Power on** the device and observe:

   **LED Behavior:**
   - **LED 2** starts **fast blinking** (waiting for WiFi connection)
   
   **Console Output:**
   ```
*** Booting nRF Connect SDK v3.0.2-89ba1294ac9b ***
*** Using Zephyr OS v4.0.99-f791c49f492c ***
[00:00:00.656,402] <inf> wifi_supplicant: wpa_supplicant initialized
[00:00:00.663,635] <inf> wifi_provision: SoftAP Wi-Fi provision sample started
[00:00:00.671,325] <dbg> softap_wifi_provision: init_entry: Registering self-signed server certificate
[00:00:08.776,000] <dbg> softap_wifi_provision: certificate_register: Self-signed server certificate provisioned
[00:00:08.786,529] <inf> network: Waiting for WiFi provisioning to complete
[00:00:08.794,250] <inf> ui: UI module started
[00:00:08.799,102] <inf> ui: LED 1 initialized (network status)
[00:00:08.805,389] <inf> ui: LED 2 initialized (provisioning status)
[00:00:08.812,133] <inf> ui: Button 1 initialized (MQTT publish)
[00:00:08.818,542] <inf> ui: Button 2 initialized (credential reset)
[00:00:08.875,061] <inf> wifi_provision: Network interface brought up
[00:00:08.882,781] <dbg> softap_wifi_provision: process_tcp4: Waiting for IPv4 HTTP connections on port 443
[00:00:08.893,005] <dbg> softap_wifi_provision: wifi_scan: Scanning for Wi-Fi networks...
[00:00:13.530,212] <dbg> softap_wifi_provision: net_mgmt_wifi_event_handler: NET_EVENT_WIFI_SCAN_DONE
[00:00:13.540,008] <dbg> softap_wifi_provision: unprovisioned_exit: Scanning for Wi-Fi networks completed, preparing protobuf payload
[00:00:13.553,222] <dbg> softap_wifi_provision: unprovisioned_exit: Protobuf payload prepared, scan results encoded, size: 191
[00:00:13.564,941] <dbg> softap_wifi_provision: provisioning_entry: Enabling AP mode to allow client to connect and provide Wi-Fi credentials.
[00:00:13.578,094] <dbg> softap_wifi_provision: provisioning_entry: Waiting for Wi-Fi credentials...
[00:00:15.480,743] <dbg> softap_wifi_provision: net_mgmt_wifi_event_handler: NET_EVENT_WIFI_AP_ENABLE_RESULT
[00:00:15.492,401] <inf> wifi_provision: Provisioning started
[00:00:15.499,511] <inf> network: Provisioning in progress, blocking network events
[00:00:15.507,843] <inf> network: L4 connected during provisioning (SoftAP mode) - not publishing network event
[00:00:15.518,341] <dbg> softap_wifi_provision: dhcp_server_start: IPv4 address added to interface
[00:00:15.527,648] <dbg> softap_wifi_provision: dhcp_server_start: IPv4 netmask set
[00:00:15.536,010] <dbg> softap_wifi_provision: dhcp_server_start: DHCPv4 server started
[00:00:15.544,464] <inf> ui: Provisioning status changed to: 1
   ```

1. **Observe LED behavior changes:**
   - **LED 2** changes to **slow blinking** (provisioning mode active)
   - **LED 1** remains **off** (no network connection yet)

2. **Download** the nRF Wi-Fi Provisioner mobile app:
   - [Android](https://play.google.com/store/apps/details?id=no.nordicsemi.android.wifi.provisioner)
   - [iOS](https://apps.apple.com/app/nrf-wi-Fi-provisioner/id1638948698)

3. **Connect** your mobile device to the SoftAP network:
   - SSID: `nrf-wifiprov` (configurable)
   - Password: None (open network by default)

4. **Open** the provisioning app and follow the setup wizard

5. **Provide** your WiFi network credentials through the app

6. **Observe successful connection:**
   - **LED 2** turns **solid on** (connected to provisioned WiFi)
   - **LED 1** turns **on** (internet connectivity established, MQTT ready)
   - Console shows MQTT connection and subscription messages

7. **Test MQTT functionality:**
   - **Press Button 1** (sw0) to publish a test message to MQTT broker
   - **Press Button 2** (sw1) to reset WiFi credentials and restart provisioning (device reboots)

### Method 2: Using Python Script

Alternatively, you can use the provided Python script:

1. **Connect** to the SoftAP Wi-Fi network (default SSID: *nrf-wifiprov*)

2. **Generate** Python structures from the protobuf schema:

   ```bash
   cd scripts
   protoc --proto_path=../../../nrf/subsys/net/lib/softap_wifi_provision/proto --python_out=. common.proto
   ```

3. **Start provisioning** by running:

   ```bash
   python3 provision.py --certificate ../certs/server_certificate.pem
   ```

   The script will display available networks and prompt you to select one:

   ```
   0: SSID: MyWiFiNetwork, BSSID: d8ec5e8f6619, RSSI: -49, Band: BAND_5_GHZ, Channel: 112, Auth: WPA_WPA2_PSK
   1: SSID: OfficeWiFi, BSSID: c87f54de23b8, RSSI: -49, Band: BAND_2_4_GHZ, Channel: 9, Auth: WPA_WPA2_PSK
   Select the network (number): 0
   Enter the passphrase for the network: ********
   Configuration successful!
   ```

4. **Observe** the device connecting to the selected network:
   - **LED 2** turns **solid on** (connected to provisioned WiFi)
   - **LED 1** turns **on** (internet connectivity, MQTT ready)
   - MQTT operations start automatically

### MQTT Testing

1. **Use an MQTT client** like [Mosquitto](https://mosquitto.org/) or [MQTT Explorer](http://mqtt-explorer.com/)

2. **Connect** to the same broker (default: `test.mosquitto.org`)

3. **Subscribe** to the topic: `<clientID>/my/publish/topic` to receive device messages

4. **Test Button 1 functionality:**
   - **Press Button 1** (sw0) on the nRF7002 DK
   - **Observe** a test message published: `"Button 1 pressed at <timestamp>"`
   - **LED 1** should be **on** (indicating MQTT connection)

5. **Publish** messages to: `<clientID>/my/subscribe/topic` to send messages to device

6. **Test Button 2 functionality:**
   - **Press Button 2** (sw1) to reset WiFi credentials
   - **Device reboots** and enters provisioning mode again
   - **LED 2** starts **fast blinking** (waiting for new WiFi connection)

7. **Observe** bidirectional message flow and LED status indicators

## Configuration

### Key Configuration Options

#### General Options

- `CONFIG_MQTT_SAMPLE_TRIGGER_TIMEOUT_SECONDS`: Message publication interval (default: 60 seconds)
- `CONFIG_MQTT_SAMPLE_TRANSPORT_RECONNECTION_TIMEOUT_SECONDS`: Reconnection timeout
- `CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_HOSTNAME`: MQTT broker hostname (default: `test.mosquitto.org`)
- `CONFIG_MQTT_SAMPLE_TRANSPORT_CLIENT_ID`: MQTT client ID (auto-generated if not set)

#### WiFi Provisioning Options

- `CONFIG_SOFTAP_WIFI_PROVISION`: Enable/disable WiFi provisioning
- `CONFIG_SOFTAP_WIFI_PROVISION_SSID`: SoftAP SSID (default: `nrf-wifiprov`)
- `CONFIG_SOFTAP_WIFI_PROVISION_PASSWORD`: SoftAP password (optional)
- `CONFIG_SOFTAP_WIFI_PROVISION_THREAD_STACK_SIZE`: SoftAP thread stack size (default: 8192)
- `CONFIG_SOFTAP_WIFI_PROVISION_SOCKET_CLOSE_ON_COMPLETION`: Close socket after provisioning (default: n)

#### MQTT Topics

- `CONFIG_MQTT_SAMPLE_TRANSPORT_PUBLISH_TOPIC`: Publish topic (default: `<clientID>/my/publish/topic`)
- `CONFIG_MQTT_SAMPLE_TRANSPORT_SUBSCRIBE_TOPIC`: Subscribe topic (default: `<clientID>/my/subscribe/topic`)

### Configuration Files

The sample provides several configuration files:

- `prj.conf`: Base project configuration
- `boards/nrf7002dk_nrf5340_cpuapp.conf`: Board-specific configuration (secure)
- `boards/nrf7002dk_nrf5340_cpuapp_ns.conf`: Board-specific configuration (non-secure)
- `overlay-softap-wifiprov-nrf70.conf`: WiFi provisioning overlay
- `overlay-tls-nrf70.conf`: TLS encryption overlay

## WiFi Provisioning Details

### SoftAP Mode

When no WiFi credentials are stored, the device automatically:

1. **Creates** a WiFi access point (SoftAP)
2. **Starts** an HTTPS server for credential provisioning (port 443)
3. **Scans** for available WiFi networks
4. **Waits** for mobile app connection
5. **Processes** credential provisioning requests

### HTTP Resources

The SoftAP WiFi provisioning library provides the following HTTP resources:

- `GET /prov/networks` - Returns available WiFi networks in protobuf format
- `POST /prov/configure` - Accepts WiFi credentials in protobuf format
- Various mDNS service discovery endpoints

### Provisioning Protocol

The provisioning uses:
- **HTTPS** with self-signed certificates for secure communication
- **Protocol Buffers** for efficient data serialization
- **mDNS** for service discovery
- **DHCPv4** server for client IP assignment

### Security Considerations

- SoftAP can be configured with WPA2 password protection
- TLS encryption for credential transmission
- Credentials are stored securely in flash memory using the WiFi credentials subsystem
- Self-signed certificates are used (can be replaced with proper certificates)

## MQTT Operations

### Connection Management

The sample automatically:
1. **Connects** to the configured MQTT broker after WiFi provisioning
2. **Subscribes** to the configured topic for incoming messages
3. **Publishes** periodic messages with device status
4. **Handles** reconnection on network failures

### Message Format

Published messages include:
- Device uptime
- Network status
- Sensor data (if available)
- Custom payload data

### TLS Support

Optional TLS encryption for MQTT communication:
- Server certificate validation
- Mutual authentication (if client certificates configured)
- Configurable cipher suites

## Dependencies

This sample uses the following libraries and subsystems:

### Nordic Libraries

- **SoftAP WiFi Provisioning Library** (`CONFIG_SOFTAP_WIFI_PROVISION`): Handles SoftAP creation and credential provisioning
- **MQTT Helper Library** (`CONFIG_MQTT_HELPER`): Simplifies MQTT operations
- **WiFi Credentials Library** (`CONFIG_WIFI_CREDENTIALS`): Secure storage of WiFi credentials
- **DK Buttons and LEDs Library** (`CONFIG_DK_LIBRARY`): Hardware abstraction for user interface

### Zephyr Subsystems

- **Connection Manager** (`CONFIG_NET_CONNECTION_MANAGER`): Network connectivity management
- **ZBus** (`CONFIG_ZBUS`): Inter-module communication
- **State Machine Framework (SMF)** (`CONFIG_SMF`): Module state management
- **Network Management** (`CONFIG_NET_MGMT`): WiFi and network control
- **Settings** (`CONFIG_SETTINGS`): Persistent configuration storage
- **WiFi Management** (`CONFIG_WIFI`): WiFi driver interface

### Third-Party Libraries

- **Mbed TLS** (`CONFIG_MBEDTLS`): Cryptographic operations and TLS support
- **nanopb** (`CONFIG_NANOPB`): Protocol buffer implementation
- **HTTP Parser** (`CONFIG_HTTP_PARSER`): HTTP request/response parsing

## License

This sample is licensed under the LicenseRef-Nordic-5-Clause license.

## Support

For support and questions, please visit the [Nordic DevZone](https://devzone.nordicsemi.com/).

### Related Resources

- [SoftAP WiFi Provisioning Library Documentation](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/libraries/networking/softap_wifi_provision.html)
- [nRF Wi-Fi Provisioner mobile app source code (Android)](https://github.com/NordicSemiconductor/Android-nRF-Wi-Fi-Provisioner)
- [nRF Wi-Fi Provisioner mobile app source code (iOS)](https://github.com/NordicSemiconductor/IOS-nRF-Wi-Fi-Provisioner)
