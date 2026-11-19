
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// --------------------------------------------------
// Wi-Fi Configuration
// --------------------------------------------------

const char* ssid = "Samsung Galaxy A10e 8801";
const char* password = "0pp0pp0pp";

// --------------------------------------------------
// MQTT Configuration
// --------------------------------------------------

const char* mqtt_server = "broker.mqtt-dashboard.com";
const int mqtt_port = 1883;

// Potentiometer - Publish
const char* potTopic = "ee470/emanuel/pot";

// Local LED - Subscribe
const char* subscribeTopic = "ee470/emanuel/led";

// Physical button - Publish to another ESP8266
const char* buttonTopic = "testtopic/temp/inTopic/akassis";

// --------------------------------------------------
// Pin Configuration
// --------------------------------------------------

const int POT_PIN = A0;
const int BUTTON_PIN = D5;
const int LED_PIN = D7;

// --------------------------------------------------
// Timing Configuration
// --------------------------------------------------

const unsigned long POT_INTERVAL = 15000;
const unsigned long BUTTON_OFF_DELAY = 5000;
const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long MQTT_RETRY_INTERVAL = 5000;

// --------------------------------------------------
// Variables
// --------------------------------------------------

unsigned long lastPotTime = 0;
unsigned long lastDebounceTime = 0;
unsigned long lastMQTTAttempt = 0;
unsigned long buttonOnTime = 0;

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;

bool buttonEventPending = false;
bool waitingToPublishZero = false;
bool wifiWasConnected = false;
bool ledState = false;

WiFiClient espClient;
PubSubClient client(espClient);

// --------------------------------------------------
// Average Potentiometer Readings
// --------------------------------------------------

int readPotentiometer() {

    long total = 0;

    for (int i = 0; i < 20; i++) {
        total += analogRead(POT_PIN);
        delay(2);
    }

    return total / 20;
}

// --------------------------------------------------
// LED Control
// --------------------------------------------------

void setLED(bool state) {

    ledState = state;
    digitalWrite(LED_PIN, state ? HIGH : LOW);

    Serial.print("[LED] ");
    Serial.println(state ? "ON" : "OFF");
}

// --------------------------------------------------
// MQTT Callback
// --------------------------------------------------

void callback(char* topic, byte* payload, unsigned int length) {

    String message = "";

    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    message.trim();

    Serial.println();
    Serial.println("========== MQTT RECEIVED ==========");

    Serial.print("[MQTT] Topic: ");
    Serial.println(topic);

    Serial.print("[MQTT] Payload: ");
    Serial.println(message);

    if (strcmp(topic, subscribeTopic) == 0) {

        if (message == "1") {
            setLED(true);
        }
        else if (message == "0") {
            setLED(false);
        }
    }

    Serial.println("===================================");
}

// --------------------------------------------------
// Serial Monitor Commands
// --------------------------------------------------

void checkSerial() {

    while (Serial.available() > 0) {

        char command = Serial.read();

        if (command == '1') {
            setLED(true);
        }
        else if (command == '0') {
            setLED(false);
        }
    }
}

// --------------------------------------------------
// Wi-Fi Monitoring
// --------------------------------------------------

void checkWiFi() {

    if (WiFi.status() == WL_CONNECTED) {

        if (!wifiWasConnected) {

            wifiWasConnected = true;

            Serial.println();
            Serial.println("[WIFI] Connected!");

            Serial.print("[WIFI] IP Address: ");
            Serial.println(WiFi.localIP());

            Serial.print("[WIFI] RSSI: ");
            Serial.print(WiFi.RSSI());
            Serial.println(" dBm");
        }

        return;
    }

    if (wifiWasConnected) {

        wifiWasConnected = false;
        Serial.println("[WIFI] Connection lost.");
    }
}

// --------------------------------------------------
// MQTT Connection
// --------------------------------------------------

void checkMQTT() {

    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    if (client.connected()) {
        return;
    }

    if (lastMQTTAttempt != 0 &&
        millis() - lastMQTTAttempt < MQTT_RETRY_INTERVAL) {
        return;
    }

    lastMQTTAttempt = millis();

    Serial.println("[MQTT] Connecting to HiveMQ...");

    String clientId = "ESP8266-Emanuel-";
    clientId += String(ESP.getChipId(), HEX);

    if (client.connect(clientId.c_str())) {

        Serial.println("[MQTT] Connected!");

        if (client.subscribe(subscribeTopic)) {

            Serial.print("[MQTT] Subscribed to: ");
            Serial.println(subscribeTopic);
        }
        else {
            Serial.println("[MQTT] Subscription failed.");
        }
    }
    else {

        Serial.print("[MQTT] Connection failed, rc=");
        Serial.println(client.state());
    }
}

