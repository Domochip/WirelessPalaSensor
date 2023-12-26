#ifndef Main_h
#define Main_h

#include <arduino.h>

// Specific
#include "data\calib.html.gz.h"

// DomoChip Informations
// Configuration Web Pages :
// http://IP/

#define APPLICATION1_HEADER "WirelessPalaSensor.h"
#define APPLICATION1_NAME "WPalaSensor"
#define APPLICATION1_DESC "DomoChip Wireless Palazzetti Sensor"
#define APPLICATION1_CLASS WebPalaSensor

#define VERSION_NUMBER "3.4.0"

#define DEFAULT_AP_SSID "WirelessPala"
#define DEFAULT_AP_PSK "PasswordPala"

// Enable status webpage EventSource
#define ENABLE_STATUS_EVTSRC 1
#define STATUS_EVTSRC_MAX_CLIENTS 2
#define ENABLE_STATUS_EVTSRC_KEEPALIVE 0

// Pin 12, 13 and 14 are used by DigiPot Bus
// Choose Pins used for DigiPot Select
#define MCP4151_5k_SSPIN D2
#define MCP4151_50k_SSPIN D1

// Choose Pin for 1Wire DS18B20 bus
#define ONEWIRE_BUS_PIN D4

// Enable developper mode (fwdev webpage and SPIFFS is used)
#define DEVELOPPER_MODE 0

// Log Serial Object
#define LOG_SERIAL Serial
// Choose Log Serial Speed
#define LOG_SERIAL_SPEED 115200

// Choose Pin used to boot in Rescue Mode
// #define RESCUE_BTN_PIN 16

// Status LED
// #define STATUS_LED_SETUP pinMode(XX, OUTPUT);pinMode(XX, OUTPUT);
// #define STATUS_LED_OFF digitalWrite(XX, HIGH);digitalWrite(XX, HIGH);
// #define STATUS_LED_ERROR digitalWrite(XX, HIGH);digitalWrite(XX, HIGH);
// #define STATUS_LED_WARNING digitalWrite(XX, HIGH);digitalWrite(XX, HIGH);
// #define STATUS_LED_GOOD digitalWrite(XX, HIGH);digitalWrite(XX, HIGH);

// construct Version text
#if DEVELOPPER_MODE
#define VERSION VERSION_NUMBER "-DEV"
#else
#define VERSION VERSION_NUMBER
#endif

#endif