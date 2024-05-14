#ifndef WirelessPalaSensor_h
#define WirelessPalaSensor_h

#include "Main.h"
#include "base/Utils.h"
#include "base/MQTTMan.h"
#include "base/Application.h"

const char appDataPredefPassword[] PROGMEM = "ewcXoCt4HHjZUvY1";

#include "data/status1.html.gz.h"
#include "data/config1.html.gz.h"

#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <math.h>
#include <Ticker.h>
#include "SingleDS18B20.h"
#include "McpDigitalPot.h"

class WebPalaSensor : public Application
{
private:
  // -------------------- DigiPots Classes--------------------
  typedef struct
  {
    float rWTotal = 0;
    double steinhartHartCoeffs[3] = {0, 0, 0};
    float rBW5KStep = 0;
    float rBW50KStep = 0;
    byte dp50kStepSize = 0;
    byte dp5kOffset = 0;
  } DigiPotsNTC;

  // -------------------- HomeAutomation Classes --------------------

#define HA_HTTP_JEEDOM 0
#define HA_HTTP_FIBARO 1

  typedef struct
  {
    byte type = HA_HTTP_JEEDOM;
    char hostname[64 + 1] = {0};
    bool tls = false;
    byte fingerPrint[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int temperatureId = 0;

    struct
    {
      char apiKey[48 + 1] = {0};
    } jeedom;

    struct
    {
      char username[64 + 1] = {0};
      char password[64 + 1] = {0};
    } fibaro;

    uint32_t cboxIp = 0;
  } HTTP;

  typedef struct
  {
    char hostname[64 + 1] = {0};
    uint32_t port = 0;
    char username[128 + 1] = {0};
    char password[150 + 1] = {0};
    char baseTopic[64 + 1] = {0};

    char temperatureTopic[64 + 1] = {0};

    char cboxT1Topic[64 + 1] = {0};
  } MQTT;

#define HA_PROTO_DISABLED 0
#define HA_PROTO_HTTP 1
#define HA_PROTO_MQTT 2

#define CBOX_PROTO_DISABLED 0
#define CBOX_PROTO_HTTP 1
#define CBOX_PROTO_MQTT 2

  typedef struct
  {
    byte maxFailedRequest = 0; // number of failed requests to HA before failover to local temperature sensor

    byte protocol = HA_PROTO_DISABLED;
    byte cboxProtocol = CBOX_PROTO_DISABLED;

    HTTP http;
    MQTT mqtt;
  } HomeAutomation;

  // --------------------

  uint8_t _refreshPeriod;
  DigiPotsNTC _digipotsNTC;
  HomeAutomation _ha;

  SingleDS18B20 _ds18b20;
  McpDigitalPot _mcp4151_5k;
  McpDigitalPot _mcp4151_50k;

  bool _needTick = false;
  Ticker _refreshTicker;
  byte _skipTick = 0;
  // Used in TimerTick for logic and calculation
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

  float _lastMqttHATemperature = 0.0;
  unsigned long _lastMqttHATemperatureMillis = 0;
  float _lastMqttStoveTemperature = 0.0;
  unsigned long _lastMqttStoveTemperatureMillis = 0;

  WiFiClient _wifiClient;

  MQTTMan _mqttMan;

  void setDualDigiPot(float temperature);
  void setDualDigiPot(int resistance);
  void setDualDigiPot(unsigned int dp50kPosition, unsigned int dp5kPosition);
  void timerTick();
  void mqttConnectedCallback(MQTTMan *mqttMan, bool firstConnection);
  void mqttCallback(char *topic, uint8_t *payload, unsigned int length);

  void setConfigDefaultValues();
  void parseConfigJSON(JsonDocument &doc);
  bool parseConfigWebRequest(ESP8266WebServer &server);
  String generateConfigJSON(bool forSaveFile);
  String generateStatusJSON();
  bool appInit(bool reInit);
  const PROGMEM char *getHTMLContent(WebPageForPlaceHolder wp);
  size_t getHTMLContentSize(WebPageForPlaceHolder wp);
  void appInitWebServer(ESP8266WebServer &server, bool &shouldReboot, bool &pauseApplication);
  void appRun();

public:
  WebPalaSensor(char appId, String fileName);
};

#endif
