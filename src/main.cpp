#include <Arduino.h>
#include <JC_Button.h> // https://github.com/JChristensen/JC_Button
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
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
#define PIN_DISPLAY_DC 12
#define PIN_DISPLAY_RESET 14

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
const float softwareVersion = 0.01;
const String otaUrl = "https://raw.githubusercontent.com/marcelverdult/diyaltimeter.de/master/ota/";

const unsigned long LONG_PRESS = 1000;   // what counts as long press
const unsigned long sleepTime = 30;      // after what time to sleep
const unsigned long sleepForTime = 10;   // sleep for how long
const int batteryCheckInterval = 300000; // in ms = every 5 min if not in freefall/canopy mode
const int timeCheckInterval = 1000;

// define global variables
bool debug = true; // debug mode? enables Serial Messages
bool demo = false; // demo mode to disable actual altitude check
bool buttonEnterActive = true;
byte mode = 3; // current work mode
byte lastMode = 254;
unsigned long currentMillis = 0;

int currentAltitude;
int currentAltitudeChangeRate;

char currentDateTime[20];
char currentTime[6];
char currentDate[11];

byte batteryLevel;

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
  if (mode > 2)
  {
    buttonEnter.read();
    buttonUp.read();
    buttonDown.read();
  }
};

/* -------------------------------------------------------------------------------------------------------- */

void checkAltitudeChangeRate()
{
  static int _changeRate[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  static unsigned long _lastAltitudeUpdate = currentMillis;
  static int _last_Altitude;
  static unsigned long _timeDiff = 1;
  static int _altitudeDiff = 0;
  static int _counter = 0;
  int _currentAltitudeChangeRate = 0;

  // if (_lastAltitudeUpdate == 0 || _lastAltitudeUpdate + 1000 < currentMillis)
  // {
  _timeDiff = (currentMillis - _lastAltitudeUpdate) / 1000;
  // avoid division by zero errors
  if (_timeDiff == 0)
  {
    _timeDiff = 1;
  }
  _altitudeDiff = currentAltitude - _last_Altitude;
  _changeRate[_counter] = _altitudeDiff / _timeDiff;
  _lastAltitudeUpdate = currentMillis;
  _last_Altitude = currentAltitude;
  if (_counter == 9)
  {
    _counter = 0;
  }
  else
  {
    _counter++;
  }

  for (int i = 0; i < 9; i++)
  {
    _currentAltitudeChangeRate = _currentAltitudeChangeRate + _changeRate[i];
  }

  currentAltitudeChangeRate = _currentAltitudeChangeRate / 10;
  // }

  // avoid division by zero errors
  if (currentAltitudeChangeRate == 0)
  {
    currentAltitudeChangeRate = 1;
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
    lastAltiCheck = currentMillis;
    checkAltitudeChangeRate();
  }
};

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
  lastMode = mode;
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
  debugMessage((String)defaultPressure1);
  debugMessage((String)defaultPressure2);
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
  static unsigned long _groundTime = currentMillis;
  static unsigned long _lastDisplayUpdate = 0;

  if (buttonEnter.isReleased() && !buttonEnterActive)
  {
    buttonEnterActive = true;
  }

  if (mode != lastMode)
  {
    _groundTime = currentMillis;
    lastMode = MODE_GROUND;
  }
  else
  {
    // check Enter Button to enter Menu
    if (buttonEnter.pressedFor(1000) && buttonEnterActive)
    {
      buttonEnterActive = false;
      changeModeTo(MODE_MENU); // change Mode to menuMode
    }
  }
  if (currentAltitude > 50)
  {
    changeModeTo(MODE_AIRPLANE);
  }

  // 30 seconds without action passed?
  else if (_groundTime + (sleepTime * mS_TO_S_FACTOR) < currentMillis)
  {
    debugMessage("going to sleep...");
    u8g2.setPowerSave(1);
    delay(100);
    esp_deep_sleep_start();
  }

  // display
  if (_lastDisplayUpdate + 5000 < currentMillis || _lastDisplayUpdate == 0)
  {
    u8g2.clearBuffer();
    u8g2.setFontDirection(0);
    u8g2.setFont(u8g2_font_courR08_tf);
    u8g2.setCursor(0, 8);
    u8g2.print("diyaltimeter.de");
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

    int temp = roundf(pressureSensor1.readTemperature());
    u8g2.setCursor(90, 60);
    u8g2.print(temp);
    u8g2.print("°C");

    u8g2.sendBuffer();
    _lastDisplayUpdate = currentMillis;
  }
};

/* -------------------------------------------------------------------------------------------------------- */

void airplaneMode()
{
  static int _timeToAltitude = 0;
  static unsigned long _lastDisplayUpdate = currentMillis;

  _timeToAltitude = (4000 - currentAltitude) / currentAltitudeChangeRate;

  if (currentAltitudeChangeRate < -15)
  {
    changeModeTo(MODE_FREEFALL);
  }

  // display
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
    u8g2.print("m");
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
    u8g2.print(currentAltitudeChangeRate);
    u8g2.print("m/s");

    u8g2.sendBuffer();
    _lastDisplayUpdate = currentMillis;
  }
};

