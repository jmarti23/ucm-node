# UCM System Architecture

## 1. Overview

The Urban Climate Mesh (UCM) is a distributed environmental sensing and data infrastructure designed to collect, transport, store, visualize, and analyze localized environmental data.

UCM combines:

- Distributed environmental sensor nodes
- Local network connectivity
- MQTT messaging
- Local computing hubs
- Time-series data storage
- APIs and GIS visualization
- Remote device management
- Over-The-Air firmware updates
- Research and analytical infrastructure

The architecture is designed to support deployment from a small number of experimental nodes to larger geographically distributed sensor networks.

The fundamental architectural principle is:

> **Move sensing and initial processing to the edge while keeping data, management, and research infrastructure modular and scalable.**

---

# 2. System Overview

At the highest level, UCM consists of four major layers:

```text
┌──────────────────────────────────────────────┐
│              RESEARCH / APPLICATION          │
│                                              │
│ Dashboards • GIS • Analytics • Research Data │
└───────────────────────▲──────────────────────┘
                        │
┌───────────────────────┴──────────────────────┐
│                    HUB                        │
│                                              │
│ MQTT • Data Ingestion • Database • API • GIS │
└───────────────────────▲──────────────────────┘
                        │
┌───────────────────────┴──────────────────────┐
│                 NETWORK                      │
│                                              │
│ Ethernet • Wi-Fi • MQTT • Local Networking   │
└───────────────────────▲──────────────────────┘
                        │
┌───────────────────────┴──────────────────────┐
│                  EDGE                        │
│                                              │
│ UCM Nodes • Sensors • Local Processing       │
└──────────────────────────────────────────────┘
```

Each layer has a distinct responsibility.

---

# 3. UCM Nodes

The UCM node is the edge component of the system.

A typical node consists of:

- ESP32-P4 processor
- ESP32-C6 wireless coprocessor
- Sensirion SEN54 environmental sensor
- Ethernet connectivity
- Wi-Fi connectivity
- Local firmware
- MQTT client
- OTA update capability
- Local web interface

The node performs the first stage of data processing.

```text
                 UCM NODE
                    │
             ┌──────┴──────┐
             │             │
          ESP32-P4       SEN54
             │
      ┌──────┼────────┐
      │      │        │
   Sensor   MQTT     OTA
   Logic   Client   Manager
      │      │        │
      └──────┼────────┘
             │
        Network Manager
          │        │
      Ethernet    Wi-Fi
```

The node is responsible for:

1. Reading environmental sensors.
2. Processing sensor measurements.
3. Maintaining its identity.
4. Publishing environmental data.
5. Publishing node metadata.
6. Publishing operational heartbeat information.
7. Receiving commands.
8. Performing remote firmware updates.
9. Validating new firmware.
10. Maintaining network connectivity.

---

# 4. Node Identity

Every node has a stable Node ID.

For example:

```text
UCM-E0A6EC
```

The Node ID is the common identifier used throughout the system.

It appears in:

```text
MQTT topics
MQTT payloads
Hub records
Sensor data
Heartbeat messages
Commands
OTA operations
Dashboards
```

The human-readable node name is separate from the Node ID.

For example:

```text
Node ID:      UCM-E0A6EC
Name:         JC Office
Description:  Downtown Jersey City
```

The name and location can change without changing the underlying node identity.

See:

`docs/architecture/node-identity.md`

---

# 5. Network Layer

UCM nodes communicate with a local UCM Hub using Ethernet or Wi-Fi.

Ethernet is preferred when available because it provides a reliable wired connection.

The node can fall back to Wi-Fi when Ethernet is unavailable.

The network architecture is therefore:

```text
                  UCM NODE
                     │
              ┌──────┴──────┐
              │             │
          Ethernet         Wi-Fi
              │             │
              └──────┬──────┘
                     │
                     ▼
                  UCM-HUB
```

The network layer provides connectivity but does not define the data architecture.

MQTT operates above the network layer.

---

# 6. MQTT Messaging Layer

MQTT provides the primary messaging mechanism between nodes and the UCM Hub.

The general topic structure is:

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

The MQTT architecture separates:

```text
Identity
Metadata
Telemetry
Status
Control
```

This creates a predictable interface for every node.

See:

`docs/architecture/mqtt-protocol.md`

---

# 7. UCM Hub

The UCM Hub provides local computing and coordination for a group of UCM nodes.

The current UCM Hub is based on a Raspberry Pi.

The Hub provides:

- MQTT broker
- Data ingestion
- Time-series database
- API services
- GIS services
- Dashboard services
- Local network services
- Node registry
- Remote node management

The current architecture includes:

```text
UCM-HUB
│
├── Mosquitto
│      MQTT broker
│
├── Telegraf
│      Data ingestion
│
├── InfluxDB
│      Time-series database
│
├── API Server
│      Data/API services
│
├── Nginx
│      Web server / reverse proxy
│
└── Grafana
       Visualization
```

