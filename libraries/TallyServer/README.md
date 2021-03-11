# TallyServer Arduino library
his TallyServer library allows Clients running Skaarhoj's (<https://skaarhoj.com>) [ATEM libraries for Arduino](https://github.com/kasperskaarhoj/SKAARHOJ-Open-Engineering/tree/master/ArduinoLibs) to connect and retrieve tally data. This is useful as ATEM switchers only allow for 5-8 connections (depending on the model), and you might need more tally lights than that.

This library can also be used for creating a tally system for other platforms, if you get tally data from another data source and use this librabry to transmit the data to your tally lights. Again, the tally lights need to run [Skaarhoj's ATEM libraries for Arduino](https://github.com/kasperskaarhoj/SKAARHOJ-Open-Engineering/tree/master/ArduinoLibs).

The library supports:
- Arduino boards with an Ethernet module or ESP8266 WiFi module.
- Stand Alone ESP8266 modeules.
- Stand Alone ESP32 modules. (Skaarhoj's libraries doesn't support this natively. Use my version of the [ATEMbase](https://github.com/AronHetLam/ATEM_tally_light_with_ESP8266/tree/master/libraries) library that fixes this)

The default constructor limits the TallyServer to accept 5 clients, as this is what the ESP8266 can handle. By using the __TallyServer(int _maxClients_)__ constructor you can raise the limit, as an ESP32 would be able to handle more clients at once, as it's a more powerful microprocessor.

# TallyServer documentation
Documentation for public methods (to be used in Arduino sketches)

## Constructors

### TallyServer()
Default constructor.

Contruct TallyServer with default capacity of 5 clients.

### TallyServer(int _maxClients_)
Construct TallyServer with a set max capacity of clients connected.

_int maxClients_: The max amount of clients to allow simultaniously.

## Methods

### void begin()
Begin tally server, letting other tally lights connect to it in [_runLoop()_](#void_runLoop()).

### void end()
Disable tally server, disconnecting all tally lights currently connected.

### void runLoop()
Handle data transmission and connections to clients.

It's important that this is called __all the time__ in your _loop()_, as else clients will disconnect.

### void setTallySources(uint8_t _tallySources_)
Set the number of tally sources to send to clients. Must be less than 41.

_uint8_t tallySources_: The amount of tally sources to send to clients.

### void setTallyFlag(uint8_t _tallyIndex_, uint8_t _tallyFlag_)
Set a tally flag to send to clients in _runLoop()_.

_uint8_t tallyIndex_: The tally index to set tally flag for. 0 indexed.

_uint8_t tallyFlag_: The tally flag value. 

Flag value | Flag meaning
--|----------------------
0 | No tally
1 | Program
2 | Preview
3 | Both program and preview

Note: The updated falgs wont be sent until _runLoop()_ is called, so you can safely update multiple falgs before sending the updated state.

### void resetTallyFlags()
Set all Tally Flags to 0 (No tally)
