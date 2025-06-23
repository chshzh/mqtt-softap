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

After boot, the device starts advertising in SoftAP mode with the SSID name set by the `CONFIG_SOFTAP_WIFI_PROVISION_SSID` option (default is *nrf-wifiprov*), allowing clients (STA) to connect to it and provide Wi-Fi credentials. Once provisioned, the device automatically connects to the configured WiFi network and begins MQTT operations.

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

- **LED 1**: Turns on when the device is advertising the SoftAP network (provisioning mode)
- **LED 2**: Turns on when the device is connected to the provisioned WiFi network

### Buttons

- **Button 1**: Press to reset stored WiFi credentials and restart provisioning

## Building and Running

### Basic Build Commands

For **nRF7002 DK (non-secure)** - ✅ **Tested and Working**:
```bash
west build -p -b nrf7002dk/nrf5340/cpuapp/ns -- -DEXTRA_CONF_FILE=overlay-softap-wifiprov-nrf70.conf
```

For **nRF7002 DK (secure)** - ✅ **Tested and Working**:
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

### Memory Usage

The sample has been optimized for memory efficiency:

| Target | Flash Usage | RAM Usage | Status |
|--------|-------------|-----------|---------|
| **nrf7002dk/nrf5340/cpuapp/ns** | 735,756B / 848KB (84.73%) | 200,889B / 400KB (49.05%) | ✅ Working |
| **nrf7002dk/nrf5340/cpuapp** | 793,124B / 960KB (80.68%) | 208,025B / 448KB (45.35%) | ✅ Working |

### Flashing

```bash
west flash
```

## Testing

### Method 1: Using Mobile App (Recommended)

1. **Flash the sample** to your nRF7002 DK
2. **Power on** the device and observe the following output:

   ```
   *** Using Zephyr OS v4.0.99-ncs1 ***
   [00:00:00.587,036] <inf> network: Network task started - using Nordic's exact SoftAP logic
   [00:00:00.597,900] <dbg> softap_wifi_provision: init_entry: Registering self-signed server certificate
   [00:00:00.708,892] <inf> network: Network interface brought up
   [00:00:00.753,021] <dbg> softap_wifi_provision: wifi_scan: Scanning for Wi-Fi networks...
   [00:00:05.615,661] <dbg> softap_wifi_provision: provisioning_entry: Enabling AP mode to allow client to connect and provide Wi-Fi credentials.
   [00:00:06.320,953] <inf> network: Provisioning started
   ```

3. **Observe LED 1** turning on, indicating provisioning mode

