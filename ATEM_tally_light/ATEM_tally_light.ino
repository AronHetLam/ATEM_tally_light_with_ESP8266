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
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ATEMmin.h>
#include <TallyServer.h>
#include <FastLED.h>

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
int color_led[8] = { CRGB::Black, CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Yellow, CRGB::Pink, CRGB::White, CRGB::Orange };

//Define states
#define STATE_STARTING                  0
#define STATE_CONNECTING_TO_WIFI        1
#define STATE_CONNECTING_TO_SWITCHER    2
#define STATE_RUNNING                   3

//Define modes of operation
#define MODE_NORMAL                     1
#define MODE_PREVIEW_STAY_ON            2
#define MODE_PROGRAM_ONLY               3

//FastLED
#define DATA_PIN      D7
#define NUM_LEDS      12
CRGB leds[NUM_LEDS];

//Initialize global variables
ESP8266WebServer server(80);

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
};

Settings settings;

bool firstRun = true;
bool activeCam = false;
bool activeCamPreview = false;

int lastActive = -1;
int lastSet = -1;

unsigned long currentMillis = 0;
unsigned long previousMillis = 0;
bool changePreview = false;

long mixRate = 3000;
long secLoop = 0;
int lowLedCount = 0;
bool lowLedOn = false;

double uBatt = 0;
char buffer[3];

