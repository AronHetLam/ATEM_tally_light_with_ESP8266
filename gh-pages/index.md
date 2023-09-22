---
title: ATEM tally light
---

Wireless tally light for use with ATEM switchers. Connects over WiFi using only a D1 mini board (ESP8266 WiFi module) and a RGB LED. This solution is __not__ limited by the ATEM switchers' connection limit, making it possible to connect as many as you need.

Should easily be convertable to use with regular Arduino boards and an ESP8266 module, by changeing the include statements and a few other things (however, this is not tested).

__DIY guide__ is available in the [wiki](https://github.com/AronHetLam/ATEM_tally_light_with_ESP8266/wiki/DIY-guide).

# What does it do?
Once set up, it will automatically connect to an ATEM swithcer over WiFi and function as a tally light.

Once the program is uploaded to the ESP8266 the setup is done with a webpage it serves over WiFi where you are able to see status details, and perform the basic setup. Depending on if it's connecting to a known network or not it will serve the webpage on it's IP address, or on [192.168.4.1](HTTP://192.168.4.1) (default) over a softAP (access point) named "Tally light setup". For more detals, see the guide int the [wiki](https://github.com/AronHetLam/ATEM_tally_light_with_ESP8266/wiki/DIY-guide).

As Atem swithcers only allow for 5-8 simultanious clients (dependant on the model) v2.0 introduced Tally Server functionality. This makes the system only require one connection from the switcher, as the tally lights can retransmit data to other tallys. An example setup is shown in the diagram below, where arrows indicate the direction of tally data from swtcher/tally unit to client tally unit.

![Example_setup](https://user-images.githubusercontent.com/5564813/104715918-74cf0f00-5727-11eb-9cac-d05d2b88414c.jpg)

NOTE: As this brings a lot of flexibility with how to connect the units, bear in mind that the ESP8266 isn't that powerful, and is limited to 5 clients each. (In some cases 5 might even be too many).

## Connection and tally state indication
The different states of connection is signalled with LED colors.

Color | Description |
------|--------------
BLUE | Connecting to WiFi
WHITE | Unable to connect to WiFi - serves softAP "Tally light setup", while still trying to connect.
PINK | Connecting to ATEM Swithcher. (Connected to WiFi)
RED | Tally program
GREEN | Tally preview
OFF | Tally neither live or preview (or no power...)

## Modes of operation
By default the tally light works as a normal tally light would in a professional enviroment. However, as I in my personal use case needed it to work a bit differently, I added the following modes of oparation for convenience.

Mode | Description
-----|------------
Normal | As describen in the above table.
Preview stay on | Tally will be green whenever not in program
Program only | Tally will be off whenever not in program

# Use Arduino IDE with ESP8266 module
See details at [ESP8266](https://github.com/esp8266/Arduino) on how to setup and use ESP8266 modules like a regular Arduino board.
They have links for further documentation as well.

# Credits
Based on ATEM libraries for Arduino by [SKAARHOJ](https://www.skaarhoj.com/), available at Git repo: [SKAARHOJ-Open-Engineering](https://github.com/kasperskaarhoj/SKAARHOJ-Open-Engineering)

Inspired by [ATEM_Wireless_Tally_Light](https://github.com/kalinchuk/ATEM_Wireless_Tally_Light) (However, this works completely different)
