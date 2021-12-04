#ifdef CAMERA_CONTROL

#include <heltec.h>
SSD1306Wire *display;

// Pins on the ESP32 hooked up to the joystick
#define PIN_PAN   37
#define PIN_TILT  36
#define PIN_ZOOM  38

#define PIN_TRANSMIT 25 // This is the builtin led on the hiltec wifi kit board, just comment this out if you don't want it to light up on transmit
// of data to the camera

#define PIN_OVERRIDE 23
#define PIN_RECALL_1 22
#define PIN_RECALL_2 19
#define PIN_RECALL_3 18

// If you want to slow down the camera movement just change these from the visca standards below
// 18 is the VISCA max -- too fast
#define PAN_SPEED_MAX   15
// 17 is the VISCA max -- too fast
#define TILT_SPEED_MAX  15
#define ZOOM_SPEED_MAX  7

int previousCamera = 0;

void cameraControlSetup() {
  Heltec.begin(true /*Display Enable*/, false /*LoRa Disable*/, true /*Serial Enable*/);
  display = Heltec.display;

  display->flipScreenVertically();

  pinMode(PIN_PAN, INPUT);
  pinMode(PIN_TILT, INPUT);
  pinMode(PIN_ZOOM, INPUT);

  pinMode(PIN_RECALL_1, INPUT_PULLUP);
  pinMode(PIN_RECALL_2, INPUT_PULLUP);
  pinMode(PIN_RECALL_3, INPUT_PULLUP);
  pinMode(PIN_OVERRIDE, INPUT_PULLUP);

#ifdef PIN_TRANSMIT
  pinMode(PIN_TRANSMIT, OUTPUT);
  digitalWrite(PIN_TRANSMIT, 0);
#endif

  // No idea if this is correct or not - grabbed it off the arduino forum
  analogReadResolution(11); // Default of 12 is not very linear. Recommended to use 10 or 11 depending on needed resolution.
  analogSetAttenuation(ADC_6db); // Default is 11db which is very noisy. Recommended to use 2.5 or 6.

  if ( needToCalibrate() ) {
    autoCalibrate();
  }
  
  // Draw the inital screen once
  displayLoop(0, 0, 0);
}

boolean overridePreview() {
  return digitalRead( PIN_OVERRIDE ) == LOW ? true : false;
}

// This is where the magic happens. Normally this will talk to the ATEM and find the current preview camera. This is nice
// so no one accidentally moves the camera that is currently live. But if the "override" button is currently pressed (held down)
// then the camera that is on program will be selected for ptz movement or recall
int getActiveCamera() {
  if ( !atemSwitcher.isConnected() ) {
    // default to cam 1
    return 0;
  } else if ( overridePreview() ) {
    //Serial.printf("atem: %d - %d\n", atemSwitcher.getPreviewInputVideoSource(0), atemSwitcher.getProgramInputVideoSource(0));
    return atemSwitcher.getProgramInputVideoSource(0) - 1;
  } else {
    return atemSwitcher.getPreviewInputVideoSource(0) - 1;
  }
}

boolean needToCalibrate() {
  if ( settings.panMid == 0xFFFF ) { // uninitialized
    return true;
  }
  return false;
}

void displayCalibrateScreen(String direction, int pct) {
  display->clear();
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(DISPLAY_WIDTH / 2, 0 , "Calibrating...");

  display->drawLine(0, 25, DISPLAY_WIDTH, 25);
  display->setFont(ArialMT_Plain_16);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(DISPLAY_WIDTH / 2, 30, direction);

  display->drawProgressBar(0, DISPLAY_HEIGHT - 11, 120, 10, pct);
  display->display();
}

