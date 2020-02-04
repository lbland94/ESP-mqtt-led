// Replace all the placeholders (starting with "<" and ending in ">") with the correct values

// Neopixel
#define DATA_PIN      18
#define NUM_LEDS      300
#define ESP32
# define MQTT_MAX_PACKET_SIZE NUM_LEDS * 14 + 2

// Wifi
char* SSID = "Panera WiFi";
char* WiFiPassword = "";
char* WiFiHostname = "buildStation";

// MQTT
const char* mqtt_server = "mqtt.panerahackathon.com";
int mqtt_port = 1883;
//const char* mqttUser = "<MQTT USER>";
//const char* mqttPass = "<MQTT PASSWORD>";

// Device Specific Topics
const char* commandlTopic = "lights/control";