/* -------------------------------------------------------------------------------------------------------- */

void freefallMode()
{
  static unsigned long _lastDisplayUpdate = 0;

  if (currentAltitudeChangeRate < 15)
  {
    changeModeTo(MODE_CANOPY);
  }

  // display
  if (_lastDisplayUpdate + 500 < currentMillis || _lastDisplayUpdate == 0)
  {
    u8g2.clearBuffer();
    u8g2.setFontDirection(0);
    u8g2.setFont(u8g2_font_logisoso62_tn);
    u8g2.setCursor(0, 64);
    u8g2.print(currentAltitude);
    u8g2.sendBuffer();
    _lastDisplayUpdate = currentMillis;
  }
}
/* -------------------------------------------------------------------------------------------------------- */

void canopyMode()
{
  static unsigned long _lastDisplayUpdate = 0;

  // display
  if (_lastDisplayUpdate + 500 < currentMillis || _lastDisplayUpdate == 0)
  {
    u8g2.clearBuffer();
    u8g2.setFontDirection(0);
    u8g2.setFont(u8g2_font_logisoso50_tn);
    u8g2.setCursor(0, 51);
    u8g2.print(currentAltitude);
    u8g2.setFont(u8g2_font_courR08_tf);
    u8g2.setCursor(0, 64);
    u8g2.print((String)currentAltitudeChangeRate);
    u8g2.print(" m/s");
    u8g2.sendBuffer();

    _lastDisplayUpdate = currentMillis;
  }

  if (currentAltitude < 4)
  {
    changeModeTo(MODE_GROUND);
  }
};

/* -------------------------------------------------------------------------------------------------------- */

typedef void (*function)();

void firstFunction()
{
  // whatever
  debugMessage("First function call");
}

void secondFunction()
{
  // whatever
  debugMessage("Second function call");
}

void menuMode()
{
  static byte _selectedItem = 0;
  const String _menuItems[] = {"Dropzones", "Planes", "Update", "Item 4", "Item 5"};
  const function arrayOfFunctions[] = {firstFunction, secondFunction};

  static bool _displayNeedUpdate = true;
  byte _numberOfItems = 5;

  if (buttonEnter.isReleased() && !buttonEnterActive)
  {
    buttonEnterActive = true;
  }

  if (lastMode != mode)
  {
    _displayNeedUpdate = true;
    lastMode = mode;
  }
  else
  {

    if (buttonDown.wasReleased() && _selectedItem < 4)
    {
      _selectedItem++;
      _displayNeedUpdate = true;
    }
    if (buttonUp.wasReleased() && _selectedItem > 0)
    {
      _selectedItem--;
      _displayNeedUpdate = true;
    }
    if (buttonEnter.wasPressed())
    {
      arrayOfFunctions[_selectedItem]();
      _displayNeedUpdate = true;
    }
    else if (buttonEnterActive && buttonEnter.pressedFor(2000))
    {
      buttonEnterActive = false;
      changeModeTo(MODE_GROUND);
    }
  }

  if (_displayNeedUpdate)
  {

    u8g2.clearBuffer();
    u8g2.setFontDirection(0);
    u8g2.setFont(u8g2_font_courR08_tf);
    u8g2.setCursor(0, 8);
    u8g2.print("Menu");
    u8g2.drawHLine(0, 10, 128);

    for (byte i = 0; i < _numberOfItems; i++)
    {
      if (i == 0)
      {
        u8g2.setCursor(0, 20);
      }
      else
      {
        u8g2.setCursor(0, 20 + (i * 10));
      }

      if (_selectedItem == i)
      {
        u8g2.print(">");
      }
      u8g2.print(_menuItems[i]);
    }

    u8g2.sendBuffer();
    _displayNeedUpdate = false;
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
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  pressureSensor1.begin(0x76);
  pressureSensor2.begin(0x77);
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFlipMode(0);

  esp_sleep_enable_timer_wakeup(sleepForTime * uS_TO_S_FACTOR);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_32, 0);
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER)
  {
    checkAltitudeAfterWakeup();
  }

  debugMessage("Startdruck:");
  defaultPressure1 = pressureSensor1.readPressure() / 100;
  defaultPressure2 = pressureSensor2.readPressure() / 100;
  debugMessage((String)defaultPressure1);
  debugMessage((String)defaultPressure2);
  debugMessage("Starthöhe:");
  checkAltitude();
  debugMessage((String)currentAltitude);

  buttonUp.begin();
  buttonDown.begin();
  buttonEnter.begin();
  debugMessage("Uhrzeit:");
  RtcDateTime now_test;
  now_test = rtc.GetDateTime();
  debugMessage((String)now_test);
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