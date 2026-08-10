# UCM Node Architecture

## 1. Overview

A UCM node is the edge computing and environmental sensing unit of the Urban Climate Mesh.

The current UCM node hardware platform is specifically:

- **Waveshare ESP32-P4 PoE WiFi development board**
- **Sensirion SEN54 environmental sensor**

The Waveshare board provides the primary ESP32-P4 processor, Ethernet/PoE networking, and ESP32-C6-based Wi-Fi connectivity.

The SEN54 provides the environmental sensing capabilities.

The node combines:

- Environmental sensing
- Local processing
- Ethernet networking
- Wi-Fi networking
- MQTT communications
- Local web services
- Persistent node identity
- OTA firmware management
- Diagnostics

The node's primary responsibility is to collect environmental observations, associate them with a persistent node identity, and reliably deliver those observations to a UCM Hub.

The node is designed to operate as an autonomous edge device rather than as a simple sensor attached to a remote computer.

---

# 2. Current Hardware Platform

The current UCM node is built around the following hardware:

| Component | Current platform |
|---|---|
| Main processor | Waveshare ESP32-P4 PoE WiFi |
| Primary MCU | ESP32-P4 |
| Wi-Fi coprocessor | ESP32-C6 |
| Wireless interface | ESP-Hosted over SDIO |
| Wired network | Ethernet |
| Power/network capability | PoE |
| Environmental sensor | Sensirion SEN54 |
| Sensor interface | I2C |

This hardware specification is important because the UCM firmware is currently developed and tested against this specific Waveshare platform.

Future UCM hardware revisions may use different boards, but those should be treated as separate hardware revisions or platforms rather than silently changing the definition of the current node.

---

# 3. Node Architecture

The current architecture is:

```text
                         UCM NODE
                            │
             ┌──────────────┴──────────────┐
             │                             │
     Waveshare ESP32-P4 PoE WiFi       SEN54
             │                       Environmental
             │                          Sensor
             │
      ┌──────┼───────────────┐
      │      │               │
    Sensor  Network         OTA
    Logic   Manager        Manager
      │      │               │
      │   ┌──┴──────┐        │
      │   │         │        │
      │ Ethernet   Wi-Fi     │
      │   │         │        │
      │   │      ESP32-C6    │
      │   │         │        │
      └───┴─────────┴────────┘
              │
       ESP32-P4 Application
              │
        ┌─────┴─────┐
        │           │
       MQTT     Web Server
        │           │
        └─────┬─────┘
              │
              ▼
           UCM-HUB
```

The **ESP32-P4** is the primary application processor.

The **ESP32-C6** provides Wi-Fi connectivity through the ESP-Hosted architecture.

The **SEN54** provides environmental measurements.

The Waveshare board provides the physical platform integrating the processor, networking, power, and associated interfaces.

---

# 4. Waveshare ESP32-P4 PoE WiFi Platform

The Waveshare ESP32-P4 PoE WiFi board is the current hardware foundation for the UCM node.

The board provides the ESP32-P4 as the primary application processor and includes networking capabilities appropriate for a permanently deployed sensor node.

The current UCM implementation uses:

- Ethernet
- PoE
- ESP32-C6 Wi-Fi coprocessor
- SDIO communication between the P4 and C6
- GPIO-connected peripherals
- I2C for the SEN54

The use of this specific board is an architectural decision rather than merely a development convenience.

It provides a practical combination of:

```text
Processing
    +
Ethernet
    +
PoE
    +
Wi-Fi
    +
Embedded hardware
```

This makes it suitable as the standard platform for the current generation of UCM nodes.

---

# 5. ESP32-P4 Primary Processor

The ESP32-P4 is the primary processor and runs the UCM application firmware.

It is responsible for:

- Application firmware
- SEN54 sensor communication
- Sensor data processing
- Network management
- MQTT
- OTA management
- Web server
- Node identity
- Configuration
- Diagnostics

The P4 is the device's primary decision-making processor.

---

# 6. ESP32-C6 Wireless Coprocessor

The Waveshare platform uses an ESP32-C6 as the wireless coprocessor.

