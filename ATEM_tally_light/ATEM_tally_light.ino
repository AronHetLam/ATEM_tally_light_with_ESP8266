/*
    Copyright (C) 2020 Aron N. Het Lam, aronhetlam@gmail.com

    This program makes an ESP8266 into a wireless tally light system for ATEM switchers,
    by using Kasper Skårhøj's (<https://skaarhoj.com>) ATEM clinet libraries for Arduino.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#define FASTLED_ESP8266_DMA

//Include libraries:
#if ESP32
#include <esp_wifi.h>
#include <WebServer.h>
#include <WiFi.h>
#else
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#endif

#include <EEPROM.h>
#include <ATEMmin.h>
#include <TallyServer.h>
#include <FastLED.h>

#if ESP32
//Define LED1 color pins
#define PIN_RED1   32
#define PIN_GREEN1 33
#define PIN_BLUE1  25

//Define LED2 color pins
#define PIN_RED2   26
#define PIN_GREEN2 27
#define PIN_BLUE2  14

#else
//Define LED1 color pins
#define PIN_RED1    D0
#define PIN_GREEN1  D2
#define PIN_BLUE1   D1

//Define LED2 color pins
#define PIN_RED2    D4
#define PIN_GREEN2  D5
#define PIN_BLUE2   D6
#endif

//Define LED colors
#define LED_OFF     0
#define LED_RED     1
#define LED_GREEN   2
#define LED_BLUE    3
#define LED_YELLOW  4
#define LED_PINK    5
#define LED_WHITE   6
#define LED_ORANGE  7

//Map "old" LED colors to CRGB colors
CRGB color_led[8] = { CRGB::Black, CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Yellow, CRGB::Pink, CRGB::White, CRGB::Orange };

//Define states
#define STATE_STARTING                  0
#define STATE_CONNECTING_TO_WIFI        1
#define STATE_CONNECTING_TO_SWITCHER    2
#define STATE_RUNNING                   3

//Define modes of operation
#define MODE_NORMAL                     1
#define MODE_PREVIEW_STAY_ON            2
#define MODE_PROGRAM_ONLY               3
#define MODE_ON_AIR                     4

#define TALLY_FLAG_OFF                  0
#define TALLY_FLAG_PROGRAM              1
#define TALLY_FLAG_PREVIEW              2

//Define Neopixel status-LED options
#define NEOPIXEL_STATUS_FIRST           1
#define NEOPIXEL_STATUS_LAST            2
#define NEOPIXEL_STATUS_NONE            3

//FastLED
#if ESP32
#define DATA_PIN    12
#else
#define DATA_PIN    D7
#endif
int numTallyLEDs;
int numStatusLEDs;
CRGB *leds;
CRGB *tallyLEDs;
CRGB *statusLED;
bool neopixelsUpdated = false;

//Initialize global variables
#if ESP32
WebServer server(80);
#else
ESP8266WebServer server(80);
#endif

ATEMmin atemSwitcher;

TallyServer tallyServer;

uint8_t state = STATE_STARTING;

//Define sturct for holding tally settings (mostly to simplify EEPROM read and write, in order to persist settings)
struct Settings {
    char tallyName[32] = "";
    uint8_t tallyNo;
    uint8_t tallyModeLED1;
    uint8_t tallyModeLED2;
    bool staticIP;
    IPAddress tallyIP;
    IPAddress tallySubnetMask;
    IPAddress tallyGateway;
    IPAddress switcherIP;
    uint16_t neopixelsAmount;
    uint8_t neopixelStatusLEDOption;
    uint8_t NeopixelBrightness;
};

Settings settings;

bool firstRun = true;

//Commented out for users without batteries
// long secLoop = 0;
// int lowLedCount = 0;
// bool lowLedOn = false;
// double uBatt = 0;
// char buffer[3];

//Perform initial setup on power on
void setup() {
    //Init pins for LED
    pinMode(PIN_RED1, OUTPUT);
    pinMode(PIN_GREEN1, OUTPUT);
    pinMode(PIN_BLUE1, OUTPUT);

    pinMode(PIN_RED2, OUTPUT);
    pinMode(PIN_GREEN2, OUTPUT);
    pinMode(PIN_BLUE2, OUTPUT);

    setBothLEDs(LED_BLUE);

    //Setup current-measuring pin - Commented out for users without batteries
    // pinMode(A0, INPUT);

    //Start Serial
    Serial.begin(115200);
    Serial.println("########################");
    Serial.println("Serial started");

    //save flash memory from being written too without need.
    //WiFi.persistent(false);

    //Read settings from EEPROM. Settings struct takes 72 bytes total (according to sizeof()). WIFI settings are stored seperately by the ESP
    EEPROM.begin(72); //Needed on ESP8266 module, as EEPROM lib works a bit differently than on a regular arduino
    EEPROM.get(0, settings);

    //Initialize LED strip
    if (0 < settings.neopixelsAmount && settings.neopixelsAmount <= 1000) {
        leds = new CRGB[settings.neopixelsAmount];
        FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, settings.neopixelsAmount);

        if (settings.neopixelStatusLEDOption != NEOPIXEL_STATUS_NONE) {
            numStatusLEDs = 1;
            numTallyLEDs = settings.neopixelsAmount - numStatusLEDs;
            if (settings.neopixelStatusLEDOption == NEOPIXEL_STATUS_FIRST) {
                statusLED = leds;
                tallyLEDs = leds + numStatusLEDs;
            } else { // if last or or other value
                statusLED = leds + numTallyLEDs;
                tallyLEDs = leds;
            }
        } else {
            numTallyLEDs = settings.neopixelsAmount;
            numStatusLEDs = 0;
            tallyLEDs = leds;
        }
    } else {
        settings.neopixelsAmount = 0;
        numTallyLEDs = 0;
        numStatusLEDs = 0;
    }

    FastLED.setBrightness(settings.NeopixelBrightness);
    setSTRIP(LED_OFF);
    setStatusLED(LED_BLUE);
    FastLED.show();

    Serial.println(settings.tallyName);
    //Serial.println(sizeof(settings)); //Check size of settings struct
    if (settings.staticIP) {
        WiFi.config(settings.tallyIP, settings.tallyGateway, settings.tallySubnetMask);
    }

    //Put WiFi into station mode and make it connect to saved network
    WiFi.mode(WIFI_STA);
    WiFi.begin();
#if ESP32
    WiFi.setHostname(settings.tallyName);
#else
    WiFi.hostname(settings.tallyName);
#endif
    WiFi.setAutoReconnect(true);

    Serial.println("------------------------");
    Serial.println("Connecting to WiFi...");
    Serial.println("Network name (SSID): " + getSSID());

    // Initialize and begin HTTP server for handeling the web interface
    server.on("/", handleRoot);
    server.on("/save", handleSave);
    server.onNotFound(handleNotFound);
    server.begin();

    tallyServer.begin();

    //Wait for result from first attempt to connect - This makes sure it only activates the softAP if it was unable to connect,
    //and not just because it hasn't had the time to do so yet. It's blocking, so don't use it inside loop()
    WiFi.waitForConnectResult();

    //Set state to connecting before entering loop
    changeState(STATE_CONNECTING_TO_WIFI);
}

void loop() {
    switch (state) {
        case STATE_CONNECTING_TO_WIFI:
            if (WiFi.status() == WL_CONNECTED) {
                WiFi.mode(WIFI_STA); // Disable softAP if connection is successful
                Serial.println("------------------------");
                Serial.println("Connected to WiFi:   " + getSSID());
                Serial.println("IP:                  " + WiFi.localIP().toString());
                Serial.println("Subnet Mask:         " + WiFi.subnetMask().toString());
                Serial.println("Gateway IP:          " + WiFi.gatewayIP().toString());
                changeState(STATE_CONNECTING_TO_SWITCHER);
            } else if (firstRun) {
                firstRun = false;
                Serial.println("Unable to connect. Serving \"Tally Light setup\" WiFi for configuration, while still trying to connect...");
                WiFi.mode(WIFI_AP_STA); // Enable softAP to access web interface in case of no WiFi
                WiFi.softAP("Tally Light setup");
                setBothLEDs(LED_WHITE);
                setStatusLED(LED_WHITE);
            }
            break;

        case STATE_CONNECTING_TO_SWITCHER:
            // Initialize a connection to the switcher:
            if (firstRun) {
                atemSwitcher.begin(settings.switcherIP);
                //atemSwitcher.serialOutput(0x80); //Makes Atem library print debug info
                Serial.println("------------------------");
                Serial.println("Connecting to switcher...");
                Serial.println((String)"Switcher IP:         " + settings.switcherIP[0] + "." + settings.switcherIP[1] + "." + settings.switcherIP[2] + "." + settings.switcherIP[3]);
                firstRun = false;
            }
            atemSwitcher.runLoop();
            if (atemSwitcher.hasInitialized()) {
                changeState(STATE_RUNNING);
                Serial.println("Connected to switcher");
            }
            break;

        case STATE_RUNNING:
            //Handle data exchange and connection to swithcher
            atemSwitcher.runLoop();

            int tallySources = atemSwitcher.getTallyByIndexSources();
            tallyServer.setTallySources(tallySources);
            for (int i = 0; i < tallySources; i++) {
                tallyServer.setTallyFlag(i, atemSwitcher.getTallyByIndexTallyFlags(i));
            }

            //Handle Tally Server
            tallyServer.runLoop();

            //Set LED and Neopixel colors accordingly
            int color = getLedColor(settings.tallyModeLED1, settings.tallyNo);
            setLED1(color);
            setSTRIP(color);

            color = getLedColor(settings.tallyModeLED2, settings.tallyNo);
            setLED2(color);

            //Commented out for userst without batteries - Also timer is not done properly
            // batteryLoop();

            //Switch state if connection is lost, dependant on which connection is lost.
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("------------------------");
                Serial.println("WiFi connection lost...");
                changeState(STATE_CONNECTING_TO_WIFI);

                //Force atem library to reset connection, in order for status to read correctly on website.
                atemSwitcher.begin(settings.switcherIP);

                //Reset tally server's tally flags, They won't get the message, but it'll be reset for when the connectoin is back.
                tallyServer.resetTallyFlags();

            } else if (!atemSwitcher.hasInitialized()) { // will return false if the connection was lost
                Serial.println("------------------------");
                Serial.println("Connection to Switcher lost...");
                changeState(STATE_CONNECTING_TO_SWITCHER);

                //Reset tally server's tally flags, so clients turn off their lights.
                tallyServer.resetTallyFlags();
            }
            break;
    }

    //Show stip only on updates
    if(neopixelsUpdated) {
        FastLED.show();
        neopixelsUpdated = false;
    }

    //Handle web interface
    server.handleClient();
}

//Handle the change of states in the program
void changeState(uint8_t stateToChangeTo) {
    firstRun = true;
    switch (stateToChangeTo) {
        case STATE_CONNECTING_TO_WIFI:
            state = STATE_CONNECTING_TO_WIFI;
            setBothLEDs(LED_BLUE);
            setStatusLED(LED_BLUE);
            setSTRIP(LED_OFF);
            break;
        case STATE_CONNECTING_TO_SWITCHER:
            state = STATE_CONNECTING_TO_SWITCHER;
            setBothLEDs(LED_PINK);
            setStatusLED(LED_PINK);
            setSTRIP(LED_OFF);
            break;
        case STATE_RUNNING:
            state = STATE_RUNNING;
            setBothLEDs(LED_GREEN);
            setStatusLED(LED_ORANGE);
            break;
    }
}

//Set the color of both LEDs
void setBothLEDs(uint8_t color) {
    setLED(color, PIN_RED1, PIN_GREEN1, PIN_BLUE1);
    setLED(color, PIN_RED2, PIN_GREEN2, PIN_BLUE2);
}

//Set the color of the 1st LED
void setLED1(uint8_t color) {
    setLED(color, PIN_RED1, PIN_GREEN1, PIN_BLUE1);
}

//Set the color of the 2nd LED
void setLED2(uint8_t color) {
    setLED(color, PIN_RED2, PIN_GREEN2, PIN_BLUE2);
}

//Set the color of a LED using the given pins
void setLED(uint8_t color, int pinRed, int pinGreen, int pinBlue) {
    switch (color) {
        case LED_OFF:
            digitalWrite(pinRed, 0);
            digitalWrite(pinGreen, 0);
            digitalWrite(pinBlue, 0);
            break;
        case LED_RED:
            digitalWrite(pinRed, 1);
            digitalWrite(pinGreen, 0);
            digitalWrite(pinBlue, 0);
            break;
        case LED_GREEN:
            digitalWrite(pinRed, 0);
            digitalWrite(pinGreen, 1);
            digitalWrite(pinBlue, 0);
            break;
        case LED_BLUE:
            digitalWrite(pinRed, 0);
            digitalWrite(pinGreen, 0);
            digitalWrite(pinBlue, 1);
            break;
        case LED_YELLOW:
            digitalWrite(pinRed, 1);
            digitalWrite(pinGreen, 1);
            digitalWrite(pinBlue, 0);
            break;
        case LED_PINK:
            digitalWrite(pinRed, 1);
            digitalWrite(pinGreen, 0);
#if ESP32
            digitalWrite(pinBlue, 1);
#else
            analogWrite(pinBlue, 0xff);
#endif
            break;
        case LED_WHITE:
            digitalWrite(pinRed, 1);
            digitalWrite(pinGreen, 1);
            digitalWrite(pinBlue, 1);
            break;
    }
}

//Set the color og the LED strip, except for the status LED
void setSTRIP(uint8_t color) {
    if(numTallyLEDs > 0 && tallyLEDs[0] != color_led[color]) {
        for (int i = 0; i < numTallyLEDs; i++) {
            tallyLEDs[i] = color_led[color];
        }
        neopixelsUpdated = true;
    }
}

//Set the single status LED (last LED)
void setStatusLED(uint8_t color) {
    if (numStatusLEDs > 0 && statusLED[0] != color_led[color]) {
        for (int i = 0; i < numStatusLEDs; i++) {
            statusLED[i] = color_led[color];
            if (color == LED_ORANGE) {
                statusLED[i].fadeToBlackBy(230);
            } else {
                statusLED[i].fadeToBlackBy(0);
            }
        }
        neopixelsUpdated = true;
    }
}

int getTallyState(uint16_t tallyNo) {
    if(tallyNo >= atemSwitcher.getTallyByIndexSources()) { //out of range
        return TALLY_FLAG_OFF;
    }

    uint8_t tallyFlag = atemSwitcher.getTallyByIndexTallyFlags(tallyNo);
    if (tallyFlag & TALLY_FLAG_PROGRAM) {
        return TALLY_FLAG_PROGRAM;
    } else if (tallyFlag & TALLY_FLAG_PREVIEW) {
        return TALLY_FLAG_PREVIEW;
    } else {
        return TALLY_FLAG_OFF;
    }
}

int getLedColor(int tallyMode, int tallyNo) {
    if(tallyMode == MODE_ON_AIR) {
        if(atemSwitcher.getStreamStreaming()) {
            return LED_RED;
        }
        return LED_OFF;
    }

    int tallyState = getTallyState(tallyNo);

    if (tallyState == TALLY_FLAG_PROGRAM) {             //if tally live
        return LED_RED;
    } else if ((tallyState == TALLY_FLAG_PREVIEW        //if tally preview
                || tallyMode == MODE_PREVIEW_STAY_ON)   //or preview stay on
               && tallyMode != MODE_PROGRAM_ONLY) {     //and not program only
        return LED_GREEN;
    } else {                                            //if tally is neither
        return LED_OFF;
    }
}

//Serve setup web page to client, by sending HTML with the correct variables
void handleRoot() {
    String html = "<!DOCTYPE html> <html> <head> <meta charset=\"ASCII\"> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> <title>Tally Light setup</title> </head> <script> function switchIpField(e) { console.log(\"switch\"); console.log(e); var target = e.srcElement || e.target; var maxLength = parseInt(target.attributes[\"maxlength\"].value, 10); var myLength = target.value.length; if (myLength >= maxLength) { var next = target.nextElementSibling; if (next != null) { if (next.className.includes(\"IP\")) { next.focus(); } } } else if (myLength == 0) { var previous = target.previousElementSibling; if (previous != null) { if (previous.className.includes(\"IP\")) { previous.focus(); } } } } function ipFieldFocus(e) { console.log(\"focus\"); console.log(e); var target = e.srcElement || e.target; target.select(); } function load() { var containers = document.getElementsByClassName(\"IP\"); for (var i = 0; i < containers.length; i++) { var container = containers[i]; container.oninput = switchIpField; container.onfocus = ipFieldFocus; } containers = document.getElementsByClassName(\"tIP\"); for (var i = 0; i < containers.length; i++) { var container = containers[i]; container.oninput = switchIpField; container.onfocus = ipFieldFocus; } toggleStaticIPFields(); } function toggleStaticIPFields() { var enabled = document.getElementById(\"staticIP\").checked; document.getElementById(\"staticIPHidden\").disabled = enabled; var staticIpFields = document.getElementsByClassName('tIP'); for (var i = 0; i < staticIpFields.length; i++) { staticIpFields[i].disabled = !enabled; } } </script> <body style=\"font-family:Verdana; white-space:nowrap;\" onload=\"load()\"> <table cellpadding=\"2\" style=\"width:100%\"> <tr bgcolor=\"#777777\" style=\"color:#ffffff;font-size:12px;\"> <td colspan=\"3\"> <h1>&nbsp;Tally Light setup</h1> <h2>&nbsp;Status:</h2> </td> </tr> <tr> <td><br></td> <td></td> <td style=\"width: 100%;\"></td> </tr> <tr> <td>Connection Status:</td> <td colspan=\"2\">";
    switch (WiFi.status()) {
        case WL_CONNECTED:
            html += "Connected to network";
            break;
        case WL_NO_SSID_AVAIL:
            html += "Network not found";
            break;
        case WL_CONNECT_FAILED:
            html += "Invalid password";
            break;
        case WL_IDLE_STATUS:
            html += "Changing state...";
            break;
        case WL_DISCONNECTED:
            html += "Station mode disabled";
            break;
        case -1:
            html += "Timeout";
            break;
    }

    html += "</td> </tr> <tr> <td>Network name (SSID):</td> <td colspan=\"2\">";
    html += getSSID();
    html += "</td> </tr> <tr> <td><br></td> </tr> <tr> <td>Signal strength:</td> <td colspan=\"2\">";
    html += WiFi.RSSI();
    html += " dBm</td> </tr>";
    //Commented out for users without batteries
    // html += "<tr> <td><br></td> </tr> <tr> <td>Battery voltage:</td> <td colspan=\"2\">";
    // html += dtostrf(uBatt, 0, 3, buffer);
    // html += " V</td> </tr>";
    html += "<tr> <td>Static IP:</td> <td colspan=\"2\">";
    html += settings.staticIP == true ? "True" : "False";
    html += "</td> </tr> <tr> <td>Tally Light IP:</td> <td colspan=\"2\">";
    html += WiFi.localIP().toString();
    html += "</td> </tr> <tr> <td>Subnet mask: </td> <td colspan=\"2\">";
    html += WiFi.subnetMask().toString();
    html += "</td> </tr> <tr> <td>Gateway: </td> <td colspan=\"2\">";
    html += WiFi.gatewayIP().toString();
    html += "</td> </tr> <tr> <td><br></td> </tr> <tr> <td>ATEM switcher status:</td> <td colspan=\"2\">";
    if (atemSwitcher.hasInitialized())
        html += "Connected - Initialized";
    else if (atemSwitcher.isConnected())
        html += "Connected - Wating for initialization - Connection might have been rejected";
    else if (WiFi.status() == WL_CONNECTED)
        html += "Disconnected - No response from switcher";
    else
        html += "Disconnected - Waiting for WiFi";
    html += "</td> </tr> <tr> <td>ATEM switcher IP:</td> <td colspan=\"2\">";
    html += (String)settings.switcherIP[0] + '.' + settings.switcherIP[1] + '.' + settings.switcherIP[2] + '.' + settings.switcherIP[3];
    html += "</td> </tr> <tr> <td><br></td> </tr> <tr bgcolor=\"#777777\" style=\"color:#ffffff;font-size:12px;\"> <td colspan=\"3\"> <h2>&nbsp;Settings:</h2> </td> </tr> <tr> <td><br></td> </tr> <form action=\"/save\" method=\"post\"> <tr> <td>Tally Light name: </td> <td> <input type=\"text\" size=\"30\" maxlength=\"30\" name=\"tName\" value=\"";
#if ESP32
    html += WiFi.getHostname();
#else
    html += WiFi.hostname();
#endif
    html += "\" required /> </td> </tr> <tr> <td><br></td> </tr> <tr> <td>Tally Light number: </td> <td> <input type=\"number\" size=\"5\" min=\"1\" max=\"41\" name=\"tNo\" value=\"";
    html += (settings.tallyNo + 1);
    html += "\" required /> </td> </tr> <tr> <td>Tally Light mode (LED 1):&nbsp;</td> <td> <select name=\"tModeLED1\"> <option value=\"";
    html += (String) MODE_NORMAL + "\" ";
    if (settings.tallyModeLED1 == MODE_NORMAL)
        html += "selected";
    html += ">Normal</option> <option value=\"";
    html += (String) MODE_PREVIEW_STAY_ON + "\" ";
    if (settings.tallyModeLED1 == MODE_PREVIEW_STAY_ON)
        html += "selected";
    html += ">Preview stay on</option> <option value=\"";
    html += (String) MODE_PROGRAM_ONLY + "\" ";
    if (settings.tallyModeLED1 == MODE_PROGRAM_ONLY)
        html += "selected";
    html += ">Program only</option> <option value=\"";
    html += (String) MODE_ON_AIR + "\" ";
    if (settings.tallyModeLED1 == MODE_ON_AIR)
        html += "selected";
    html += ">On Air</option> </select> </td> </tr> <tr> <td>Tally Light mode (LED 2):</td> <td> <select name=\"tModeLED2\"> <option value=\"";
    html += (String) MODE_NORMAL + "\" ";
    if (settings.tallyModeLED2 == MODE_NORMAL)
        html += "selected";
    html += ">Normal</option> <option value=\"";
    html += (String) MODE_PREVIEW_STAY_ON + "\" ";
    if (settings.tallyModeLED2 == MODE_PREVIEW_STAY_ON)
        html += "selected";
    html += ">Preview stay on</option> <option value=\"";
    html += (String) MODE_PROGRAM_ONLY + "\" ";
    if (settings.tallyModeLED2 == MODE_PROGRAM_ONLY)
        html += "selected";
    html += ">Program only</option> <option value=\"";
    html += (String)MODE_ON_AIR + "\" ";
    if (settings.tallyModeLED2 == MODE_ON_AIR)
        html += "selected";
    html += ">On Air</option> </select> </td> </tr> <tr> <td>Amount of Neopixels:</td> <td> <input type=\"number\" size=\"5\" min=\"0\" max=\"1000\" name=\"neoPxAmount\" value=\"";
    html += settings.neopixelsAmount;
    html += "\" required /> </td> </tr> <tr> <td>Neopixel status LED: </td> <td> <select name=\"neoPxStatus\"> <option value=\"";
    html += (String) NEOPIXEL_STATUS_FIRST + "\" ";
    if (settings.neopixelStatusLEDOption == NEOPIXEL_STATUS_FIRST)
        html += "selected";
    html += ">First LED</option> <option value=\"";
    html += (String) NEOPIXEL_STATUS_LAST + "\" ";
    if (settings.neopixelStatusLEDOption == NEOPIXEL_STATUS_LAST)
        html += "selected";
    html += ">Last LED</option> <option value=\"";
    html += (String) NEOPIXEL_STATUS_NONE + "\" ";
    if (settings.neopixelStatusLEDOption == NEOPIXEL_STATUS_NONE)
        html += "selected";
    html += ">None</option> </select> </td> </tr> <tr> <td> Neopixel brightness: </td> <td> <input type=\"number\" size=\"5\" min=\"0\" max=\"255\" name=\"neoPxBright\" value=\"";
    html += settings.NeopixelBrightness;
    html +=  "\" required /> </td> </tr> <tr> <td><br></td> </tr> <tr> <td>Network name(SSID): </td> <td> <input type =\"text\" size=\"30\" maxlength=\"30\" name=\"ssid\" value=\"";
    html += getSSID();
    html += "\" required /> </td> </tr> <tr> <td>Network password: </td> <td> <input type=\"password\" size=\"30\" maxlength=\"30\" name=\"pwd\" pattern=\"^$|.{8,32}\" value=\"";
    if (WiFi.isConnected()) //As a minimum security meassure, to only send the wifi password if it's currently connected to the given network.
        html += WiFi.psk();
    html += "\" /> </td> </tr> <tr> <td><br></td> </tr> <tr> <td>Use static IP: </td> <td> <input type=\"hidden\" id=\"staticIPHidden\" name=\"staticIP\" value=\"false\" /> <input id=\"staticIP\" type=\"checkbox\" name=\"staticIP\" value=\"true\" onchange=\"toggleStaticIPFields()\" ";
    if (settings.staticIP)
        html += "checked";
    html += "/> </td> </tr> <tr> <td>Tally Light IP: </td> <td> <input class=\"tIP\" type=\"text\" size=\"3\" maxlength=\"3\" name=\"tIP1\" pattern=\"\\d{0,3}\" value=\"";
    html += settings.tallyIP[0];
    html += "\" required />. <input class=\"tIP\" type=\"text\" size=\"3\" maxlength=\"3\" name=\"tIP2\" pattern=\"\\d{0,3}\" value=\"";
    html += settings.tallyIP[1];
    html += "\" required />. <input class=\"tIP\" type=\"text\" size=\"3\" maxlength=\"3\" name=\"tIP3\" pattern=\"\\d{0,3}\" value=\"";
    html += settings.tallyIP[2];
    html += "\" required />. <input class=\"tIP\" type=\"text\" size=\"3\" maxlength=\"3\" name=\"tIP4\" pattern=\"\\d{0,3}\" value=\"";
    html += settings.tallyIP[3];
    html += "\" required /> </td> </tr> <tr> <td>Subnet mask: </td> <td> <input class=\"tIP\" type=\"text\" size=\"3\" maxlength=\"3\" name=\"mask1\" pattern=\"\\d{0,3}\" value=\"";
    html += settings.tallySubnetMask[0];
    html += "\" required />. <input class=\"tIP\" type=\"text\" size=\"3\" maxlength=\"3\" name=\"mask2\" pattern=\"\\d{0,3}\" value=\"";
    html += settings.tallySubnetMask[1];
    html += "\" required />. <input class=\"tIP\" type=\"text\" size=\"3\" maxlength=\"3\" name=\"mask3\" pattern=\"\\d{0,3}\" value=\"";
    html += settings.tallySubnetMask[2];
    html += "\" required />. <input class=\"tIP\" type=\"text\" size=\"3\" maxlength=\"3\" name=\"mask4\" pattern=\"\\d{0,3}\" value=\"";
    html += settings.tallySubnetMask[3];
    html += "\" required /> </td> </tr> <tr> <td>Gateway: </td> <td> <input class=\"tIP\" type=\"text\" size=\"3\" maxlength=\"3\" name=\"gate1\" pattern=\"\\d{0,3}\" value=\"";
    html += settings.tallyGateway[0];
    html += "\" required />. <input class=\"tIP\" type=\"text\" size=\"3\" maxlength=\"3\" name=\"gate2\" pattern=\"\\d{0,3}\" value=\"";
    html += settings.tallyGateway[1];
    html += "\" required />. <input class=\"tIP\" type=\"text\" size=\"3\" maxlength=\"3\" name=\"gate3\" pattern=\"\\d{0,3}\" value=\"";
    html += settings.tallyGateway[2];
    html += "\" required />. <input class=\"tIP\" type=\"text\" size=\"3\" maxlength=\"3\" name=\"gate4\" pattern=\"\\d{0,3}\" value=\"";
    html += settings.tallyGateway[3];
    html += "\" required /> </td> </tr> <tr> <td><br></td> </tr> <tr> <td>ATEM switcher IP: </td> <td> <input class=\"IP\" type=\"text\" size=\"3\" maxlength=\"3\" name=\"aIP1\" pattern=\"\\d{0,3}\" value=\"";
    html += settings.switcherIP[0];
    html += "\" required />. <input class=\"IP\" type=\"text\" size=\"3\" maxlength=\"3\" name=\"aIP2\" pattern=\"\\d{0,3}\" value=\"";
    html += settings.switcherIP[1];
    html += "\" required />. <input class=\"IP\" type=\"text\" size=\"3\" maxlength=\"3\" name=\"aIP3\" pattern=\"\\d{0,3}\" value=\"";
    html += settings.switcherIP[2];
    html += "\" required />. <input class=\"IP\" type=\"text\" size=\"3\" maxlength=\"3\" name=\"aIP4\" pattern=\"\\d{0,3}\" value=\"";
    html += settings.switcherIP[3];
    html += "\" required /> </tr> <tr> <td><br></td> </tr> <tr> <td /> <td style=\"float: right;\"> <input type=\"submit\" value=\"Save Changes\" /> </td> </tr> </form> </table> </body> </html>";

    server.send(200, "text/html", html);
}

//Save new settings from client in EEPROM and restart the ESP8266 module
void handleSave() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/html", "<!DOCTYPE html> <html> <head> <meta charset=\"ASCII\"> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> <title>Tally Light setup</title> </head> <body style=\"font-family:Verdana;\"> <table bgcolor=\"#777777\" border=\"0\" width=\"100%\" cellpadding=\"1\" style=\"color:#ffffff;font-size:12px;\"> <tr> <td> <h1>&nbsp;Tally Light setup</h1> </td> </tr> </table><br>Request without posting settings not allowed</body></html>");
    } else {
        String ssid;
        String pwd;
        bool change = false;
        for (uint8_t i = 0; i < server.args(); i++) {
            change = true;
            String var = server.argName(i);
            String val = server.arg(i);

            if (var == "tName") {
                val.toCharArray(settings.tallyName, (uint8_t)32);
            } else if (var == "tModeLED1") {
                settings.tallyModeLED1 = val.toInt();
            } else if (var == "tModeLED2") {
                settings.tallyModeLED2 = val.toInt();
            } else if (var == "neoPxAmount") {
                settings.neopixelsAmount = val.toInt();
            } else if (var == "neoPxStatus") {
                settings.neopixelStatusLEDOption = val.toInt();
            } else if (var == "neoPxBright") {
                settings.NeopixelBrightness = val.toInt();
            } else if (var == "tNo") {
                settings.tallyNo = val.toInt() - 1;
            } else if (var == "ssid") {
                ssid = String(val);
            } else if (var == "pwd") {
                pwd = String(val);
            } else if (var == "staticIP") {
                settings.staticIP = (val == "true");
            } else if (var == "tIP1") {
                settings.tallyIP[0] = val.toInt();
            } else if (var == "tIP2") {
                settings.tallyIP[1] = val.toInt();
            } else if (var == "tIP3") {
                settings.tallyIP[2] = val.toInt();
            } else if (var == "tIP4") {
                settings.tallyIP[3] = val.toInt();
            } else if (var == "mask1") {
                settings.tallySubnetMask[0] = val.toInt();
            } else if (var == "mask2") {
                settings.tallySubnetMask[1] = val.toInt();
            } else if (var == "mask3") {
                settings.tallySubnetMask[2] = val.toInt();
            } else if (var == "mask4") {
                settings.tallySubnetMask[3] = val.toInt();
            } else if (var == "gate1") {
                settings.tallyGateway[0] = val.toInt();
            } else if (var == "gate2") {
                settings.tallyGateway[1] = val.toInt();
            } else if (var == "gate3") {
                settings.tallyGateway[2] = val.toInt();
            } else if (var == "gate4") {
                settings.tallyGateway[3] = val.toInt();
            } else if (var == "aIP1") {
                settings.switcherIP[0] = val.toInt();
            } else if (var == "aIP2") {
                settings.switcherIP[1] = val.toInt();
            } else if (var == "aIP3") {
                settings.switcherIP[2] = val.toInt();
            } else if (var == "aIP4") {
                settings.switcherIP[3] = val.toInt();
            }
        }

        if (change) {
            EEPROM.put(0, settings);
            EEPROM.commit();

            server.send(200, "text/html", (String)"<!DOCTYPE html> <html> <head> <meta charset=\"ASCII\"> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> <title>Tally Light setup</title> </head> <body> <table bgcolor=\"#777777\" border=\"0\" width=\"100%\" cellpadding=\"1\" style=\"font-family:Verdana;color:#ffffff;font-size:12px;\"> <tr> <td> <h1>&nbsp;Tally Light setup</h1> </td> </tr> </table><br>Settings saved successfully.</body></html>");

            //Delay to let data be saved, and the responce to be sent properly to the client
            delay(5000);

            if (ssid && pwd && (ssid != getSSID() || pwd != WiFi.psk())) {
                WiFi.begin(ssid.c_str(), pwd.c_str());
            }

            ESP.restart();
        }
    }
}

//Send 404 to client in case of invalid webpage being requested.
void handleNotFound() {
    server.send(404, "text/html", "<!DOCTYPE html> <html> <head> <meta charset=\"ASCII\"> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> <title>Tally Light setup</title> </head> <body style=\"font-family:Verdana;\"> <table bgcolor=\"#777777\" border=\"0\" width=\"100%\" cellpadding=\"1\" style=\"color:#ffffff;font-size:12px;\"> <tr> <td> <h1>&nbsp Tally Light setup</h1> </td> </tr> </table><br>404 - Page not found</body></html>");
}

String getSSID() {
#if ESP32
    wifi_config_t conf;
    esp_wifi_get_config(WIFI_IF_STA, &conf);
    return String(reinterpret_cast<const char *>(conf.sta.ssid));
#else
    return WiFi.SSID();
#endif
}

//Commented out for userst without batteries - Also timer is not done properly
//Main loop for things that should work every second
// void batteryLoop() {
//     if (secLoop >= 400) {
//         //Get and calculate battery current
//         int raw = analogRead(A0);
//         uBatt = (double)raw / 1023 * 4.2;

//         //Set back status LED after one second to working LED_BLUE if it was changed by anything
//         if (lowLedOn) {
//             setStatusLED(LED_ORANGE);
//             lowLedOn = false;
//         }

//         //Blink every 5 seconds for one second if battery current is under 3.6V
//         if (lowLedCount >= 5 && uBatt <= 3.600) {
//             setStatusLED(LED_YELLOW);
//             lowLedOn = true;
//             lowLedCount = 0;
//         }
//         lowLedCount++;

//        //Turn stripes of and put ESP to deepsleep if battery is too low
//        if(uBatt <= 3.499) {
//            setSTRIP(LED_OFF);
//            setStatusLED(LED_OFF);
//            ESP.deepSleep(0, WAKE_NO_RFCAL);
//        }

//         secLoop = 0;
//     }
//     secLoop++;
// }