//Perform initial setup on power on
void setup() {
    //Setup current measuring pin
    pinMode(A0, INPUT);

    //Initialize LED strip
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);

    setSTRIP(LED_OFF);

    //Set LED Status
    setStatusLED(LED_YELLOW);
    FastLED.show();

    //Start Serial
    Serial.begin(115200);
    Serial.println("########################");
    Serial.println("Serial started");

    //save flash memory from being written too without need.
    WiFi.persistent(false);

    //Read settings from EEPROM. Settings struct takes 68 bytes total (according to sizeof()). WIFI settings are stored seperately by the ESP
    EEPROM.begin(68); //Needed on ESP8266 module, as EEPROM lib works a bit differently than on a regular arduino
    EEPROM.get(0, settings);

    Serial.println(settings.tallyName);
    //Serial.println(sizeof(settings)); //Check size of settings struct
    if (settings.staticIP) {
        WiFi.config(settings.tallyIP, settings.tallyGateway, settings.tallySubnetMask);
    }

    //Put WiFi into station mode and make it connect to saved network
    WiFi.mode(WIFI_STA);
    WiFi.begin();
    WiFi.hostname(settings.tallyName);
    WiFi.setAutoReconnect(true);

    Serial.println("------------------------");
    Serial.println("Connecting to WiFi...");
    Serial.println("Network name (SSID): " + WiFi.SSID());

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
        case STATE_CONNECTING_TO_WIFI: {
                if (WiFi.status() == WL_CONNECTED) {
                    WiFi.mode(WIFI_STA); // Disable softAP if connection is successful
                    Serial.println("------------------------");
                    Serial.println("Connected to WiFi:   " + WiFi.SSID());
                    Serial.println("IP:                  " + WiFi.localIP().toString());
                    Serial.println("Subnet Mask:         " + WiFi.subnetMask().toString());
                    Serial.println("Gateway IP:          " + WiFi.gatewayIP().toString());
                    changeState(STATE_CONNECTING_TO_SWITCHER);
                } else if (firstRun) {
                    firstRun = false;
                    WiFi.mode(WIFI_AP_STA); // Enable softAP to access web interface in case of no WiFi
                    WiFi.softAP("Tally Light setup");
                    setStatusLED(LED_WHITE);
                    FastLED.show();
                }
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
            
            //Get current set transition time of "mix" transition for switching back preview
            mixRate = round((atemSwitcher.getTransitionMixRate(0)*33.33))*0.02;
            
            //Main loop for things that should work every second
            if(secLoop >= 400) {
              //Get and calculate battery current
              int raw = analogRead(A0);
              uBatt=(double)raw/1023*4.2;

              //Set back status LED after one second to working LED_BLUE if it was changed by anything
              if(lowLedOn) {
                setStatusLED(LED_BLUE);
                lowLedOn = false;
              }

              //Blink every 5 seconds for one second if battery current is under 3.6V
              if(lowLedCount >= 5 && uBatt <= 3.600) {
                setStatusLED(LED_YELLOW);
                lowLedOn = true;
                lowLedCount = 0;
              }
              lowLedCount++;

              //Turn stripes of and put ESP to deepsleep if battery is too low
              if(uBatt <= 3.499) {
                setSTRIP(LED_OFF);
                setStatusLED(LED_OFF);
                FastLED.show();
                ESP.deepSleep(0, WAKE_NO_RFCAL);
              }

              secLoop = 0;
            }
            secLoop++;

            int tallySources = atemSwitcher.getTallyByIndexSources();
            tallyServer.setTallySources(tallySources);
            for (int i = 0; i < tallySources; i++) {
                tallyServer.setTallyFlag(i, atemSwitcher.getTallyByIndexTallyFlags(i));
            }

            //Handle Tally Server
            tallyServer.runLoop();

            //Set bool for active cam 
            if (atemSwitcher.getTallyByIndexTallyFlags(settings.tallyNo) & 0x01) {              //if tally live
              activeCam = true;

              //When active cam became true run a small logic to switch preview cam also to active cam to disable all other lights
              if(activeCamPreview) {
                changePreview = true;
                currentMillis = 0;
              }
            } else {
              activeCam = false;
            }

            //Switch preview cam to active cam after the correct amount of time
            if (changePreview) {
              currentMillis++;
              
              if (currentMillis >= mixRate) {
                setActive(atemSwitcher.getProgramInputVideoSource(0));
                changePreview = false;
              }
            }

            //set last active cam to current cam if no preview switching is in progress (after it is done)
            if(!changePreview) {
              lastActive = atemSwitcher.getProgramInputVideoSource(0);
            }

            //Set bool for preview cam
            if (atemSwitcher.getTallyByIndexTallyFlags(settings.tallyNo) & 0x02) {   //and tally preview
              activeCamPreview = true;
            } else {
              activeCamPreview = false;
            }

            //Set tally light accordingly
            if (activeCam) {              //if tally live
                setSTRIP(LED_RED);
            } else if ((!(settings.tallyModeLED1 == MODE_PROGRAM_ONLY))                             //if not program only
                       && ((activeCamPreview)                                                       //and tally preview
                           || settings.tallyModeLED1 == MODE_PREVIEW_STAY_ON)) {                    //or preview stay on
                setSTRIP(LED_GREEN);
            } else {                                                                            // If tally is neither
                setSTRIP(LED_OFF);
            }

            //Show LED Strip changes
            FastLED.show();

            //Switch state if connection is lost, dependant on which connection is lost.
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("------------------------");
                Serial.println("WiFi connection lost...");
                setSTRIP(LED_OFF);
                changeState(STATE_CONNECTING_TO_WIFI);

                //Force atem library to reset connection, in order for status to read correctly on website.
                atemSwitcher.begin(settings.switcherIP);

                //Reset tally server's tally flags, They won't get the message, but it'll be reset for when the connectoin is back.
                tallyServer.resetTallyFlags();

            } else if (!atemSwitcher.hasInitialized()) { // will return false if the connection was lost
                Serial.println("------------------------");
                Serial.println("Connection to Switcher lost...");
                setSTRIP(LED_OFF);
                changeState(STATE_CONNECTING_TO_SWITCHER);

                //Reset tally server's tally flags, so clients turn off their lights.
                tallyServer.resetTallyFlags();
            }
            break;
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
            setStatusLED(LED_YELLOW);
            break;
        case STATE_CONNECTING_TO_SWITCHER:
            state = STATE_CONNECTING_TO_SWITCHER;
            setStatusLED(LED_ORANGE);
            break;
        case STATE_RUNNING:
            state = STATE_RUNNING;
            setStatusLED(LED_BLUE);
            break;
    }
    FastLED.show();
}

//Set current cam as preview cam to disable lights on all other tallys
void setActive(uint8_t camNumber) {
  Serial.println("---------------------");
  Serial.println(lastActive);
  Serial.println(lastSet);
  Serial.println(camNumber);
  if(lastActive != lastSet) {
      atemSwitcher.setPreviewInputVideoSource(0, camNumber);
      lastSet = camNumber;
  }
}

//Set the color of the 1st LED
void setSTRIP(uint8_t color) {
    int color_led[3] = { CRGB::Black, CRGB::Red, CRGB::Green };
  
    for (int i = 0; i < (NUM_LEDS-1); i++) {
      leds[i] = color_led[color];
    }
}