The P4 communicates with the C6 through ESP-Hosted over SDIO.

Current UCM configuration:

```text
SDIO
CLK  = GPIO18
CMD  = GPIO19
D0   = GPIO14
D1   = GPIO15
D2   = GPIO16
D3   = GPIO17

Slave Reset = GPIO54
```

The ESP-Hosted architecture allows the ESP32-P4 application to use the ESP32-C6's wireless capabilities.

Conceptually:

```text
ESP32-P4
   │
   │ SDIO / ESP-Hosted
   │
   ▼
ESP32-C6
   │
   ▼
Wi-Fi
```

The C6 is therefore a networking coprocessor within the current UCM hardware platform.

---

# 7. Sensirion SEN54

The current UCM environmental sensor is the **Sensirion SEN54**.

The SEN54 is connected directly to the ESP32-P4 using I2C.

Current configuration:

```text
I2C Port = 0
SDA      = GPIO7
SCL      = GPIO8
Address  = 0x69
```

The SEN54 provides the environmental measurements used by the current UCM node.

These include:

```text
PM1.0
PM2.5
PM4.0
PM10
Temperature
Relative Humidity
VOC Index
NOx Index
```

NOx availability depends on the sensor's operating conditions and measurement configuration.

---

# 8. SEN54 Sensor Interface

The UCM firmware initializes the SEN54 during node startup.

The general sequence is:

```text
ESP32-P4 starts
       │
       ▼
Initialize I2C
       │
       ▼
Initialize SEN54
       │
       ▼
Reset sensor
       │
       ▼
Start measurement
       │
       ▼
Periodic measurement
       │
       ▼
Process sensor data
       │
       ▼
Publish through MQTT
```

The sensor is therefore directly controlled by the ESP32-P4.

The Hub does not directly communicate with the SEN54.

---

# 9. Ethernet and PoE

The Waveshare ESP32-P4 PoE WiFi board provides Ethernet connectivity and PoE capability.

The current UCM firmware uses the board's Ethernet interface when available.

The Ethernet subsystem uses:

```text
PHY:         IC+ IP101GR
Speed:       10/100 Mbps
Interface:   RMII
MAC:         ESP32-P4 internal EMAC
PHY Address: 1

MDC:         GPIO31
MDIO:        GPIO52
PHY Reset:   GPIO51
```

PoE provides a particularly useful deployment option because a single Ethernet cable can provide both:

```text
Network connectivity
        +
Power
```

This makes the platform suitable for fixed installations where Ethernet infrastructure is available.

---

# 10. Network Selection

The node supports both Ethernet and Wi-Fi.

The current network strategy is:

```text
Start Network Manager
        │
        ▼
Initialize Ethernet
        │
        ▼
Ethernet available?
    ┌───┴───┐
   YES      NO
    │        │
    ▼        ▼
 Ethernet   Wi-Fi
    │        │
    └────┬───┘
         ▼
     IP Address
         │
         ▼
     MQTT / Web
```

Ethernet is attempted first.

If Ethernet is unavailable, the node proceeds to Wi-Fi using its stored credentials.

This allows the same hardware platform to be deployed in either wired or wireless environments.

---

# 11. Wi-Fi Provisioning

Wi-Fi is provided through the ESP32-C6 wireless coprocessor.

The node stores Wi-Fi credentials locally.

When valid credentials are available, the node connects to the configured wireless network.

After obtaining an IP address, the node starts mDNS.

For example:

```text
http://jc-office.local
```

The mDNS hostname is a human-readable network name.

It is distinct from the permanent UCM Node ID.

---

# 12. Node Identity

Every UCM node has a persistent Node ID.

Example:

```text
UCM-E0A6EC
```

The Node ID is used throughout the UCM architecture.

It appears in:

- MQTT topics
- MQTT payloads
- Node metadata
- Environmental data
- Heartbeats
- Command topics
- OTA operations
- Hub records

The human-readable node name is separate from the Node ID.

Example:

```text
Node ID:     UCM-E0A6EC
Node Name:   JC Office
Description: Downtown Jersey City
```