The Hub is therefore more than an MQTT broker. It is a local computing and data infrastructure platform.

---

# 8. MQTT Broker

Mosquitto provides the MQTT broker on the Hub.

The broker provides communication between:

```text
UCM Nodes
    │
    ▼
Mosquitto
    │
    ├── Data consumers
    ├── Management services
    └── Research systems
```

The broker decouples sensor nodes from downstream applications.

A node does not need to know whether its data is being consumed by:

- InfluxDB
- Grafana
- the UCM API
- another research application
- another Hub
- an external service

It simply publishes to its MQTT namespace.

---

# 9. Data Ingestion

Environmental data flows from the node through MQTT to the Hub.

The current data path is:

```text
SEN54
  │
  ▼
ESP32-P4
  │
  │ JSON
  ▼
MQTT
  │
  ▼
Mosquitto
  │
  ▼
Telegraf
  │
  ▼
InfluxDB
```

The node therefore does not need to communicate directly with the database.

This provides an important separation between edge devices and data infrastructure.

---

# 10. Data Storage

InfluxDB provides time-series storage for environmental measurements.

The database can store measurements such as:

- PM1
- PM2.5
- PM4
- PM10
- Temperature
- Relative humidity
- VOC
- NOx where available

Measurements are associated with the Node ID and timestamp.

Conceptually:

```text
Node
  │
  ├── identity
  ├── location
  └── measurements
          │
          ├── timestamp
          ├── PM2.5
          ├── temperature
          ├── humidity
          └── VOC
```

This allows historical environmental conditions to be analyzed at the node level.

---

# 11. API Layer

The Hub provides an API between stored data and applications.

The API allows applications to retrieve information without directly accessing the underlying database.

For example:

```text
Dashboard
    │
    ▼
UCM API
    │
    ▼
UCM Data
```

This provides a stable application interface even if the underlying database or storage implementation changes.

The API can provide:

- Latest observations
- Historical measurements
- Node information
- Node locations
- Node status
- Network information

---

# 12. Visualization

The UCM dashboard provides a human-facing view of the network.

The dashboard can combine:

```text
Node locations
       +
Current sensor values
       +
Historical data
       +
Node status
```

A typical architecture is:

```text
                  UCM DATA
                     │
              ┌──────┴──────┐
              │             │
           Grafana        UCM API
              │             │
              │             ▼
              │        GIS / Map
              │             │
              └──────┬──────┘
                     │
                     ▼
                 Researcher
```

This allows the same underlying data to support both operational monitoring and research applications.

---

# 13. OTA Firmware Management

Firmware management is integrated into the UCM control architecture.

The Hub sends an OTA command through MQTT:

```text
ucm/node-<NODE_ID>/command
```

The node receives the command and performs the update.

The node uses two OTA firmware partitions:

```text
ota_0
ota_1
```

One partition runs the current firmware while the other is available for the new image.

After reboot, the new firmware performs validation.

The architecture therefore supports:

```text
Remote update
      ↓
New firmware
      ↓
Reboot
      ↓
Validation
      ↓
VALID
   or
ROLLBACK
```

See:

`docs/architecture/ota-architecture.md`

---

# 14. Command and Control

The MQTT command channel provides remote management of nodes.

Current functionality includes OTA firmware updates.

The architecture can support additional commands such as:

```text
reboot
status
configuration
diagnostics
sensor control
network configuration
```

Commands are addressed using the Node ID.

For example:

```text
ucm/node-UCM-E0A6EC/command
```

This provides a foundation for remote management as the network grows.

---

# 15. Node-to-Hub Relationship

The current UCM architecture treats a Hub as the local coordination point for a group of sensor nodes.

```text
                  UCM-HUB
                     │
        ┌────────────┼────────────┐
        │            │            │
        ▼            ▼            ▼
     Node A        Node B       Node C
```

Each node publishes to its own MQTT namespace.

The Hub aggregates information from all nodes without requiring the nodes to communicate directly with one another.

This creates a hub-and-spoke architecture at the local network level.

---

# 16. Multi-Hub Architecture

The architecture is designed to support multiple UCM Hubs.

For example:

```text
                  UCM NETWORK
                       │
          ┌────────────┴────────────┐
          │                         │
    Operations Hub             Research Hub
          │                         │
      Sensor Nodes              Research
                                Systems
```

An Operations Hub can manage local sensor nodes while a Research Hub consumes selected data for research and analysis.

This separation allows operational infrastructure and research infrastructure to evolve independently.

A larger deployment could therefore contain:

```text
Region A
   │
UCM-HUB-A
   │
Nodes

Region B
   │
UCM-HUB-B
   │
Nodes

Region C
   │
UCM-HUB-C
   │
Nodes
```

