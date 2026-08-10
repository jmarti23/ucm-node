# UCM Node Power Requirements

**Status:** Preliminary Engineering Specification  
**Hardware Platform:** Waveshare ESP32-P4-WIFI6-POE-ETH  
**Environmental Sensor:** Sensirion SEN54  
**Last Updated:** August 2026

---

## 1. Purpose

This document establishes the preliminary power requirements for the UCM environmental sensor node based on the manufacturer's published specifications for the hardware currently used by the project.

The purpose is to provide a common technical reference for:

- Node deployment
- Power infrastructure planning
- PoE deployment
- Prototype development
- Network expansion
- Future field testing

The specifications in this document describe the **expected electrical requirements of the component hardware**. They do not yet constitute a measured power specification for a complete UCM node.

A system-level power characterization will be performed during a future field-validation phase.

---

## 2. UCM Node Hardware

The current UCM environmental node is built around the following hardware:

### Processing and communications

**Waveshare ESP32-P4-WIFI6-POE-ETH**

The board integrates:

- ESP32-P4 processor
- ESP32-C6-MINI-1U-H8 wireless coprocessor
- 100 Mbps Ethernet
- IP101 Ethernet PHY
- Flash memory
- Power regulation
- USB and other board interfaces
- PoE capability

### Environmental sensing

**Sensirion SEN54**

The SEN54 provides:

- PM1.0
- PM2.5
- PM4.0
- PM10
- Temperature
- Relative humidity
- VOC Index
- NOx Index

The UCM firmware currently operates the SEN54 as a continuously available environmental sensor.

---

## 3. Power Architecture

The UCM node can operate using network connectivity provided by either Ethernet or Wi-Fi.

For Ethernet deployments, the Waveshare board supports Power over Ethernet, allowing network communication and electrical power to be delivered through the Ethernet connection.

The conceptual architecture is:

```text
             Ethernet / PoE
                   │
                   ▼
     ┌───────────────────────────┐
     │ Waveshare ESP32-P4        │
     │ WIFI6-POE-ETH             │
     │                           │
     │ ESP32-P4                  │
     │ ESP32-C6                  │
     │ Ethernet PHY              │
     │ Power regulation          │
     └─────────────┬─────────────┘
                   │
                   │ 5 V
                   ▼
             ┌──────────┐
             │  SEN54   │
             └──────────┘
```

The board's PoE circuitry and voltage regulation mean that the electrical current available to the low-voltage electronics is not necessarily the same as the current drawn from the PoE source.

This distinction becomes important when determining the final deployment requirements.

---

## 4. Manufacturer-Published Electrical Specifications

### 4.1 Waveshare ESP32-P4-WIFI6-POE-ETH

Waveshare documents the board's power interfaces and PoE functionality, including its 5 V power input and Ethernet-based PoE capability.

The board combines several active subsystems, including the ESP32-P4, ESP32-C6, Ethernet PHY, memory, and power-management circuitry.

Waveshare does not provide a single board-level operating-current specification that represents the complete UCM workload.

Therefore, the Waveshare documentation establishes the **available power interfaces and electrical operating requirements**, but not a definitive UCM node current-consumption figure.

Reference: Waveshare ESP32-P4-WIFI6-POE-ETH documentation.

---

### 4.2 ESP32-P4

The ESP32-P4 is the primary application processor in the UCM node.

Espressif publishes electrical and power specifications for the ESP32-P4 itself. These specifications describe the semiconductor under defined operating conditions.

They should not be interpreted as the current consumption of the complete Waveshare board.

The final node current also includes the ESP32-C6, Ethernet PHY, memory, regulators, peripherals, sensor, and other board-level circuitry.

---

### 4.3 ESP32-C6

The ESP32-C6-MINI-1U-H8 provides wireless connectivity for the UCM node.

In the current implementation, the ESP32-P4 communicates with the C6 through the ESP-Hosted/SDIO architecture.

The C6's contribution to system power will vary with wireless activity, including:

- Association with the access point
- Receive activity
- Transmit activity
- Network traffic
- Signal conditions
- ESP-Hosted activity

Consequently, the C6 datasheet provides component-level information rather than a complete UCM operating-current specification.

---

