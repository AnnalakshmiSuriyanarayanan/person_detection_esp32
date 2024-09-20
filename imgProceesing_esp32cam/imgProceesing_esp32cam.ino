#include <WiFi.h>
#include <esp32cam.h>
#include <WebServer.h>
#include <PubSubClient.h> // Include the PubSubClient library
// #define APP_VER "0.1"


#define FLASH_LED_PIN 4

// WiFi credentials
const char* WIFI_SSID = "IVATechnos11";
const char* WIFI_PASS = "Technos2020";

// MQTT broker details
const char* MQTT_SERVER = "test.mosquitto.org"; // Replace with your MQTT broker's address
const int MQTT_PORT = 1883; // Replace with your MQTT broker's port

WiFiClient espClient;
PubSubClient mqttClient(espClient); // Use PubSubClient
WebServer server(80);

static auto loRes = esp32cam::Resolution::find(320, 240);
static auto midRes = esp32cam::Resolution::find(350, 530);
static auto hiRes = esp32cam::Resolution::find(1200, 800);

const int ledPin = 2; // LED connected to digital pin 2
bool isLedOn = false; // Track the LED state

void serveJpg() {
  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("CAPTURE FAIL");
    server.send(503, "", "");
    return;
  }
  Serial.printf("CAPTURE OK %dx%d %db\n", frame->getWidth(), frame->getHeight(), static_cast<int>(frame->size()));
  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");
  WiFiClient client = server.client();
  frame->writeTo(client);
}

void handleJpgLo() {
  if (!esp32cam::Camera.changeResolution(loRes)) {
    Serial.println("SET-LO-RES FAIL");
  }
  serveJpg();
}

void handleJpgHi() {
  if (!esp32cam::Camera.changeResolution(hiRes)) {
    Serial.println("SET-HI-RES FAIL");
  }
  serveJpg();
}

void handleJpgMid() {
  if (!esp32cam::Camera.changeResolution(midRes)) {
    Serial.println("SET-MID-RES FAIL");
  }
  serveJpg();
}

// Handle LED control
void handleLedOn() {
  if (!isLedOn) { // Only publish if the state changes
    digitalWrite(ledPin, HIGH); // Turn LED on
    isLedOn = true;  // Update the state
    Serial.println("LED is ON");
    server.send(200, "text/plain", "LED is ON");
    mqttClient.publish("IVA/IRCTRL/AC/ON_AC", "1"); // Publish to MQTT
    delay(3000);
   mqttClient.publish("IVA/IRCTRL/FAN/ON", "3"); // Publish to MQTT
  } else {
    server.send(200, "text/plain", "LED is already ON");
  }
}

void handleLedOff() {
  if (isLedOn) { // Only publish if the state changes
    digitalWrite(ledPin, LOW); // Turn LED off
    isLedOn = false;  // Update the state
    Serial.println("LED is OFF");
    server.send(200, "text/plain", "LED is OFF");
    mqttClient.publish("IVA/IRCTRL/AC/OFF", "0"); // Publish to MQTT
    delay(3000);
    mqttClient.publish("IVA/IRCTRL/FAN/OFF", "2000"); // Publish to MQTT
  } else {
    server.send(200, "text/plain", "LED is already OFF");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  pinMode(ledPin, OUTPUT); // Initialize the LED pin as an output
  digitalWrite(ledPin, isLedOn); // Initialize the LED state
  pinMode(FLASH_LED_PIN, OUTPUT);

  // Set the flash LED pin to LOW to turn off the flash LED
  digitalWrite(FLASH_LED_PIN, LOW);
  {
    using namespace esp32cam;
    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(hiRes);
    cfg.setBufferCount(2);
    cfg.setJpeg(80);
    bool ok = Camera.begin(cfg);
    Serial.println(ok ? "CAMERA OK" : "CAMERA FAIL");
  }
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.print("http://");
  Serial.println(WiFi.localIP());
  Serial.println("  /cam-lo.jpg");
  Serial.println("  /cam-hi.jpg");
  Serial.println("  /cam-mid.jpg");
  Serial.println("  /led-on");
  Serial.println("  /led-off");
  server.on("/cam-lo.jpg", handleJpgLo);
  server.on("/cam-hi.jpg", handleJpgHi);
  server.on("/cam-mid.jpg", handleJpgMid);
  server.on("/led-on", handleLedOn);
  server.on("/led-off", handleLedOff);
  server.begin();

  // Setup MQTT
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
}

void loop() {
  server.handleClient();
  if (!mqttClient.connected()) {
    // Reconnect to the MQTT broker
    while (!mqttClient.connected()) {
      String clientId = "ESP32Client-";
      clientId += String(random(0xffff), HEX);
      if (mqttClient.connect(clientId.c_str())) {
        Serial.println("MQTT connected");
      } else {
        delay(5000);
      }
    }
  }
  mqttClient.loop();
}

