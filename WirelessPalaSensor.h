#ifndef WirelessPalaSensor_h
#define WirelessPalaSensor_h

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPI.h>
#include <math.h>

#include "Main.h"
#include "src\Utils.h"

#include "SingleDS18B20.h"
#include "SimpleTimer.h"
#include "McpDigitalPot.h"

const char appDataPredefPassword[] PROGMEM = "ewcXoCt4HHjZUvY1";

//Structure of Application Data 1
class AppData1 {

  public:

    typedef struct {
      float rWTotal = 240.0; //TODO
      double steinhartHartCoeffs[3] = {0, 0, 0};
      float rBW5KStep = 19.0; //TODO
      float rBW50KStep = 190.0; //TODO
      byte dp50kStepSize = 1; //TODO
      byte dp5kOffset = 10; //TODO
    } DigiPotsNTC;
    DigiPotsNTC digipotsNTC;

    typedef struct {
      char apiKey[48 + 1] = {0};
    } Jeedom;

    typedef struct {
      char username[64 + 1] = {0};
      char password[64 + 1] = {0};
    } Fibaro;

    typedef struct {
      byte enabled = 0; //0 : no HA; 1 : Jeedom; 2 : Fibaro
      bool tls = false;
      byte fingerPrint[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
      char hostname[64 + 1] = {0};
      int temperatureId = 0;
      Jeedom jeedom;
      Fibaro fibaro;
    } HomeAutomation;
    HomeAutomation HA;

    typedef struct {
      bool enabled = false;
      byte ip[4] = {0, 0, 0, 0};
    } ConnectionBox;
    ConnectionBox connectionBox;

    void SetDefaultValues() {

      digipotsNTC.rWTotal = 240.0;
      digipotsNTC.steinhartHartCoeffs[0] = 0.001067860568;
      digipotsNTC.steinhartHartCoeffs[1] = 0.0002269969431;
      digipotsNTC.steinhartHartCoeffs[2] = 0.0000002641627999;
      digipotsNTC.rBW5KStep = 19.0; //TODO
      digipotsNTC.rBW50KStep = 190.0;
      digipotsNTC.dp50kStepSize = 1;
      digipotsNTC.dp5kOffset = 10; //TODO

      HA.enabled = 0;
      HA.tls = true;
      memset(HA.fingerPrint, 0, 20);
      HA.hostname[0] = 0;
      HA.temperatureId = 0;
      HA.jeedom.apiKey[0] = 0;
      HA.fibaro.username[0] = 0;
      HA.fibaro.password[0] = 0;

      connectionBox.enabled = false;
      memset(connectionBox.ip, 0, 4);
    }

    String GetJSON();
    bool SetFromParameters(AsyncWebServerRequest* request, AppData1 &tempAppData);
};


class WebPalaSensor {

  private:
    AppData1* _appData1;

    McpDigitalPot _mcp4151_5k;
    McpDigitalPot _mcp4151_50k;
    SingleDS18B20 _ds18b20;

    SimpleTimer _refreshTimer;
    byte _skipTick = 0;
    //Used in TimerTick for logic and calculation
    int _homeAutomationRequestResult = 0;
    float _homeAutomationTemperature = 0.0;
    int _homeAutomationFailedCount = 0;
    int _stoveRequestResult = 0;
    float _stoveTemperature = 0.0;
    int _stoveRequestFailedCount = 0;
    float _owTemperature = 0.0;
    bool _homeAutomationTemperatureUsed = false;
    float _stoveDelta = 0.0;
    float _pushedTemperature = 0.0;

    void SetDualDigiPot(float temperature);
    void SetDualDigiPot(int resistance);
    void SetDualDigiPot(int dp50kPosition, int dp5kPosition);
    void TimerTick();

    String GetStatus();

  public:
    WebPalaSensor();
    void Init(AppData1 &appData1);
    void InitWebServer(AsyncWebServer &server);
    void Run();
};

#endif
