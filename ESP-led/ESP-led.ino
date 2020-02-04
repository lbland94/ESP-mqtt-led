#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <ArduinoJson.h>
#include "config.h"


int LED_BUILTIN = 2;

// FastLED
CRGB leds[NUM_LEDS];
int ledArray[NUM_LEDS][3]; // Custom array to allow us to buffer changes before sending them
int ledArrayTwo[NUM_LEDS][3]; // Array to store changes for fading

// Wifi
WiFiClient espClient;

// MQTT
PubSubClient client(espClient);
const uint8_t bufferSize = 20;
char buffer[bufferSize];

// Reconnect Variables
unsigned long reconnectStart = 0;
unsigned long lastReconnectMessage = 0;
unsigned long messageInterval = 1000;
int currentReconnectStep = 0;
boolean offlineMode = true;
boolean recovered = false;

unsigned long lastLog = 0;

// pixel variables
int brightness = 255;
unsigned long colourDelay = 1;
unsigned long colourStep = 6;
unsigned long previousMillis = 0;

// Reconnect to wifi and mqtt, possibly across multiple loops
void reconnect() {
  // 0 - Turn the LED on and log the reconnect start time
  if (currentReconnectStep == 0) {
    digitalWrite(LED_BUILTIN, LOW);
    reconnectStart = millis();
    currentReconnectStep++;
  
   // If the ESP is reconnecting after a connection failure then wait a second before starting the reconnect routine
    if (offlineMode == false) {
      delay(1000);
    }
  }

  // If we've previously had a connection and have been trying to connect for more than 2 minutes then restart the ESP.
  // We don't do this if we've never had a connection as that means the issue isn't temporary and we don't want the relay
  // to turn off every 2 minutes.
  if (offlineMode == false && ((millis() - reconnectStart) > 120000)) {
    Serial.println("Restarting!");
    ESP.restart();
  }

  // 1 - Check WiFi Connection
  if (currentReconnectStep == 1) {
    if (WiFi.status() != WL_CONNECTED) {
      if ((millis() - lastReconnectMessage) > messageInterval) {
        Serial.print("Awaiting WiFi Connection (");
        Serial.print((millis() - reconnectStart) / 1000);
        Serial.println("s)");
        lastReconnectMessage = millis();
      }
    }
    else {
      Serial.println("WiFi connected!");
      Serial.print("SSID: ");
      Serial.print(WiFi.SSID());
      Serial.println("");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.println("");

      lastReconnectMessage = 0;
      currentReconnectStep = 2;
    }
  }

  // 2 - Check MQTT Connection
  if (currentReconnectStep == 2) {
    if (!client.connected()) {
      if ((millis() - lastReconnectMessage) > messageInterval) {
        Serial.print("Awaiting MQTT Connection (");
        Serial.print((millis() - reconnectStart) / 1000);
        Serial.println("s)");
        lastReconnectMessage = millis();

        String clientId = "buildStation-";
        clientId += String(random(0xffff), HEX);
        client.connect(clientId.c_str());
      }

      // Check the MQTT again and go forward if necessary
      if (client.connected()) {
        Serial.println("MQTT connected!");
        Serial.println("");

        lastReconnectMessage = 0;
        currentReconnectStep = 3;
      }

      // Check the WiFi again and go back if necessary
      else if (WiFi.status() != WL_CONNECTED) {
        currentReconnectStep = 1;
      }
    }
    else {
      Serial.println("MQTT connected!");
      Serial.println("");

      lastReconnectMessage = 0;
      currentReconnectStep = 3;
    }
  }

  // 3 - All connected, turn the LED back on and then subscribe to the MQTT topics
  if (currentReconnectStep == 3) {
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off

    // MQTT Subscriptions
    client.subscribe(commandlTopic);

    if (offlineMode == true) {
      offlineMode = false;
      Serial.println("Offline Mode Deactivated");
      Serial.println("");
    }

    currentReconnectStep = 0; // Reset
  }
}

// Update physical strip from ledArray variable
void updateStripFromLedArray() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i][0] = ledArray[i][0];
    leds[i][1] = ledArray[i][1];
    leds[i][2] = ledArray[i][2];
  }
  FastLED.show();
}

// Fade physical strip to colors stored in ledArrayTwo
void fadeToColour() {
    unsigned long currentMillis = millis();
    if (previousMillis > currentMillis || currentMillis - previousMillis >= colourDelay) {
      int count = 0; // Reset the count
      for (int pixel = 0;pixel < NUM_LEDS;pixel ++){               // For every pixel
        if (ledArray[pixel][0] != ledArrayTwo[pixel][0] | ledArray[pixel][1] != ledArrayTwo[pixel][1] | ledArray[pixel][2] != ledArrayTwo[pixel][2]) {
          for (int colour = 0;colour < 3;colour++) {                    // For each colour
            if (ledArray[pixel][colour] < ledArrayTwo[pixel][colour]) {          // If the colour value is less that it should be
              ledArray[pixel][colour] += colourStep;                                 // Add colourStep
            }
            else if (ledArray[pixel][colour] > ledArrayTwo[pixel][colour]) {     // If the colour value is more that it should be
              ledArray[pixel][colour] -= colourStep;                                      // Subtract colourStep
            }
            if (abs(ledArray[pixel][colour] - ledArrayTwo[pixel][colour]) < colourStep) {
              ledArray[pixel][colour] = ledArrayTwo[pixel][colour];
            }
            leds[pixel][colour] = ledArray[pixel][colour];
          }
        }
        else {
          count ++;
        }
      }
      FastLED.show();
    previousMillis = currentMillis;
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on to indicate no connection

  Serial.begin(115200);

  // FastLED setup
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);

  // Wifi setup
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID);
  WiFi.setHostname(WiFiHostname);

  // MQTT setup
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("> MQTT Received");

  if (String(commandlTopic).equals(topic)) {
    if ((char)payload[0] == '[') {       // It's a JSON array
      char message_buff[NUM_LEDS * 14 + 3];
      for (int i = 0; i < length; i++) {
        message_buff[i] = payload[i];
        if (i == (length - 1)) {        // If we're at the last character
          message_buff[i + 1] = '\0';   // Add a string terminator to the next character
        }
      }  
      DynamicJsonBuffer jsonBuffer;                            // Step 1: Reserve memory space
      JsonArray& root = jsonBuffer.parseArray(message_buff);    // Step 2: Deserialize the JSON string

      if (!root.success()) {
        Serial.println("parseArray() failed");
      }
      else if (root.success()) {
        for (int i=0; i<NUM_LEDS; i++) {
          if (i<root.size()) {
            JsonArray& nestedRoot = root[i];
            for (int j=0; j<nestedRoot.size(); j++) {
              ledArrayTwo[i][j] = nestedRoot[j];
            }
          } else {
            for (int j=0; j<3; j++) {
              ledArrayTwo[i][j] = 0;
            }
          }
        }
      }
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  else {
    client.loop();
  }
  fadeToColour();

//  if (millis() - lastLog > 5000) {
//    lastLog = millis();
//    Serial.print("[ ");
//    for (int i=0; i<NUM_LEDS; i++) {
//    Serial.print("[");
//      for (int j=0; j<3; j++) {
//        Serial.print(ledArray[i][j]);
//        Serial.print(", ");
//      }
//      Serial.print("], ");
//    }
//    Serial.println("]");
//  }
  yield();
}
