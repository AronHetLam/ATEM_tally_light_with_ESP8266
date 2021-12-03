#ifdef CAMERA_CONTROL

// wait wait time for a visca packet to come back from the camera
#define VISCA_MAX_WAIT        20
#define VISCA_MAX_RESPONSE    1024
#define VISCA_PORT            52381

//#define VISCA_DEBUG 1

byte visca_empty[256] = {};
byte visca_previous[256] = {};


#define RECALL_NUM_BYTE 13
byte visca_recall_bytes[] = { 0x01, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x02, 0x81, 0x01, 0x04, 0x3f, 0x02, 0x02, 0xff };
//                            [payload ]  [ payload]  [   sequence number  ]  [           payload                    ]
//                              type        length        LSB big-endian
// Change bytes 12 and 13 for pan and tilt speed
#define PAN_SPEED_BYTE 12
#define TILT_SPEED_BYTE 13
#define PT_DIR1_BYTE 14
#define PT_DIR2_BYTE 15
byte visca_pt_drive_bytes[] = { 0x01, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x02, 0x81, 0x01, 0x06, 0x01, 0x00, 0x00, 0x03, 0x03, 0xff };

// 8x 01 04 07 2p FF
#define ZOOM_DIR_BYTE 12
#define ZOOM_TELE 2 << 8
#define ZOOM_WIDE 3 << 8
byte visca_zoom_bytes[] = { 0x01, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x02, 0x81, 0x01, 0x04, 0x07, 0x00, 0xff };


// 8x 09 04 47 FF
byte visca_zoom_inq_bytes[] = { 0x01, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x02, 0x81, 0x09, 0x04, 0x47, 0xff };
#define ZOOM_INQ_MIN 0x0000
#define ZOOM_INQ_MAX 0x6000
byte visca_zoom_inq_resp_bytes[] = { 0x01, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x02, 0x00, 0x50, 0x0e, 0x0e, 0x0e, 0x0e, 0xFF };

uint32_t sequenceNumber = 1;

boolean pt_stopped = true;
boolean zoom_stopped = true;

// For debugging packets to strings
void printBytes(byte array[], unsigned int len)
{
  int b = 0;
  char buffer[1024] = "";

  for (unsigned int i = 0; i < len; i++)
  {
    byte nib1 = (array[i] >> 4) & 0x0F;
    byte nib2 = (array[i] >> 0) & 0x0F;
    buffer[b++] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
    buffer[b++] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
    buffer[b++] = ' ';
  }
  buffer[b++] = '\0';

  if (len > 0) {
    Serial.println(buffer);
  }
}

void writeBytes( uint32_t value, byte packet[], int position ) {
  byte b4 = (byte)(value & 0xFFu);         // LSB
  byte b3 = (byte)((value >> 8) & 0xFFu);
  byte b2 = (byte)((value >> 16) & 0xFFu);
  byte b1 = (byte)((value >> 24) & 0xFFu); // MSB

  packet[position++] = b1;
  packet[position++] = b2;
  packet[position++] = b3;
  packet[position++] = b4;
}

void visca_send(byte packet[], int size, int cameraNumber, boolean waitForAck = true, boolean waitForComplete = true, byte response[] = visca_empty, int responseSize = 256 ) {
  WiFiUDP udp;

  // Don't keep sending the same packet -- compare to previous
  if ( previousCamera == cameraNumber && 0 == memcmp( visca_previous, packet, size ) ) {
#ifdef VISCA_DEBUG
    Serial.println( "Duplicate packet on same camera: ignoring" );
    Serial.printf("previousCamera: %d cameraNumber: %d\n", previousCamera, cameraNumber);
    printBytes( visca_previous, size );
    printBytes( packet, size );
#endif
    return;
  }

  // PSK BUG IMPORTANT - Need to call begin( VISCA_PORT ) so I can listen for incomign messages, I'm not sure how I'm getting any packets right now
  udp.begin(VISCA_PORT);

  // PSK BUG Keep a UDP socket around for each camera so I'm not creating each one every message
  // SONY MANUAL:
  // If the multiple commands are send without waiting for the reply, the possibility of non-execution of the command and errors due to
  // buffer overflow become high, because of limitations of order to receive commands or execution interval of command.
  // It may cause efficiency to be reduced substantially.

  if ( cameraNumber < 0 || cameraNumber > NUM_CAMERAS ) {
    Serial.printf("Invalid cameraNumber: %d\n", cameraNumber);
    return;
  }

#ifdef PIN_TRANSMIT
  digitalWrite(PIN_TRANSMIT, HIGH);
#endif

  // Serial.printf("sending packet to camera: %d at ip address: %s\n", cameraNumber + 1, settings.cameraIP[cameraNumber].toString().c_str());
  int status = udp.beginPacket(settings.cameraIP[cameraNumber], VISCA_PORT );
  if ( !status ) {
    Serial.printf("beginPacket returned zero: %d\n", status);
    return;
  }

  // Add sequence number - cameras will respond to the same sequence number.. tested this.
  writeBytes(sequenceNumber, packet, 4);
  sequenceNumber++;

  //   Serial.println(packet);
  int written = udp.write(packet, size);
  if ( written != size ) {
    Serial.printf("Didn't write enough bytes: %d vs %d\n", size, written);
    return;
  }
  status = udp.endPacket();
  if ( !status ) {
    Serial.printf("endPacket returned zero: %d\n", status);
    return;
  }

  memcpy( visca_previous, packet, size );

  if ( waitForAck ) {
    // Wait for an ACK response from the camera
    boolean ackRecieved = false;
    for ( int i = 0; i < VISCA_MAX_WAIT; i++) {
      if ( udp.parsePacket() > 0 ) {
        ackRecieved = true;
        break;
      }
      delay(100);
#ifdef VISCA_DEBUG
      Serial.print(".");
#endif
    }
    if ( !ackRecieved ) {
      Serial.println("No ACK response");
      waitForComplete = false;
    } else {
      // receive incoming UDP packets
      int len = udp.read(response, VISCA_MAX_RESPONSE);
      
      // Check for valid ACK
      if ( 0x40 != (response[9] & 0xF0) ) {
        Serial.print("Invalid ACK: ");
        printBytes(response, len);
        waitForComplete = false;
      }
#ifdef VISCA_DEBUG
      Serial.printf("ACK Received %d bytes from %s, port %d: ", len, udp.remoteIP().toString().c_str(), udp.remotePort());
      printBytes(response, len);
#endif
    }

    if ( waitForComplete ) {
      boolean completeRecieved = false;
      // Wait for a COMPLETE message from the camera
      for ( int i = 0; i < VISCA_MAX_WAIT; i++) {
        if ( udp.parsePacket() > 0 ) {
          completeRecieved = true;
          break;
        }
        delay(100);
#ifdef VISCA_DEBUG
        Serial.print(".");
#endif
      }
      if ( !completeRecieved ) {
        Serial.println("No COMPLETE response");
      } else {
        // receive incoming UDP packets
        int len = udp.read(response, VISCA_MAX_RESPONSE);
        
        // Check for valid Complete
        if ( 0x50 != (response[9] & 0xF0) ) {
          Serial.print("Invalid COMPLETE: ");
          printBytes(response, len);
          waitForComplete = false;
        }
#ifdef VISCA_DEBUG
        Serial.printf("COMPLETE Received %d bytes from %s, port %d: ", len, udp.remoteIP().toString().c_str(), udp.remotePort());
        printBytes(response, len);
#endif
      }
    }
  }
#ifdef PIN_TRANSMIT
  digitalWrite(PIN_TRANSMIT, LOW);
#endif
}

