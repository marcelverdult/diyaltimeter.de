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

// original settings
// #define PIN_BUTTON_UP 25
// #define PIN_BUTTON_DOWN 33
// #define PIN_BUTTON_ENTER 32

// test setup
#define PIN_BUTTON_UP 32
#define PIN_BUTTON_DOWN 33
#define PIN_BUTTON_ENTER 34

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

// original
//U8G2_SSD1309_128X64_NONAME2_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/PIN_DISPLAY_CLOCK, /* data=*/PIN_DISPLAY_DATA, /* cs=*/PIN_DISPLAY_CS, /* dc=*/PIN_DISPLAY_DC, /* reset=*/PIN_DISPLAY_RESET);

U8G2_SSD1309_128X64_NONAME2_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/18, /* data=*/19, /* cs=*/5, /* dc=*/23, /* reset=*/27);

RtcDS3231<TwoWire> rtc(Wire);

Button buttonUp(PIN_BUTTON_UP);
Button buttonDown(PIN_BUTTON_DOWN);
Button buttonEnter(PIN_BUTTON_ENTER);

// define constants
const float softwareVersion = 0.01;
const String otaUrl = "https://raw.githubusercontent.com/marcelverdult/diyaltimeter.de/master/ota/";

const unsigned long LONG_PRESS = 1000;   // what counts as long press
const unsigned long sleepTime = 30;      // after what time to sleep
const unsigned long sleepForTime = 30;   // sleep for how long
const int batteryCheckInterval = 300000; // in ms = every 5 min if not in freefall/canopy mode
const int timeCheckInterval = 1000;
const int displayUpdateInterval = 5000;

// define global variables
bool debug = true; // debug mode? enables Serial Messages
bool demo = false; // demo mode to disable actual altitude check
bool feet = true;
float feetFactor = 0.3048;
byte mode = 3; // current work mode
unsigned long currentMillis = 0;

int currentAltitude;
int currentClimbRate;

char currentDateTime[20];
char currentTime[6];
char currentDate[11];

byte batteryLevel;

unsigned long lastDisplayUpdate = 0;
unsigned long lastAction = 0;

// variables to keep during sleep
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
  // if (mode > 2)
  // {
  buttonEnter.read();
  buttonUp.read();
  buttonDown.read();
  // }
};

/* -------------------------------------------------------------------------------------------------------- */

void getClimbRate()
{
  static int _climbRate = 0;
  static unsigned long _lastAltitudeUpdate = currentMillis;
  static int _last_Altitude;
  static unsigned long _timeDiff = 1;
  static int _altitudeDiff = 0;

  if (_lastAltitudeUpdate == 0 || _lastAltitudeUpdate + 1000 < currentMillis)
  {
    _timeDiff = (currentMillis - _lastAltitudeUpdate) / 1000;
    if(_timeDiff == 0)
    {
      _timeDiff = 1;
    }
    _altitudeDiff = currentAltitude - _last_Altitude;
    _climbRate = _altitudeDiff / _timeDiff;
    _lastAltitudeUpdate = currentMillis;
    _last_Altitude = currentAltitude;
  }
  if (_climbRate == 0)
  {
    currentClimbRate = 1;
  }
  else
  {
    currentClimbRate = _climbRate;
  }
}

/* -------------------------------------------------------------------------------------------------------- */

// Altitude check - filter - smoothen

void checkAltitude()
{
  static unsigned long lastAltiCheck = 0;
  int altitude1;
  int altitude2;

  if (lastAltiCheck + 200 < currentMillis || lastAltiCheck == 0)
  {
    altitude1 = pressureSensor1.readAltitude(defaultPressure1);
    altitude2 = pressureSensor2.readAltitude(defaultPressure2);
    currentAltitude = (altitude1 + altitude2) / 2;
    currentAltitude = currentAltitude / feetFactor;
    lastAltiCheck = currentMillis;
  }
  getClimbRate();
};

/* -------------------------------------------------------------------------------------------------------- */

void getTemperature()
{
  // TODO
}

/* -------------------------------------------------------------------------------------------------------- */

