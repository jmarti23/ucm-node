# UCM OTA Firmware Update Architecture

## 1. Overview

The Urban Climate Mesh (UCM) node uses **Over-The-Air (OTA) firmware updates** so that deployed ESP32-P4 environmental sensor nodes can receive new firmware without being physically connected to a computer.

The OTA system is designed around three principles:

1. **Remote updates** through the UCM MQTT command system.
2. **Dual firmware partitions** so the existing firmware remains available while a new firmware image is installed.
3. **Firmware validation and rollback** so a failed update does not permanently disable a node.

The result is a system in which a node can update itself remotely while retaining a known-good firmware image as a fallback.

---

# 2. OTA Partition Architecture

The UCM node uses two OTA application partitions:

```text
Flash
┌──────────────────────────────────────────┐
│ nvs                                      │
├──────────────────────────────────────────┤
│ otadata                                  │
├──────────────────────────────────────────┤
│ phy_init                                 │
├──────────────────────────────────────────┤
│ ota_0                                    │
│ Firmware image                           │
├──────────────────────────────────────────┤
│ ota_1                                    │
│ Firmware image                           │
└──────────────────────────────────────────┘
```

The current partition table uses:

```text
ota_0   offset 0x00020000   size 0x001a9000
ota_1   offset 0x001d0000   size 0x001a9000
```

Only one OTA application partition is running at any given time.

The other partition provides space for the next firmware image.

---

# 3. Why Two Firmware Partitions?

The node never needs to overwrite the firmware that is currently running.

For example:

```text
Current:

ota_0 = running firmware
ota_1 = available for update
```

An OTA update writes the new firmware to `ota_1`:

```text
ota_0 = known-good firmware
ota_1 = new firmware
```

The boot partition is then changed so that the next reboot selects `ota_1`.

After reboot:

```text
ota_0 = previous firmware
ota_1 = running new firmware
```

If the new firmware passes validation, it becomes the new known-good firmware.

If it fails, the system can return to the previous firmware.

---

# 4. OTA Update Command

OTA updates are initiated through MQTT.

The node subscribes to its own command topic:

```text
ucm/node-<NODE_ID>/command
```

For example:

```text
ucm/node-UCM-E0A6EC/command
```

The Hub can request an OTA update with:

```bash
mosquitto_pub -h UCM-HUB.local \
  -t 'ucm/node-UCM-E0A6EC/command' \
  -m '{"command":"ota"}'
```

The important architectural feature is that the command is addressed using the node's **stable Node ID**.

The human-readable node name is not used as the MQTT address.

---

# 5. OTA Update Sequence

The complete update process is:

```text
UCM-HUB
   │
   │ MQTT OTA command
   ▼
UCM Node
   │
   │ download firmware
   ▼
Inactive OTA partition
   │
   │ verify/write image
   ▼
OTA data updated
   │
   │ reboot
   ▼
Bootloader
   │
   │ select new partition
   ▼
New firmware
   │
   │ initialize
   │ connect network
   │ connect MQTT
   │ run health checks
   ▼
Firmware validation
   │
   ├── PASS → mark VALID
   │
   └── FAIL → rollback
```

---

# 6. Writing the New Firmware

The OTA subsystem determines which partition is currently running.

The new firmware is written to the **other OTA partition**.

For example, if:

```text
Running partition: ota_1
```

the update is written to:

```text
ota_0
```

The OTA subsystem then changes the boot configuration:

```text
Next boot partition set to: ota_0
```

The running firmware is not overwritten.

---

# 7. Reboot

Once the new image has been successfully written, the node reports:

```text
UCM_OTA: OTA update successful
UCM_OTA: Restarting into new firmware...
```

The node then performs a software reset.

The ESP-IDF bootloader reads the OTA metadata and loads the selected partition.

For example:

```text
Loaded app from partition at offset 0x20000
```

This corresponds to:

```text
ota_0
```

---

# 8. Firmware Identification

After booting, the firmware reports its application information.

Example:

```text
Project name: wifi_test
App version: v0.6-ota-validation-dirty
Compile time: Aug 9 2026 07:38:56
ESP-IDF: v5.5.5-dirty
```

This provides a way to verify that the expected firmware image actually started.

Firmware versions should eventually use a clean release/versioning scheme rather than development suffixes such as `-dirty`.

---

# 9. OTA Image State

ESP-IDF maintains OTA image state information.

Relevant states include:

```text
UNDEFINED
PENDING_VERIFY
VALID
INVALID
ABORTED
```

`UNDEFINED` means that no explicit OTA validation state has been assigned to the image.

A newly installed OTA image can be placed into a verification process before it is considered permanently trusted.

The intended UCM behavior is:

```text
New firmware
     │
     ▼
PENDING_VERIFY
     │
     ▼
Run health checks
     │
 ┌───┴────┐
 │        │
PASS     FAIL
 │        │
 ▼        ▼
VALID   rollback
```

---

# 10. Firmware Validation

A new firmware image should not be considered successful simply because it boots.

The UCM node should verify that essential services operate correctly.

Potential validation checks include:

- Application starts normally.
- SEN54/SEN5x sensor initializes.
- Environmental measurements can be obtained.
- Network connectivity is established.
- MQTT connection succeeds.
- Node metadata can be published.
- Heartbeat can be published.
- Command subscription succeeds.
- Critical application tasks are running normally.
- Sufficient heap remains available.

Only after these checks succeed should the firmware be marked valid.

The current implementation reports:

```text
UCM_OTA: Running firmware confirmed VALID
P4_SENSOR_TEST: OTA firmware confirmed successfully
```

This confirms that the validation mechanism successfully accepted the new firmware during testing.

---

# 11. Rollback

The purpose of the dual-partition architecture is to prevent a failed firmware update from permanently disabling a node.

Suppose the node is running:

```text
ota_0 = known-good firmware
ota_1 = new firmware
```

The node switches to `ota_1`.

If `ota_1` fails validation, the system should return to:

```text
ota_0 = known-good firmware
ota_1 = failed firmware
```

The node can then continue operating using the previous firmware.

This is particularly important for UCM because nodes may eventually be deployed in locations where physical access is difficult or expensive.

---

# 12. Successful OTA Test

On August 10, 2026, the UCM node successfully completed an OTA update.

The node initially ran from:

```text
ota_1
```

The update selected:

```text
ota_0
```

The system reported:

```text
Next boot partition set to: ota_0
UCM_OTA: OTA update successful
UCM_OTA: Restarting into new firmware...
```

After reboot, the ESP32-P4 bootloader loaded the application from:

```text
offset 0x20000
```

which corresponds to:

```text
ota_0
```

The new firmware identified itself as:

```text
v0.6-ota-validation-dirty
```

The node then:

1. Initialized the SEN5x sensor.
2. Initialized Ethernet.
3. Fell back to Wi-Fi when Ethernet was unavailable.
4. Connected to the configured Wi-Fi network.
5. Obtained an IP address.
6. Synchronized time.
7. Connected to MQTT.
8. Subscribed to its command topic.
9. Published node information.
10. Started the web server.
11. Confirmed the firmware as valid.
12. Resumed sensor measurements and heartbeat publication.

The final OTA validation messages were:

```text
UCM_OTA: Running firmware confirmed VALID
P4_SENSOR_TEST: OTA firmware confirmed successfully
```

This constitutes a successful end-to-end OTA update.

---

# 13. OTA and MQTT Architecture

MQTT provides the **control mechanism** for initiating an OTA update.

OTA provides the **firmware update mechanism**.

They are separate layers:

```text
             UCM-HUB
                │
                │ MQTT
                ▼
       node-<NODE_ID>/command
                │
                ▼
          MQTT Manager
                │
                ▼
           OTA Manager
                │
                ▼
        OTA Application Slot
                │
                ▼
            Bootloader
                │
                ▼
          New Firmware
```

This separation is intentional.

MQTT does not need to know how firmware is stored or booted.

The OTA subsystem does not need to know how the command was transported.

Each subsystem has a clear responsibility.

---

# 14. OTA Safety Model

The UCM OTA system should follow this rule:

> **Never replace the last known-good firmware until the new firmware has demonstrated that it can operate correctly.**

This protects against:

- corrupted firmware images,
- incomplete updates,
- application crashes,
- networking failures,
- sensor initialization failures,
- configuration incompatibilities,
- unexpected firmware regressions.

The dual-partition architecture provides the physical mechanism for this protection, while firmware validation provides the logical mechanism.

---

# 15. Future OTA Improvements

The current OTA implementation establishes the basic update and validation architecture.

Future work should include:

### Firmware version management

Establish a formal versioning scheme such as:

```text
v0.7.0
v0.8.0
v1.0.0
```

rather than relying on development build names.

### Hardware compatibility

The node should verify that a firmware image is intended for the hardware on which it is being installed.

Potential checks include:

- hardware revision,
- board type,
- supported peripherals,
- firmware compatibility version.

A firmware intended for one UCM hardware revision should not blindly install on another.

### Image integrity

OTA images should be verified before they are accepted.

### Update authorization

The Hub should eventually be able to determine which nodes are authorized to receive a particular firmware release.

### Staged deployment

For a larger network, firmware updates should be deployable to:

```text
one node
    ↓
small test group
    ↓
larger group
    ↓
entire network
```

This reduces the risk of deploying a defective firmware release to the entire UCM network simultaneously.

---

# 16. Architectural Summary

The UCM OTA architecture combines:

```text
MQTT
  │
  │ command
  ▼
OTA Manager
  │
  │ write inactive partition
  ▼
Dual OTA partitions
  │
  │ reboot
  ▼
ESP-IDF Bootloader
  │
  │ select new image
  ▼
Firmware
  │
  │ health checks
  ▼
VALID
  │
  └── or ──→ ROLLBACK
```

The resulting system allows UCM nodes to be remotely updated while maintaining a recovery path to previously validated firmware.

This architecture is intended to support the eventual deployment of a distributed UCM network in which nodes may be geographically dispersed and not easily accessible for physical firmware updates.