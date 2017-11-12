Light Pet Client
================

This repository contains code and related documentation for the Light Pet hardware.

## NodeMCU

The hardware platform is the NodeMCU obtained from [here](http://www.aliexpress.com/item/Update-Industry-4-0-New-esp8266-NodeMCU-v2-Lua-WIFI-networking-development-kit-board-based-on/32358722888.html).
The version I have appears to be the 2nd generation / v1.0 / V2 board ([comparison of NodeMCU boards](http://frightanic.com/iot/comparison-of-esp8266-nodemcu-development-boards))
and is pictured below along with a pinout.
My board appears to be the 'official' Amica version that complies with the NodeMCU spec, but with Aliexpress it's
always a bit difficult to tell.

![Image of NodeMCU v1.0]
(docs/images/nodemcuv2.jpg)

![NodeMCU v1.0 pinout]
(docs/images/NodeMCUPinout.jpg)

## Bill of materials

Quantity | Item | Notes
---------|------|------
1 | [NodeMCU v1.0](http://www.aliexpress.com/item/Update-Industry-4-0-New-esp8266-NodeMCU-v2-Lua-WIFI-networking-development-kit-board-based-on/32358722888.html) |
1 | [TSL257 light to voltage converter](http://www.aliexpress.com/item/Free-shipping-new-original-TSL257-TSL257-LF-red-ray-receiving-sensor/32598330231.html) | I chose this over regular photresistors because this seems to give a greater range of usable results.
1 | [KY-037 microphone module](http://www.aliexpress.com/item/5PCS-High-Sensitivity-Sound-Microphone-Sensor-Detection-Module-For-Arduino-AVR-PIC-KY-037/32569630843.html) | I may swap this out for something else in the future, or some other sort of sound sensor. This isn't quite as sensitive as I'd hoped for, and it's impossible to read a usable sound wave without an external DAC and possibly expanded storage to store the sound samples.
1 | [SHT21 I2C temperature and humdity sensor](http://www.aliexpress.com/item/1pc-Humidity-Sensor-with-I2C-Interface-Si7021-Arduino-Industrial-High-Precision/32562012725.html) |
1 | 4052 analog multiplexer | This is used to multiplex the TSL257 signal and the KY-037 signal into the NodeMCU's single input. I have the TI version (CD4052B) but NXP make an equivalent unit with the same pinout under the part number 74HCT4052.
  | Assorted resistors
  | Assorted capacitors
  | Assorted jumper cables for breadboarding

At the moment everything is constructed on a breadboard (pictured below), however at some point it would
be nice to make things more permanent with a home etched circuit board.

![Light pet on breadboard]
(docs/images/breadboard.jpg)

Below is a schematic of the circuit you need to get all of the sensors working properly. Also note that I have
included a [Fritzing](http://fritzing.org/home/) file for your convenience in the `docs` folder.
Datasheets for the parts that I used can be found in the `docs/datasheets` folder if more information on the reasons
for particular parts is needed.

Briefly:

* The SHT21 datasheet shows a sample circuit which includes the two resistors and the capacitor.
* We use a voltage divider across the TSL257 to just lower the output voltage a little so that it doesn't clip on the NodeMCU's analog input

![Light pet schematic]
(docs/images/schematic.png)

## Software

My NodeMCU board came with the Arduino firmware. If yours doesn't (I think some versions come with Lua based firmware)
then you'll need to flash the Arduino firmware to use the code I've provided.

This code was originally developed using the Arduino IDE, but I've since switched to the wonderful PlatformIO plugin for popular
editors. I think this is a much better solution, and it will come with pre-installed drivers for the ESP8266 and will automatically
install the required libraries since they are defined in the `platformio.ini` file.

We currently use a fork of the ESP8266 Arduino library that has HTTPS support. Once this is mainlined, we should revert back.

## Usage

1. Download the nanopb binaries from https://jpa.kapsi.fi/nanopb/download/ and unzip them to a local directory.
2. Update the path at the top of the makefile to point to the right location
3. Ensure that you have python3 installed, this is used to build the `configuration.json` into a header file for upload
4. Run `make` from the root directory to update the configuration header and to build the protobuf for usage by the arduino code
5. You should now be able to build and upload using PlatformIO