# TallyServer documentation
Documentation for public methods (to be used in Arduino sketches)

## TallyServer()
Default constructor.

Contruct TallyServer with default capacity of 5 clients.

## TallyServer(int _maxClients_)
Construct TallyServer with a set max capacity of clients connected.

_maxClients_ - The max number of clients to accept.

## void begin()
Begin tally server, letting other tally lights connect to it in _runLoop()_

## void end()
Disable tally server, disconnecting all tally lights currently connected.

## void runLoop()
Handle data transmission and connections to clients.
It's important that this is called __all the time__ in your _loop()_, as else the connection to clients will be dropped.

## void setTallySources(uint8_t _tallySources_)
Set the number of tally sources to send to clients. Must be less than 21.

_tallySources_ - The number of TallySources to set.

## void setTallyFlag(uint8_t _tallyIndex_, uint8_t _tallyFlag_)
Set a tally flag to send to clients in _runLoop()_.

_tallyIndex_ - the tally index to set tally flag for

_tallyFlag_ - the tally flag to set for the given index

Flag value | Flag meaning
0 | No tally
1 | Program
2 | Preview

## void resetTallyFlags()
Set all Tally Flags to 0 (No tally)
