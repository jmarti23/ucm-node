# UCM Node Identity Architecture

## 1. Overview

Every Urban Climate Mesh (UCM) sensor node has a unique **Node ID** that identifies the physical device within the UCM network.

Node identity is separate from the human-readable name, description, and location associated with the node.

This distinction is fundamental to the UCM architecture:

> **The Node ID identifies the device. Metadata describes the device.**

This allows a node to be renamed, relocated, reassigned, or updated without changing its underlying network identity.

---

# 2. The Node ID

A UCM node has a unique identifier such as:

```text
UCM-E0A6EC
```

The Node ID is used throughout the UCM system.

It appears in:

- MQTT topics
- MQTT payloads
- Node metadata
- Heartbeat messages
- Environmental measurements
- Hub databases
- Dashboards
- Logs
- OTA commands

For example:

```text
ucm/node-UCM-E0A6EC/environment
```

The Node ID therefore provides the common key that connects the different parts of the UCM system.

---

# 3. Identity vs. Name

The UCM system deliberately separates machine identity from human-readable naming.

For example:

```text
Node ID:
UCM-E0A6EC

Name:
JC Office

Description:
Downtown Jersey City
```

The Node ID identifies the physical node.

The name and description provide information for people.

The name may change:

```text
JC Office
```

could become:

```text
Urban Climate Lab
```

The Node ID remains:

```text
UCM-E0A6EC
```

The MQTT topics therefore remain unchanged.

---

# 4. Why the Node ID Must Be Stable

The Node ID acts as the node's primary identity throughout the UCM system.

If the human-readable name were used as the identity, changing the name would require changes to:

- MQTT topics
- database records
- dashboards
- monitoring configuration
- command destinations
- historical data associations

Using a stable Node ID avoids this problem.

The principle is:

```text
Node ID → stable identity
Name    → changeable description
Location → changeable metadata
```

---

# 5. Node Identity and MQTT

The Node ID forms the basis of the UCM MQTT namespace.

The general structure is:

```text
ucm/node-<NODE_ID>/<MESSAGE_TYPE>
```

For example:

```text
ucm/node-UCM-E0A6EC/info
ucm/node-UCM-E0A6EC/environment
ucm/node-UCM-E0A6EC/heartbeat
ucm/node-UCM-E0A6EC/command
```

This means that every node has a predictable namespace.

A second node might use:

```text
UCM-E0A695
```

and therefore have:

```text
ucm/node-UCM-E0A695/info
ucm/node-UCM-E0A695/environment
ucm/node-UCM-E0A695/heartbeat
ucm/node-UCM-E0A695/command
```

The same MQTT protocol can therefore be used for any number of nodes.

---

# 6. Node Metadata

A node publishes metadata describing itself.

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

The `node` field contains the Node ID.

The remaining fields describe the node.

Conceptually:

```text
Identity
   │
   └── UCM-E0A6EC

Metadata
   ├── name
   ├── description
   ├── latitude
   └── longitude
```

This allows other UCM systems to associate measurements and status information with a physical location and human-readable description.

---

# 7. Node Identity in Environmental Data

Environmental measurements also include the Node ID.

Example:

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

The Node ID allows the Hub and downstream systems to associate the measurement with the correct physical node.

This remains true even if the node is later moved.

---

# 8. Node Identity in Heartbeats

Heartbeat messages also contain the Node ID.

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

The Hub can therefore associate operational information with the same node identity used for sensor measurements and commands.

---

# 9. Node Identity and Commands

Commands are directed to a node using its Node ID.

For example:

```text
ucm/node-UCM-E0A6EC/command
```

An OTA command can therefore be sent directly to that node:

```json
{
  "command": "ota"
}
```

The command does not need to contain the human-readable node name.

The MQTT topic already identifies the intended node.

This produces a clean separation:

```text
Topic
  ↓
Who should receive the command?

Payload
  ↓
What should the node do?
```

---

# 10. Node Identity and OTA

The same Node ID is used when remotely updating firmware.

For example:

```text
ucm/node-UCM-E0A6EC/command
```

with:

```json
{
  "command": "ota"
}
```

The OTA subsystem therefore does not need a separate identity system.

The MQTT layer identifies the node, while the OTA subsystem performs the firmware update.

This is an example of a broader UCM principle:

> **Different subsystems should share a common node identity rather than maintain separate identities.**

---

# 11. Node Identity Across the UCM System

The Node ID provides the common relationship between systems.

```text
                    UCM-E0A6EC
                         │
          ┌──────────────┼──────────────┐
          │              │              │
        MQTT           Hub          Dashboard
          │              │              │
     ┌────┴────┐         │              │
     │         │         │              │
 telemetry  command   database       map/location
     │         │         │              │
     └─────────┴─────────┴──────────────┘
                         │
                    same node
```

