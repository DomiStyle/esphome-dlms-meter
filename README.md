# Overview

A custom component for ESPHome for reading meter data sent by the Kaifa MA309M via M-Bus.

# Features

* Exposes all data sent from the smart meter as sensors
* Allows grouping data together in a single report for storing in InfluxDB or similar

# Supported meters

* Kaifa MA309M

# Supported providers

* [TINETZ-Tiroler Netze GmbH](https://www.tinetz.at)
* [Salzburg Netz GmbH](https://www.salzburgnetz.at)
* [Innsbrucker Kommunalbetriebe Aktiengesellschaft](https://www.ikb.at)
* [Vorarlberger Energienetze GmbH](https://www.vorarlbergnetz.at)

# Exposed sensors

* Voltage L1
* Voltage L2
* Voltage L3
* Amperage L1
* Amperage L2
* Amperage L3
* Active Power Plus
* Active Power Minus
* Active Energy Plus
* Active Energy Minus
* Reactive Energy Plus
* Reactive Energy Minus

# Requirements

* ESP32 ([supported by ESPHome](https://esphome.io/#devices))
* RJ11 cable
* M-Bus to UART board (e.g. https://www.mikroe.com/m-bus-slave-click)
* RJ11 breakout board **or** soldering iron with some wires
* ESPHome 1.15.0 or newer

# Notes

* Tested with [ESP32-POE](https://www.olimex.com/Products/IoT/ESP32/ESP32-POE/open-source-hardware) (should work without problems over wifi)

# Software installation

This software installation guide assumes some familiarity with ESPHome.

* Clone this repository into your ESPHome config files: `git clone git@github.com:DomiStyle/esphome-dlms-meter.git`
* Copy the meter01.example.yaml into your ESPHome config folder and adjust it to your needs
* **Don't forget to add your decryption key in meter01.yaml**
* Connect your ESP
* Run `esphome meter01.yaml run` and choose your serial port (or do this via the Home Assistant UI)
* Disconnect the ESP and continue with hardware installation

Make sure your directory structure looks like this:

* config
  * meter01.yaml
  * esphome-dlms-meter
    * The files from this repo (espdm.h, ...)

# Hardware installation

* Cut one end of the RJ11 cable and connect wires to pin 3 & 4 **OR** Plug RJ11 into a breakout board
* Connect everything according to this table:

| **ESP32** | **M-Bus Board**           | **RJ11** | **Notes** |
| --------- | ------------- | ---------------- | ----------- |
| 3.3V        | 3V3 | - | 3.3V power |
| GND      | GND | - | Ground |
| GPIO4       | RX    | - | Connect RX from the M-Bus board to TX from the ESP |
| GPIO36    | TX    | - | Connect TX from the M-Bus board to RX from the ESP |
| -    | MBUS1    | 3 | Connect M-Bus board to smart meter, polarity does not matter |
| -    | MBUS2    | 4 | Connect M-Bus board to smart meter, polarity does not matter |