// PSK TODO Break this into viscaPTDrive() and viscaZoom()
void ptzDrive( int panSpeed, int tiltSpeed, int zoomSpeed ) {
  int activeCamera = getActiveCamera();
  // First check to see if we switch the preview camera in the middle of a pan/tilt or zoom
  // If so, stop the movement on the previous camera
  // Serial.printf("ptzLoop: previousCamera: %d, pt_stopped: %d, zoom_stopped: %d\n", previousCamera, pt_stopped, zoom_stopped);
  if ( previousCamera != activeCamera ) {
    if ( !pt_stopped ) {
      visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x03;
      visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x03;
      Serial.println("Stopping pt on previous camera");
      visca_send(visca_pt_drive_bytes, sizeof(visca_pt_drive_bytes), previousCamera);
    }
    if ( !zoom_stopped ) {
      visca_zoom_bytes[ZOOM_DIR_BYTE] = 0x0;
      Serial.println("Stopping zoom on previous camera");
      visca_send(visca_zoom_bytes, sizeof(visca_zoom_bytes), previousCamera);
    }
  }

  // PSK TODO Get ridof these.. totally unnessesary and confusing
  // Just ignore the visca command if it is a duplicate in viscaSend()
  boolean pt_stopping = false;
  boolean zoom_stopping = false;

  visca_pt_drive_bytes[PAN_SPEED_BYTE] = byte(abs(panSpeed));
  visca_pt_drive_bytes[TILT_SPEED_BYTE] = byte(abs(tiltSpeed));

  if ( panSpeed > 0 && tiltSpeed > 0 ) {      // upright
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x02;
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x01;
  } else if ( panSpeed < 0 && tiltSpeed < 0 ) { // downleft
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x01;
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x02;
  } else if ( panSpeed > 0 && tiltSpeed < 0 ) { // downright
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x02;
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x02;
  } else if ( panSpeed < 0 && tiltSpeed > 0 ) { // upleft
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x01;
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x01;
  } else if ( panSpeed > 0 ) {                // right
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x02;
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x03;
  } else if ( panSpeed < 0 ) {                // left
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x01;
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x03;
  } else if ( tiltSpeed > 0 ) {               // up
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x03;
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x01;
  } else if ( tiltSpeed < 0 ) {               // down
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x03;
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x02;
  } else {                                    // stop
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x03;
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x03;
    pt_stopping = true;
  }
  if ( !(pt_stopped == true && pt_stopping == true) ) {
    visca_send(visca_pt_drive_bytes, sizeof(visca_pt_drive_bytes), activeCamera);
  }

  if ( zoomSpeed > 0 ) {
    visca_zoom_bytes[ZOOM_DIR_BYTE] = 0x02; //ZOOM_TELE + byte(abs(zoomSpeed));
  } else if ( zoomSpeed < 0 ) {
    visca_zoom_bytes[ZOOM_DIR_BYTE] = 0x03; // ZOOM_WIDE + byte(abs(zoomSpeed));
  } else {
    visca_zoom_bytes[ZOOM_DIR_BYTE] = 0x0;  // Stop
    zoom_stopping = true;
  }
  if ( !(zoom_stopped == true && zoom_stopping == true) ) {
    visca_send(visca_zoom_bytes, sizeof(visca_zoom_bytes), activeCamera);
  }

  pt_stopped = pt_stopping;
  zoom_stopped = zoom_stopping;
}

void visca_recall( int pos ) {
  visca_recall_bytes[RECALL_NUM_BYTE] = byte(pos);
  visca_send(visca_recall_bytes, sizeof(visca_recall_bytes), getActiveCamera());
}

#endif
