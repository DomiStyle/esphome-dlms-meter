# Overview

A custom component for ESPHome for reading meter data sent by the Kaifa MA309M.

# Features

* Exposes all data sent from the smart meter as sensors
* Allows grouping data together in a single report for storing in InfluxDB or similar

# Supported meters

* Kaifa MA309M

# Supported providers

* [Tinetz](https://www.tinetz.at/)

# Requirements

* ESP32 ([supported by ESPHome](https://esphome.io/#devices))
* RJ11 cable
* ESPHome 1.15.0 or newer

# Notes

* Tested with [ESP32-POE](https://www.olimex.com/Products/IoT/ESP32/ESP32-POE/open-source-hardware) (should work without problems over wifi)

# Software installation

This software installation guide assumes some familiarity with ESPHome.

* Pull this repository into your ESPHome config files
* Adjust the meter01.example.yaml to your needs, copy it into your ESPHome config folder as meter01.yaml
* **Don't forget to add your decryption key in meter01.yaml**
* Connect your ESP
* Run `esphome meter01.yaml run` and choose your serial port (or do this via the Home Assistant UI)
* Disconnect the ESP and continue with hardware installation

# Hardware installation

TODO