4. **Download** the nRF Wi-Fi Provisioner mobile app:
   - [Android](https://play.google.com/store/apps/details?id=no.nordicsemi.android.wifi.provisioner)
   - [iOS](https://apps.apple.com/app/nrf-wi-Fi-provisioner/id1638948698)

5. **Connect** your mobile device to the SoftAP network:
   - SSID: `nrf-wifiprov` (configurable)
   - Password: None (open network by default)

6. **Open** the provisioning app and follow the setup wizard

7. **Provide** your WiFi network credentials through the app

8. **Observe LED 2** turning on once connected to your WiFi network

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

4. **Observe** the device connecting to the selected network and starting MQTT operations

### MQTT Testing

1. **Use an MQTT client** like [Mosquitto](https://mosquitto.org/) or [MQTT Explorer](http://mqtt-explorer.com/)

2. **Connect** to the same broker (default: `test.mosquitto.org`)

3. **Subscribe** to the topic: `<clientID>/my/subscribe/topic`

4. **Publish** messages to: `<clientID>/my/publish/topic`

5. **Observe** bidirectional message flow

## Configuration

### Key Configuration Options

#### General Options

- `CONFIG_MQTT_SAMPLE_TRIGGER_TIMEOUT_SECONDS`: Message publication interval (default: 60 seconds)
- `CONFIG_MQTT_SAMPLE_TRANSPORT_RECONNECTION_TIMEOUT_SECONDS`: Reconnection timeout
- `CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_HOSTNAME`: MQTT broker hostname (default: `test.mosquitto.org`)
- `CONFIG_MQTT_SAMPLE_TRANSPORT_CLIENT_ID`: MQTT client ID (auto-generated if not set)

#### WiFi Provisioning Options

- `CONFIG_MQTT_SAMPLE_WIFI_PROVISION`: Enable/disable WiFi provisioning
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

## Troubleshooting

### Common Issues

#### Build Errors

**Problem**: `CONFIG_MQTT_SAMPLE_WIFI_PROVISION` undefined
**Solution**: Ensure the WiFi provisioning module is properly configured in Kconfig

**Problem**: Memory overflow during linking
**Solution**: Use memory optimization overlays or reduce feature set

**Problem**: TLS certificate errors
**Solution**: Ensure certificates are properly generated and placed in the certs folder

#### Runtime Issues

**Problem**: Device doesn't enter provisioning mode
**Solution**: Press Button 1 to reset credentials and restart provisioning

**Problem**: Cannot connect to SoftAP
**Solution**: Check SoftAP SSID configuration and mobile device WiFi settings

**Problem**: "Failed to set beacon data" error
**Solution**: This is a known issue with the SoftAP library's channel selection. The device will retry automatically.

**Problem**: MQTT connection fails
**Solution**: Verify broker hostname, port, and network connectivity

**Problem**: TLS handshake failures
**Solution**: Check certificate configuration and server name indication (SNI) settings

#### Certificate Issues

If you change the hostname using `CONFIG_NET_HOSTNAME`, the server certificate must be regenerated with the Subject Alternate Name (SAN) field set to the new hostname to ensure Server Name Indication (SNI) succeeds during the TLS handshake.

### Debug Logging

Enable debug logging for specific modules:

```bash
# WiFi provisioning debug
west build -- -DCONFIG_SOFTAP_WIFI_PROVISION_LOG_LEVEL_DBG=y

# MQTT debug
west build -- -DCONFIG_MQTT_HELPER_LOG_LEVEL_DBG=y

# General networking debug
west build -- -DCONFIG_LOG_DEFAULT_LEVEL=4
```

### Memory Optimization

For memory-constrained builds, use these optimizations:

```conf
CONFIG_LOG_MODE_MINIMAL=y
CONFIG_SIZE_OPTIMIZATIONS=y
CONFIG_NET_IPV6=n
CONFIG_ASSERT=n
CONFIG_LTO=y
CONFIG_ISR_TABLES_LOCAL_DECLARATION=y
```

## Sample Output

### Successful Provisioning and MQTT Connection

```
*** Booting nRF Connect SDK v3.0.2-89ba1294ac9b ***
*** Using Zephyr OS v4.0.99-f791c49f492c ***
[00:00:00.587,036] <inf> network: Network task started - using Nordic's exact SoftAP logic
[00:00:00.597,900] <dbg> softap_wifi_provision: init_entry: Registering self-signed server certificate
[00:00:00.708,892] <inf> network: Network interface brought up
[00:00:00.753,021] <dbg> softap_wifi_provision: wifi_scan: Scanning for Wi-Fi networks...
[00:00:05.615,661] <dbg> softap_wifi_provision: provisioning_entry: Enabling AP mode to allow client to connect and provide Wi-Fi credentials.
[00:00:06.320,953] <inf> network: Provisioning started
[00:00:14.054,565] <dbg> softap_wifi_provision: dhcp_server_start: DHCPv4 server started

# Client connects and provides credentials
[00:01:18.950,469] <dbg> softap_wifi_provision: print_mac: Client STA connected, MAC: 04:00:91:9E:31:EA
[00:01:18.960,174] <inf> network: Client connected
[00:00:37.468,536] <inf> network: Wi-Fi credentials received
[00:00:38.728,881] <inf> network: Provisioning completed

# Device connects to WiFi and starts MQTT
[00:00:40.389,312] <inf> network: Network connected
[00:00:45.123] <inf> transport: Connected to MQTT broker
[00:00:45.124] <inf> transport: Hostname: test.mosquitto.org
[00:00:45.125] <inf> transport: Client ID: A1B2C3D4E5F6
[00:00:45.126] <inf> transport: Port: 1883
[00:00:45.189] <inf> transport: Subscribed to topic A1B2C3D4E5F6/my/subscribe/topic
[00:01:45.234] <inf> transport: Publishing message: "Hello MQTT! Uptime: 105234ms"
```

---

## License

This sample is licensed under the LicenseRef-Nordic-5-Clause license.

## Support

For support and questions, please visit the [Nordic DevZone](https://devzone.nordicsemi.com/).

### Related Resources

- [SoftAP WiFi Provisioning Library Documentation](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/libraries/networking/softap_wifi_provision.html)
- [nRF Wi-Fi Provisioner mobile app source code (Android)](https://github.com/NordicSemiconductor/Android-nRF-Wi-Fi-Provisioner)
- [nRF Wi-Fi Provisioner mobile app source code (iOS)](https://github.com/NordicSemiconductor/IOS-nRF-Wi-Fi-Provisioner)