void autoCalibrate() {
  String instruction = "Center Joystick";
  displayCalibrateScreen(instruction, 0);

  // listen for 3 seconds takings readings ever 10ms and average

  int panInt = 0;
  int tiltInt = 0;
  int zoomInt = 0;
  for ( int i = 0; i < 100; i++) {
    panInt += analogRead( PIN_PAN );
    tiltInt += analogRead( PIN_TILT );
    zoomInt += analogRead( PIN_ZOOM );
    //Serial.printf("%d: %d - %d - %d\n", i, panInt, tiltInt, zoomInt);
    displayCalibrateScreen(instruction, i);
    delay(30);
  }
  settings.panMid = (uint16_t)(panInt / 100);
  settings.tiltMid = (uint16_t)(tiltInt / 100);
  settings.zoomMid = (uint16_t)(zoomInt / 100);
  Serial.printf("panMid: %d tiltMid: %d zoomMid: %d\n", settings.panMid, settings.tiltMid, settings.zoomMid);

  // Change display line 2 to "Keep joystick in lower right for 3 seconds"
  instruction = "Lower Right";
  // listen for 3 seconds aking readings every 10ms and take the max X and Y (or Min Y?)
  uint16_t pan = 0;
  uint16_t tilt = 0;
  for ( int i = 0; i < 100; i++) {
    pan = max( pan, analogRead( PIN_PAN ));
    tilt = max( tilt, analogRead( PIN_TILT ));
    displayCalibrateScreen(instruction, i);
    delay(30);
  }
  settings.panMax = pan;
  settings.tiltMax = tilt;
  Serial.printf("panMax: %d tiltMax: %d \n", settings.panMax, settings.tiltMax);

  // Change display line 2 to "Keep joystick in upper left for 3 seconds"
  instruction = "Upper Left";
  pan = UINT16_MAX;
  tilt = UINT16_MAX;
  for ( int i = 0; i < 100; i++) {
    pan = min( pan, analogRead( PIN_PAN ));
    tilt = min( tilt, analogRead( PIN_TILT ));
    displayCalibrateScreen(instruction, i);
    delay(30);
  }
  settings.panMin = pan;
  settings.tiltMin = tilt;
  Serial.printf("panMin: %d tiltMin: %d \n", settings.panMin, settings.tiltMin);

  instruction = "Zoom In";
  uint16_t zoom = 0;
  for ( int i = 0; i < 100; i++) {
    zoom = max( zoom, analogRead( PIN_ZOOM ));
    displayCalibrateScreen(instruction, i);
    delay(30);
  }
  settings.zoomMax = zoom;
  Serial.printf("zoomMax: %d\n", settings.zoomMax);

  instruction = "Zoom Out";
  zoom = UINT16_MAX;
  for ( int i = 0; i < 100; i++) {
    zoom = min( zoom, analogRead( PIN_ZOOM ));
    displayCalibrateScreen(instruction, i);
    delay(30);
  }
  settings.zoomMin = zoom;
  Serial.printf("zoomMin: %d\n", settings.zoomMin);

  // Store in settings and save them
  EEPROM.put(0, settings);
  EEPROM.commit();
}

int mapOffset( int input, int in_min, int in_mid, int in_max, int out_min, int out_max) {
  int in_threshold = (in_max - in_min) / 10; // 10% threshold variance

  if ( input > (in_mid - in_threshold) && input < (in_mid + in_threshold) ) {
    return 0;
  } else if ( input > in_mid ) {
    return map(input, in_mid + in_threshold, in_max, 0, out_max);
  } else {
    return map(input, in_min, in_mid - in_threshold, -1 * out_max, 0);
  }
}

void buttonLoop() {
  if ( digitalRead( PIN_RECALL_1 ) == LOW && digitalRead( PIN_RECALL_2) == LOW ) {
    autoCalibrate();
  } else if ( digitalRead( PIN_RECALL_1 ) == LOW) {
    visca_recall(1);
  } else if ( digitalRead( PIN_RECALL_2 ) == LOW) {
    visca_recall(2);
  } else if ( digitalRead( PIN_RECALL_3 ) == LOW) {
    visca_recall(3);
  }
}

void cameraControlLoop() {
  buttonLoop();

  int pan = analogRead( PIN_PAN );
  int tilt = analogRead( PIN_TILT );
  int zoom = analogRead( PIN_ZOOM );

  int panSpeed = mapOffset( pan, settings.panMin, settings.panMid, settings.panMax, -PAN_SPEED_MAX, PAN_SPEED_MAX );
  int tiltSpeed = mapOffset( tilt, settings.tiltMin, settings.tiltMid, settings.tiltMax, TILT_SPEED_MAX, -TILT_SPEED_MAX );
  int zoomSpeed = mapOffset( zoom, settings.zoomMin, settings.zoomMid, settings.zoomMax, -ZOOM_SPEED_MAX, ZOOM_SPEED_MAX );

  int activeCamera = getActiveCamera();

  displayLoop( panSpeed, tiltSpeed, zoomSpeed );
  ptzDrive( panSpeed, tiltSpeed, zoomSpeed );

  previousCamera = activeCamera;
}

void displayLoop( int panSpeed, int tiltSpeed, int zoomSpeed ) {
  display->clear();
  // Print to the screen
  if ( overridePreview() ) {
    display->fillRect(0, 0, DISPLAY_WIDTH, 25);
    display->setColor( BLACK );
  }
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(10, 0, "Camera: " + String(1 + getActiveCamera()));
  display->drawLine(0, 25, DISPLAY_WIDTH, 25);

  display->setColor( WHITE );
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 30, "Pan:");
  display->drawString(40, 30, String(panSpeed));
  display->drawString(0, 42, "Tilt:");
  display->drawString(40, 42, String(tiltSpeed));
  display->drawString(0, 54, "Zoom:");
  display->drawString(40, 54, String(zoomSpeed));

  // Wifi And ATEM status
  display->setFont(ArialMT_Plain_16);
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->drawString(DISPLAY_WIDTH - 15, 30, "WiFi:");
  drawBoolean( DISPLAY_WIDTH - 12, 34, 10, WiFi.status() == WL_CONNECTED );
  display->drawString(DISPLAY_WIDTH - 15, 48, "ATEM:");
  drawBoolean( DISPLAY_WIDTH - 12, 52, 10, atemSwitcher.isConnected() );

  // Draw it to the internal screen buffer
  //display->drawLogBuffer(0, 0);
  // Display it on the screen
  display->display();
}

void drawBoolean( int x, int y, int sz, boolean value ) {
  if ( value ) {
    display->fillRect( x, y, sz, sz );
  } else {
    display->drawRect( x, y, sz, sz );
  }
}
#endif