Higher-level systems can consume data from multiple Hubs.

---

# 17. Edge-to-Research Data Flow

The complete data path can be represented as:

```text
┌─────────┐
│  SEN54  │
└────┬────┘
     │
     ▼
┌─────────────┐
│ ESP32-P4    │
│ UCM Node    │
└──────┬──────┘
       │
       │ MQTT
       ▼
┌─────────────┐
│  Mosquitto  │
│  UCM-HUB    │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Telegraf   │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  InfluxDB   │
└──────┬──────┘
       │
       ├──────────────► Grafana
       │
       ▼
┌─────────────┐
│   UCM API   │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ GIS / Web   │
│ Dashboard   │
└──────┬──────┘
       │
       ▼
 Researchers
```

This architecture separates sensing, communication, storage, visualization, and research.

---

# 18. Separation of Responsibilities

The UCM architecture deliberately assigns different responsibilities to different components.

| Component | Primary responsibility |
|---|---|
| SEN54 | Environmental sensing |
| ESP32-P4 | Edge processing and node control |
| ESP32-C6 | Wireless connectivity |
| Ethernet/Wi-Fi | Network connectivity |
| MQTT | Messaging |
| Mosquitto | Message brokering |
| Telegraf | Data ingestion |
| InfluxDB | Time-series storage |
| API | Application data access |
| Nginx | Web serving / reverse proxy |
| Grafana | Visualization |
| OTA Manager | Firmware updates |
| Hub | Local coordination and computing |

This separation makes individual components replaceable without requiring the entire system to be redesigned.

---

# 19. Design Principles

The UCM architecture is guided by several principles.

### Edge autonomy

Nodes should be capable of operating independently once configured.

### Stable identity

Every physical node has a persistent Node ID.

### Loose coupling

Nodes communicate with services through defined interfaces rather than direct dependencies.

### Local resilience

The Hub provides local services so the system does not depend entirely on remote cloud infrastructure.

### Modular data architecture

Sensor data can be consumed by multiple systems without changing the sensor node.

### Remote management

Nodes should be manageable without requiring physical access.

### Safe firmware updates

OTA updates must preserve a path back to known-good firmware.

### Scalability

The architecture should support additional nodes, Hubs, and research systems without changing the fundamental protocol.

### Open infrastructure

The architecture should favor open protocols, open-source software, and replaceable components.

---

# 20. Current UCM Architecture

The current implementation can be summarized as:

```text
                     UCM SYSTEM
                         │
       ┌─────────────────┴─────────────────┐
       │                                   │
   EDGE NODES                            HUB
       │                                   │
 ┌─────┴─────┐                    ┌────────┴─────────┐
 │           │                    │                  │
ESP32-P4   SEN54              MQTT Broker       Data Services
 │                              │                  │
 │                         ┌────┴────┐        ┌────┴────┐
 │                         │         │        │         │
Network                   Nodes   Commands  InfluxDB  API/GIS
 │
 ├── Ethernet
 │
 └── Wi-Fi
       │
       ▼
    UCM-HUB
       │
       ├── Mosquitto
       ├── Telegraf
       ├── InfluxDB
       ├── Grafana
       ├── Nginx
       └── API
```

---

# 21. Architectural Direction

The current implementation establishes the foundation for a distributed environmental sensing infrastructure.

The architecture can evolve in several directions without changing its fundamental model.

Future capabilities may include:

- Additional sensor types
- Additional node hardware
- Multiple hardware revisions
- Multiple UCM Hubs
- Hub-to-Hub data exchange
- Research data federation
- Edge analytics
- Local AI/ML processing
- Remote configuration
- Staged firmware deployment
- Automated node discovery
- Device health monitoring
- Long-term environmental datasets

The important constraint is that new capabilities should build on the existing separation between:

```text
Identity
   +
Sensing
   +
Messaging
   +
Storage
   +
Management
   +
Research
```

rather than creating tightly coupled dependencies between components.

---

# 22. Summary

UCM is a distributed edge-to-research infrastructure.

At the edge, UCM nodes collect environmental measurements and communicate through MQTT.

At the Hub, those measurements are ingested, stored, exposed through APIs, and visualized.

The same architecture also provides remote node management and OTA firmware updates.

The system is intentionally modular:

```text
Sensors
   ↓
Nodes
   ↓
Network
   ↓
MQTT
   ↓
Hubs
   ↓
Data
   ↓
Applications
   ↓
Research
```

The architecture is designed so that each layer can evolve independently while maintaining stable interfaces between layers.

The Node ID provides persistent identity across the system, MQTT provides the messaging interface, the Hub provides local computing and data infrastructure, and OTA provides remote firmware lifecycle management.

Together these components form the foundation of the Urban Climate Mesh.