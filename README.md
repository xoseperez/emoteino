# Moteino Energy Monitor

A simple energy monitor shield for Moteino and the needed firmware to make it work with different configurations.

## Features

* LiPo battery connector with battery monitor at A7 (470k resistors for low power)
* Simple biased reading (low consumption voltage divider) with optional burden resistor
* 3.5mm jack input compatible with most CT out there
* Stackable up to 3 shields to read 3 different lines.
* Jumper pads to select input pin (A1, A2 or A3)

Check my **[Moteino Energy Monitor Shield][1]** post for further information about this project.

## Hardware

The hardware is shield for **Moteino** with a **battery monitor circuitry** and a **3.5mm plug** for commonly available current transformers (a.k.a. non-invasive current sensors). There is a footprint for an optional burden resistor (SMD0805) and a bias voltage divider using 2 470K resistors for low power consumption.

![Energy Monitor Shield Schematic](/docs/images/emoteino_schema_0.2.20161214.png)

The battery monitoring is based on a voltage divider with (again) two 470k resistors. The downstream resistor is tied to GND and the mid point to A7.

![Energy Monitor Shield](/docs/images/20161214_102044x.jpg)

## Firmware for Moteino + RFM69

### Dependencies

The code is very straight forward and there are comments where I thought it was important. It uses the following libraries:

* **[RFM69_ATC][3]** by Felix Rusu and Thomas Studwell
* **[Low-Power][4]** by RocketScream

These libraries will be automatically installed if you use [PlatformIO][2].

It also relies on my **RFM69Manager library** to wrap RFM69_ATC. This library manages radio setup and also message sending protocol. Messages from this node have the following format: ```<key>:<value>:<packetID>```. Packet ID is then used in the gateway to detect duplicates or missing packets from a node. If you don't want to send the packetID change the SEND_PACKET_ID value in RFM69Manager.h to 0.

### Configuration

Rename or copy the settings.h.sample file to settings.h and change its values to fit your needs. Check the descriptions for each value.

### Flashing

The project is ready to be build using **[PlatformIO][2]**.
Please refer to their web page for instructions on how to install the builder. Once installed connect the Moteino to your favourite FTDI-like programmer and:

```bash
> platformio run --target upload
```

[1]: http://tinkerman.cat/moteino-energy-monitor-shield/
[2]: http://www.platformio.org
[3]: https://github.com/LowPowerLab/RFM69
[4]: https://github.com/rocketscream/Low-Power/
