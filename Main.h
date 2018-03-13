#ifndef Main_h
#define Main_h

#include "data\status.html.gz.h"
#include "data\config.html.gz.h"
#include "data\fw.html.gz.h"
#include "data\discover.html.gz.h"

//Specific
#include "data\calib.html.gz.h"

//DomoChip Informations
//Project done for ESP-12
//------------Compile for 1M 64K SPIFFS------------
//Configuration Web Pages :
//http://IP/
//http://IP/config
//http://IP/fw
//http://IP/calib

//include Application header file
#include "WirelessPalaSensor.h"

#define APPLICATION_NAME "DomoChip Wireless Palazzetti Sensor"
#define APPLICATION_CLASS WebPalaSensor
#define APPLICATION_VAR webPalaSensor

#define VERSION_NUMBER "3.2.2"

#define MODEL "WPalaSensor"

#define DEFAULT_AP_SSID "WirelessPala"
#define DEFAULT_AP_PSK "PasswordPala"

//Time between request to home automation
#define REFRESH_PERIOD 30000

//Pin 12, 13 and 14 are used by DigiPot Bus
//Choose Pins used for DigiPot Select
#define MCP4151_5k_SSPIN 4
#define MCP4151_50k_SSPIN 5

//Choose Pin for 1Wire DS18B20 bus
#define ONEWIRE_BUS_PIN 2

//Enable developper mode (fwdev webpage and SPIFFS is used)
#define DEVELOPPER_MODE 0

//Choose Serial Speed
#define SERIAL_SPEED 115200

//Choose Pin used to boot in Rescue Mode
#define RESCUE_BTN_PIN 16

//Status LED
//#define STATUS_LED_SETUP pinMode(XX, OUTPUT);pinMode(XX, OUTPUT);
//#define STATUS_LED_OFF digitalWrite(XX, HIGH);digitalWrite(XX, HIGH);
//#define STATUS_LED_ERROR digitalWrite(XX, HIGH);digitalWrite(XX, HIGH);
//#define STATUS_LED_WARNING digitalWrite(XX, HIGH);digitalWrite(XX, HIGH);
//#define STATUS_LED_GOOD digitalWrite(XX, HIGH);digitalWrite(XX, HIGH);

//construct Version text
#if DEVELOPPER_MODE
#define VERSION VERSION_NUMBER "-DEV"
#else
#define VERSION VERSION_NUMBER
#endif

#endif


