/*

MOTEINO LORAWAN NODE

Copyright (C) 2016 by Xose Pérez <xose dot perez at gmail dot com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <TheThingsNetwork.h>
#include <LowPower.h>
#include "settings.h"

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------

// Hold the last time we wake up, to calculate how much we have to sleep next time
unsigned long wakeTime = millis();

// Hold current sum
double current;

// Counts the one minute parcials, values from 0 to 5 (6 parcials)
byte count1 = 0;

// Counts de 5 minutes power consumptions, values from 0 to 4 (5 values)
byte count2 = 0;

// Holds de 5 minutes power consumptions, values from 0 to 4 (5 values)
unsigned int power[SENDING_COUNTS];

SoftwareSerial loraSerial(SOFTSERIAL_RX_PIN, SOFTSERIAL_TX_PIN);
TheThingsNetwork ttn(loraSerial, Serial, TTN_FREQPLAN);

// -----------------------------------------------------------------------------
// Utils
// -----------------------------------------------------------------------------

void blink(byte times, byte mseconds) {
    pinMode(LED_PIN, OUTPUT);
    for (byte i=0; i<times; i++) {
        if (i>0) delay(mseconds);
        digitalWrite(LED_PIN, HIGH);
        delay(mseconds);
        digitalWrite(LED_PIN, LOW);
    }
    pinMode(LED_PIN, INPUT);
}

// -----------------------------------------------------------------------------
// Readings
// -----------------------------------------------------------------------------

// Based on EmonLib calcIrms method
double getCurrent(unsigned long samples) {

    static double offset = (ADC_COUNTS >> 1);
    unsigned long sum = 0;

    for (unsigned int n = 0; n < samples; n++) {

        // Get the reading
        unsigned long reading = analogRead(CURRENT_PIN);

        // Digital low pass
        offset = ( offset + ( reading - offset ) / 1024 );
        unsigned long filtered = reading - offset;

        // Root-mean-square method current
        sum += ( filtered * filtered );

    }

    double current = CURRENT_RATIO * sqrt(sum / samples) * ADC_REFERENCE / ADC_COUNTS;
    #ifdef DEBUG
        Serial.print("[MAIN] Current (A): ");
        Serial.println(current);
    #endif
    return current;

}

unsigned int getBattery() {
    unsigned int voltage = BATTERY_RATIO * analogRead(BATTERY_PIN);
    #ifdef DEBUG
        Serial.print("[MAIN] Battery: ");
        Serial.println(voltage);
    #endif
    return voltage;
}

// -----------------------------------------------------------------------------
// Sleeping
// -----------------------------------------------------------------------------

void awake() {
    // Nothing to do
}

bool sleepRadio() {
    #ifdef DEBUG
        Serial.println("[MAIN] Sleeping the radio");
    #endif
    bool response = true;
    ttn.sleep(SLEEP_INTERVAL - millis() + wakeTime);
    if (loraSerial.available()) loraSerial.read();
    return response;
}

void sleepController() {
    #ifdef DEBUG
        Serial.println("[MAIN] Sleeping the controller");
        Serial.println();
        delay(10);
    #endif
    attachInterrupt(1, awake, CHANGE);
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
    wakeTime = millis();
    detachInterrupt(1);
    if (loraSerial.available()) loraSerial.read();
    #ifdef DEBUG
        Serial.println("[MAIN] Awake!");
    #endif
}

// -----------------------------------------------------------------------------
// Actions
// -----------------------------------------------------------------------------

// Reads the sensor value and adds it to the accumulator
void doRead() {
    current = current + getCurrent(CURRENT_SAMPLES);
}

// Stores the current average into the minutes array
void doStore() {
    power[count2-1] = (current * MAINS_VOLTAGE) / READING_COUNTS;
    #ifdef DEBUG
        char buffer[50];
        sprintf(buffer, "[MAIN] Storing power in slot #%d: %dW\n", count2, power[count2-1]);
        Serial.print(buffer);
    #endif
    current = 0;
}

// Sends the 5 minute averages
void doSend() {

    byte size = SENDING_COUNTS * 2 + 2;
    byte payload[size];

    for (int i=0; i<SENDING_COUNTS; i++) {
        payload[i*2] = (power[i] >> 8) & 0xFF;
        payload[i*2+1] = power[i] & 0xFF;
    }

    unsigned int voltage = getBattery();
    payload[SENDING_COUNTS * 2] = (voltage >> 8) & 0xFF;
    payload[SENDING_COUNTS * 2 + 1] = voltage & 0xFF;

    #ifdef DEBUG
        Serial.print("[MAIN] Sending: ");
        char buffer[6];
        for (byte i=0; i<size; i++) {
            sprintf(buffer, "0x%02X ", payload[i]);
            Serial.print(buffer);
        }
        Serial.println();
    #endif
    ttn.sendBytes(payload, SENDING_COUNTS * 2 + 2, 1, false);

}

// -----------------------------------------------------------------------------
// RN2483
// -----------------------------------------------------------------------------

void loraReset() {
    pinMode(RN2483_RESET_PIN, OUTPUT);
    digitalWrite(RN2483_RESET_PIN, HIGH);
    delay(50);
    digitalWrite(RN2483_RESET_PIN, LOW);
    delay(50);
    digitalWrite(RN2483_RESET_PIN, HIGH);
    delay(50);
    while (!loraSerial.available()) delay(100);
    while (loraSerial.available()) loraSerial.read();
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

void setup() {

    #ifdef DEBUG
        Serial.begin(SERIAL_BAUD);
        Serial.println("[MAIN] LoRaWAN Current Sensor v0.1-ttn");
    #endif

    //pinMode(CURRENT_PIN, INPUT);
    //pinMode(BATTERY_PIN, INPUT);

    // Warmup the current monitoring
    getCurrent(CURRENT_SAMPLES * 3);

    // Configure radio
    loraSerial.begin(SOFTSERIAL_BAUD);
    loraSerial.flush();
    loraReset();

    #if TTN_ABP
        ttn.personalize(TTN_DEVADDR, TTN_NWKSKEY, TTN_APPSKEY);
        ttn.showStatus();
    #else
        ttn.showStatus();
        ttn.join(TTN_APPEUI, TTN_APPKEY);
    #endif

    // Set initial wake up time
    wakeTime = millis();

}

void loop() {

    // Update counters
    if (++count1 == READING_COUNTS) ++count2;

    // We are only sending if both counters have overflown
    // so to save power we shutoff the radio now if no need to send
    if (count2 < SENDING_COUNTS) sleepRadio();

    // Visual notification
    blink(1, 5);

    // Always perform a new reading
    doRead();

    // We are storing it if count1 has overflown
    if (count1 == READING_COUNTS) doStore();

    // We are only sending if both counters have overflow
    if (count2 == SENDING_COUNTS) {
        blink(3, 5);
        doSend();
        sleepRadio();
    }

    // Overflow counters
    count1 %= READING_COUNTS;
    count2 %= SENDING_COUNTS;

    // Sleep the controller, the radio will wake it up
    sleepController();

}
