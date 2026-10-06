#include <Arduino.h>

#define VOLTAGE_PIN A0

// Voltage divider: 10k on top, 30k on bottom
const float TOP_RESISTOR = 9960.0;
const float BOTTOM_RESISTOR = 29890.0;

// ESP8266 ADC settings
const float ADC_REFERENCE = 3.3;
const float ADC_RANGE = 1023.0;

// Calibration multiplier
const float CALIBRATION = 0.9633;

// Measured voltage divider multiplier
const float DIVIDER_MULTIPLIER = 1.504;

// Sampling settings
const int READ_COUNT = 20;
const unsigned long READ_DELAY = 60000;

// Used to keep track of time and measurements
unsigned long lastReading = 0;
unsigned long readingNumber = 0;


void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("LiPo Battery Voltage Logger");
    Serial.println("Sample,Time_min,Raw_ADC,ADC_Voltage,Battery_Voltage");

    // Allows the first measurement to happen immediately
    lastReading = millis() - READ_DELAY;
}


void loop() {
    unsigned long now = millis();

    if (now - lastReading >= READ_DELAY) {
        lastReading = now;
        readingNumber++;

        float rawSum = 0.0;

        // Collect 20 readings for a more accurate measurement
        for (int sample = 0; sample < READ_COUNT; sample++) {
            rawSum += analogRead(VOLTAGE_PIN);
            delay(10);
        }

        float rawAverage = rawSum / READ_COUNT;

        // Convert the ADC value into the voltage measured at A0
        float measuredVoltage =
            (rawAverage / ADC_RANGE) * ADC_REFERENCE * CALIBRATION;

        // Undo the voltage divider to estimate battery voltage
        float actualVoltage =
            measuredVoltage * DIVIDER_MULTIPLIER;

        // Convert elapsed milliseconds into minutes
        float minutesPassed = now / 60000.0;

        // Print one CSV row
        Serial.print(readingNumber);
        Serial.print(",");

        Serial.print(minutesPassed, 2);
        Serial.print(",");

        Serial.print(rawAverage, 1);
        Serial.print(",");

        Serial.print(measuredVoltage, 3);
        Serial.print(",");

        Serial.println(actualVoltage, 3);
    }
}