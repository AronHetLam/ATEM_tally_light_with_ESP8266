#include "TallyServer.h"

TallyServer::TallyServer() {
#ifdef ESP8266
    WiFiUDP Udp;
#else
    EthernetUDP Udp;
#endif

    _udp = Udp;
}

void TallyServer::begin() {
    _udp.begin(9910);
}

/** Handle data transmission and connections to clients
 *
 **/
void TallyServer::runLoop() {
    // Handle incoming data
    uint16_t packetSize = _udp.parsePacket();

    if (_udp.available()) {
        IPAddress remoteIP = _udp.remoteIP();
        uint16_t remotePort = _udp.remotePort();

        _udp.read(_buffer, 12);
        uint8_t flags = _buffer[0] & 0b11111000;
        uint16_t packetLen = (_buffer[0] & 0b00000111) + _buffer[1];
        #if TALLY_SERVER_DEBUG
        Serial.print("paket size:   ");
        Serial.println(packetSize);
        Serial.print("paket length: ");
        Serial.println(packetLen);
        #endif

        if(packetSize == packetLen) { //If not the same something went wrong and we skip the packet.
            TallyClient *client = _getTallyClient(remoteIP, remotePort);

            if (client) {
                client->_sessionID = (_buffer[2] << 8) + _buffer[3];
                uint16_t remotePacketID = (_buffer[10] << 8) + _buffer[11];
                if (remotePacketID > 0) client->_lastRemotePacketID = remotePacketID;
                client->_lastRecv = millis();

                if (client->_isInitialized) {
                    if(flags & TALLY_SERVER_FLAG_ACK) {
                        client->_lastAckedID = (_buffer[4] << 8) + _buffer[5];
                        #if TALLY_SERVER_DEBUG
                        Serial.println("Ack recieved");
                        #endif

                    } if(flags & TALLY_SERVER_FLAG_ACK_REQUEST) {
                        _resetBuffer();
                        _createHeader(client, TALLY_SERVER_FLAG_ACK, 12, remotePacketID);
                        _sendBuffer(client, 12);
                        #if TALLY_SERVER_DEBUG
                        Serial.println("Ack resquest recieved - responded");
                        #endif

                    } if(flags & TALLY_SERVER_FLAG_RESEND_REQUEST) { //All we ever send is tally data... So let's just do that again.
                        _resetBuffer();
                        uint16_t cmdLen = 12 + _createTallyDataCmd();
                        uint16_t resendPacketID = (_buffer[6] << 8) + _buffer[7];
                        _createHeader(client, TALLY_SERVER_FLAG_RESENT_PACKAGE, cmdLen, 0, resendPacketID);
                        _sendBuffer(client, cmdLen);
                        #if TALLY_SERVER_DEBUG
                        Serial.println("Resend resquest recieved - sent tally data with wanted packageID");
                        #endif
                    }

                    #if TALLY_SERVER_DEBUG
                        if(flags & TALLY_SERVER_FLAG_RESENT_PACKAGE) Serial.println("Resent package recieved - ignoring it.");
                        if(flags & TALLY_SERVER_FLAG_HELLO) Serial.println("Hello packet recieved - ignoring it.");
                    #endif

                } else if (client->_isConnected) { // Initialize new connection
                    if(flags & TALLY_SERVER_FLAG_ACK) {
                        _resetBuffer();
                        uint16_t cmdLen = 12 + _createTallyDataCmd();
                        _createHeader(client, TALLY_SERVER_FLAG_ACK_REQUEST, cmdLen);
                        _sendBuffer(client, cmdLen);

                        _resetBuffer();
                        _createHeader(client, TALLY_SERVER_FLAG_ACK_REQUEST, 12);
                        _sendBuffer(client, 12);

                        client->_isInitialized = true;
                        #if TALLY_SERVER_DEBUG
                        Serial.println("Client Initialized");
                        #endif
                    }

                    #if TALLY_SERVER_DEBUG
                    else Serial.println("Not initialized - Expected ack for hello packet");
                    #endif

                } else { //New connection
                    if (flags & TALLY_SERVER_FLAG_HELLO) {//Respond to first hello packet.
                        _resetBuffer();
                        _createHeader(client, TALLY_SERVER_FLAG_HELLO, 20);
                        _buffer[12] = TALLY_SERVER_FLAG_CONNECTION_ACCEPTED;
                        _sendBuffer(client, 20);
                        client->_isConnected = true;
                        #if TALLY_SERVER_DEBUG
                        Serial.println("Client connected");
                        #endif

                    } else { //Something is wrong - reset client. 
                        _resetClient(client);
                        #if TALLY_SERVER_DEBUG
                        Serial.println("First packet not hello packet - reset client");
                        #endif
                    }
                }
            } else { //No client means no empty spot
                if (flags & TALLY_SERVER_FLAG_HELLO) { //Reject connection
                    _resetBuffer();
                    _createHeader(client, TALLY_SERVER_FLAG_HELLO, 20);
                    _buffer[12] = TALLY_SERVER_FLAG_CONNECTION_REJECTED;
                    _sendBuffer(client, 20);
                    #if TALLY_SERVER_DEBUG
                    Serial.println("Connection rejected - no empty spot");
                    #endif
                } //Else we ignore what came in..
                
                #if TALLY_SERVER_DEBUG
                else Serial.println("Connection rejected - packet not hello packet");
                #endif
            }
        }
    }

    if(_tallyFlagsChanged) { //Send new tally data to clients
        //Reset buffer and construct tally data cmd. The cmd is the same for all clients
        #if TALLY_SERVER_DEBUG
        Serial.println("Sending new tally data");
        #endif
        _resetBuffer();
        uint16_t cmdLen = 12 + _createTallyDataCmd();

        //We build a client specific header and send the packet, if it's initialized
        for(int i = 0; i < TALLY_SERVER_MAX_CLIENTS; i++) {
            TallyClient *client = &_clients[i];
            if(client->_isInitialized) {
                _createHeader(client, TALLY_SERVER_FLAG_ACK_REQUEST, cmdLen);
                _sendBuffer(client, cmdLen);
            }
        }

        _tallyFlagsChanged = false;
    }

    //Keep connections alive by requesting ACK pacages form them wtih a given interval
    for(int i = 0; i < TALLY_SERVER_MAX_CLIENTS; i++) {
        TallyClient *client = &_clients[i];
        if(client->_isInitialized) {
            if(client->_lastAckedID < client->_localPacketIdCounter && _hasTimePassed(client->_lastSend, 500)) {
                _resetBuffer();
                uint16_t cmdLen = 12 + _createTallyDataCmd();
                _createHeader(client, TALLY_SERVER_FLAG_ACK_REQUEST, cmdLen);
                _sendBuffer(client, cmdLen);

            } else if(_hasTimePassed(client->_lastRecv, TALLY_SERVER_KEEP_ALIVE_MSG_INTERVAL) && _hasTimePassed(client->_lastSend, TALLY_SERVER_KEEP_ALIVE_MSG_INTERVAL)) {
                _resetBuffer();
                _createHeader(client, TALLY_SERVER_FLAG_ACK_REQUEST, 12);
                _sendBuffer(client, 12);

            } else if(_hasTimePassed(client->_lastRecv, 5000)) {
                _resetClient(client);
                #if TALLY_SERVER_DEBUG
                Serial.println("Client disconnected - Was initialized");
                #endif
            }

        } else if(client->_isConnected) {
            if(_hasTimePassed(client->_lastSend, TALLY_SERVER_KEEP_ALIVE_MSG_INTERVAL)) {
                _resetBuffer();
                _createHeader(client, TALLY_SERVER_FLAG_HELLO, 20);
                _buffer[12] = TALLY_SERVER_FLAG_CONNECTION_ACCEPTED;
                _sendBuffer(client, 20);

            } else if(_hasTimePassed(client->_lastRecv, 5000)) {
                _resetClient(client);
                #if TALLY_SERVER_DEBUG
                Serial.println("Client disconnected - Was not initialized");
                #endif
            }
        }
    }
}