The same identifier follows the node through the entire architecture.

---

# 12. Node Name and Location Are Metadata

The UCM system should treat names and locations as attributes of a node rather than as the node's identity.

For example:

```text
Node ID:      UCM-E0A6EC
Name:         JC Office
Description:  Downtown Jersey City
Latitude:     40.71780
Longitude:    -74.04310
```

The node could later be relocated:

```text
Node ID:      UCM-E0A6EC
Name:         South Bronx Node
Description:  Community monitoring site
Latitude:     [new location]
Longitude:    [new location]
```

The historical identity remains:

```text
UCM-E0A6EC
```

This is important for maintaining continuity in the data.

---

# 13. Historical Data

Because measurements are associated with the stable Node ID, historical observations remain associated with the same device even if its location or name changes.

For example:

```text
UCM-E0A6EC
     │
     ├── 2026-08-01
     │     JC Office
     │
     ├── 2026-09-01
     │     JC Office
     │
     └── 2026-10-01
           South Bronx
```

The physical node remains the same node.

Its metadata changed.

This distinction becomes increasingly important as UCM deployments grow and nodes move between research sites.

---

# 14. Node Identity and Hardware Versions

Node identity should also be distinguished from hardware configuration.

A node can have:

```text
Node ID:
UCM-E0A6EC

Hardware:
ESP32-P4 UCM Node

Hardware revision:
1.x

Firmware:
v0.6.0
```

These represent different concepts.

```text
Node ID       → Which physical node?
Hardware      → What platform?
Hardware rev. → Which hardware design?
Firmware      → What software is installed?
```

This separation becomes important when UCM supports multiple hardware revisions.

Firmware can then determine whether it is compatible with the hardware on which it is being installed.

---

# 15. Node Identity and Reprovisioning

Reprovisioning should not automatically create a new Node ID.

For example, changing:

- Wi-Fi credentials
- node name
- description
- location
- MQTT configuration

does not necessarily mean that the physical node has changed.

The identity remains:

```text
UCM-E0A6EC
```

This allows configuration to change without breaking historical identity.

A new Node ID should generally represent a different physical node.

---

# 16. Node Identity and Replacement

If a physical device is permanently replaced, the replacement should normally receive its own Node ID.

For example:

```text
Old device:
UCM-E0A6EC

Replacement device:
UCM-F3B821
```

This preserves the distinction between two physical devices.

The Hub can maintain metadata indicating that the second device replaced the first.

This is preferable to silently transferring the old identity to a completely different physical device.

---

# 17. Node ID as the Primary Key

Within UCM software systems, the Node ID should function as the primary identifier for a node.

Conceptually:

```text
Node
├── node_id
├── name
├── description
├── latitude
├── longitude
├── hardware_revision
├── firmware_version
└── status
```

The `node_id` is the stable key.

Other properties may change over time.

This structure should be reflected in the Hub's node registry and database design.

---

# 18. Design Principle

The fundamental UCM identity principle is:

> **A node has one stable identity and many changeable attributes.**

The stable identity is the Node ID.

The attributes include:

- name,
- description,
- location,
- hardware revision,
- firmware version,
- network status,
- configuration.

This allows UCM to evolve without losing track of individual physical nodes.

---

# 19. Current UCM Identity Pattern

The current UCM architecture follows this pattern:

```text
Node ID
   │
   ├── MQTT namespace
   │
   ├── node metadata
   │
   ├── environmental data
   │
   ├── heartbeat/status
   │
   ├── commands
   │
   ├── OTA updates
   │
   ├── Hub database
   │
   └── visualization/dashboard
```

For example:

```text
UCM-E0A6EC
```

is the common identifier connecting:

```text
ucm/node-UCM-E0A6EC/info
ucm/node-UCM-E0A6EC/environment
ucm/node-UCM-E0A6EC/heartbeat
ucm/node-UCM-E0A6EC/command
```

This provides a simple and scalable identity model for the UCM network.

---

# 20. Summary

The UCM node identity architecture is based on a simple rule:

```text
Stable Node ID
       +
Changeable Metadata
       =
Persistent Node Identity
```

The Node ID identifies the physical device.

The metadata describes the device.

MQTT uses the Node ID for addressing.

The Hub uses the Node ID for data association.

The dashboard uses the Node ID to associate data with locations.

OTA uses the Node ID to target firmware updates.

As the UCM network grows, the same identity mechanism can continue to support additional nodes, hubs, applications, and services without changing the fundamental protocol.