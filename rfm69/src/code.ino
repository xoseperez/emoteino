 /*

Moteino Power Monitor

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

#include "settings.h"
#include <RFM69Manager.h>
#include <LowPower.h>

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------

RFM69Manager radio;
#if HAS_FLASH
    SPIFlash flash(FLASH_SS, 0xEF30);
#endif

unsigned short count = 0;
double sum = 0;
double max = 0;

// -----------------------------------------------------------------------------
// Hardware
// -----------------------------------------------------------------------------

void blink(byte times, byte mseconds) {
    pinMode(LED_PIN, OUTPUT);
    for (byte i=0; i<times; i++) {
        if (i>0) delay(mseconds);
        digitalWrite(LED_PIN, HIGH);
        delay(mseconds);
        digitalWrite(LED_PIN, LOW);
    }
}

void hardwareSetup() {
    Serial.begin(SERIAL_BAUD);
    pinMode(LED_PIN, OUTPUT);
    pinMode(CURRENT_PIN, INPUT);
    pinMode(BATTERY_PIN, INPUT);
    delay(1);
}

// -----------------------------------------------------------------------------
// RFM69
// -----------------------------------------------------------------------------

void radioSetup() {
    radio.initialize(FREQUENCY, NODEID, NETWORKID, ENCRYPTKEY, GATEWAYID, ATC_RSSI);
    radio.sleep();
}

// -----------------------------------------------------------------------------
// Flash
// -----------------------------------------------------------------------------

#if HAS_FLASH
void flashSetup() {
    if (flash.initialize()) {
        flash.sleep();
    }
}
#endif

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
    return current;

}

unsigned int getBattery() {
    unsigned int voltage = BATTERY_RATIO * analogRead(BATTERY_PIN);
    return voltage;
}

// -----------------------------------------------------------------------------
// Messages
// -----------------------------------------------------------------------------

void sendPower() {

    double current = (count > 0) ? sum / count : 0;
    unsigned int power_avg = current * MAINS_VOLTAGE;
    unsigned int power_max = max * MAINS_VOLTAGE;
    max = 0;

    char buffer[6];
    #if SEND_POWER
        sprintf(buffer, "%d", power_avg);
        radio.send((char *) "POW", buffer, (uint8_t) 2);
    #endif
    #if SEND_POWER_MAX
        sprintf(buffer, "%d", power_max);
        radio.send((char *) "MAX", buffer, (uint8_t) 2);
    #endif

}

void sendBattery() {

    unsigned int voltage = getBattery();

    char buffer[6];
    sprintf(buffer, "%d", voltage);
    radio.send((char *) "BAT", buffer, (uint8_t) 2);

}

void send()  {

    // Send current power
    sendPower();

    // Send battery status once every 10 messages, starting with the first one
    #if SEND_BATTERY
        static unsigned char batteryCountdown = 0;
        if (batteryCountdown == 0) sendBattery();
        batteryCountdown = (batteryCountdown + 1) % SEND_BATTERY_EVERY;
    #endif

    // Radio back to sleep
    radio.sleep();

    // Show visual notification
    blink(1, NOTIFICATION_TIME);

}

// -----------------------------------------------------------------------------
// Common methods
// -----------------------------------------------------------------------------

void setup() {
    hardwareSetup();
    #if HAS_FLASH
        flashSetup();
    #endif
    radioSetup();
    delay(50);
}

void loop() {

    // Reset the accumulator before entering the loop
    sum = 0;
    count = 0;

    // Sleep loop
    // 15 times 4 seconds equals 1 minute,
    // but in real life messages are received every 77 seconds
    // with this set up, so I'm using 13 here instead...
    for (byte i = 0; i < SLEEP_CYCLE; i++) {

        // Sleep for 8 seconds (the maximum the WDT accepts)
        LowPower.powerDown(SLEEP_FOR, ADC_OFF, BOD_OFF);

        // At this point we perform a reading
        double current = getCurrent(CURRENT_SAMPLES);
        sum += current;
        if (current > max) max = current;
        ++count;

        // Debug
        //Serial.print("[MAIN] Current: ");
        //Serial.println(current);
        //delay(50);

    }

    // Send the readings
    send();

}
