#include "WirelessPalaSensor.h"

//-----------------------------------------------------------------------
// Steinhart–Hart reverse function
//-----------------------------------------------------------------------
void WebPalaSensor::SetDualDigiPot(float temperature)
{
  //convert temperature from Celsius to Kevin degrees
  float temperatureK = temperature + 273.15;

  //calculate and return resistance value based on provided temperature
  double x = (1 / digipotsNTC.steinhartHartCoeffs[2]) * (digipotsNTC.steinhartHartCoeffs[0] - (1 / temperatureK));
  double y = sqrt(pow(digipotsNTC.steinhartHartCoeffs[1] / (3 * digipotsNTC.steinhartHartCoeffs[2]), 3) + pow(x / 2, 2));
  SetDualDigiPot((int)(exp(pow(y - (x / 2), 1.0F / 3) - pow(y + (x / 2), 1.0F / 3))));
}
//-----------------------------------------------------------------------
// Set Dual DigiPot resistance (serial rBW)
//-----------------------------------------------------------------------
void WebPalaSensor::SetDualDigiPot(int resistance)
{
  float adjustedResistance = resistance - digipotsNTC.rWTotal - (digipotsNTC.rBW5KStep * digipotsNTC.dp5kOffset);

  //DigiPot positions calculation
  int digiPot50k_position = floor((adjustedResistance) / (digipotsNTC.rBW50KStep * digipotsNTC.dp50kStepSize)) * digipotsNTC.dp50kStepSize;
  int digiPot5k_position = round((adjustedResistance - (digiPot50k_position * digipotsNTC.rBW50KStep)) / digipotsNTC.rBW5KStep);
  SetDualDigiPot(digiPot50k_position, digiPot5k_position + digipotsNTC.dp5kOffset);
}

void WebPalaSensor::SetDualDigiPot(int dp50kPosition, int dp5kPosition)
{
  //Set DigiPot position
  if (_mcp4151_50k.getPosition(0) != dp50kPosition)
    _mcp4151_50k.setPosition(0, dp50kPosition);
  if (_mcp4151_5k.getPosition(0) != dp5kPosition)
    _mcp4151_5k.setPosition(0, dp5kPosition);
}

