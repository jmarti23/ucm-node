import time
import json
import paho.mqtt.client as mqtt

from sensirion_i2c_driver import I2cConnection
from sensirion_i2c_driver.linux_i2c_transceiver import LinuxI2cTransceiver
from sensirion_i2c_sen5x.device import Sen5xI2cDevice


# ---------------- MQTT CONFIG ----------------
MQTT_BROKER = "192.168.1.236"   # <-- CHANGE THIS
MQTT_TOPIC = "ucm/sensors/sen54"
MQTT_PORT = 1883

client = mqtt.Client()
client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_start()


# ---------------- SENSOR INIT ----------------
with LinuxI2cTransceiver('/dev/i2c-1') as transceiver:
    sensor = Sen5xI2cDevice(I2cConnection(transceiver))

    print("Serial:", sensor.get_serial_number())
    print("Product:", sensor.get_product_name())

    sensor.start_measurement()
    time.sleep(3)

    # ---------------- MAIN LOOP ----------------
    while True:
        v = sensor.read_measured_values()

        data = {
        "pm1": v.mass_concentration_1p0.physical if v.mass_concentration_1p0 else None,
        "pm25": v.mass_concentration_2p5.physical if v.mass_concentration_2p5 else None,
        "pm4": v.mass_concentration_4p0.physical if v.mass_concentration_4p0 else None,
        "pm10": v.mass_concentration_10p0.physical if v.mass_concentration_10p0 else None,
        "temp": v.ambient_temperature.degrees_celsius if v.ambient_temperature else None,
        "rh": v.ambient_humidity.percent_rh if v.ambient_humidity else None,
        "timestamp": time.time()
}
        # Print locally
        print("-" * 40)
        print(json.dumps(data, indent=2))

        # Send MQTT
        try:
            client.publish(MQTT_TOPIC, json.dumps(data))
            info=client.publish(MQTT_TOPIC, json.dumps(data))
            # Publish the data and print the status result directly
            res = client.publish(MQTT_TOPIC, json.dumps(data))
            print("Publish status code: " + str(res[0]) + " (0 is success)")

        except Exception as e:
            print("MQTT send failed:", e)

        time.sleep(2)
