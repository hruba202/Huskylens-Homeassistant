#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "HUSKYLENS.h"
#include <ArduinoJson.h>

// WiFi settings
const char* WIFI_SSID     = "";
const char* WIFI_PASSWORD = "";

// MQTT settings
const char* MQTT_SERVER    = "xxx.xxx.x.xx";
const int   MQTT_PORT      = 1883;
const char* MQTT_USER      = "mqtt_user";
const char* MQTT_PASSWORD  = "mqtt-passwd";
const char* MQTT_CLIENT_ID = "ESP32_HuskyLens";
const char* MQTT_TOPIC     = "huskyLens/data";
const char* MQTT_STATUS_TOPIC = "huskyLens/status";

// Home Assistant discovery
const char* HA_DISCOVERY_PREFIX = "homeassistant";
const char* DEVICE_ID   = "huskylens_esp32";
const char* DEVICE_NAME = "HuskyLens";

// I2C piny
const int HUSKYLENS_SDA_PIN = 20;
const int HUSKYLENS_SCL_PIN = 21;

WiFiClient      wifiClient;
PubSubClient    mqttClient(wifiClient);
HUSKYLENS       huskylens;

unsigned long lastMqttReconnectAttempt = 0;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;
bool discoveryPublished = false;

// Forward declarations
void mqttCallback(char* topic, byte* payload, unsigned int length);
boolean reconnectMQTT();
void publishDiscoveryConfigs();

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println("\n=== ESP32 HuskyLens (I2C) → MQTT Bridge ===");

  Wire.begin(HUSKYLENS_SDA_PIN, HUSKYLENS_SCL_PIN);
  while (!huskylens.begin(Wire)) {
    Serial.println("HuskyLens connect failed – zkontroluj I2C zapojeni (SDA/SCL) a napajeni");
    delay(500);
  }
  Serial.println("HuskyLens connected!");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print('.'); }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

  // Zvětšený buffer, protože discovery zprávy jsou delší než 256 B
  mqttClient.setBufferSize(1024);
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}

void loop() {
  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt > MQTT_RECONNECT_INTERVAL) {
      lastMqttReconnectAttempt = now;
      if (reconnectMQTT()) lastMqttReconnectAttempt = 0;
    }
  } else {
    mqttClient.loop();
  }

  if (!huskylens.request()) {
    Serial.println("HuskyLens request failed");
    delay(100);
    return;
  }
  if (huskylens.available()) {
    HUSKYLENSResult result = huskylens.read();

    StaticJsonDocument<256> doc;
    doc["timestamp"] = millis();
    doc["command"]   = result.command;
    doc["id"]        = result.ID;
    doc["x"]         = result.xCenter;
    doc["y"]         = result.yCenter;
    doc["width"]     = result.width;
    doc["height"]    = result.height;

    char payload[256];
    serializeJson(doc, payload);

    Serial.println(payload);
    mqttClient.publish(MQTT_TOPIC, (uint8_t*)payload, strlen(payload));
  }
  delay(50);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message [");
  Serial.print(topic);
  Serial.print("]: ");
  for (unsigned int i = 0; i < length; ++i) Serial.print((char)payload[i]);
  Serial.println();
}

boolean reconnectMQTT() {
  Serial.print("MQTT connect attempt... ");
  boolean connected;
  if (strlen(MQTT_USER) > 0) {
    connected = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD,
                                    MQTT_STATUS_TOPIC, 0, true, "offline");
  } else {
    connected = mqttClient.connect(MQTT_CLIENT_ID, nullptr, nullptr,
                                    MQTT_STATUS_TOPIC, 0, true, "offline");
  }
  if (connected) {
    Serial.println("connected");
    mqttClient.publish(MQTT_STATUS_TOPIC, (uint8_t*)"online", 6, true);
    mqttClient.subscribe("huskyLens/command");

    if (!discoveryPublished) {
      publishDiscoveryConfigs();
      discoveryPublished = true;
    }
  } else {
    Serial.print("failed (rc=");
    Serial.print(mqttClient.state());
    Serial.println(")");
  }
  return connected;
}

// Publikuje retained MQTT Discovery config zprávy pro Home Assistant.
// HA je zpracuje a entity vytvoří automaticky, bez nutnosti ručního YAML.
void publishDiscoveryConfigs() {
  struct SensorDef {
    const char* objectId;     // unikátní část ID entity
    const char* name;         // zobrazovaný název v HA
    const char* valueKey;     // klíč v JSON payloadu (value_template)
    const char* icon;         // mdi ikona
  };

  SensorDef sensors[] = {
    { "id",     "HuskyLens ID",     "id",     "mdi:identifier" },
    { "x",      "HuskyLens X",      "x",      "mdi:axis-x-arrow" },
    { "y",      "HuskyLens Y",      "y",      "mdi:axis-y-arrow" },
    { "width",  "HuskyLens Width",  "width",  "mdi:arrow-expand-horizontal" },
    { "height", "HuskyLens Height", "height", "mdi:arrow-expand-vertical" },
    { "command","HuskyLens Command","command","mdi:cog-outline" },
  };

  for (auto &s : sensors) {
    char configTopic[128];
    snprintf(configTopic, sizeof(configTopic), "%s/sensor/%s/%s/config",
             HA_DISCOVERY_PREFIX, DEVICE_ID, s.objectId);

    StaticJsonDocument<512> doc;
    doc["name"] = s.name;
    doc["unique_id"] = String(DEVICE_ID) + "_" + s.objectId;
    doc["state_topic"] = MQTT_TOPIC;
    doc["value_template"] = String("{{ value_json.") + s.valueKey + " }}";
    doc["availability_topic"] = MQTT_STATUS_TOPIC;
    doc["payload_available"] = "online";
    doc["payload_not_available"] = "offline";
    doc["icon"] = s.icon;

    JsonObject device = doc.createNestedObject("device");
    device["identifiers"][0] = DEVICE_ID;
    device["name"] = DEVICE_NAME;
    device["manufacturer"] = "DFRobot";
    device["model"] = "HuskyLens (ESP32 bridge)";

    char payload[768];
    serializeJson(doc, payload);

    mqttClient.publish(configTopic, (uint8_t*)payload, strlen(payload), true);
    delay(50); // ať broker stihne zpracovat, než pošleme další
  }

  Serial.println("HA discovery configs published.");
}