The name and location may change without changing the physical node's identity.

See:

`docs/architecture/node-identity.md`

---

# 13. Firmware Architecture

The UCM firmware is organized into functional subsystems.

The major areas include:

```text
UCM Node Firmware
│
├── SEN54 / Sensor Management
│
├── Network Manager
│   ├── Ethernet
│   └── Wi-Fi / ESP-Hosted
│
├── MQTT Manager
│
├── OTA Manager
│
├── Wi-Fi Provisioning
│
├── Web Server
│
└── Node Configuration / Identity
```

This organization separates hardware-specific functionality from application-level behavior.

---

# 14. Application Startup

The general startup sequence is:

```text
Bootloader
    │
    ▼
ESP32-P4 application starts
    │
    ▼
Initialize OTA manager
    │
    ▼
Initialize SEN54
    │
    ▼
Start Network Manager
    │
    ├── Ethernet
    │
    └── Wi-Fi fallback
    │
    ▼
Obtain IP address
    │
    ▼
Synchronize time
    │
    ▼
Determine Node ID
    │
    ▼
Start MQTT
    │
    ▼
Subscribe to command topic
    │
    ▼
Publish node information
    │
    ▼
Start web server
    │
    ▼
Confirm running firmware
    │
    ▼
Begin normal operation
```

This sequence establishes the node's identity, network connectivity, management interfaces, and sensor operation before normal data collection proceeds.

---

# 15. MQTT Client

MQTT provides the primary communications interface between the node and the UCM Hub.

The node connects to the MQTT broker after establishing network connectivity.

The node subscribes to its command topic.

For example:

```text
ucm/node-UCM-E0A6EC/command
```

The node publishes information such as:

```text
ucm/node-UCM-E0A6EC/info
ucm/node-UCM-E0A6EC/environment
ucm/node-UCM-E0A6EC/heartbeat
```

The Node ID establishes the node's MQTT namespace.

---

# 16. Node Information

After connecting to MQTT, the node publishes metadata describing itself.

Example:

```json
{
  "node": "UCM-E0A6EC",
  "name": "JC Office",
  "description": "Downtown Jersey City",
  "lat": 40.71780,
  "lon": -74.04310
}
```

The node identity, human-readable name, description, and location are represented separately.

This allows operational metadata to change without changing the node's fundamental identity.

---

# 17. Environmental Data

The SEN54 periodically provides environmental measurements to the ESP32-P4.

The firmware processes the readings and publishes them through MQTT.

The current sensor data model includes:

```text
PM1.0
PM2.5
PM4.0
PM10
Temperature
Relative Humidity
VOC Index
NOx Index
```

Each observation is associated with:

```text
Node ID
Timestamp
Sensor measurements
```

The Hub can therefore associate observations with both a physical device and a point in time.

---

# 18. Heartbeat

The node periodically publishes operational information.

Example:

```json
{
  "node": "UCM-E0A6EC",
  "timestamp": "2026-08-10T12:05:52Z",
  "uptime_s": 32,
  "free_heap": 457000,
  "rssi": -29
}
```

The heartbeat provides a lightweight indication of node health.

Current information includes:

- Node ID
- Timestamp
- Uptime
- Free heap
- Wi-Fi signal strength where applicable

Future versions may include:

- Firmware version
- Hardware revision
- Network interface
- IP address
- Sensor status
- Error counters
- OTA state

---

# 19. Local Web Server

The node runs a local web server after network connectivity has been established.

The local web interface provides a management and diagnostic interface without requiring access to the central UCM dashboard.

The node can be accessed through its mDNS hostname on the local network.

For example:

```text
http://jc-office.local
```

This is particularly useful during deployment and troubleshooting.

---

# 20. OTA Firmware Management

OTA management is integrated into the node firmware.

The current partition table provides two application partitions:

```text
ota_0
ota_1
```

Current partition configuration:

```text
ota_0
offset = 0x00020000
size   = 0x001a9000

ota_1
offset = 0x001d0000
size   = 0x001a9000
```

The node runs from one partition while the other is available for an update.

The OTA sequence is:

```text
Current Firmware
      │
      ▼
Receive OTA command
      │
      ▼
Download new firmware
      │
      ▼
Write inactive OTA partition
      │
      ▼
Set next boot partition
      │
      ▼
Restart
      │
      ▼
Boot new firmware
      │
      ▼
Validate firmware
      │
   ┌──┴──┐
 VALID  INVALID
   │       │
   ▼       ▼
Continue Rollback
```

This provides a recovery path when a new firmware image fails.

---

# 21. Firmware Validation

The node uses the ESP-IDF OTA image state mechanism to validate firmware following an OTA update.

After successfully booting the new firmware, the application confirms that the image is valid.

A successful update therefore involves more than simply writing a firmware image to flash.

The process is:

```text
Download
   ↓
Write
   ↓
Select
   ↓
Reboot
   ↓
Boot
   ↓
Run
   ↓
Confirm VALID
```

This is an important part of the UCM reliability architecture.

---

# 22. Node Command Interface

Commands are delivered through the node's MQTT command topic.

Example:

```text
ucm/node-UCM-E0A6EC/command
```

The command payload identifies the requested operation.

For example:

```json
{
  "command": "ota"
}
```

The node itself is identified by the MQTT topic.

The command does not need to repeat the Node ID in the payload.

This gives UCM a clean separation between:

```text
Destination
    =
MQTT topic

Operation
    =
Command payload
```

This pattern should be retained as additional node commands are developed.

---

# 23. Local Autonomy

The UCM node is designed to remain functional when higher-level applications are unavailable.

The node should be capable of:

- Reading the SEN54
- Maintaining its identity
- Establishing network connectivity
- Connecting to MQTT
- Publishing environmental data
- Publishing health information
- Running its local web interface
- Receiving management commands
- Performing OTA updates

The node does not depend on the dashboard for basic sensing.

Likewise, the dashboard does not need to directly communicate with the SEN54.

---

# 24. Failure Domains

The architecture attempts to isolate failures between subsystems.

For example:

```text
SEN54 failure
     │
     └──► Node reports sensor problem

Wi-Fi failure
     │
     └──► Ethernet may remain available

Ethernet failure
     │
     └──► Wi-Fi may provide connectivity

MQTT failure
     │
     └──► Node remains locally operational

Dashboard failure
     │
     └──► Node continues sensing

OTA failure
     │
     └──► Previous firmware remains available
```

The goal is to prevent a failure in one subsystem from unnecessarily disabling the entire node.

---

# 25. Hardware and Firmware Versioning

The current hardware platform should be explicitly identified in firmware and documentation.

At minimum, UCM should distinguish:

```text
Node ID
Hardware Platform
Hardware Revision
Firmware Version
```

For example:

```text
Node ID:
UCM-E0A6EC

Hardware Platform:
Waveshare ESP32-P4 PoE WiFi

Sensor:
Sensirion SEN54

Hardware Revision:
1.x

Firmware:
v0.6-ota-validation
```

This distinction becomes important when UCM begins deploying multiple hardware revisions.

OTA deployment should eventually verify that a firmware image is compatible with the target hardware platform and revision.

---

# 26. Diagnostics

The node provides diagnostic information through the serial console and MQTT heartbeat system.

Important diagnostic areas include:

```text
Boot
Sensor
I2C
Ethernet
Wi-Fi
ESP-Hosted
MQTT
OTA
Memory
Network
```

The serial console remains an important development and field-debugging interface.

Examples from the current platform include:

```text
P4_SENSOR_TEST: PM1.0=5.8 PM2.5=6.1
NETWORK: Ethernet unavailable
app_wifi_prov: Successfully connected to AP
MQTT_MANAGER: MQTT connected
UCM_OTA: OTA firmware confirmed VALID
```

The diagnostic system should evolve toward consistent log categories as the firmware matures.

---

# 27. Security Considerations

The current architecture provides the foundation for additional security controls.

Future security measures should include:

- Authenticated MQTT connections
- MQTT over TLS
- Authenticated OTA images
- Firmware signing
- Secure boot
- Flash encryption
- Command authorization
- Device credentials
- Per-node credentials
- Protected local web interfaces