/** 
 * Set number of tally sources to send to clients. Must be less than 21.
 */
void TallyServer::setTallySources(uint8_t tallySources) {
    if(tallySources < 21)
        _atemTallySources = tallySources;
}

/**
 * Set tally flag to send to clients
 */
void TallyServer::setTallyFlag(uint8_t tallyIndex, uint8_t tallyFlag) {
    if (tallyIndex < 21 && _atemTallyFlags[tallyIndex] != tallyFlag) {
        _atemTallyFlags[tallyIndex] = tallyFlag;
        _tallyFlagsChanged = true;
    }
}

/**
 * Build tally by index commant in the command buffer
 * based on _atemTallySources and _atemTallyFlags, and
 * return the commands length.
 */
uint16_t TallyServer::_createTallyDataCmd() {
    uint16_t cmdLen = 10 + _atemTallySources; //header = 8 + num sources = 2 + *num sources*

    //Cmd Length
    _buffer[12] = cmdLen >> 8;
    _buffer[13] = cmdLen;

    //Cmd header byte 3 and 4's use is unknown, and aren't needed by the ATEM library...

    //Cmd name
    _buffer[16] = 'T';
    _buffer[17] = 'l';
    _buffer[18] = 'I';
    _buffer[19] = 'n';
    
    //Number of tally sources
    _buffer[20] = _atemTallySources >> 8;
    _buffer[21] = _atemTallySources;

    //Tally flag for each source
    for(int i = 0; i < _atemTallySources; i++) {
        _buffer[22 + i] = _atemTallyFlags[i];
    }

    return cmdLen;
}

