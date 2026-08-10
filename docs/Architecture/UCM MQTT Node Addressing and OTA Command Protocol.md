# UCM MQTT Node Addressing and OTA Command Protocol

## 1. Overview

The Urban Climate Mesh (UCM) uses MQTT to provide communication between environmental sensor nodes and the UCM Hub.

Each UCM sensor node has a unique **Node ID**. The Node ID is used to address that specific physical device on the network.

The system separates:

- **Node identity**: Who is the device?
- **Node name and metadata**: What do we call it and where is it?
- **Sensor data**: What is the device measuring?
- **Heartbeat/status**: Is the device alive and operating?
- **Commands**: What should the device do?

This separation allows nodes to be added, renamed, relocated, or replaced without changing the basic MQTT architecture.

---

# 2. Node Identity

Every UCM node has a unique identifier such as:

```text
UCM-E0A6EC
```

The Node ID is the device's stable network identity.

It is used in MQTT topics:

```text
ucm/node-UCM-E0A6EC/...
```

The Node ID is **not the same thing as the human-readable node name**.

For example:

```text
Node ID:       UCM-E0A6EC
Node name:     JC Office
Description:   Downtown Jersey City
```

The name can change without changing the Node ID.

For example, the node could later be renamed:

```text
Node ID:       UCM-E0A6EC
Node name:     NYC Climate Lab
```

The MQTT address remains:

```text
ucm/node-UCM-E0A6EC/...
```

This is intentional.

---

# 3. MQTT Topic Structure

UCM uses the following general topic structure:

```text
ucm/node-<NODE_ID>/<MESSAGE_TYPE>
```

For example:

```text
ucm/node-UCM-E0A6EC/environment
ucm/node-UCM-E0A6EC/info
ucm/node-UCM-E0A6EC/heartbeat
ucm/node-UCM-E0A6EC/command
```

Each topic has a specific purpose.

| Topic | Purpose |
|---|---|
| `environment` | Environmental sensor measurements |
| `info` | Node identity and metadata |
| `heartbeat` | Operational status |
| `command` | Commands sent to the node |

This creates a predictable interface for every UCM node.

---

# 4. Node Information

When a node connects to the UCM Hub, it publishes information describing itself.

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

This information allows the Hub and other UCM software to associate the technical Node ID with meaningful information.

The important distinction is:

```text
Node ID
    ↓
UCM-E0A6EC
    ↓
stable identity
```

versus:

```text
Node name
    ↓
JC Office
    ↓
human-readable label
```

The Node ID should remain stable even when the name, location, or description changes.

---

# 5. Environmental Data

Environmental measurements are published using the same Node ID.

Example:

```text
ucm/node-UCM-E0A6EC/environment
```

Example payload:

```json
{
  "node": "UCM-E0A6EC",
  "timestamp": "2026-08-10T12:05:52Z",
  "pm1": 5.8,
  "pm25": 6.1,
  "pm4": 6.1,
  "pm10": 6.1,
  "temperature": 32.2,
  "humidity": 37.2,
  "voc": 0.0
}
```

The topic identifies **which node** produced the measurement.

The JSON provides the measurement and timestamp.

This means the data can be consumed by different systems without those systems needing to know anything about the physical hardware.

---

# 6. Heartbeat

Nodes periodically publish a heartbeat showing that they are alive.

Example:

```text
ucm/node-UCM-E0A6EC/heartbeat
```

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

The heartbeat provides basic operational information.

The Hub can therefore distinguish between:

```text
Node exists
```

and:

```text
Node is currently alive and communicating
```

---

# 7. Commands

Commands are sent to the node-specific command topic:

```text
ucm/node-UCM-E0A6EC/command
```

For example, the Hub can request an OTA firmware update with:

```bash
mosquitto_pub -h UCM-HUB.local \
  -t 'ucm/node-UCM-E0A6EC/command' \
  -m '{"command":"ota"}'
```

The important point is that the command is addressed using the **Node ID**, not the human-readable node name.

The command therefore means:

> Send the OTA command to the device whose Node ID is UCM-E0A6EC.

---

