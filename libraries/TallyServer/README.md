# TallyServer libraby
This TallyServer library allows Clients running Skårhøj's (<https://skaarhoj.com>) Arduino libraries
for ATEM switchers to connect and retrieve tally data. This is useful as ATEM switchers only allow
for 5-8 connections (depending on the model), and you might need more tally lights than that. 

The library supports:
- Arduino boards with an Ethernet module or ESP8266 WiFi module.
- Stand Alone ESP8266 modeules.
- Stand Alone ESP32 modules.

The default constructor limits the TallyServer to accept 5 clients, as this is what the ESP8266 can handle. By using the __TallyServer(int _maxClients_)__ constructor you can raise the limit, as an ESP32 would be able to handle more clients at once, since it's a more powerful microprocessor.

## TallyServer documentation
Documentation for public methods (to be used in Arduino sketches)

### TallyServer()
Default constructor.

Contruct TallyServer with default capacity of 5 clients.

### TallyServer(int _maxClients_)
Construct TallyServer with a set max capacity of clients connected.

_maxClients_ - The max number of clients to accept.

### void begin()
Begin tally server, letting other tally lights connect to it in when calling _runLoop()_

## void end()
Disable tally server, disconnecting all tally lights currently connected.

### void runLoop()
Handle data transmission and connections to clients.

It's important that this is called __all the time__ in your _loop()_, as else clients will disconnect.

### void setTallySources(uint8_t _tallySources_)
Set the number of tally sources to send to clients. Must be less than 21.

_tallySources_ - The number of TallySources to set.

### void setTallyFlag(uint8_t _tallyIndex_, uint8_t _tallyFlag_)
Set a tally flag to send to clients in _runLoop()_.

_tallyIndex_ - the tally index to set tally flag for

_tallyFlag_ - the tally flag to set for the given index

Flag value | Flag meaning
--|----------------------
0 | No tally
1 | Program
2 | Preview

Note: The updated falgs wont be sent until _runLoop()_ has ben called, so you can safely update multiple falgs before sending the updated state.  

### void resetTallyFlags()
Set all Tally Flags to 0 (No tally)