void getTime()
{
  static unsigned long lastTimeCheck = 0;
  RtcDateTime now;

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
  static unsigned long _lastBatteryCheck = 0;
  int vbat;
  float voltage; // battery voltage

  if (mode > 2 || _lastBatteryCheck == 0)
  {
    if (_lastBatteryCheck + batteryCheckInterval < currentMillis || _lastBatteryCheck == 0)
    {
      vbat = analogRead(35);
      voltage = 7.445 * vbat / 4096.0;
      debugMessage("Battery voltage");
      debugMessage((String)voltage);
      _lastBatteryCheck = currentMillis;

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
  lastAction = currentMillis;
  debugMessage("switching to mode:" + (String)newMode);
  mode = newMode;
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

/* -------------------------------------------------------------------------------------------------------- */

// check if altitude changed during sleep
// yes -> airplaneMode
// no -> sleep again
void checkAltitudeAfterWakeup()
{

  checkAltitude();
  debugMessage("check alti after wakeup");

  u8g2.clearBuffer();
  u8g2.setFontDirection(0);
  u8g2.setFont(u8g2_font_courB10_tf);
  u8g2.setCursor(0, 10);
  u8g2.print("Wakeup check!");
  u8g2.setCursor(0, 22);
  u8g2.print("Altitude: ");
  u8g2.print(currentAltitude);
  u8g2.setCursor(0, 34);
  if (currentAltitude > 50)
  {
    u8g2.print("-> PlaneMode");
  }
  else
  {
    u8g2.print("-> Sleep");
  }

  u8g2.sendBuffer();
  delay(1000);

  debugMessage("current altitude");
  debugMessage((String)currentAltitude);

  if (currentAltitude > 50)
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
  static unsigned long _groundTime = 0;

  if ((feet && currentAltitude > 150) || (!feet && currentAltitude > 50))
  {
    _groundTime = 0;
    changeModeTo(MODE_AIRPLANE);
  }

  if (_groundTime == 0)
  {
    _groundTime = millis();
  }
  // 30 seconds without action passed?
  else if (_groundTime + (sleepTime * mS_TO_S_FACTOR) < currentMillis)
  {
    debugMessage("going to sleep...");
    u8g2.setPowerSave(1);
    delay(100);
    esp_deep_sleep_start();
  }
  // check Enter Button to enter Menu
  if (buttonUp.pressedFor(1000))
  {
    // _groundTime = 0;
    changeModeTo(MODE_MENU); // change Mode to menuMode
  }

  // display
  if (lastDisplayUpdate + displayUpdateInterval < currentMillis || lastDisplayUpdate == 0)
  {
    u8g2.clearBuffer();
    u8g2.setFontDirection(0);
    u8g2.setFont(u8g2_font_courR08_tf);
    u8g2.setCursor(0, 8);
    u8g2.print("8=========D");
    u8g2.drawHLine(0, 10, 128);

    displayBatteryLevel();

    u8g2.setFontDirection(0);
    u8g2.setFont(u8g2_font_courB24_tn);
    u8g2.setCursor(0, 35);
    u8g2.print(currentTime);
    u8g2.setFont(u8g2_font_courB10_tn);
    u8g2.setCursor(0, 47);
    u8g2.print(currentDate);

    u8g2.setFont(u8g2_font_courR08_tf);
    u8g2.setCursor(20, 60);
    if (feet)
    {
      u8g2.print("feet");
    }
    else
    {
      u8g2.print("meter");
    }
    int temp = roundf(pressureSensor1.readTemperature());
    u8g2.setCursor(70, 60);
    u8g2.print(defaultPressure1);
    u8g2.print("Pa");

    u8g2.sendBuffer();
    lastDisplayUpdate = currentMillis;
  }
};

/* -------------------------------------------------------------------------------------------------------- */

void airplaneMode()
{
  static int _climbRate = 0;
  static int _timeToAltitude = 0;
  static unsigned long _lastDisplayUpdate = currentMillis;

  _climbRate = currentClimbRate;

  if (feet)
  {
    _timeToAltitude = (13500 - currentAltitude) / _climbRate;
  }
  else
  {
    _timeToAltitude = (4000 - currentAltitude) / _climbRate;
  }

  if ((feet && _climbRate < -15) || (!feet && _climbRate < -5))
  {
    changeModeTo(MODE_FREEFALL);
  }

  if (_lastDisplayUpdate + 1000 < currentMillis || _lastDisplayUpdate == 0)
  {
    u8g2.clearBuffer();
    u8g2.setFontDirection(0);
    u8g2.setFont(u8g2_font_courR08_tf);
    u8g2.setCursor(0, 8);
    u8g2.print(currentDate);
    u8g2.print(" ");
    u8g2.print(currentTime);
    u8g2.drawHLine(0, 10, 128);

    displayBatteryLevel();

    u8g2.setFontDirection(0);
    u8g2.setFont(u8g2_font_courB24_tr);
    u8g2.setCursor(0, 35);
    u8g2.print(currentAltitude);
    if (feet)
    {
      u8g2.print("ft");
    }
    else
    {
      u8g2.print("m");
    }

    u8g2.setFont(u8g2_font_courR08_tr);
    if (_timeToAltitude > 0)
    {
      u8g2.setCursor(0, 47);
      u8g2.print("4km in ");
      u8g2.print((int)_timeToAltitude / 60);
      u8g2.print("min ");
      u8g2.print((int)_timeToAltitude % 60);
      u8g2.print("sec");
    }
    u8g2.setCursor(0, 60);
    u8g2.print("climb rate: ");
    u8g2.print((int)_climbRate * 60);
    if (feet)
    {
      u8g2.print("ft/min");
    }
    else
    {
      u8g2.print("m/min");
    }

    u8g2.sendBuffer();
    _lastDisplayUpdate = currentMillis;
  }
};

/* -------------------------------------------------------------------------------------------------------- */

void freefallMode()
{
  static unsigned long _lastDisplayUpdate = 0;

  if (_lastDisplayUpdate + 500 < currentMillis || _lastDisplayUpdate == 0)
  {
    u8g2.clearBuffer();
    u8g2.setFontDirection(0);
    u8g2.setFont(u8g2_font_7Segments_26x42_mn);
    u8g2.setCursor(0, 53);
    u8g2.print(currentAltitude - currentAltitude % 50);
    u8g2.sendBuffer();
    _lastDisplayUpdate = currentMillis;
  }

  if((feet && currentClimbRate < 60) || (!feet && currentClimbRate < 20))
  {
    changeModeTo(MODE_CANOPY);
  }
}
/* -------------------------------------------------------------------------------------------------------- */

void canopyMode()
{
  static unsigned long _lastDisplayUpdate = 0;

  if (_lastDisplayUpdate + 500 < currentMillis || _lastDisplayUpdate == 0)
  {
    u8g2.clearBuffer();

    u8g2.setFontDirection(0);
    u8g2.setFont(u8g2_font_7Segments_26x42_mn);
    u8g2.setCursor(0, 42);
    u8g2.print(currentAltitude);

    u8g2.setFont(u8g2_font_courR08_tf);
    u8g2.setCursor(0, 60);
    u8g2.print((String)currentClimbRate);
    if (feet)
    {
      u8g2.print("ft/s");
    }
    else
    {
      u8g2.print("m/s");
    }

    u8g2.sendBuffer();

    _lastDisplayUpdate = currentMillis;
  }

  if ((feet && currentAltitude < 5) || (!feet && currentAltitude < 15))
  {
    debugMessage("canopy ended -> groundMode");
    changeModeTo(MODE_GROUND);
  }
};

/* -------------------------------------------------------------------------------------------------------- */

void menuMode()
{
  if (!feet)
  {
    feetFactor = 0.3048;
    feet = true;
  }
  else
  {
    feetFactor = 1;
    feet = false;
  }
  u8g2.clearBuffer();

  u8g2.setFontDirection(0);
  u8g2.setFont(u8g2_font_courR08_tf);
  u8g2.setCursor(0, 15);
  u8g2.print("Switching");
  u8g2.setCursor(0, 30);
  if (feet)
  {
    u8g2.print("meter -> feet");
  }
  else
  {
    u8g2.print("feet -> meter");
  }

  u8g2.sendBuffer();
  delay(3000);
  changeModeTo(MODE_GROUND);
};

/* -------------------------------------------------------------------------------------------------------- */

void updateMode(){

};

/* -------------------------------------------------------------------------------------------------------- */

void setup()
{

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  buttonUp.begin();
  buttonDown.begin();
  buttonEnter.begin();
  if (debug)
  {
    Serial.begin(115200);
  }
  pressureSensor1.begin(0x76);
  pressureSensor2.begin(0x77);

  u8g2.begin();
  u8g2.enableUTF8Print();
  // original
  // u8g2.setFlipMode(1);
  u8g2.setFlipMode(0);

  esp_sleep_enable_timer_wakeup(sleepForTime * uS_TO_S_FACTOR);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_32, 0);

  if (defaultPressure1 == 0)
  {
    debugMessage("alter druck");
    debugMessage((String)defaultPressure1);
    defaultPressure1 = pressureSensor1.readPressure() / 100;
    defaultPressure2 = pressureSensor2.readPressure() / 100;
  }

  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER)
  {
    checkAltitudeAfterWakeup();
  }

  debugMessage("Startdruck:");

  debugMessage((String)defaultPressure1);
  debugMessage((String)defaultPressure2);
  debugMessage("Starthöhe:");
  checkAltitude();
  debugMessage((String)currentAltitude);
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