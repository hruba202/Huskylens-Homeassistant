#include <WiFi.h>
#include <PubSubClient.h>
#include <HuskyLensAffection.h>
#include <EEPROM.h>

// WiFi Configuration
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// MQTT Configuration
const char* mqtt_server = "YOUR_MQTT_BROKER_IP";
const int mqtt_port = 1883;
const char* mqtt_user = "YOUR_MQTT_USER";
const char* mqtt_password = "YOUR_MQTT_PASSWORD";
const char* mqtt_client_id = "esp32_huskylens";

// MQTT Topics
const char* topic_publish = "homeassistant/huskylens/detect";
const char* topic_subscribe = "homeassistant/huskylens/command";

// HuskyLens Configuration
#define HUSKYLENS_SERIAL Serial1  // RX2, TX2 on ESP32

// Global variables
WiFiClient espClient;
PubSubClient client(espClient);
HuskyLensAffection huskylens;

void setup() {
  Serial.begin(115200);
  HUSKYLENS_SERIAL.begin(9600, SERIAL_8N1, 16, 17);  // RX=16, TX=17
  
  delay(2000);
  Serial.println("\n\nHuskyLens ESP32 MQTT - Starting...");
  
  // Initialize HuskyLens
  initHuskyLens();
  
  // Connect to WiFi
  setup_wifi();
  
  // Setup MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqtt_callback);
}

void loop() {
  // Maintain MQTT connection
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // Read HuskyLens data
  if (huskylens.request()) {
    if (huskylens.isBoxDetected()) {
      publishDetections();
    }
  } else {
    Serial.println("HuskyLens request failed");
  }
  
  delay(100);  // Small delay between reads
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connection failed");
  }
}

void reconnect() {
  int attempts = 0;
  while (!client.connected() && attempts < 5) {
    Serial.print("Attempting MQTT connection...");
    
    if (client.connect(mqtt_client_id, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      // Subscribe to commands
      client.subscribe(topic_subscribe);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
      attempts++;
    }
  }
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);
  
  // Parse command from MQTT
  String command = "";
  for (int i = 0; i < length; i++) {
    command += (char)payload[i];
  }
  
  Serial.print("Command: ");
  Serial.println(command);
  
  // Process commands
  if (command == "learn") {
    // Add learning command if needed
    Serial.println("Learn mode triggered");
  }
}

void publishDetections() {
  String payload = "{";
  payload += "\"count\":" + String(huskylens.count()) + ",";
  payload += "\"detections\":[";
  
  for (int i = 0; i < huskylens.count(); i++) {
    if (i > 0) payload += ",";
    
    HuskyLensResult result = huskylens.resultQueue[i];
    payload += "{";
    payload += "\"id\":" + String(result.ID) + ",";
    payload += "\"x\":" + String(result.xCenter) + ",";
    payload += "\"y\":" + String(result.yCenter) + ",";
    payload += "\"width\":" + String(result.width) + ",";
    payload += "\"height\":" + String(result.height);
    payload += "}";
  }
  
  payload += "]}";
  
  // Publish to MQTT
  client.publish(topic_publish, payload.c_str());
  
  Serial.println("Published: " + payload);
}

void initHuskyLens() {
  Serial.println("Initializing HuskyLens...");
  
  if (!huskylens.begin(HUSKYLENS_SERIAL)) {
    Serial.println("HuskyLens initialization failed");
  } else {
    Serial.println("HuskyLens initialized successfully");
  }
}