# 8. Why Use the Node ID Instead of the Node Name?

This is an important architectural decision.

A name such as:

```text
JC Office
```

is descriptive but not necessarily permanent.

A node might be:

- renamed,
- moved,
- reassigned,
- given a different description,
- transferred to another research project.

The Node ID should remain stable.

Therefore:

```text
Node ID = identity
Node name = description
```

This is similar to the distinction between a person's unique identifier in a database and their display name.

---

# 9. The Node Constructs Its Own Command Topic

The firmware does not need a unique hard-coded command topic for every device.

Instead, it knows its own Node ID and constructs its MQTT topic.

Conceptually:

```text
ucm/
   node-<MY_NODE_ID>/
       command
```

For UCM-E0A6EC:

```text
ucm/node-UCM-E0A6EC/command
```

For another node:

```text
ucm/node-UCM-E0A695/command
```

This means the same firmware architecture can be deployed to many nodes.

Each node automatically listens to its own command channel.

---

# 10. OTA Example

The OTA process demonstrates why this architecture is useful.

The Hub sends:

```text
ucm/node-UCM-E0A6EC/command
```

with:

```json
{
  "command": "ota"
}
```

The node receives the command and performs the update.

In the successful test performed on August 10, 2026, the sequence was:

```text
MQTT command
      ↓
Node receives command
      ↓
OTA firmware downloaded
      ↓
Inactive OTA partition written
      ↓
Next boot partition selected
      ↓
Node reboots
      ↓
New firmware starts
      ↓
Network reconnects
      ↓
MQTT reconnects
      ↓
Firmware validation succeeds
      ↓
Firmware marked VALID
```

The node successfully switched from `ota_1` to `ota_0` and confirmed the new firmware as valid.

The resulting log included:

```text
UCM_OTA: OTA update successful
UCM_OTA: Restarting into new firmware...
```

and after reboot:

```text
UCM_OTA: Running firmware confirmed VALID
P4_SENSOR_TEST: OTA firmware confirmed successfully
```

This demonstrates that the MQTT command, OTA subsystem, partition management, reboot, network reconnection, and firmware validation are functioning together.

---

# 11. Scalable Network Architecture

The same addressing system works as the network grows.

With three nodes:

```text
UCM-HUB
   │
   ├── UCM-E0A695
   ├── UCM-E0A6EC
   └── UCM-XXXXXX
```

Each node has its own MQTT namespace:

```text
ucm/node-UCM-E0A695/...
ucm/node-UCM-E0A6EC/...
ucm/node-UCM-XXXXXX/...
```

Adding another node does not require redesigning the MQTT protocol.

The new node simply receives a unique Node ID and begins publishing under its own namespace.

---

# 12. Separation of Responsibilities

The UCM architecture can therefore be understood as four layers.

### Identity

```text
Who is this device?

UCM-E0A6EC
```

### Metadata

```text
What do we know about it?

JC Office
Downtown Jersey City
40.71780, -74.04310
```

### Telemetry

```text
What is it measuring?

PM2.5
Temperature
Humidity
VOC
etc.
```

### Control

```text
What should it do?

OTA
reboot
configuration
diagnostics
etc.
```

Each layer uses the same Node ID to maintain the relationship between the physical device and its data.

---

# 13. The Design Principle

The central design principle is:

> **Use stable machine-readable identifiers for addressing and human-readable metadata for description.**

This gives UCM a clean separation between the identity of a device and the way humans describe that device.

It also means that the MQTT protocol does not need to change as the network grows.

A node can move.

A node can be renamed.

A node can change location.

A node can receive new firmware.

A node can acquire additional capabilities.

The underlying identity and MQTT addressing remain stable.

---

# 14. Current UCM MQTT Pattern

The current protocol can therefore be summarized as:

```text
ucm/
└── node-<NODE_ID>/
    ├── info
    ├── environment
    ├── heartbeat
    └── command
```

For example:

```text
ucm/
└── node-UCM-E0A6EC/
    ├── info
    ├── environment
    ├── heartbeat
    └── command
```

This provides a simple, predictable, and scalable foundation for the UCM sensor network.