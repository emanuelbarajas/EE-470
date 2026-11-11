
/*
 * ------------------------------------------------------------
 * Project/Program Name : EE 470 ESP8266 IoT Sensor System
 * File Name            : main.cpp
 * Version              : 5.0
 * Author               : Emanuel Barajas
 * Date                 : 11/10/2026
 *
 * Purpose:
 * Collect photoresistor and ultrasonic measurements
 * when a pushbutton or tilt switch is activated.
 *
 * Measurements are captured even without WiFi.
 * Failed deliveries are retried every 30 seconds.
 * Original sensor readings and measurement times
 * are preserved during retransmission.
 *
 * Inputs:
 * A0 - Photoresistor
 * D1 - Ultrasonic TRIG
 * D2 - Ultrasonic ECHO
 * D5 - Pushbutton (node_1)
 * D6 - Tilt switch (node_2)
 * USB Serial - Time zone selection
 *
 * Outputs:
 * Sensor readings, timestamps, WiFi information,
 * HTTP responses, and retransmission debug messages.
 *
 * Dependencies:
 * ESP8266WiFi
 * ESP8266HTTPClient
 * WiFiClientSecureBearSSL
 * ArduinoJson
 * time.h
 *
 * Notes:
 * - Pacific Time is the default.
 * - Pending messages are stored in RAM.
 * - A valid clock is required before delivery.
 * - HTTPS certificate validation is disabled
 *   for initial testing.
 * ------------------------------------------------------------
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>
#include <time.h>

// --------------------------------------------------
// WiFi Configuration
// --------------------------------------------------

const char* WIFI_SSID = "Samsung Galaxy A10e 8801";
const char* WIFI_PASSWORD = "0pp0pp0pp";

// --------------------------------------------------
// Server Configuration
// --------------------------------------------------

const char* SERVER_URL =
    "https://emanuelbarajas.com/"
    "AssignmentESPProgramming/insert_esp8266.php";

const char* TIME_API_BASE =
    "https://timeapi.io/api/Time/current/zone"
    "?timeZone=";

// --------------------------------------------------
// Pin Configuration
// --------------------------------------------------

#define LIGHT_PIN A0
#define TRIG_PIN D1
#define ECHO_PIN D2
#define BUTTON_PIN D5
#define TILT_PIN D6

// --------------------------------------------------
// Timing
// --------------------------------------------------

const unsigned long DEBOUNCE_TIME = 250;
const unsigned long RETRY_INTERVAL = 30000;
const unsigned long WIFI_RETRY_INTERVAL = 10000;
const unsigned long TIME_ZONE_WAIT = 10000;

// --------------------------------------------------
// Time Zone Configuration
// --------------------------------------------------

String selectedTimeZone = "America/Los_Angeles";

// --------------------------------------------------
// Switch State
// --------------------------------------------------

struct SwitchState {
    bool lastRawState;
    bool stableState;
    unsigned long lastChangeTime;
};

SwitchState buttonState = {false, false, 0};
SwitchState tiltState = {false, false, 0};

// --------------------------------------------------
// Measurement Structure
// --------------------------------------------------

struct Measurement {
    String node;
    String timestamp;
    String timeZone;

    float lightVoltage;
    float distanceCM;

    unsigned long captureMillis;
    unsigned long lastAttempt;

    unsigned int attempts;
    bool occupied;
};

const int MAX_PENDING = 10;
Measurement pending[MAX_PENDING];

// --------------------------------------------------
// Clock State
// --------------------------------------------------

bool clockReady = false;
bool clockWasReady = false;

// --------------------------------------------------
// Function Prototypes
// --------------------------------------------------

void connectWiFi();
void checkWiFi();
void printNetworkInfo();

void initializeClock();
void checkClock();
bool hasValidClock();

void printTimeZoneMenu();
void selectTimeZone();
void setTimeZone(char choice);

String getCurrentTime();

String getLocalTimestamp(
    time_t epoch,
    const String &zone
);

String getTimestampAt(
    unsigned long captureTime,
    const String &zone
);

const char* getPosixTimeZone(
    const String &zone
);

float readLightVoltage();
float readDistanceCM();

void checkSwitches();

void updateSwitch(
    SwitchState &state,
    bool currentState,
    const char* switchName,
    const char* nodeName
);

void handleActivation(const char* nodeName);

bool addMeasurement(
    const String &node,
    const String &timestamp,
    const String &zone,
    float light,
    float distance,
    unsigned long captureTime
);

void processPendingMeasurements();

bool sendMeasurement(Measurement &measurement);

String urlEncode(const String &value);

// --------------------------------------------------
// Setup
// --------------------------------------------------

void setup() {

    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("================================");
    Serial.println("EE 470 ESP8266 IoT SENSOR SYSTEM");
    Serial.println("Version 5.0");
    Serial.println("================================");

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    digitalWrite(TRIG_PIN, LOW);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(TILT_PIN, INPUT_PULLUP);

    buttonState.lastRawState =
        digitalRead(BUTTON_PIN) == LOW;

    buttonState.stableState =
        buttonState.lastRawState;

    buttonState.lastChangeTime = millis();

    tiltState.lastRawState =
        digitalRead(TILT_PIN) == LOW;

    tiltState.stableState =
        tiltState.lastRawState;

    tiltState.lastChangeTime = millis();

    for (int i = 0; i < MAX_PENDING; i++) {
        pending[i].occupied = false;
        pending[i].attempts = 0;
    }

    Serial.println("[DEBUG] GPIO initialized.");
    Serial.println("[DEBUG] Debounce: 250 ms");
    Serial.println("[DEBUG] Retry: 30 seconds");
    Serial.println("[DEBUG] Offline collection enabled.");

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    connectWiFi();

    initializeClock();

    selectTimeZone();

    Serial.println();
    Serial.println("[SYSTEM] Ready.");
    Serial.println("[SYSTEM] Pushbutton = node_1");
    Serial.println("[SYSTEM] Tilt switch = node_2");
    Serial.println("[SYSTEM] Offline collection enabled.");
    Serial.println("[SYSTEM] Enter 1-7 to change time zone.");
}

// --------------------------------------------------
// Main Loop
// --------------------------------------------------

void loop() {

    checkWiFi();
    checkClock();

    checkSwitches();

    processPendingMeasurements();

    if (Serial.available()) {

        char choice = Serial.read();

        if (choice >= '1' && choice <= '7') {
            setTimeZone(choice);
        }
    }

    delay(10);
}

// --------------------------------------------------
// WiFi Connection
// --------------------------------------------------

void connectWiFi() {

    Serial.println();
    Serial.println("[WIFI] Connecting...");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long startTime = millis();

    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - startTime < 15000
    ) {

        delay(500);
        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {

        Serial.println("[WIFI] Connected!");
        printNetworkInfo();

    } else {

        Serial.println("[WIFI] Not connected.");
        Serial.println("[SYSTEM] Offline mode available.");
    }
}

// --------------------------------------------------
// WiFi Monitoring
// --------------------------------------------------

void checkWiFi() {

    static unsigned long lastAttempt = 0;
    static bool wasConnected = false;

    bool connected = WiFi.status() == WL_CONNECTED;

    if (connected && !wasConnected) {
        Serial.println();
        Serial.println("[WIFI] Connection established.");
        printNetworkInfo();
    }

    if (!connected && wasConnected) {
        Serial.println();
        Serial.println("[WIFI] Connection lost.");
        Serial.println("[SYSTEM] Offline mode active.");
    }

    // Reconnect silently every 10 seconds
    if (!connected &&
        millis() - lastAttempt >= WIFI_RETRY_INTERVAL) {

        WiFi.reconnect();
        lastAttempt = millis();
    }

    wasConnected = connected;
}

// --------------------------------------------------
// Network Information
// --------------------------------------------------

void printNetworkInfo() {

    Serial.print("[WIFI] IP Address: ");
    Serial.println(WiFi.localIP());

    Serial.print("[WIFI] MAC Address: ");
    Serial.println(WiFi.macAddress());

    Serial.print("[WIFI] RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
}

// --------------------------------------------------
// Initialize NTP Clock
// --------------------------------------------------

void initializeClock() {

    Serial.println("[TIME] Initializing clock...");

    // Synchronize UTC time using NTP.
    configTime(
        0,
        0,
        "pool.ntp.org",
        "time.nist.gov"
    );

    if (WiFi.status() != WL_CONNECTED) {

        Serial.println("[TIME] Waiting for WiFi.");
        return;
    }

    unsigned long start = millis();

    while (
        !hasValidClock() &&
        millis() - start < 6000
    ) {
        delay(200);
    }

    if (hasValidClock()) {

        clockReady = true;
        clockWasReady = true;

        Serial.println("[TIME] Clock synchronized.");

    } else {

        Serial.println("[TIME] Clock not synchronized yet.");
    }
}

// --------------------------------------------------
// Check Clock
// --------------------------------------------------

void checkClock() {

    clockReady = hasValidClock();

    if (clockReady && !clockWasReady) {

        Serial.println();
        Serial.println("[TIME] Clock synchronized.");
        Serial.println("[TIME] Offline timestamps available.");
    }

    clockWasReady = clockReady;
}

// --------------------------------------------------
// Verify Clock
// --------------------------------------------------

bool hasValidClock() {

    time_t now = time(nullptr);

    return now > 1700000000;
}

// --------------------------------------------------
// Time Zone Menu
// --------------------------------------------------

void printTimeZoneMenu() {

    Serial.println();
    Serial.println("========== TIME ZONE MENU ==========");
    Serial.println("1) Eastern Time Zone (ET)");
    Serial.println("2) Central Time Zone (CT)");
    Serial.println("3) Mountain Time Zone (MT)");
    Serial.println("4) Pacific Time Zone (PT)");
    Serial.println("5) Alaska Time Zone (AKT)");
    Serial.println("6) Hawaii-Aleutian Time Zone (HAT)");
    Serial.println("7) Atlantic Time Zone (AT)");
    Serial.println();

    Serial.println("Select Your Time Zone");
    Serial.println("Default: Pacific Time (4)");
    Serial.println("Enter a number from 1 to 7:");
}

// --------------------------------------------------
// Initial Time Zone Selection
// --------------------------------------------------

void selectTimeZone() {

    printTimeZoneMenu();

    unsigned long start = millis();

    while (millis() - start < TIME_ZONE_WAIT) {

        if (Serial.available()) {

            char choice = Serial.read();

            if (choice >= '1' && choice <= '7') {

                setTimeZone(choice);
                return;
            }
        }

        delay(10);
    }

    Serial.println("[TIME] Default: Pacific Time.");
}

// --------------------------------------------------
// Change Time Zone
// --------------------------------------------------

void setTimeZone(char choice) {

    switch (choice) {

        case '1':
            selectedTimeZone = "America/New_York";
            break;

        case '2':
            selectedTimeZone = "America/Chicago";
            break;

        case '3':
            selectedTimeZone = "America/Denver";
            break;

        case '4':
            selectedTimeZone = "America/Los_Angeles";
            break;

        case '5':
            selectedTimeZone = "America/Anchorage";
            break;

        case '6':
            selectedTimeZone = "Pacific/Honolulu";
            break;

        case '7':
            selectedTimeZone = "America/Puerto_Rico";
            break;

        default:
            return;
    }

    Serial.print("[TIME] Selected Time Zone: ");
    Serial.println(selectedTimeZone);
}

// --------------------------------------------------
// POSIX Time Zone Mapping
// --------------------------------------------------

const char* getPosixTimeZone(
    const String &zone
) {

    if (zone == "America/New_York")
        return "EST5EDT,M3.2.0/2,M11.1.0/2";

    if (zone == "America/Chicago")
        return "CST6CDT,M3.2.0/2,M11.1.0/2";

    if (zone == "America/Denver")
        return "MST7MDT,M3.2.0/2,M11.1.0/2";

    if (zone == "America/Anchorage")
        return "AKST9AKDT,M3.2.0/2,M11.1.0/2";

    if (zone == "Pacific/Honolulu")
        return "HST10";

    if (zone == "America/Puerto_Rico")
        return "AST4";

    return "PST8PDT,M3.2.0/2,M11.1.0/2";
}

// --------------------------------------------------
// Convert UTC Epoch to Selected Local Time
// --------------------------------------------------

String getLocalTimestamp(
    time_t epoch,
    const String &zone
) {

    setenv("TZ", getPosixTimeZone(zone), 1);
    tzset();

    struct tm localTime;

    if (localtime_r(&epoch, &localTime) == nullptr) {
        return "";
    }

    char buffer[24];

    if (strftime(
        buffer,
        sizeof(buffer),
        "%Y-%m-%d %H:%M:%S",
        &localTime
    ) == 0) {
        return "";
    }

    return String(buffer);
}

// --------------------------------------------------
// Timestamp for Captured Measurement
// --------------------------------------------------

String getTimestampAt(
    unsigned long captureTime,
    const String &zone
) {

    if (!hasValidClock()) {
        return "";
    }

    time_t now = time(nullptr);

    unsigned long elapsed =
        millis() - captureTime;

    time_t capturedEpoch =
        now - (time_t)(elapsed / 1000UL);

    return getLocalTimestamp(
        capturedEpoch,
        zone
    );
}

// --------------------------------------------------
// Retrieve Current Time From timeapi.io
// --------------------------------------------------

String getCurrentTime() {

    if (WiFi.status() != WL_CONNECTED) {
        return "";
    }

    Serial.println("[TIME] Requesting timestamp...");

    BearSSL::WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(8000);

    String url =
        String(TIME_API_BASE) + selectedTimeZone;

    Serial.print("[TIME] API Zone: ");
    Serial.println(selectedTimeZone);

    if (!http.begin(client, url)) {

        Serial.println("[TIME] API connection failed.");
        return "";
    }

    int code = http.GET();

    Serial.print("[TIME] HTTP Response: ");
    Serial.println(code);

    if (code != HTTP_CODE_OK) {

        http.end();
        return "";
    }

    String response = http.getString();
    http.end();

    JsonDocument doc;

    if (deserializeJson(doc, response)) {

        Serial.println("[TIME] JSON parsing failed.");
        return "";
    }

    String dateTime = doc["dateTime"] | "";

    if (dateTime.length() < 19) {
        return "";
    }

    String timestamp =
        dateTime.substring(0, 19);

    timestamp.replace("T", " ");

    Serial.print("[TIME] API Timestamp: ");
    Serial.println(timestamp);

    return timestamp;
}

// --------------------------------------------------
// Read Photoresistor
// --------------------------------------------------

float readLightVoltage() {

    int raw = analogRead(LIGHT_PIN);

    float voltage =
        raw * (3.3 / 1023.0);

    Serial.print("[SENSOR] Light ADC: ");
    Serial.println(raw);

    return voltage;
}

// --------------------------------------------------
// Read Ultrasonic Sensor
// --------------------------------------------------

float readDistanceCM() {

    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    unsigned long duration =
        pulseIn(ECHO_PIN, HIGH, 30000);

    if (duration == 0) {

        Serial.println("[ERROR] Ultrasonic: No echo.");
        return -1.0;
    }

    return (duration * 0.0343) / 2.0;
}

// --------------------------------------------------
// Check Switches
// --------------------------------------------------

void checkSwitches() {

    bool buttonActive =
        digitalRead(BUTTON_PIN) == LOW;

    bool tiltActive =
        digitalRead(TILT_PIN) == LOW;

    updateSwitch(
        buttonState,
        buttonActive,
        "PUSHBUTTON",
        "node_1"
    );

    updateSwitch(
        tiltState,
        tiltActive,
        "TILT",
        "node_2"
    );
}

// --------------------------------------------------
// Debounce Switches
// --------------------------------------------------

void updateSwitch(
    SwitchState &state,
    bool currentState,
    const char* switchName,
    const char* nodeName
) {

    if (currentState != state.lastRawState) {

        state.lastChangeTime = millis();
        state.lastRawState = currentState;
    }

    if (
        millis() - state.lastChangeTime >= DEBOUNCE_TIME &&
        currentState != state.stableState
    ) {

        state.stableState = currentState;

        if (currentState) {

            Serial.println();

            Serial.print("[EVENT] ");
            Serial.print(switchName);
            Serial.println(" ACTIVATED");

            handleActivation(nodeName);

        } else {

            Serial.print("[EVENT] ");
            Serial.print(switchName);
            Serial.println(" RELEASED");
        }
    }
}

// --------------------------------------------------
// Handle New Activation
// --------------------------------------------------

void handleActivation(const char* nodeName) {

    unsigned long captureTime = millis();

    Serial.println();
    Serial.println("========== NEW MEASUREMENT ==========");

    Serial.print("[DATA] Node: ");
    Serial.println(nodeName);

    // Capture sensor values immediately
    float light = readLightVoltage();
    float distance = readDistanceCM();

    if (distance < 0) {

        Serial.println("[ERROR] Invalid distance.");
        return;
    }

    String timestamp =
        getTimestampAt(
            captureTime,
            selectedTimeZone
        );

    // If clock unavailable, save without timestamp.
    // Timestamp is reconstructed after synchronization.

    if (timestamp.length() == 0) {

        Serial.println("[TIME] Clock unavailable.");
        Serial.println("[TIME] Timestamp pending.");

    } else {

        Serial.print("[TIME] Measurement Timestamp: ");
        Serial.println(timestamp);
    }

    Serial.print("[DATA] Light Voltage: ");
    Serial.print(light, 3);
    Serial.println(" V");

    Serial.print("[DATA] Distance: ");
    Serial.print(distance, 2);
    Serial.println(" cm");

    if (addMeasurement(
        nodeName,
        timestamp,
        selectedTimeZone,
        light,
        distance,
        captureTime
    )) {

        Serial.println("[QUEUE] Measurement saved.");

    } else {

        Serial.println("[ERROR] Queue full.");
    }

    Serial.println("=====================================");
}

// --------------------------------------------------
// Add Measurement to Queue
// --------------------------------------------------

bool addMeasurement(
    const String &node,
    const String &timestamp,
    const String &zone,
    float light,
    float distance,
    unsigned long captureTime
) {

    for (int i = 0; i < MAX_PENDING; i++) {

        if (!pending[i].occupied) {

            pending[i].node = node;
            pending[i].timestamp = timestamp;
            pending[i].timeZone = zone;

            pending[i].lightVoltage = light;
            pending[i].distanceCM = distance;

            pending[i].captureMillis = captureTime;
            pending[i].lastAttempt = 0;
            pending[i].attempts = 0;

            pending[i].occupied = true;

            return true;
        }
    }

    return false;
}

// --------------------------------------------------
// Process Pending Measurements
// --------------------------------------------------

void processPendingMeasurements() {

    for (int i = 0; i < MAX_PENDING; i++) {

        if (!pending[i].occupied) {
            continue;
        }

        unsigned long now = millis();

        bool ready =
            pending[i].attempts == 0 ||
            now - pending[i].lastAttempt >= RETRY_INTERVAL;

        if (!ready) {
            continue;
        }

        pending[i].lastAttempt = now;
        pending[i].attempts++;

        Serial.println();
        Serial.println("[HTTP] Transmission attempt");

        Serial.print("[HTTP] Attempt: ");
        Serial.println(pending[i].attempts);

        Serial.print("[HTTP] Node: ");
        Serial.println(pending[i].node);

        // No WiFi: count scheduled attempt.
        if (WiFi.status() != WL_CONNECTED) {

            Serial.println("[RETRY] WiFi unavailable.");
            Serial.println("[RETRY] Measurement preserved.");
            Serial.println("[RETRY] Next attempt in 30 seconds.");

            break;
        }

        // Reconstruct timestamp only if missing.
        if (pending[i].timestamp.length() == 0) {

            pending[i].timestamp =
                getTimestampAt(
                    pending[i].captureMillis,
                    pending[i].timeZone
                );

            if (pending[i].timestamp.length() == 0) {

                Serial.println("[RETRY] Waiting for clock sync.");
                Serial.println("[RETRY] Next attempt in 30 seconds.");

                break;
            }

            Serial.print("[TIME] Recovered Timestamp: ");
            Serial.println(pending[i].timestamp);
        }

        if (sendMeasurement(pending[i])) {

            Serial.println("[QUEUE] Delivery confirmed.");
            pending[i].occupied = false;

        } else {

            Serial.println("[RETRY] Delivery not confirmed.");
            Serial.println("[RETRY] Next attempt in 30 seconds.");
        }

        // One pending measurement per loop
        break;
    }
}

// --------------------------------------------------
// Send Measurement to PHP
// --------------------------------------------------

bool sendMeasurement(Measurement &measurement) {

    BearSSL::WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(10000);

    if (!http.begin(client, SERVER_URL)) {

        Serial.println("[ERROR] Server connection failed.");
        return false;
    }

    http.addHeader(
        "Content-Type",
        "application/x-www-form-urlencoded"
    );

    String postData =
        "node_name=" + urlEncode(measurement.node) +
        "&time_received=" + urlEncode(measurement.timestamp) +
        "&light_voltage=" + String(measurement.lightVoltage, 3) +
        "&distance_cm=" + String(measurement.distanceCM, 2);

    Serial.print("[HTTP] Timestamp: ");
    Serial.println(measurement.timestamp);

    Serial.print("[HTTP] Light: ");
    Serial.println(measurement.lightVoltage, 3);

    Serial.print("[HTTP] Distance: ");
    Serial.println(measurement.distanceCM, 2);

    int httpCode = http.POST(postData);

    Serial.print("[HTTP] Response Code: ");
    Serial.println(httpCode);

    if (httpCode <= 0) {

        Serial.print("[ERROR] HTTP request failed: ");
        Serial.println(http.errorToString(httpCode));

        http.end();
        return false;
    }

    // Keep error output short.
    if (httpCode != 200 && httpCode != 201) {

        Serial.print("[ERROR] HTTP ");
        Serial.println(httpCode);

        http.end();
        return false;
    }

    String response = http.getString();
    http.end();

    Serial.print("[HTTP] Server Response: ");
    Serial.println(response);

    JsonDocument doc;

    DeserializationError error =
        deserializeJson(doc, response);

    if (error) {

        Serial.println("[ERROR] Invalid JSON response.");
        return false;
    }

    String status = doc["status"] | "";

    if (httpCode == 201 && status == "success") {

        Serial.println("[SUCCESS] New measurement stored.");
        return true;
    }

    if (httpCode == 200 && status == "duplicate") {

        Serial.println("[SUCCESS] Measurement already stored.");
        return true;
    }

    Serial.println("[ERROR] Server rejected measurement.");
    return false;
}

// --------------------------------------------------
// URL Encoding
// --------------------------------------------------

String urlEncode(const String &value) {

    String encoded = "";
    const char* hex = "0123456789ABCDEF";

    for (unsigned int i = 0; i < value.length(); i++) {

        unsigned char c = value[i];

        if (
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~'
        ) {

            encoded += (char)c;

        } else {

            encoded += '%';
            encoded += hex[c >> 4];
            encoded += hex[c & 15];
        }
    }

    return encoded;
}
