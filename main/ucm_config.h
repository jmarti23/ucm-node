
#define UCM_CONFIG_H


// -----------------------------
// UCM NODE IDENTITY
// -----------------------------

#define NODE_ID       "JC-001"

// Location of this node
#define NODE_LAT      40.7280
#define NODE_LON      -74.0776

// -----------------------------
// WIFI
// -----------------------------

#define WIFI_SSID      "Verizon_GV3VCY-IoT"
#define WIFI_PASS      "ankle6cat3kitty"
#define MAXIMUM_RETRY  5

// -----------------------------
// MQTT
// -----------------------------

#define MQTT_SERVER    "192.168.1.236"
#define MQTT_PORT      1883

// -----------------------------
// SENSOR
// -----------------------------

#define SEN5X_SDA      GPIO_NUM_7
#define SEN5X_SCL      GPIO_NUM_8
#define SEN5X_I2C_PORT I2C_NUM_0
#define SEN5X_ADDRESS  0x69

#endif
'@ | Set-Content .\main\ucm_config.h