## 5. Sensirion SEN54 Electrical Requirements

The SEN54 has a defined supply-voltage range specified by Sensirion.

### Supply voltage

```text
Minimum: 4.5 V
Typical: 5.0 V
Maximum: 5.5 V
```

Sensirion specifies different current requirements depending on the operating mode.

Published values include approximately:

| Operating condition | Typical current | Maximum current |
|---|---:|---:|
| Idle | 0.7 mA | 1.0 mA |
| RHT/gas measurement | 6.5 mA | 7.7 mA |
| Broader SEN5x operating specification | ~63 mA average | ~110 mA maximum |

These figures describe the **SEN54 itself**, not the complete UCM node.

The higher current figures are particularly relevant because the UCM configuration uses the sensor's particulate-matter and environmental sensing capabilities rather than operating exclusively in a low-power gas/RHT mode.

Reference: Sensirion SEN5x Datasheet.

---

## 6. Understanding Current Versus Power

Power requirements are often expressed either in watts or amperes.

The relationship is:

```text
P = V × I
```

where:

```text
P = power in watts
V = voltage
I = current in amperes
```

Therefore:

```text
I = P / V
```

This means that an amperage specification is meaningful only when the associated voltage and measurement point are identified.

For example, 1 W represents:

```text
At 5 V:

1 W / 5 V = 0.20 A
```

but:

```text
At 48 V:

1 W / 48 V = 0.021 A
```

The two measurements represent the same power but very different currents.

For UCM, this distinction is particularly important because a node powered through PoE has a high-voltage input to a power-conversion system that ultimately supplies the lower-voltage electronics.

---

## 7. System-Level Current Requirement

The complete UCM node consists of:

```text
ESP32-P4
+
ESP32-C6
+
Ethernet PHY
+
Memory
+
Power regulation
+
Other board electronics
+
SEN54
```

The node's current consumption therefore depends on the operating state of all of these components.

Factors affecting current include:

- Processor workload
- Wi-Fi activity
- Ethernet activity
- SEN54 measurement cycle
- MQTT communication
- Web-server activity
- OTA operations
- Flash activity
- Board peripherals
- Power-conversion efficiency

For this reason, the published SEN54 maximum current of approximately 110 mA should **not** be interpreted as the current requirement of the UCM node.

Likewise, the current specification of the ESP32-P4 or ESP32-C6 should not be interpreted as the current requirement of the complete node.

The complete node must ultimately be characterized as a system.

---

## 8. Power Requirements by Operating Mode

The UCM node has several meaningful operating states.

| Operating mode | Current | Power | Status |
|---|---:|---:|---|
| Boot | TBD | TBD | Field measurement required |
| Idle | TBD | TBD | Field measurement required |
| Normal Ethernet operation | TBD | TBD | Field measurement required |
| Normal Wi-Fi operation | TBD | TBD | Field measurement required |
| Normal environmental sensing | TBD | TBD | Field measurement required |
| MQTT communication | TBD | TBD | Field measurement required |
| OTA update | TBD | TBD | Field measurement required |
| Maximum observed load | TBD | TBD | Field measurement required |

This table is intentionally left open.

The objective is to replace the TBD values with measurements from an actual UCM node running the deployed firmware.

---

## 9. Preliminary Infrastructure Planning

Until system-level measurements are available, power infrastructure should be selected based on the manufacturer's electrical specifications for the Waveshare platform and its power source, with appropriate engineering margin.

For PoE deployments, the network infrastructure should have sufficient power capacity for the node and reasonable allowance for startup and transient conditions.

The UCM project should not currently publish a single amperage value as the definitive node requirement.

Instead, the current engineering position is:

> The UCM node's system-level current requirement has not yet been experimentally characterized.

The published SEN54 current figures provide a component-level reference but are insufficient to establish the current requirement of the assembled node.

---

## 10. Future Field Validation

A complete power characterization should be performed once appropriate measurement equipment is available.

The measurement point should be the actual electrical input to the UCM node.

For a low-voltage power configuration:

```text
Power Source
     │
     ▼
Voltage / Current Measurement
     │
     ▼
Waveshare ESP32-P4-WIFI6-POE-ETH
     │
     ▼
SEN54
```

