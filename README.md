# ATEM-tally-light
Wireless tally light for use with ATEM switchers. Connects over WiFi using only a D1 mini board (ESP8266 WiFi module) and a RGB LED. Should easily be convertable to use with regular Arduino boards and an ESP2866 module, by changeing the include statements and a few other things (however, this is not tested).

# What does it do?
It will serve a webpage over WiFi where you are able to see status details, and perform the basic setup. Depending on if it's connecting to a known network or not it will serve the webpage on it's IP address, og on 192.168.4.1 (default) over a softAP named "Tally light setup". (On first run you might run into issues with what it reads from the EEPROM if it connects to a known WiFi, as it can set an invalid static IP. To avoid this, make sure it's unable to reconnect to an earlier known WiFi, so that it will enable a softAP).
Once it's setup woth the correct tallyNO and IP-addresses, it should automatically connect to the swithcer over WiFi.
The different states of connection is signalled with LED colors:
Color | Description
------|--------
BLUE | Connecting to WiFi
WHITE | Unable to connect to WiFi - serves softAP "Tally light setup", while still trying to connect.
PINK | Connecting to ATEM Swithcher. (Connected to WiFi)
RED | Tally live
GREEN | Tally preview
OFF | Tally neither live or preview (or no power...)

# Use Arduino IDE with ESP8266 module
See details at [ESP8266](https://github.com/esp8266/Arduino) on how to setup and use ESP8266 modules like a regular Arduino board.
They have links for further documentation as well.

# Credits
Based on ATEM libraries for Arduino by [SKAARHOJ](https://www.skaarhoj.com/), available at Git repo: [SKAARHOJ-Open-Engineering](https://github.com/kasperskaarhoj/SKAARHOJ-Open-Engineering)

Inspired by [ATEM_Wireless_Tally_Light](https://github.com/kalinchuk/ATEM_Wireless_Tally_Light) (However, this works completely different)
