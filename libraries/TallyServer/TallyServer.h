/*
Copyright (C) 2020 Aron N. Het Lam, aronhetlam@gmail.com

This file is a part of the Tally Sever library for use with Kasper Skårhøj's
(<https://skaarhoj.com>) ATEM clinet libraries for Arduino.

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

#include "Arduino.h"

#define TALLY_SERVER_DEBUG 0

#if defined ESP8266 || defined ESP32
#include <WifiUDP.h>
#else
#include <EthernetUdp.h>
#endif

#define TALLY_SERVER_FLAG_ACK               0b10000000
#define TALLY_SERVER_FLAG_RESEND_REQUEST    0b01000000
#define TALLY_SERVER_FLAG_RESENT_PACKAGE    0b00100000
#define TALLY_SERVER_FLAG_HELLO             0b00010000
#define TALLY_SERVER_FLAG_ACK_REQUEST       0b00001000

#define TALLY_SERVER_FLAG_CONNECTION_RECUEST    0b00000001
#define TALLY_SERVER_FLAG_CONNECTION_ACCEPTED   0b00000010
#define TALLY_SERVER_FLAG_CONNECTION_REJECTED   0b00000100
#define TALLY_SERVER_FLAG_CONNECTION_LOST       0b00001000

#define TALLY_SERVER_MAX_TALLY_FLAGS    41

#define TALLY_SERVER_BUFFER_LENGTH  62 //Max 42: Header = 12 + cmdHeader = 8 + tallySources = 2 + max 40 tally flags

#define TALLY_SERVER_DEFAULT_MAX_CLIENTS    5

#define TALLY_SERVER_KEEP_ALIVE_MSG_INTERVAL 1500

class TallyServer {
private:
#if defined ESP8266 || defined ESP32
    WiFiUDP _udp;
#else
    EthernetUDP _udp;
#endif

    struct TallyClient {
        IPAddress _tallyIP;
        uint16_t _tallyPort;
        bool _isConnected;
        bool _isInitialized;
        uint16_t _localPacketIdCounter;
        uint16_t _sessionID;
        unsigned long _lastRecv;
        unsigned long _lastSend;
        uint16_t _lastAckedID;
        uint16_t _lastRemotePacketID;
    };

    uint8_t _buffer[TALLY_SERVER_BUFFER_LENGTH];

    TallyClient* _clients;
    int _maxClients = 0; 

    uint16_t _atemTallySources;
    uint8_t _atemTallyFlags[TALLY_SERVER_MAX_TALLY_FLAGS];
    bool _tallyFlagsChanged;

    TallyClient *_getTallyClient(IPAddress clientIP, uint16_t clientPort);

    uint16_t _createTallyDataCmd();
    
    void _createHeader(TallyClient *client, uint8_t falgs, uint16_t lengthOfData);
    void _createHeader(TallyClient *client, uint8_t falgs, uint16_t lengthOfData, uint16_t remotePacketID);
    void _createHeader(TallyClient *client, uint8_t falgs, uint16_t lengthOfData, uint16_t remotePacketID,  uint16_t resendPacketID);
    
    void _sendBuffer(TallyClient *client, uint8_t length);
    void _sendBuffer(IPAddress ip, uint16_t port, uint8_t length);

    void _resetBuffer();

    void _resetClient(TallyClient *client);

    bool _hasTimePassed(unsigned long timestamp, uint16_t interval);

public:
    TallyServer();
    TallyServer(int maxClients);
    void begin();
    void end();
    void runLoop();
    void setTallySources(uint8_t tallySources);
    void setTallyFlag(uint8_t tallyIndex, uint8_t tallyFlag);
    void resetTallyFlags();
};