For a PoE configuration:

```text
PoE Source
     │
     ▼
PoE Power Measurement
     │
     ▼
Waveshare ESP32-P4-WIFI6-POE-ETH
     │
     ▼
SEN54
```

Measurements should include both voltage and current so that actual power consumption can be calculated.

---

## 11. Proposed Field Test Conditions

The following conditions should eventually be measured.

### 11.1 Startup

Measure:

- Startup voltage
- Peak startup current
- Peak startup power
- Duration of startup transient

### 11.2 Normal Ethernet Operation

Measure:

- Idle current
- Normal operating current
- Average power
- Peak power

### 11.3 Normal Wi-Fi Operation

Repeat the measurements with Ethernet unavailable and Wi-Fi active.

This will establish whether Wi-Fi operation has a meaningful power difference from Ethernet operation.

### 11.4 Normal Sensor Operation

Measure the complete system while the SEN54 is operating under the same measurement cycle used by the UCM firmware.

### 11.5 MQTT Activity

Measure the node during normal MQTT communication.

This should reflect the actual production telemetry pattern rather than an artificial test workload.

### 11.6 OTA Update

Measure:

- OTA download
- Flash writing
- Reboot
- Firmware validation

This establishes whether OTA produces a meaningful transient power requirement.

---

## 12. Required Final Measurements

The eventual UCM power specification should report:

### Low-voltage input

```text
Typical voltage:       TBD
Typical current:       TBD A
Typical power:         TBD W

Peak current:           TBD A
Peak power:             TBD W
```

### PoE input

```text
Typical PoE power:      TBD W
Peak PoE power:         TBD W
Typical PoE current:    TBD A
Peak PoE current:       TBD A
```

The measurement conditions and instrumentation should be recorded with the final values.

---

## 13. Engineering Interpretation

The published manufacturer specifications establish the electrical envelope of the individual components.

They do not establish the power consumption of a complete UCM node.

This distinction is important because UCM is not deploying an ESP32-P4, an ESP32-C6, or a SEN54 independently.

It is deploying an integrated system:

```text
Waveshare ESP32-P4-WIFI6-POE-ETH
             +
        Sensirion SEN54
             +
        UCM firmware
             +
       Network services
```

The final power requirement is therefore a **system property**.

---

## 14. Current Status

**Preliminary Engineering Specification**

### Established from manufacturer documentation

- Current UCM hardware platform
- Board power interfaces
- PoE capability
- SEN54 operating voltage
- SEN54 published current specifications
- Component-level electrical requirements

### Not yet established experimentally

- Complete node operating current
- Complete node peak current
- Ethernet operating current
- Wi-Fi operating current
- PoE input current
- Boot current
- OTA current
- Actual power-conversion losses

These values remain **TBD pending field validation**.

---

## 15. References

### Waveshare

Waveshare, *ESP32-P4-WIFI6-POE-ETH Wiki and Product Documentation.*

[Waveshare ESP32-P4-WIFI6-POE-ETH documentation](https://www.waveshare.com/wiki/ESP32-P4-_WIFI6-POE-ETH?utm_source=chatgpt.com)

### Sensirion

Sensirion, *SEN5x Environmental Sensor Datasheet.*

[Sensirion SEN5x documentation](https://sensirion.com/resource/datasheet/sen5x?utm_source=chatgpt.com)

### Espressif

Espressif, *ESP32-P4 Datasheet.*

[Espressif ESP32-P4 documentation](https://www.espressif.com/en/products/socs/esp32-p4?utm_source=chatgpt.com)

---

## 16. Summary

The current UCM node is based on the **Waveshare ESP32-P4-WIFI6-POE-ETH** platform and **Sensirion SEN54** environmental sensor.

Manufacturer documentation provides the electrical requirements and operating characteristics of the individual components. In particular, the SEN54 has published current specifications that provide a useful reference for the sensor portion of the system.

However, the complete UCM node incorporates the processor, wireless coprocessor, Ethernet subsystem, power regulation, sensor, and UCM application workload.

Consequently, the final node-level current and power requirements must be established through measurement.

**The UCM project will treat system-level power consumption as a field-validation requirement and will update this document when measured data become available.**