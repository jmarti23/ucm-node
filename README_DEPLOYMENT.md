UCM Sensor Node Deployment README


Project Overview

UCM (Urban Climate Mesh) is a distributed environmental sensing platform using Raspberry Pi sensor nodes, Sensirion SEN54 environmental sensors, MQTT messaging, InfluxDB time-series storage, Telegraf ingestion, and Grafana/Leaflet visualization.
This node is configured for autonomous reboot recovery and persistent environmental monitoring.
---

System Architecture


Hub

Component
Value
Hostname
UCM-HUB
IP Address
192.168.1.236
OS
Trixie OS Lite
Services
MQTT, InfluxDB, Telegraf, Nginx, Grafana

---

Sensor Node

Component
Value
IP Address
192.168.1.237
Sensor
Sensirion SEN54
OS
Trixie OS Lite
Runtime Path
/opt/ucm
Service Name
sen54.service

---

Software Stack


Node

- Python 3
- Virtual Environment (venv)
- paho-mqtt
- sensirion-i2c-driver
- sensirion-i2c-sen5x

Hub

- Mosquitto MQTT
- InfluxDB v2
- Telegraf
- Grafana
- Nginx
- Leaflet
---

InfluxDB Configuration

Setting
Value
Organization
NYC-Mesh-Project
Bucket
Microclimate-Data

---

Runtime Directory Structure

```
/opt/ucm/
├── sen54_mqtt.py
├── requirements.txt
├── README_DEPLOYMENT.md
├── sen54.service
└── venv/

```

---

Python Environment


Activate Environment

```
source /opt/ucm/venv/bin/activate

```

Install Dependencies

```
pip install -r requirements.txt

```

---

systemd Service


Service File Location

```
/etc/systemd/system/sen54.service

```

Current Service Configuration

```
[Unit]
Description=UCM SEN54 Sensor Node
After=network-online.target
Wants=network-online.target

[Service]
User=jmarti23
WorkingDirectory=/opt/ucm
ExecStart=/opt/ucm/venv/bin/python /opt/ucm/sen54_mqtt.py
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target

```

---

Service Management


Enable Auto-Start

```
sudo systemctl enable sen54.service

```

Start Service

```
sudo systemctl start sen54.service

```

Restart Service

```
sudo systemctl restart sen54.service

```

Stop Service

```
sudo systemctl stop sen54.service

```

Service Status

```
systemctl status sen54.service

```

---

Logging and Diagnostics


View Live Logs

```
journalctl -u sen54.service -f

```

View Recent Logs

```
journalctl -u sen54.service -n 50

```

---

MQTT Verification


Verify MQTT Messages on Hub

```
mosquitto_sub -h localhost -t "#" -v

```

---

InfluxDB Verification


Verify Influx Connectivity

```
influx bucket list

```

Query Recent Data

```
influx query 'from(bucket:"Microclimate-Data") |> range(start: -10m)'

```

---

I2C Verification


Detect SEN54 Sensor

```
i2cdetect -y 1

```

---

Reboot Recovery Verification

After reboot:

Verify Service Running

```
systemctl status sen54.service

```
Expected:
- active (running)

Verify Process Exists

```
ps aux | grep sen54

```

Verify MQTT Traffic

```
mosquitto_sub -h localhost -t "#" -v

```

Verify Influx Data

```
influx query 'from(bucket:"Microclimate-Data") |> range(start: -5m)'

```

---

Recovery Notes


Common Failure Modes


Python Module Errors

Reinstall dependencies:
```
pip install -r requirements.txt

```

---

Permission Errors

Verify ownership:
```
sudo chown -R jmarti23:jmarti23 /opt/ucm

```

---

Service Fails at Boot

Reload systemd:
```
sudo systemctl daemon-reload
sudo systemctl restart sen54.service

```

---

MQTT Not Receiving Data

Verify:
- Hub reachable
- Mosquitto running
- MQTT broker IP correct
---

Future Improvements

- Multi-node MQTT schema standardization
- Automatic MQTT reconnect handling
- Structured logging
- Outdoor radiation shield enclosure
- Node metadata integration
- Leaflet geospatial overlays
- Sensor calibration tracking
---

Operational Notes

This node has been verified to:
- survive reboot
- automatically reconnect
- resume MQTT publishing
- restore InfluxDB ingestion without manual intervention
This deployment is considered operational.