// --------------------------------------------------
// Publish MQTT Message
// --------------------------------------------------

bool publishMessage(const char* topic, const char* message) {

    if (!client.connected()) {
        return false;
    }

    bool success = client.publish(topic, message);

    Serial.print("[MQTT] Topic: ");
    Serial.println(topic);

    Serial.print("[MQTT] Payload: ");
    Serial.println(message);

    Serial.println(success
        ? "[MQTT] Publish sent."
        : "[MQTT] Publish failed.");

    return success;
}

// --------------------------------------------------
// Potentiometer Publishing
// --------------------------------------------------

void checkPotentiometer() {

    if (millis() - lastPotTime < POT_INTERVAL) {
        return;
    }

    lastPotTime = millis();

    int adcValue = readPotentiometer();

    float voltage = adcValue * (3.3 / 1023.0);

    char payload[16];
    snprintf(payload, sizeof(payload), "%.3f", voltage);

    Serial.println();
    Serial.println("----- POTENTIOMETER -----");

    Serial.print("[ADC] Value: ");
    Serial.println(adcValue);

    Serial.print("[VOLTAGE] Value: ");
    Serial.print(voltage, 3);
    Serial.println(" V");

    publishMessage(potTopic, payload);
}

// --------------------------------------------------
// Pushbutton Detection
// --------------------------------------------------

void checkButton() {

    bool reading = digitalRead(BUTTON_PIN);

    if (reading != lastButtonReading) {
        lastDebounceTime = millis();
    }

    if (millis() - lastDebounceTime >= DEBOUNCE_DELAY) {

        if (reading != stableButtonState) {

            stableButtonState = reading;

            if (stableButtonState == LOW) {

                Serial.println("[BUTTON] PRESSED");

                // Trigger MQTT publication immediately
                // after the button is debounced.
                if (!waitingToPublishZero) {
                    buttonEventPending = true;
                }
            }
            else {

                Serial.println("[BUTTON] RELEASED");
            }
        }
    }

    lastButtonReading = reading;
}

// --------------------------------------------------
// Pushbutton MQTT Publishing
// --------------------------------------------------

void handleButtonPublishing() {

    // Publish 1 when button is pressed
    if (buttonEventPending && !waitingToPublishZero) {

        if (publishMessage(buttonTopic, "1")) {

            Serial.println("[BUTTON] Published 1");

            buttonEventPending = false;
            waitingToPublishZero = true;

            buttonOnTime = millis();
        }
    }

    // Publish 0 five seconds after publishing 1
    if (waitingToPublishZero &&
        millis() - buttonOnTime >= BUTTON_OFF_DELAY) {

        if (publishMessage(buttonTopic, "0")) {

            Serial.println("[BUTTON] Published 0");

            waitingToPublishZero = false;
        }
    }
}

// --------------------------------------------------
// Setup
// --------------------------------------------------

void setup() {

    Serial.begin(9600);
    delay(1000);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);

    digitalWrite(LED_PIN, LOW);

    Serial.println();
    Serial.println("================================");
    Serial.println("EE 470 ESP8266 MQTT SYSTEM");
    Serial.println("Version 4.2");
    Serial.println("Sections 4.2(A), (B), (C), (E)");
    Serial.println("================================");

    Serial.println("[SYSTEM] Potentiometer: A0");
    Serial.println("[SYSTEM] Pushbutton: D5");
    Serial.println("[SYSTEM] LED: D7");

    Serial.print("[SYSTEM] Button Publish Topic: ");
    Serial.println(buttonTopic);

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);

    Serial.println("[WIFI] Connecting...");
    WiFi.begin(ssid, password);

    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
    client.setKeepAlive(30);

    Serial.println("[SYSTEM] Ready.");
}

// --------------------------------------------------
// Main Loop
// --------------------------------------------------

void loop() {

    checkSerial();
    checkButton();
    checkWiFi();

    if (WiFi.status() == WL_CONNECTED) {

        checkMQTT();

        if (client.connected()) {
            client.loop();
        }
    }

    handleButtonPublishing();
    checkPotentiometer();
}
