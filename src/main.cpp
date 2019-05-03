#include <Arduino.h>
#include <JC_Button.h> // https://github.com/JChristensen/JC_Button
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BMP280.h"
#include <RtcDS3231.h>
#include <U8g2lib.h>
#include <SPI.h>

#define MODE_GROUND 3
#define MODE_AIRPLANE 2 
#define MODE_FREEFALL 0
#define MODE_CANOPY 1
#define MODE_MENU 4
#define MODE_UPDATE 5

#define PIN_BUTTON_UP 25
#define PIN_BUTTON_DOWN 33
#define PIN_BUTTON_ENTER 32

#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22

#define PIN_DISPLAY_CLOCK 18
#define PIN_DISPLAY_DATA 23
#define PIN_DISPLAY_CS 5
#define PIN_DISPLAY_DC 14
#define PIN_DISPLAY_RESET 12

#define uS_TO_S_FACTOR 1000000 //Conversion factor for micro seconds to seconds
#define mS_TO_S_FACTOR 1000    //Conversion factor for milli seconds to seconds

Adafruit_BMP280 pressureSensor1;
Adafruit_BMP280 pressureSensor2;

U8G2_SSD1309_128X64_NONAME2_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/PIN_DISPLAY_CLOCK, /* data=*/PIN_DISPLAY_DATA, /* cs=*/PIN_DISPLAY_CS, /* dc=*/PIN_DISPLAY_DC, /* reset=*/PIN_DISPLAY_RESET);

RtcDS3231<TwoWire> rtc(Wire);

Button buttonUp(PIN_BUTTON_UP);
Button buttonDown(PIN_BUTTON_DOWN);
Button buttonEnter(PIN_BUTTON_ENTER);

// define constants
const unsigned long LONG_PRESS = 1000; // what counts as long press
const unsigned long sleepTime = 30;    // after what time to sleep
const unsigned long sleepForTime = 30; // sleep for how long

// define variables
bool debug = true; // debug mode? enables Serial Messages
bool demo = false;
byte mode;         // current work mode
unsigned long currentMillis = 0;
unsigned long groundTime = 0;
int altitude1;
int altitude2;
int currentAltitude;

unsigned long lastAltiCheck = 0;
unsigned long lastAction = 0;

// variables to keep during sleep
RTC_DATA_ATTR int altitudeBeforeSleep;
RTC_DATA_ATTR float defaultPressure1;
RTC_DATA_ATTR float defaultPressure2;

/* -------------------------------------------------------------------------------------------------------- */

// function to display debug messages on the serial console if DEBUG == true
void debugMessage(String message)
{
  if (debug)
  {
    Serial.println(message);
    delay(100);
  }
};

/* -------------------------------------------------------------------------------------------------------- */

void readButtons()
{
  if (mode > 2)
  {
    buttonEnter.read();
    buttonUp.read();
    buttonDown.read();
  }
};

/* -------------------------------------------------------------------------------------------------------- */

// Altitude check - filter - smoothen

void checkAltitude()
{
  if(!demo)
  {
  if (lastAltiCheck + 200 < currentMillis)
  {
    altitude1 = pressureSensor1.readAltitude(defaultPressure1);
    //altitude2 = pressureSensor2.readAltitude(defaultPressure2);
    //currentAltitude = ( altitude1 + altitude2 ) / 2;
    currentAltitude = altitude1;
    lastAltiCheck = currentMillis;
  }
  }
};

/* -------------------------------------------------------------------------------------------------------- */

// change to newMode
void changeModeTo(byte newMode)
{
  lastAction = 0;
  debugMessage("switching to mode:" + (String)newMode);
  mode = newMode; // change to menuMode
};

/* -------------------------------------------------------------------------------------------------------- */

