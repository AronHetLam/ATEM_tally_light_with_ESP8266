# ATEM-tally-light
Wireless tally light for use with ATEM switchers. Connects over WiFi using only a D1 mini board (ESP8266 WiFi module) and a RGB LED. Should easily be convertable to use with regular Arduino boards and an ESP2866 module, by changeing the include statements and a few other things (however, this is not tested).

__DIY guide__ is available in the [wiki](https://github.com/AronHetLam/ATEM_tally_light_with_ESP8266/wiki/DIY-guide).

# What does it do?
Once set up, it will automatically connect to an ATEM swithcer over WiFi and function as a tally light.

Once the program is uploaded to the ESP8266 the setup is done with a webpage it serves over WiFi where you are able to see status details, and perform the basic setup. Depending on if it's connecting to a known network or not it will serve the webpage on it's IP address, or on [192.168.4.1](HTTP://192.168.4.1) (default) over a softAP (access point) named "Tally light setup". (On first run you might run into issues with earlier WiFi settings if it connects to a known WiFi, as it can set an invalid static IP-address. To avoid this, select "_Sketch + WiFi Settings_" in Tools -> Erase Flash: "_setting_" -> ... when uploading the program, which will reset the WiFi settings).

Once set up with the correct tallyNO and IP-addresses, it should automatically connect to the swithcer over WiFi, and start working as a tally light. (IMPORTANT - Swtitcher and tally light must be connected to the same network!)

The different states of connection is signalled with LED colors.
Color | Description
------|--------
BLUE | Connecting to WiFi
WHITE | Unable to connect to WiFi - serves softAP "Tally light setup", while still trying to connect.
PINK | Connecting to ATEM Swithcher. (Connected to WiFi)
RED | Tally program
GREEN | Tally preview
OFF | Tally neither live or preview (or no power...)

## Modes of operation
By default the tally light should be set up to work as a normal tally light would in a professional enviroment. However, as I in my personal use case needed it to work a bit differently, I added the following modes of oparation for convenience.
Mode | Description
-----|------------
Normal | As describen in the above table.
Preview stay on | Tally will be green whenever not in program
Program only | Tally will be off whenever not in program

NOTE: Atem swithcers only allow for 5-8 simultanious clients (dependant on the model). Be mindful of this as each of these tally lights will act as a seperate client alongside eg. a computer running Atem software control.

# Use Arduino IDE with ESP8266 module
See details at [ESP8266](https://github.com/esp8266/Arduino) on how to setup and use ESP8266 modules like a regular Arduino board.
They have links for further documentation as well.

# Credits
Based on ATEM libraries for Arduino by [SKAARHOJ](https://www.skaarhoj.com/), available at Git repo: [SKAARHOJ-Open-Engineering](https://github.com/kasperskaarhoj/SKAARHOJ-Open-Engineering)

Inspired by [ATEM_Wireless_Tally_Light](https://github.com/kalinchuk/ATEM_Wireless_Tally_Light) (However, this works completely different)
