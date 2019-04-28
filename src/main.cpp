#include <Arduino.h>

#include "freefallMode.h"
#include "canopyMode.h"
#include "airplaneMode.h"
#include "groundMode.h"
#include "menuMode.h"
#include "updateMode.h"

#include <JC_Button.h> // https://github.com/JChristensen/JC_Button
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BMP280.h"
#include <RtcDS3231.h>
#include <U8g2lib.h>
#include <SPI.h>

bool debug = false;
byte mode;

#define PIN_BUTTON_UP 15
#define PIN_BUTTON_DOWN 16
#define PIN_BUTTON_ENTER 19

#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22

#define PIN_DISPLAY_CLOCK 18
#define PIN_DISPLAY_DATA 23
#define PIN_DISPLAY_CS 5
#define PIN_DISPLAY_DC 14
#define PIN_DISPLAY_RESET 12

int counter = 4000;
int counter_old = 0;
const unsigned long LONG_PRESS = 1000;
bool longPress = false;

Adafruit_BMP280 pressureSensor1;
Adafruit_BMP280 pressureSensor2;

U8G2_SSD1309_128X64_NONAME2_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/PIN_DISPLAY_CLOCK, /* data=*/PIN_DISPLAY_DATA, /* cs=*/PIN_DISPLAY_CS, /* dc=*/PIN_DISPLAY_DC, /* reset=*/PIN_DISPLAY_RESET);

RtcDS3231<TwoWire> rtc(Wire);

Button buttonUp(PIN_BUTTON_UP);
Button buttonDown(PIN_BUTTON_DOWN);
Button buttonEnter(PIN_BUTTON_ENTER);

void setup()
{

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Serial.begin(115200);

  buttonUp.begin();
  buttonDown.begin();
  buttonEnter.begin();

  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFlipMode(1);

  mode = 3;
}

void loop()
{

  buttonUp.read();
  buttonDown.read();
  buttonEnter.read();

  switch (mode)
  {
  case 0: // freefall
    freefallMode();
    break;

  case 1: // canopy
    canopyMode();
    break;

  case 2: // airplane
    airplaneMode();
    break;

  case 3: // ground
    groundMode();
    break;

  case 4: // menu
    menuMode();
    break;
  
  case 5: // update
    updateMode();
    break;
  }
  
}