//Set the single status LED (last LED)
void setStatusLED(uint8_t color) {
  leds[11] = color_led[color];
  if(color == LED_BLUE) {
    leds[11].fadeToBlackBy(230);
  } else {
    leds[11].fadeToBlackBy(0);
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
    html += WiFi.SSID();
    html += "</td> </tr> <tr> <td><br></td> </tr> <tr> <td>Signal strength:</td> <td colspan=\"2\">";
    html += WiFi.RSSI();
    html += " dBm</td> </tr> <tr> <td><br></td> </tr> <tr> <td>Battery voltage:</td> <td colspan=\"2\">";
    html += dtostrf(uBatt, 0, 3, buffer);
    html += " V</td> </tr> <tr> <td>Static IP:</td> <td colspan=\"2\">";
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
    html += WiFi.hostname();
    html += "\" required /> </td> </tr> <tr> <td><br></td> </tr> <tr> <td>Tally Light number: </td> <td> <input type=\"number\" size=\"5\" min=\"1\" max=\"21\" name=\"tNo\" value=\"";
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
    html += ">Program only</option> </select> </td> </tr> <tr> <td>Tally Light mode (LED 2):</td> <td> <select name=\"tModeLED2\"> <option value=\"";
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
    html += ">Program only</option> </select> </td> </tr> <tr> <td><br></td> </tr> <tr> <td>Network name (SSID): </td> <td> <input type=\"text\" size=\"30\" maxlength=\"30\" name=\"ssid\" value=\"";
    html += WiFi.SSID();
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

            if (var ==  "tName") {
                val.toCharArray(settings.tallyName, (uint8_t)32);
            } else if (var ==  "tModeLED1") {
                settings.tallyModeLED1 = val.toInt();
            } else if (var ==  "tModeLED2") {
                settings.tallyModeLED2 = val.toInt();
            } else if (var ==  "tNo") {
                settings.tallyNo = val.toInt() - 1;
            } else if (var ==  "ssid") {
                ssid = String(val);
            } else if (var ==  "pwd") {
                pwd = String(val);
            } else if (var ==  "staticIP") {
                settings.staticIP = (val == "true");
            } else if (var ==  "tIP1") {
                settings.tallyIP[0] = val.toInt();
            } else if (var ==  "tIP2") {
                settings.tallyIP[1] = val.toInt();
            } else if (var ==  "tIP3") {
                settings.tallyIP[2] = val.toInt();
            } else if (var ==  "tIP4") {
                settings.tallyIP[3] = val.toInt();
            } else if (var ==  "mask1") {
                settings.tallySubnetMask[0] = val.toInt();
            } else if (var ==  "mask2") {
                settings.tallySubnetMask[1] = val.toInt();
            } else if (var ==  "mask3") {
                settings.tallySubnetMask[2] = val.toInt();
            } else if (var ==  "mask4") {
                settings.tallySubnetMask[3] = val.toInt();
            } else if (var ==  "gate1") {
                settings.tallyGateway[0] = val.toInt();
            } else if (var ==  "gate2") {
                settings.tallyGateway[1] = val.toInt();
            } else if (var ==  "gate3") {
                settings.tallyGateway[2] = val.toInt();
            } else if (var ==  "gate4") {
                settings.tallyGateway[3] = val.toInt();
            } else if (var ==  "aIP1") {
                settings.switcherIP[0] = val.toInt();
            } else if (var ==  "aIP2") {
                settings.switcherIP[1] = val.toInt();
            } else if (var ==  "aIP3") {
                settings.switcherIP[2] = val.toInt();
            } else if (var ==  "aIP4") {
                settings.switcherIP[3] = val.toInt();
            }
        }

        if (change) {
            EEPROM.put(0, settings);
            EEPROM.commit();

            server.send(200, "text/html", (String)"<!DOCTYPE html> <html> <head> <meta charset=\"ASCII\"> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> <title>Tally Light setup</title> </head> <body> <table bgcolor=\"#777777\" border=\"0\" width=\"100%\" cellpadding=\"1\" style=\"font-family:Verdana;color:#ffffff;font-size:12px;\"> <tr> <td> <h1>&nbsp;Tally Light setup</h1> </td> </tr> </table><br>Settings saved successfully.</body></html>");

            //Delay to let data be saved, and the responce to be sent properly to the client
            delay(5000);

            if (ssid && pwd && (ssid != WiFi.SSID() || pwd != WiFi.psk())) {
                WiFi.persistent(true);
                WiFi.begin(ssid, pwd);
                WiFi.persistent(false);
            }

            ESP.restart();
        }
    }
}

//Send 404 to client in case of invalid webpage being requested.
void handleNotFound() {
    server.send(404, "text/html", "<!DOCTYPE html> <html> <head> <meta charset=\"ASCII\"> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> <title>Tally Light setup</title> </head> <body style=\"font-family:Verdana;\"> <table bgcolor=\"#777777\" border=\"0\" width=\"100%\" cellpadding=\"1\" style=\"color:#ffffff;font-size:12px;\"> <tr> <td> <h1>&nbsp Tally Light setup</h1> </td> </tr> </table><br>404 - Page not found</body></html>");
}
