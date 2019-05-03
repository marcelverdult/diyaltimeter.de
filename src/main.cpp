#include <Arduino.h>
#include <JC_Button.h> // https://github.com/JChristensen/JC_Button
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BMP280.h"
#include <RtcDS3231.h>
#include <U8g2lib.h>
#include <SPI.h>

#define MODE_FREEFALL 0
#define MODE_CANOPY 1
#define MODE_AIRPLANE 2
#define MODE_GROUND 3
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

#define PIN_BATTERY 35

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
const unsigned long LONG_PRESS = 1000;   // what counts as long press
const unsigned long sleepTime = 30;      // after what time to sleep
const unsigned long sleepForTime = 30;   // sleep for how long
const int batteryCheckInterval = 300000; // in ms = every 5 min if not in freefall/canopy mode
const int timeCheckInterval = 1000;
const int displayUpdateInterval = 5000;

// define variables
bool debug = true; // debug mode? enables Serial Messages
bool demo = false;
byte mode; // current work mode
unsigned long currentMillis = 0;
unsigned long groundTime = 0;
int altitude1;
int altitude2;
int currentAltitude;

RtcDateTime now;
char currentDateTime[20];
char currentTime[6];
char currentDate[11];

int vbat;
float voltage; // battery voltage
byte batteryLevel;

unsigned long lastAltiCheck = 0;
unsigned long lastBatteryCheck = 0;
unsigned long lastTimeCheck = 0;
unsigned long lastDisplayUpdate = 0;
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
  if (!demo)
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

void getTime()
{
  if (lastTimeCheck + timeCheckInterval < currentMillis || lastTimeCheck == 0)
  {
    now = rtc.GetDateTime();

    snprintf_P(currentDateTime,
               sizeof(currentDateTime),
               PSTR("%02u.%02u.%04u %02u:%02u:%02u"),
               now.Day(),
               now.Month(),
               now.Year(),
               now.Hour(),
               now.Minute(),
               now.Second());

    snprintf_P(currentDate,
               sizeof(currentDate),
               PSTR("%02u.%02u.%04u"),
               now.Day(),
               now.Month(),
               now.Year());

    snprintf_P(currentTime,
               sizeof(currentTime),
               PSTR("%02u:%02u"),
               now.Hour(),
               now.Minute());
    lastTimeCheck = currentMillis;
  }
}

/* -------------------------------------------------------------------------------------------------------- */

void checkBattery()
{
  if (mode > 2)
  {
    if (lastBatteryCheck + batteryCheckInterval < currentMillis || lastBatteryCheck == 0)
    {
      vbat = analogRead(35);
      voltage = 7.445 * vbat / 4096.0;
      debugMessage("Battery voltage");
      debugMessage((String)voltage);
      lastBatteryCheck = currentMillis;

      if (voltage > 4.0)
      {
        batteryLevel = 5;
      }
      else if (voltage > 3.9)
      {
        batteryLevel = 4;
      }
      else if (voltage > 3.8)
      {
        batteryLevel = 3;
      }
      else if (voltage > 3.7)
      {
        batteryLevel = 2;
      }
      else if (voltage > 3.6)
      {
        batteryLevel = 1;
      }
      else
      {
        batteryLevel = 0;
      }
    }
  }
}

/* -------------------------------------------------------------------------------------------------------- */

// change to newMode
void changeModeTo(byte newMode)
{
  lastAction = 0;
  debugMessage("switching to mode:" + (String)newMode);
  mode = newMode; // change to menuMode
};

/* -------------------------------------------------------------------------------------------------------- */

void displayBatteryLevel()
{
  // display battery level
  u8g2.setFontDirection(1);
  u8g2.setCursor(106, 0);
  u8g2.setFont(u8g2_font_battery19_tn);
  u8g2.print(batteryLevel);
}

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
    changeModeTo(MODE_AIRPLANE);
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
    changeModeTo(MODE_MENU); // change Mode to menuMode
  }

  if (lastDisplayUpdate + displayUpdateInterval < currentMillis || lastDisplayUpdate == 0)
  { // display
    u8g2.clearBuffer();
    u8g2.setFontDirection(0);
    u8g2.setFont(u8g2_font_courR08_tf);
    u8g2.setCursor(0, 8);
    u8g2.print("8==========D");
    u8g2.drawHLine(0, 10, 128);

    u8g2.setFont(u8g2_font_courB24_tn);
    u8g2.setCursor(0, 34);
    u8g2.print(currentTime);

    u8g2.setFont(u8g2_font_courB10_tn);
    u8g2.setCursor(0, 46);
    u8g2.print(currentDate);

    displayBatteryLevel();

    u8g2.sendBuffer();
    lastDisplayUpdate = currentMillis;
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
    changeModeTo(MODE_FREEFALL);
  }
};

/* -------------------------------------------------------------------------------------------------------- */

void freefallMode()
{
  if (lastAction == 0)
  {
    lastAction = currentMillis;
  }
  else if (lastAction + 20000 < currentMillis)
  {
    debugMessage("DEMO: freefall ended -> canopyMode");
    changeModeTo(MODE_CANOPY);
  }
};

/* -------------------------------------------------------------------------------------------------------- */

void canopyMode()
{
  if (lastAction == 0)
  {
    lastAction = currentMillis;
  }
  else if (lastAction + 20000 < currentMillis)
  {
    debugMessage("DEMO: landed! -> groundMode");
    changeModeTo(MODE_GROUND);
  }
};

/* -------------------------------------------------------------------------------------------------------- */

void menuMode()
{
  if (buttonEnter.pressedFor(2000))
  {
    changeModeTo(MODE_GROUND);
  }
};

/* -------------------------------------------------------------------------------------------------------- */

void updateMode(){

};

/* -------------------------------------------------------------------------------------------------------- */

void setup()
{
  if (debug)
  {
    Serial.begin(115200);
  }
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

    mode = MODE_GROUND;
  }
}

/* -------------------------------------------------------------------------------------------------------- */

void loop()
{
  currentMillis = millis();
  checkAltitude();
  readButtons();
  getTime();
  checkBattery();

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