/**
 * Get the client struct with the given IP and Port. If no match, a disconnected spot is
 * returned with the given IP and Port. If no disconnected spots are availabel, NULL is returned.
 */
TallyServer::TallyClient *TallyServer::_getTallyClient(IPAddress clientIP, uint16_t clientPort) {
    int8_t emptySpot = -1;
    for (int i = 0; i < TALLY_SERVER_MAX_CLIENTS; i++) {
        if (_clients[i]._isConnected && _clients[i]._tallyIP == clientIP && _clients[i]._tallyPort == clientPort) {
            return &_clients[i];
        } else if (emptySpot < 0 && !_clients[i]._isConnected) {
            emptySpot = i;
        }
    }

    if (emptySpot >= 0) {
        _clients[emptySpot]._tallyIP = clientIP;
        _clients[emptySpot]._tallyPort = clientPort;
        return &_clients[emptySpot];
    }

    return NULL;
}

/**
 * _createHeader without remotePacketID and and resendPacketID
 */
void TallyServer::_createHeader(TallyClient *client, uint8_t flags, uint16_t lengthOfData) {
    _createHeader(client, flags, lengthOfData, 0);
}

/**
 * Main _createHeader method, which builds a header to send for the ATEM protocol.
 */
void TallyServer::_createHeader(TallyClient *client, uint8_t flags, uint16_t lengthOfData, uint16_t remotePacketID) {
    _buffer[0] = (flags) | (lengthOfData >> 8) & 0b00000111;    //Flags + length
    _buffer[1] = lengthOfData;                                  //Length

    _buffer[2] = client->_sessionID >> 8;   //Session ID
    _buffer[3] = client->_sessionID;        //Session ID

    _buffer[4] = remotePacketID >> 8;       //Remote Packet ID
    _buffer[5] = remotePacketID;            //Remote Packet ID

    if(flags & TALLY_SERVER_FLAG_ACK_REQUEST && !(flags & (TALLY_SERVER_FLAG_RESENT_PACKAGE | TALLY_SERVER_FLAG_RESEND_REQUEST | TALLY_SERVER_FLAG_HELLO ))) {
        client->_localPacketIdCounter++;    //Increase local packet ID on new Ack request

        _buffer[10] = client->_localPacketIdCounter >> 8;   //Local Packet ID
        _buffer[11] = client->_localPacketIdCounter;        //Local Packet ID
    }
}

/**
 * _createHeader with resendPacketID, for when immitating a resent packet. 
 */
void TallyServer::_createHeader(TallyClient *client, uint8_t flags, uint16_t lengthOfData, uint16_t remotePacketID,  uint16_t resendPacketID) {
    _createHeader(client, flags, lengthOfData, remotePacketID);

    _buffer[10] = resendPacketID >> 8;
    _buffer[11] = resendPacketID;
}

/**
 * Send length of what's in the buffer to the given client.
 */
void TallyServer::_sendBuffer(TallyClient *client, uint8_t length) {
    _sendBuffer(client->_tallyIP, client->_tallyPort, length);
    client->_lastSend = millis();
}

/**
 * Send length of what's in the buffer to the given IP and Port
 */
void TallyServer::_sendBuffer(IPAddress ip, uint16_t port, uint8_t length) {
    _udp.beginPacket(ip, port);
    _udp.write(_buffer, length);
    _udp.endPacket();
}

/**
 * Reset buffer - set all bytes to 0
 */
void TallyServer::_resetBuffer() {
    memset(_buffer, 0, TALLY_SERVER_BUFFER_LENGTH);
}

/**
 * Reset given client struct, so that it's ready for a new client connecting
 */
void TallyServer::_resetClient(TallyClient *client) {
    client->_isConnected = false;
    client->_isInitialized = false;
    client->_lastRecv = 0;
    client->_localPacketIdCounter = 0;
    client->_lastRemotePacketID = 0;
    client->_sessionID = 0;
}

/**
 * Check if an interval of time has passed since the given timestamp
 */
bool TallyServer::_hasTimePassed(unsigned long timestamp, uint16_t interval) {
    if((unsigned long)millis() - timestamp >= interval) return true; //This takes account for rollover
    else return false;
}

/**
 * Set all _atemTallyFlags to 0
 */
void TallyServer::resetTallyFlags() {
    memset(_atemTallyFlags, 0, 21);
}
