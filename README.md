# Using pms5003 air quality monitor with esp8266

I live in California, due to climate change, every year we have fires
and hence smoke. The smoke has a significant impact on [air
quality index](https://en.wikipedia.org/wiki/Air_quality_index). The
air quality index gets bad and it is important to know if it is
safe to go outside without a mask.

The EPA via [https://www.airnow.gov/](https://www.airnow.gov/)
provides air quality information.

Sites like https://www.purpleair.com/ allow people to buy air quality
monitors and provide information for the map. Interestingly on
https://fire.airnow.gov the square readings are from
https://www.purpleair.com.

I noticed that [purpleair use the esp8266 and the
pms5003](https://www2.purpleair.com/pages/technology), so I decided to
try and build my own indoor solution. Note that
https://www.purpleair.com/ uses two of the pms5003 sensors. There seem
to be many concerns about the accuracy of air quality sensors. This is 
why https://www.purpleair.com/ use two sensors.

## PMS5003
This device is really easy to use it requires 5V power (provided by
esp8266 vin pin). The default operating mode of the device is to
continuously generate 32 bytes via a serial interface. The esp8266
contains at least one serial interface, which can be connected to the
pms5003. Be aware that uploading of code from the arduino IDE via
serial port will not work if the pms5003 is connected.

## Wiring
Just three wires required
```
esp8266		pms5003
gnd	-	gnd
vin	-	vcc
rxd	-	tx
```

## esp8266

### Installing the software
Instructions on installing the software can be found below. Firstly
edit *config.ino* and set your **ssid** and **password**. The code
supports OTA (Over the air updates). The code has to loaded only once
via the USB<->serial inteface, subsequent updates can be over the air.
There is continous debugging output on the serial port. In my setup I
have the esp8266 connected to a raspberry pi. The raspberry pi
provides power via USB and captures debugging output.

$ **cu -l /dev/ttyUSB0 -s 9600**
```
Connected.
PM1.0: 5 ug/m3
PM2.5: 7 ug/m3
PM10: 7 ug/m3
```

### Finding IP address of the device
#### The device appears in local DNS
In my setup dhcp reqests appear in DNS https://en.wikipedia.org/wiki/Dnsmasq
#### From serial interface
The arduino IDE has a serial line monitor.
```
Connected to inter net
IP address: 172.16.1.69
mDNS responder started (esp8266-pms5003)
HTTP server started
OTA updater started
```
#### mDNS
##### MacOS
$ dns-sd -q esp8266-pms5003
```
DATE: ---Thu 27 Aug 2020---
23:06:09.336  ...STARTING...
Timestamp     A/R Flags if Name                          Type  Class   Rdata
23:06:09.339  Add     2  0 esp8266-pms5003.lan.          Addr   IN     172.16.1.69
```
#### Linux if the avahi daemon is running.
$ avahi-resolve --name esp8266-pms5003.local
```
esp8266-pms5003.local	172.16.1.69
```
### Useful URLs

$ **curl esp8266-pms5003**
```
PM1.0: 4 ug/m3
PM2.5: 6 ug/m3
PM10: 6 ug/m3
```

https://prometheus.io/

$ **curl esp8266-pms5003/metrics**
```
# HELP pm_1_0 particles less than 1.0 microns
# TYPE pm_1_0 gauge
pm_1_0 5
# HELP pm_2_5 particles less than 2.5 microns
# TYPE pm_2_5 gauge
pm_2_5 6
# HELP pm_10_0 particles less than 10.0 microns
# TYPE pm_10_0 gauge
pm_10_0 6
```
$ **curl esp8266-pms5003/stats**
```
read 194488
bad length: 0
bad packet: 0
	 bad ch1: 0
	 bad ch2: 0
	 bad packet length: 0
	 bad checksum: 0
time since boot: 168258287ms
```

## Components

### PMS5003
I purchased mine from [adafruit.com]( https://www.adafruit.com/product/3686)

[The pms5003 datasheet](https://cdn-shop.adafruit.com/product-files/3686/plantower-pms5003-manual_v2-3.pdf)

### esp8266
This is processor is made by [espressif.com](https://www.espressif.com/en/products/socs/esp8266/overview)

It can be programed using the [Arduino IDE](https://www.arduino.cc/)

There are many variants of the esp8266 my personal favourite is the [ESP8266 NodeMCU CP2102 ESP-12E](https://www.amazon.com/gp/product/B081CSJV2V/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1)

Instructions on how to install software on the esp8266 [https://arduino-esp8266.readthedocs.io/](https://arduino-esp8266.readthedocs.io/en/latest/).

### Breadboard
https://en.wikipedia.org/wiki/Breadboard

### Dupoint wires
https://www.amazon.com/dupont-cable/s?k=dupont+cable