// check if altitude changed during sleep
// yes -> airplaneMode
// no -> sleep again
void checkAltitudeAfterWakeup()
{
  debugMessage("check alti after wakeup");

  // REMOVE - Demo start planeMode after first sleep
  if (debug)
    currentAltitude = currentAltitude + 60;
  // REMOVE

  debugMessage("current altitude");
  debugMessage((String)currentAltitude);

  if (currentAltitude > altitudeBeforeSleep + 50)
  {
    debugMessage("Altitude change! Switching to planeMode");
    changeModeTo(2); // change to planeMode
  }
  else
  {
    debugMessage("no altitude change going to sleep again...");
    esp_deep_sleep_start();
  }
};

/* -------------------------------------------------------------------------------------------------------- */

void groundMode()
{
  if (groundTime == 0)
  {
    groundTime = millis();
  }
  // 30 seconds without action passed?
  else if (groundTime + (sleepTime * mS_TO_S_FACTOR) < currentMillis)
  {
    debugMessage("going to sleep...");
    esp_deep_sleep_start();
  }
  // check Enter Button to enter Menu
  if (buttonEnter.wasPressed())
  {
    groundTime = 0;
    changeModeTo(4); // change Mode to menuMode
  }
};

/* -------------------------------------------------------------------------------------------------------- */

void airplaneMode()
{
  // check alti
  // calculate climb speed
  // calculate time to 4000
  // display stuff
  // check if climb is negativ -> switch to freefallMode
  if (lastAction == 0)
  {
    lastAction = currentMillis;
  }
  else if (lastAction + 20000 < currentMillis)
  {
    debugMessage("DEMO: arrived at 4000m exit no -> freefallMode");
    changeModeTo(0); // change to freefallMode
  }
};

/* -------------------------------------------------------------------------------------------------------- */

void freefallMode(){
 if (lastAction == 0)
  {
    lastAction = currentMillis;
  }
  else if (lastAction + 20000 < currentMillis)
  {
    debugMessage("DEMO: freefall ended -> canopyMode");
    changeModeTo(1); // change to freefallMode
  }
};

/* -------------------------------------------------------------------------------------------------------- */

void canopyMode(){
   if (lastAction == 0)
  {
    lastAction = currentMillis;
  }
  else if (lastAction + 20000 < currentMillis)
  {
    debugMessage("DEMO: landed! -> groundMode");
    changeModeTo(3); // change to groundMode
  }
};

/* -------------------------------------------------------------------------------------------------------- */

void menuMode()
{
  if (buttonEnter.pressedFor(2000))
  {
    debugMessage("switching to groundMode");
    mode = 3;
    delay(500);
  }
};

/* -------------------------------------------------------------------------------------------------------- */

void updateMode(){

};

/* -------------------------------------------------------------------------------------------------------- */

void setup()
{
  if (debug)
    Serial.begin(115200);

  pressureSensor1.begin(0x76);
  // pressureSensor2.begin(0x77);

  esp_sleep_enable_timer_wakeup(sleepForTime * uS_TO_S_FACTOR);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_32, 0);

  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER)
  {
    checkAltitudeAfterWakeup();
  }
  else
  {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    debugMessage("Startdruck:");
    defaultPressure1 = pressureSensor1.readPressure() / 100;
    // defaultPressure2 = pressureSensor2.readPressure() / 100;
    debugMessage((String)defaultPressure1);
    //debugMessage((String)defaultPressure2);
    debugMessage("Starthöhe:");
    checkAltitude();
    debugMessage((String)currentAltitude);

    buttonUp.begin();
    buttonDown.begin();
    buttonEnter.begin();

    u8g2.begin();
    u8g2.enableUTF8Print();
    u8g2.setFlipMode(1);

    mode = 3;
  }
}

/* -------------------------------------------------------------------------------------------------------- */

void loop()
{
  currentMillis = millis();
  checkAltitude();
  readButtons();

  switch (mode)
  {
  case MODE_FREEFALL:
    freefallMode();
    break;

  case MODE_CANOPY:
    canopyMode();
    break;

  case MODE_AIRPLANE:
    airplaneMode();
    break;

  case MODE_GROUND:
    groundMode();
    break;

  case MODE_MENU:
    menuMode();
    break;

  case MODE_UPDATE:
    updateMode();
    break;
  }
}