Security improvements should preserve the fundamental UCM identity and messaging architecture.

---

# 28. Hardware Expansion

The current UCM node is standardized around the:

**Waveshare ESP32-P4 PoE WiFi + Sensirion SEN54**

Additional sensors may be added in future versions.

For example:

```text
Waveshare ESP32-P4 PoE WiFi
            │
            ├── SEN54
            ├── Additional environmental sensors
            ├── Weather sensors
            ├── Energy sensors
            ├── Water sensors
            └── Research instrumentation
```

Additional sensors should be incorporated without unnecessarily changing the fundamental node communication architecture.

The MQTT interface should remain stable as sensing capabilities expand.

---

# 29. Current Node Architecture Summary

The current UCM node can be summarized as:

```text
              WAVESHARE ESP32-P4 PoE WiFi
                         │
              ┌──────────┴──────────┐
              │                     │
          ESP32-P4              ESP32-C6
          Processor            Wi-Fi Coprocessor
              │                     │
       ┌──────┼───────┐             │
       │      │       │             │
    SEN54   Ethernet  OTA        Wi-Fi
       │      │       │             │
       │      │       │             │
       └──────┼───────┴─────────────┘
              │
       UCM Application
              │
       ┌──────┴──────┐
       │             │
      MQTT       Web Server
       │
       ▼
    UCM-HUB
```

The node is therefore both an **environmental sensing platform** and an **edge computing platform**.

---

# 30. Relationship to the Overall UCM Architecture

The node is one layer of the larger UCM system:

```text
┌─────────────────────────────────────────┐
│              RESEARCH                   │
├─────────────────────────────────────────┤
│       Applications / Dashboards         │
├─────────────────────────────────────────┤
│             UCM HUB                     │
│ MQTT • Database • API • GIS • Grafana   │
├─────────────────────────────────────────┤
│             NETWORK                     │
├─────────────────────────────────────────┤
│             UCM NODE                    │
│                                         │
│ Waveshare ESP32-P4 PoE WiFi             │
│          +                              │
│ Sensirion SEN54                         │
└─────────────────────────────────────────┘
```

The node provides the standardized edge interface between physical environmental sensing and the larger UCM data infrastructure.

---

# 31. Design Principles

The current node architecture follows these principles.

### Specific hardware platform

The current UCM node is explicitly based on the Waveshare ESP32-P4 PoE WiFi board and Sensirion SEN54.

### Persistent identity

A physical node has a stable Node ID.

### Separation of concerns

Sensor, network, MQTT, OTA, and web functionality are separate subsystems.

### Network flexibility

Ethernet and Wi-Fi provide alternative network paths.

### Local autonomy

The node should perform its primary functions without the dashboard.

### Safe firmware updates

OTA updates should preserve a known-good firmware path.

### Hardware awareness

Firmware and OTA management should distinguish hardware platforms and revisions.

### Modular expansion

Additional sensing capabilities should not require redesigning the entire communication architecture.

### Observable operation

The node should provide enough diagnostic information to determine its operating state locally and remotely.

---

# 32. Summary

The UCM node is the fundamental edge unit of the Urban Climate Mesh.

The current implementation is specifically built around:

**Waveshare ESP32-P4 PoE WiFi**

and

**Sensirion SEN54**

The Waveshare board provides the processing, Ethernet, PoE, and ESP32-C6 wireless infrastructure.

The SEN54 provides the environmental measurements.

The UCM firmware integrates these components with:

```text
Network Management
       +
MQTT
       +
Node Identity
       +
Web Services
       +
OTA Firmware Management
       +
Diagnostics
```

The resulting device provides a standardized interface between physical environmental sensing and the UCM Hub.

The fundamental node contract is:

> **A UCM node identifies itself, senses its environment through the SEN54, communicates its observations through MQTT, reports its operational state, accepts authorized management commands, and can safely update its firmware.**

Future UCM hardware platforms may differ, but they should be explicitly identified as different hardware platforms or revisions rather than being conflated with the current Waveshare ESP32-P4 PoE WiFi implementation.