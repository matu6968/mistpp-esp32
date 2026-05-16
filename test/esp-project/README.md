# Mist++ ESP32 Sample application

This example shows how to set up and communicate over a TurboWarp project over Mist++.

## Table of Contents

- [Hardware Required](#hardware-required)
- [Configure the project](#configure-the-project)
- [Pre-configured SDK Configurations](#pre-configured-sdk-configurations)
- [Server Certificate Verification](#server-certificate-verification)
- [Build and Flash](#build-and-flash)
- [Example Output](#example-output)

## Quick Start

1. **Configure and build:**
   ```bash
   idf.py menuconfig  # Configure WiFi/Ethernet and WebSocket URI
   idf.py build
   ```

2. **Flash and monitor:**
   ```bash
   idf.py -p PORT flash monitor
   ```

## How to Use Example

### Hardware Required

This example can be executed on any ESP32 board, the only required interface is WiFi and connection to internet or a local server.

### Configure the project

* Open the project configuration menu (`idf.py menuconfig`)
* Configure Wi-Fi or Ethernet under "Example Connection Configuration" menu.
* Configure the websocket endpoint URI under "Example Configuration", if "WEBSOCKET_URI_FROM_STDIN" is selected then the example application will connect to the URI it reads from stdin (used for testing)
* To test a WebSocket client example over TLS, please enable one of the following configurations: `CONFIG_WS_OVER_TLS_MUTUAL_AUTH` or `CONFIG_WS_OVER_TLS_SERVER_AUTH`. See the sections below for more details.

### Pre-configured SDK Configurations

This example includes several pre-configured `sdkconfig.ci.*` files for different testing scenarios:

* **sdkconfig.ci** - Default configuration with WebSocket over Ethernet (IP101 PHY, ESP32, IPv6) and hardcoded URI.
* **sdkconfig.ci.plain_tcp** - WebSocket over plain TCP (no TLS, URI from stdin) using Ethernet (IP101 PHY, ESP32, IPv6).
* **sdkconfig.ci.mutual_auth** - WebSocket with mutual TLS authentication (client/server certificate verification, skips CN check) and URI from stdin.
* **sdkconfig.ci.dynamic_buffer** - WebSocket with dynamic buffer allocation, Ethernet (IP101 PHY, ESP32, IPv6), and hardcoded URI.

Example:
```
idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.ci.plain_tcp" build
```

### Server Certificate Verification

* Mutual Authentication: When `CONFIG_WS_OVER_TLS_MUTUAL_AUTH=y` is enabled, it's essential to provide valid certificates for both the server and client.
  This ensures a secure two-way verification process.
* Server-Only Authentication: To perform verification of the server's certificate only (without requiring a client certificate), set `CONFIG_WS_OVER_TLS_SERVER_AUTH=y`.
  This method skips client certificate verification.

For TurboWarp, only Server-Only Authentication should be used due to the nature of short-lived certificates within the TLS provider (Let's Encrypt).

For more details, see the [websocket test client example](https://github.com/espressif/esp-protocols/tree/master/components/esp_websocket_client/examples/target) in the ESP-IDF repository.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.


## Example Output

```
I (680) example_connect: Connecting to myssid...

I (5520) example_common: Connected to example_netif_sta
I (5520) example_common: - IPv4 address: 192.168.0.144,
I (5530) example_common: - IPv6 address: fe80:0000:0000:0000:9aa3:16ff:fe8f:4038, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (5540) mist_example: Creating Mist connection with username: esp32_93072
I (5550) mist_example: Connecting to TurboWarp cloud...

I (7120) esp-x509-crt-bundle: Certificate validated
I (8240) mist: WEBSOCKET_EVENT_CONNECTED
I (8240) mist_example: Connected: Connected successfully.
I (8280) mist_example: Client started. Waiting for connection and handshake...
I (8750) mist_example: Variable "☁ High Score" changed to: 20
I (8760) mist_example: Variable "☁ Test Variable" changed to: 9
I (13280) mist_example: Starting variable updates...
I (13280) mist_example: Set "☁ Test Variable" to: 0
I (14280) mist_example: Set "☁ Test Variable" to: 1
I (15280) mist_example: Set "☁ Test Variable" to: 2
I (16280) mist_example: Set "☁ Test Variable" to: 3
I (17280) mist_example: Set "☁ Test Variable" to: 4
I (18280) mist_example: Set "☁ Test Variable" to: 5
I (19280) mist_example: Set "☁ Test Variable" to: 6
I (20280) mist_example: Set "☁ Test Variable" to: 7
I (21280) mist_example: Set "☁ Test Variable" to: 8
I (22280) mist_example: Set "☁ Test Variable" to: 9
I (23280) mist_example: Current value of "☁ Test Variable": 9
I (23280) mist_example: Disconnecting...
W (23500) transport_ws: esp_transport_ws_poll_connection_closed: unexpected data readable on socket=54
W (23500) websocket_client: Connection terminated while waiting for clean TCP close
I (23510) mist_example: Disconnected: Disconnected by client request.
I (23510) mist_example: Mist example completed. Deleting task...
```

## Troubleshooting

### Common Issues

**Connection failed:**
- Verify WiFi/Ethernet configuration in `idf.py menuconfig`
- Check if the WebSocket server is running and accessible
- Ensure the URI is correct (use `wss://` for TLS, `ws://` for plain TCP)

**TLS certificate errors:**
- **Certificate verification failed:** The most common cause is CN mismatch. Ensure the server certificate's Common Name matches the hostname/IP you're connecting to:
  - Check your connection URI (e.g., if connecting to `wss://192.168.1.100:8080`, the certificate CN must be `192.168.1.100`)
  - Regenerate certificates with the correct CN: `./generate_certs.sh <your_hostname_or_ip>`
  - For testing only, you can bypass CN check with `CONFIG_WS_OVER_TLS_SKIP_COMMON_NAME_CHECK=y` (NOT recommended for production)
- Verify certificate files are properly formatted and accessible
- Ensure the CA certificate used to sign the server certificate is loaded on the ESP32

**Build errors:**
- Clean build: `idf.py fullclean`
- Check ESP-IDF version compatibility
- Verify all dependencies are installed

**Test failures:**
- Ensure the device is connected and accessible via the specified port
- Check that the target device matches the configuration (`--target esp32`)
- Verify pytest dependencies are installed correctly