//-----------------------------------------------------------------------
// Main Timer Tick (aka this should be done every 30sec)
//-----------------------------------------------------------------------
void WebPalaSensor::TimerTick()
{
  if (_skipTick)
  {
    _skipTick--;
    return;
  }

  float temperatureToDisplay = 20.0;
  float previousTemperatureToDisplay;
  if (_homeAutomationTemperatureUsed)
    previousTemperatureToDisplay = _homeAutomationTemperature;
  else
    previousTemperatureToDisplay = _owTemperature;
  _stoveTemperature = 0.0;
  _homeAutomationTemperature = 0.0;
  _homeAutomationTemperatureUsed = false;

  //LOG
  Serial.println(F("TimerTick"));

  //read temperature from the local sensor
  _owTemperature = _ds18b20.ReadTemp();
  if (_owTemperature == 12.3456F)
    _owTemperature = 20.0; //if reading of local sensor failed so push 20°C
  else
  {
    //round it to tenth
    _owTemperature *= 10;
    _owTemperature = round(_owTemperature);
    _owTemperature /= 10;
  }

  char payload[60];
  WiFiClient *stream;

  //if ConnectionBox option enabled in config
  if (connectionBox.enabled)
  {
    HTTPClient http1;

    //try to get current stove temperature info ----------------------
    http1.begin(String(F("http://")) + IPAddress(connectionBox.ip).toString() + F("/sendmsg.php?cmd=GET%20TMPS"));
    //set timeOut
    http1.setTimeout(5000);
    //send request
    _stoveRequestResult = http1.GET();
    //if we get successfull HTTP answer
    if (_stoveRequestResult == 200)
    {

      stream = http1.getStreamPtr();

      //if we found TMP_ROOM_WATER in answer
      if (stream->find("\"TMP_ROOM_WATER\""))
      {
        //read until the comma into payload variable
        int nb = stream->readBytesUntil(',', payload, sizeof(payload) - 1);
        payload[nb] = 0; //end payload char[]
        //if we read some bytes
        if (nb)
        {
          //look for start position of TMP_ROOM_WATER value
          byte posTRW = 0;
          while ((payload[posTRW] == ' ' || payload[posTRW] == ':' || payload[posTRW] == '\t') && posTRW < nb)
            posTRW++;

          _stoveTemperature = atof(payload + posTRW); //convert
        }
        payload[0] = 0;
      }
    }
    http1.end();
  }

  //if Jeedom option enabled in config
  if (ha.enabled == 1)
  {
    HTTPClient http2;

    //try to get house automation sensor value -----------------
    String completeURI = String(F("http")) + (ha.tls ? F("s") : F("")) + F("://") + ha.hostname + F("/core/api/jeeApi.php?apikey=") + ha.jeedom.apiKey + F("&type=cmd&id=") + ha.temperatureId;
    if (!ha.tls)
      http2.begin(completeURI);
    else
    {
      char fpStr[41];
      http2.begin(completeURI, Utils::FingerPrintA2S(fpStr, ha.fingerPrint));
    }
    //set timeOut
    http2.setTimeout(5000);
    //send request
    _homeAutomationRequestResult = http2.GET();
    //if we get successfull HTTP answer
    if (_homeAutomationRequestResult == 200)
    {

      stream = http2.getStreamPtr();

      //get the answer line
      int nb = stream->readBytes(payload, (http2.getSize() > sizeof(payload) - 1) ? sizeof(payload) - 1 : http2.getSize());
      payload[nb] = 0;

      if (nb)
      {

        //convert
        _homeAutomationTemperature = atof(payload);
        //round it to tenth
        _homeAutomationTemperature *= 10;
        _homeAutomationTemperature = round(_homeAutomationTemperature);
        _homeAutomationTemperature /= 10;
      }
      payload[0] = 0;
    }
    http2.end();
  }

  //if Fibaro option enabled in config
  if (ha.enabled == 2)
  {
    HTTPClient http3;

    //try to get house automation sensor value -----------------
    String completeURI = String(F("http")) + (ha.tls ? F("s") : F("")) + F("://") + ha.hostname + F("/api/devices?id=") + ha.temperatureId;
    //String completeURI = String(F("http")) + (ha.tls ? F("s") : F("")) + F("://") + ha.hostname + F("/devices.json");
    if (!ha.tls)
      http3.begin(completeURI);
    else
    {
      char fpStr[41];
      http3.begin(completeURI, Utils::FingerPrintA2S(fpStr, ha.fingerPrint));
    }

    //Pass authentication if specified in configuration
    if (ha.fibaro.username[0])
      http3.setAuthorization(ha.fibaro.username, ha.fibaro.password);

    //set timeOut
    http3.setTimeout(5000);
    //send request
    _homeAutomationRequestResult = http3.GET();

    //if we get successfull HTTP answer
    if (_homeAutomationRequestResult == 200)
    {

      stream = http3.getStreamPtr();

      while (http3.connected() && stream->find("\"value\""))
      {

        //go to first next double quote (or return false if a comma appears first)
        if (stream->findUntil("\"", ","))
        {
          //read value (read until next doublequote)
          int nb = stream->readBytesUntil('"', payload, sizeof(payload) - 1);
          payload[nb] = 0;
          if (nb)
          {
            //convert
            _homeAutomationTemperature = atof(payload);
            //round it to tenth
            _homeAutomationTemperature *= 10;
            _homeAutomationTemperature = round(_homeAutomationTemperature);
            _homeAutomationTemperature /= 10;
          }
          payload[0] = 0;
        }
      }
    }
    http3.end();
  }

  //select temperature source
  if (ha.enabled)
  {
    //if we got an HA temperature
    if (_homeAutomationTemperature > 0.1)
    {
      _homeAutomationFailedCount = 0;
      _homeAutomationTemperatureUsed = true;
      temperatureToDisplay = _homeAutomationTemperature;
    }
    else
    {
      //else if failed count is good and previousTemperature is good too
      if (_homeAutomationFailedCount < 6 && previousTemperatureToDisplay > 0.1)
      {
        _homeAutomationFailedCount++;
        _homeAutomationTemperatureUsed = true;
        _homeAutomationTemperature = previousTemperatureToDisplay;
        temperatureToDisplay = previousTemperatureToDisplay;
      }
      //otherwise fail back to oneWire
      else
        temperatureToDisplay = _owTemperature;
    }
  }
  else
    temperatureToDisplay = _owTemperature; //HA not enable

  //if connectionBox is enabled
  if (connectionBox.enabled)
  {

    //if _stoveTemperature is correct so failed counter reset
    if (_stoveTemperature > 0.1)
      _stoveRequestFailedCount = 0;
    else if (_stoveRequestFailedCount < 6)
      _stoveRequestFailedCount++; //else increment failed counter

    //if failed counter reached 5 then reset calculated delta
    if (_stoveRequestFailedCount >= 5)
      _stoveDelta = 0;
    //if stoveTemp is ok and previousTemperatureToDisplay also so adjust delta
    if (_stoveTemperature > 0.1 && previousTemperatureToDisplay > 0.1)
      _stoveDelta += (previousTemperatureToDisplay - _stoveTemperature) / 2.5F;
  }

  //Set DigiPot position according to resistance calculated from temperature to display with delta
  SetDualDigiPot(temperatureToDisplay + _stoveDelta);

  _pushedTemperature = temperatureToDisplay + _stoveDelta;
}

//------------------------------------------
//Used to initialize configuration properties to default values
void WebPalaSensor::SetConfigDefaultValues()
{
  digipotsNTC.rWTotal = 240.0;
  digipotsNTC.steinhartHartCoeffs[0] = 0.001067860568;
  digipotsNTC.steinhartHartCoeffs[1] = 0.0002269969431;
  digipotsNTC.steinhartHartCoeffs[2] = 0.0000002641627999;
  digipotsNTC.rBW5KStep = 19.0; //TODO
  digipotsNTC.rBW50KStep = 190.0;
  digipotsNTC.dp50kStepSize = 1;
  digipotsNTC.dp5kOffset = 10; //TODO

  ha.enabled = 0;
  ha.tls = true;
  memset(ha.fingerPrint, 0, 20);
  ha.hostname[0] = 0;
  ha.temperatureId = 0;
  ha.jeedom.apiKey[0] = 0;
  ha.fibaro.username[0] = 0;
  ha.fibaro.password[0] = 0;

  connectionBox.enabled = false;
  connectionBox.ip = 0;
};
//------------------------------------------
//Parse JSON object into configuration properties
void WebPalaSensor::ParseConfigJSON(JsonObject &root)
{
  if (root[F("sha")].success())
    digipotsNTC.steinhartHartCoeffs[0] = root[F("sha")];
  if (root[F("shb")].success())
    digipotsNTC.steinhartHartCoeffs[1] = root[F("shb")];
  if (root[F("shc")].success())
    digipotsNTC.steinhartHartCoeffs[2] = root[F("shc")];

  if (root[F("hae")].success())
    ha.enabled = root[F("hae")];
  if (root[F("hatls")].success())
    ha.tls = root[F("hatls")];
  if (root[F("hah")].success())
    strlcpy(ha.hostname, root["hah"], sizeof(ha.hostname));
  if (root[F("hatid")].success())
    ha.temperatureId = root[F("hatid")];
  if (root["hafp"].success())
    Utils::FingerPrintS2A(ha.fingerPrint, root["hafp"]);

  if (root["ja"].success())
    strlcpy(ha.jeedom.apiKey, root["ja"], sizeof(ha.jeedom.apiKey));

  if (root["fu"].success())
    strlcpy(ha.fibaro.username, root["fu"], sizeof(ha.fibaro.username));
  if (root["fp"].success())
    strlcpy(ha.fibaro.password, root["fp"], sizeof(ha.fibaro.password));

  if (root[F("cbe")].success())
    connectionBox.enabled = root[F("cbe")];
  if (root[F("cbi")].success())
    connectionBox.ip = root[F("cbi")];
};
//------------------------------------------
//Parse HTTP POST parameters in request into configuration properties
bool WebPalaSensor::ParseConfigWebRequest(AsyncWebServerRequest *request)
{
  //Find Steinhart-Hart coeff then convert to double
  //AND handle scientific notation
  if (request->hasParam(F("sha"), true))
    digipotsNTC.steinhartHartCoeffs[0] = request->getParam(F("sha"), true)->value().toFloat();
  if (request->hasParam(F("shb"), true))
    digipotsNTC.steinhartHartCoeffs[1] = request->getParam(F("shb"), true)->value().toFloat();
  if (request->hasParam(F("shc"), true))
    digipotsNTC.steinhartHartCoeffs[2] = request->getParam(F("shc"), true)->value().toFloat();

  if (request->hasParam(F("hae"), true))
    ha.enabled = request->getParam(F("hae"), true)->value().toInt();
  //if an home Automation system is enabled then get common param
  if (ha.enabled)
  {
    if (request->hasParam(F("hatls"), true))
      ha.tls = (request->getParam(F("hatls"), true)->value() == F("on"));
    else
      ha.tls = false;
    if (request->hasParam(F("hah"), true) && request->getParam(F("hah"), true)->value().length() < sizeof(ha.hostname))
      strcpy(ha.hostname, request->getParam(F("hah"), true)->value().c_str());
    if (request->hasParam(F("hatid"), true))
      ha.temperatureId = request->getParam(F("hatid"), true)->value().toInt();
    if (request->hasParam(F("hafp"), true))
      Utils::FingerPrintS2A(ha.fingerPrint, request->getParam(F("hafp"), true)->value().c_str());
  }

  //Now get specific param
  switch (ha.enabled)
  {
  case 1: //Jeedom
    char tempApiKey[48 + 1];
    //put apiKey into temporary one for predefpassword
    if (request->hasParam(F("ja"), true) && request->getParam(F("ja"), true)->value().length() < sizeof(tempApiKey))
      strcpy(tempApiKey, request->getParam(F("ja"), true)->value().c_str());
    //check for previous apiKey (there is a predefined special password that mean to keep already saved one)
    if (strcmp_P(tempApiKey, appDataPredefPassword))
      strcpy(ha.jeedom.apiKey, tempApiKey);
    if (!ha.hostname[0] || !ha.jeedom.apiKey[0])
      ha.enabled = 0;
    break;
  case 2: //Fibaro
    char tempFibaroPassword[64 + 1];
    if (request->hasParam(F("fu"), true) && request->getParam(F("fu"), true)->value().length() < sizeof(ha.fibaro.username))
      strcpy(ha.fibaro.username, request->getParam(F("fu"), true)->value().c_str());
    if (request->hasParam(F("fp"), true) && request->getParam(F("fp"), true)->value().length() < sizeof(ha.fibaro.password))
      strcpy(tempFibaroPassword, request->getParam(F("fp"), true)->value().c_str());
    //check for previous fibaro password (there is a predefined special password that mean to keep already saved one)
    if (strcmp_P(tempFibaroPassword, appDataPredefPassword))
      strcpy(ha.fibaro.password, tempFibaroPassword);
    if (!ha.hostname[0])
      ha.enabled = 0;
    break;
  }

  if (request->hasParam(F("cbe"), true))
    connectionBox.enabled = (request->getParam(F("cbe"), true)->value() == F("on"));
  else
    connectionBox.enabled = false;

  if (request->hasParam(F("cbi"), true))
  {
    IPAddress ipParser;
    if (ipParser.fromString(request->getParam(F("cbi"), true)->value()))
      connectionBox.ip = static_cast<uint32_t>(ipParser);
    else
      connectionBox.ip = 0;
  }

  return true;
};
//------------------------------------------
//Generate JSON from configuration properties
String WebPalaSensor::GenerateConfigJSON(bool forSaveFile = false)
{
  String gc('{');

  char fpStr[60];

  gc = gc + F("\"sha\":") + String(digipotsNTC.steinhartHartCoeffs[0], 16);
  gc = gc + F(",\"shb\":") + String(digipotsNTC.steinhartHartCoeffs[1], 16);
  gc = gc + F(",\"shc\":") + String(digipotsNTC.steinhartHartCoeffs[2], 16);

  gc = gc + F(",\"hae\":") + ha.enabled;
  gc = gc + F(",\"hatls\":") + ha.tls;
  gc = gc + F(",\"hah\":\"") + ha.hostname + '"';
  gc = gc + F(",\"hatid\":") + ha.temperatureId;
  gc = gc + F(",\"hafp\":\"") + Utils::FingerPrintA2S(fpStr, ha.fingerPrint, ':') + '"';
  if (forSaveFile)
  {
    if (ha.enabled == 1)
      gc = gc + F(",\"ja\":\"") + ha.jeedom.apiKey + '"';
  }
  else
    //Jeedom apiKey: there is a predefined special password (mean to keep already saved one)
    gc = gc + F(",\"ja\":\"") + (__FlashStringHelper *)appDataPredefPassword + '"';

  gc = gc + F(",\"fu\":\"") + ha.fibaro.username + '"';
  if (forSaveFile)
  {
    if (ha.enabled == 2)
      gc = gc + F(",\"fp\":\"") + ha.fibaro.password + '"';
  }
  else
    //Fibaro password : there is a predefined special password (mean to keep already saved one)
    gc = gc + F(",\"fp\":\"") + (__FlashStringHelper *)appDataPredefPassword + '"';

  gc = gc + F(",\"cbe\":") + connectionBox.enabled;
  if (forSaveFile)
    gc = gc + F(",\"cbi\":") + connectionBox.ip;
  else
    gc = gc + F(",\"cbi\":\"") + IPAddress(connectionBox.ip).toString() + '"';

  gc += '}';

  return gc;
};
//------------------------------------------
//Generate JSON of application status
String WebPalaSensor::GenerateStatusJSON()
{
  String gs('{');

  //Home Automation infos
  if (ha.enabled)
  {
    gs = gs + F("\"lhar\":") + _homeAutomationRequestResult;
    gs = gs + F(",\"lhat\":") + _homeAutomationTemperature;
    gs = gs + F(",\"hafc\":") + _homeAutomationFailedCount;
  }
  else
    gs = gs + F("\"lhar\":\"NA\",\"lhat\":\"NA\",\"hafc\":\"NA\"");

  //stove(ConnectionBox) infos
  if (connectionBox.enabled)
  {
    gs = gs + F(",\"lcr\":") + _stoveRequestResult;
    gs = gs + F(",\"lct\":") + _stoveTemperature;
    gs = gs + F(",\"cfc\":") + _stoveRequestFailedCount;
  }
  else
    gs = gs + F(",\"lcr\":\"NA\",\"lct\":\"NA\",\"cfc\":\"NA\"");

  gs = gs + F(",\"low\":") + _owTemperature;
  gs = gs + F(",\"owu\":\"") + (_homeAutomationTemperatureUsed ? F("Not ") : F("")) + '"';
  gs = gs + F(",\"pt\":") + _pushedTemperature;
  gs = gs + F(",\"p50\":") + _mcp4151_50k.getPosition(0);
  gs = gs + F(",\"p5\":") + _mcp4151_5k.getPosition(0);

  gs += '}';

  return gs;
};
//------------------------------------------
//code to execute during initialization and reinitialization of the app
bool WebPalaSensor::AppInit(bool reInit)
{
  if (reInit)
  {
    //reset run variables to initial values
    _homeAutomationRequestResult = 0;
    _homeAutomationTemperature = 0.0;
    _homeAutomationFailedCount = 0;
    _stoveRequestResult = 0;
    _stoveTemperature = 0.0;
    _stoveRequestFailedCount = 0;
    _owTemperature = 0.0;
    _homeAutomationTemperatureUsed = false;
    _stoveDelta = 0.0;
    _pushedTemperature = 0.0;
  }

  //first call
  TimerTick();

  //then next will be done by SimpleTimer object
  if (!reInit)
    _refreshTimer.setInterval(REFRESH_PERIOD, [this]() {
      this->TimerTick();
    });

  return _ds18b20.GetReady();
};
//------------------------------------------
//code to register web request answer to the web server
void WebPalaSensor::AppInitWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication)
{
  server.on("/calib", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), (const uint8_t *)calibhtmlgz, sizeof(calibhtmlgz));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  //GetDigiPot
  server.on("/gdp", HTTP_GET, [this](AsyncWebServerRequest *request) {

    String dpJSON('{');
    dpJSON = dpJSON + F("\"r\":") + (_mcp4151_50k.getPosition(0) * digipotsNTC.rBW50KStep + _mcp4151_5k.getPosition(0) * digipotsNTC.rBW5KStep + digipotsNTC.rWTotal);
#if DEVELOPPER_MODE
    dpJSON = dpJSON + F(",\"dp5k\":") + _mcp4151_5k.getPosition(0);
    dpJSON = dpJSON + F(",\"dp50k\":") + _mcp4151_50k.getPosition(0);
#endif
    dpJSON += '}';

    request->send(200, F("text/json"), dpJSON);
  });

  //SetDigiPot
  server.on("/sdp", HTTP_POST, [this](AsyncWebServerRequest *request) {

#define TICK_TO_SKIP 20

    char parseBuffer[6] = {0}; //6 because of 5 numbers value + end for resistance

    //look for temperature to apply
    if (request->hasParam(F("temperature"), true))
    {
      //convert and set it
      SetDualDigiPot(request->getParam(F("temperature"), true)->value().toFloat());
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

    //look for increase of digipots
    if (request->hasParam(F("up"), true))
    {
      //go one step up
      SetDualDigiPot((int)(_mcp4151_50k.getPosition(0) * digipotsNTC.rBW50KStep + _mcp4151_5k.getPosition(0) * digipotsNTC.rBW5KStep + digipotsNTC.rWTotal + digipotsNTC.rBW5KStep));
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

    //look for decrease of digipots
    if (request->hasParam(F("down"), true))
    {
      //go one step down
      SetDualDigiPot((int)(_mcp4151_50k.getPosition(0) * digipotsNTC.rBW50KStep + _mcp4151_5k.getPosition(0) * digipotsNTC.rBW5KStep + digipotsNTC.rWTotal - digipotsNTC.rBW5KStep));
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

#if DEVELOPPER_MODE
    //look for 5k digipot requested position
    if (request->hasParam(F("dp5k"), true))
    {
      //convert and set it
      _mcp4151_5k.setPosition(0, request->getParam(F("dp5k"), true)->value().toInt());
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

    //look for 50k digipot requested position
    if (request->hasParam(F("dp50k"), true))
    {
      //convert and set it
      _mcp4151_50k.setPosition(0, request->getParam(F("dp50k"), true)->value().toInt());
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

    //look for resistance to apply
    if (request->hasParam(F("resistance"), true))
    {
      //convert resistance value and call right function
      SetDualDigiPot(0, request->getParam(F("resistance"), true)->value().toInt());
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }
#endif

    request->send(200);
  });
};

//------------------------------------------
//Run for timer
void WebPalaSensor::AppRun()
{
  _refreshTimer.run();
}

//------------------------------------------
//Constructor
WebPalaSensor::WebPalaSensor(char appId, String appName) : Application(appId, appName), _ds18b20(ONEWIRE_BUS_PIN), _mcp4151_5k(MCP4151_5k_SSPIN), _mcp4151_50k(MCP4151_50k_SSPIN)
{
  //Init SPI for DigiPot
  SPI.begin();
  //Init DigiPots @20°C
  _mcp4151_50k.setPosition(0, 61);
  _mcp4151_5k.setPosition(0, 5);
}