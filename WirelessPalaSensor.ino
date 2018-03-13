#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include "WirelessPalaSensor.h"


//Return JSON of AppData1 content
String AppData1::GetJSON() {

  char fpStr[60];
  String gc;

  gc = gc + F("\"sha\":") + String(digipotsNTC.steinhartHartCoeffs[0], 16) + F(",\"shb\":") + String(digipotsNTC.steinhartHartCoeffs[1], 16) + F(",\"shc\":") + String(digipotsNTC.steinhartHartCoeffs[2], 16);

  gc = gc + F(",\"hae\":") + HA.enabled + F(",\"hatls\":\"") + (HA.tls ? F("on") : F("off")) + F("\",\"hah\":\"") + HA.hostname + F("\",\"hatid\":") + HA.temperatureId + F(",\"hafp\":\"") + Utils::FingerPrintA2S(fpStr, HA.fingerPrint, ':') + '"';
  //Jeedom apiKey: there is a predefined special password (mean to keep already saved one)
  gc = gc +  F(",\"ja\":\"") + (__FlashStringHelper*)appDataPredefPassword  + '"';
  //Fibaro username/password : there is a predefined special password (mean to keep already saved one)
  gc = gc + F(",\"fu\":\"") + HA.fibaro.username + F("\",\"fp\":\"") + (__FlashStringHelper*)appDataPredefPassword  + '"';

  gc = gc + F(",\"cbe\":\"") + (connectionBox.enabled ? F("on") : F("off")) + F("\",\"cbi\":\"") + connectionBox.ip[0] + '.' + connectionBox.ip[1] + '.' + connectionBox.ip[2] + '.' + connectionBox.ip[3] + F("\"");

  return gc;
}

//Parse HTTP Request into an AppData1 structure
bool AppData1::SetFromParameters(AsyncWebServerRequest* request, AppData1 &tempAppData1) {

  //Find Steinhart-Hart coeff then convert to double
  //AND handle scientific notation
  if (request->hasParam(F("sha"), true)) tempAppData1.digipotsNTC.steinhartHartCoeffs[0] = request->getParam(F("sha"), true)->value().toFloat();
  if (request->hasParam(F("shb"), true)) tempAppData1.digipotsNTC.steinhartHartCoeffs[1] = request->getParam(F("shb"), true)->value().toFloat();
  if (request->hasParam(F("shc"), true)) tempAppData1.digipotsNTC.steinhartHartCoeffs[2] = request->getParam(F("shc"), true)->value().toFloat();


  if (request->hasParam(F("hae"), true)) tempAppData1.HA.enabled = request->getParam(F("hae"), true)->value().toInt();
  //if an home Automation system is enabled then get common param
  if (tempAppData1.HA.enabled) {
    if (request->hasParam(F("hatls"), true)) tempAppData1.HA.tls = (request->getParam(F("hatls"), true)->value() == F("on"));
    if (request->hasParam(F("hah"), true) && request->getParam(F("hah"), true)->value().length() < sizeof(tempAppData1.HA.hostname)) strcpy(tempAppData1.HA.hostname, request->getParam(F("hah"), true)->value().c_str());
    if (request->hasParam(F("hatid"), true)) tempAppData1.HA.temperatureId = request->getParam(F("hatid"), true)->value().toInt();
    if (request->hasParam(F("hafp"), true)) Utils::FingerPrintS2A(tempAppData1.HA.fingerPrint, request->getParam(F("hafp"), true)->value().c_str());
  }

  //Now get specific param
  switch (tempAppData1.HA.enabled) {
    case 1: //Jeedom
      if (request->hasParam(F("ja"), true) && request->getParam(F("ja"), true)->value().length() < sizeof(tempAppData1.HA.jeedom.apiKey)) strcpy(tempAppData1.HA.jeedom.apiKey, request->getParam(F("ja"), true)->value().c_str());
      if (!tempAppData1.HA.hostname[0] || !tempAppData1.HA.jeedom.apiKey[0]) tempAppData1.HA.enabled = 0;
      break;
    case 2: //Fibaro
      if (request->hasParam(F("fu"), true) && request->getParam(F("fu"), true)->value().length() < sizeof(tempAppData1.HA.fibaro.username)) strcpy(tempAppData1.HA.fibaro.username, request->getParam(F("fu"), true)->value().c_str());
      if (request->hasParam(F("fp"), true) && request->getParam(F("fp"), true)->value().length() < sizeof(tempAppData1.HA.fibaro.password)) strcpy(tempAppData1.HA.fibaro.password, request->getParam(F("fp"), true)->value().c_str());
      break;
  }

  if (request->hasParam(F("cbe"), true)) tempAppData1.connectionBox.enabled = (request->getParam(F("cbe"), true)->value() == F("on"));

  if (request->hasParam(F("cbi"), true)) {
    IPAddress ipParser;
    if (ipParser.fromString(request->getParam(F("cbi"), true)->value())) {
      tempAppData1.connectionBox.ip[0] = ipParser[0];
      tempAppData1.connectionBox.ip[1] = ipParser[1];
      tempAppData1.connectionBox.ip[2] = ipParser[2];
      tempAppData1.connectionBox.ip[3] = ipParser[3];
    }
  }

  //check for previous fibaro password or apiKey (there is a predefined special password that mean to keep already saved one)
  if (!strcmp_P(tempAppData1.HA.jeedom.apiKey, appDataPredefPassword)) strcpy(tempAppData1.HA.jeedom.apiKey, HA.jeedom.apiKey);
  if (!strcmp_P(tempAppData1.HA.fibaro.password, appDataPredefPassword)) strcpy(tempAppData1.HA.fibaro.password, HA.fibaro.password);

  return true;
}







//-----------------------------------------------------------------------
// Steinhart–Hart reverse function
//-----------------------------------------------------------------------
void WebPalaSensor::SetDualDigiPot(float temperature) {

  //convert temperature from Celsius to Kevin degrees
  float temperatureK = temperature + 273.15;

  //calculate and return resistance value based on provided temperature
  double x = (1 / _appData1->digipotsNTC.steinhartHartCoeffs[2]) * (_appData1->digipotsNTC.steinhartHartCoeffs[0] - (1 / temperatureK));
  double y = sqrt(pow(_appData1->digipotsNTC.steinhartHartCoeffs[1] / (3 * _appData1->digipotsNTC.steinhartHartCoeffs[2]), 3) + pow(x / 2, 2));
  SetDualDigiPot( (int)(exp(pow(y - (x / 2), 1.0F / 3) - pow(y + (x / 2), 1.0F / 3))));
}
//-----------------------------------------------------------------------
// Set Dual DigiPot resistance (serial rBW)
//-----------------------------------------------------------------------
void WebPalaSensor::SetDualDigiPot(int resistance) {

  float adjustedResistance = resistance - _appData1->digipotsNTC.rWTotal - (_appData1->digipotsNTC.rBW5KStep * _appData1->digipotsNTC.dp5kOffset);

  //DigiPot positions calculation
  int digiPot50k_position = floor((adjustedResistance) / (_appData1->digipotsNTC.rBW50KStep * _appData1->digipotsNTC.dp50kStepSize)) * _appData1->digipotsNTC.dp50kStepSize;
  int digiPot5k_position = round((adjustedResistance - (digiPot50k_position * _appData1->digipotsNTC.rBW50KStep)) / _appData1->digipotsNTC.rBW5KStep);
  SetDualDigiPot(digiPot50k_position, digiPot5k_position + _appData1->digipotsNTC.dp5kOffset);
}

void WebPalaSensor::SetDualDigiPot(int dp50kPosition, int dp5kPosition) {
  //Set DigiPot position
  if (_mcp4151_50k.getPosition(0) != dp50kPosition) _mcp4151_50k.setPosition(0, dp50kPosition);
  if (_mcp4151_5k.getPosition(0) != dp5kPosition) _mcp4151_5k.setPosition(0, dp5kPosition);
}

//-----------------------------------------------------------------------
// Main Timer Tick (aka this should be done every 30sec)
//-----------------------------------------------------------------------
void WebPalaSensor::TimerTick() {

  if (_skipTick) {
    _skipTick--;
    return;
  }

  float temperatureToDisplay = 20.0;
  float previousTemperatureToDisplay;
  if (_homeAutomationTemperatureUsed) previousTemperatureToDisplay = _homeAutomationTemperature;
  else previousTemperatureToDisplay = _owTemperature;
  _stoveTemperature = 0.0;
  _homeAutomationTemperature = 0.0;
  _homeAutomationTemperatureUsed = false;



  //LOG
  Serial.println(F("TimerTick"));

  //read temperature from the local sensor
  _owTemperature = _ds18b20.ReadTemp();
  if (_owTemperature == 12.3456F) _owTemperature = 20.0; //if reading of local sensor failed so push 20°C
  else {
    //round it to tenth
    _owTemperature *= 10;
    _owTemperature = round(_owTemperature);
    _owTemperature /= 10;
  }

  char payload[60];
  WiFiClient * stream;


  //if ConnectionBox option enabled in config
  if (_appData1->connectionBox.enabled) {
    HTTPClient http1;

    //try to get current stove temperature info ----------------------
    http1.begin(String(F("http://")) + _appData1->connectionBox.ip[0] + '.' + _appData1->connectionBox.ip[1] + '.' + _appData1->connectionBox.ip[2] + '.' + _appData1->connectionBox.ip[3] + F("/sendmsg.php?cmd=GET%20TMPS"));
    //set timeOut
    http1.setTimeout(5000);
    //send request
    _stoveRequestResult = http1.GET();
    //if we get successfull HTTP answer
    if (_stoveRequestResult == 200) {

      stream = http1.getStreamPtr();

      //if we found TMP_ROOM_WATER in answer
      if (stream->find("\"TMP_ROOM_WATER\"")) {
        //read until the comma into payload variable
        int nb = stream->readBytesUntil(',', payload, sizeof(payload) - 1);
        payload[nb] = 0; //end payload char[]
        //if we read some bytes
        if (nb) {
          //look for start position of TMP_ROOM_WATER value
          byte posTRW = 0;
          while ((payload[posTRW] == ' ' || payload[posTRW] == ':' || payload[posTRW] == '\t') && posTRW < nb) posTRW++;

          _stoveTemperature = atof(payload + posTRW); //convert
        }
        payload[0] = 0;
      }
    }
    http1.end();
  }

  //if Jeedom option enabled in config
  if (_appData1->HA.enabled == 1) {
    HTTPClient http2;

    //try to get house automation sensor value -----------------
    String completeURI = String(F("http")) + (_appData1->HA.tls ? F("s") : F("")) + F("://") + _appData1->HA.hostname + F("/core/api/jeeApi.php?apikey=") + _appData1->HA.jeedom.apiKey + F("&type=cmd&id=") + _appData1->HA.temperatureId;
    if (!_appData1->HA.tls) http2.begin(completeURI);
    else {
      char fpStr[41];
      http2.begin(completeURI, Utils::FingerPrintA2S(fpStr, _appData1->HA.fingerPrint));
    }
    //set timeOut
    http2.setTimeout(5000);
    //send request
    _homeAutomationRequestResult = http2.GET();
    //if we get successfull HTTP answer
    if (_homeAutomationRequestResult == 200) {

      stream = http2.getStreamPtr();

      //get the answer line
      int nb = stream->readBytes(payload, (http2.getSize() > sizeof(payload) - 1) ? sizeof(payload) - 1 : http2.getSize());
      payload[nb] = 0;

      if (nb) {

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
  if (_appData1->HA.enabled == 2) {
    HTTPClient http3;

    //try to get house automation sensor value -----------------
    String completeURI = String(F("http")) + (_appData1->HA.tls ? F("s") : F("")) + F("://") + _appData1->HA.hostname + F("/api/devices?id=") + _appData1->HA.temperatureId;
    //String completeURI = String(F("http")) + (_appData1->HA.tls ? F("s") : F("")) + F("://") + _appData1->HA.hostname + F("/devices.json");
    if (!_appData1->HA.tls) http3.begin(completeURI);
    else {
      char fpStr[41];
      http3.begin(completeURI, Utils::FingerPrintA2S(fpStr, _appData1->HA.fingerPrint));
    }

    //Pass authentication if specified in configuration
    if (_appData1->HA.fibaro.username[0]) http3.setAuthorization(_appData1->HA.fibaro.username, _appData1->HA.fibaro.password);

    //set timeOut
    http3.setTimeout(5000);
    //send request
    _homeAutomationRequestResult = http3.GET();

    //if we get successfull HTTP answer
    if (_homeAutomationRequestResult == 200) {

      stream = http3.getStreamPtr();

      while (http3.connected() && stream->find("\"value\"")) {

        //go to first next double quote (or return false if a comma appears first)
        if (stream->findUntil("\"", ",")) {
          //read value (read until next doublequote)
          int nb = stream->readBytesUntil('"', payload, sizeof(payload) - 1);
          payload[nb] = 0;
          if (nb) {
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
  if (_appData1->HA.enabled) {
    //if we got an HA temperature
    if (_homeAutomationTemperature > 0.1) {
      _homeAutomationFailedCount = 0;
      _homeAutomationTemperatureUsed = true;
      temperatureToDisplay = _homeAutomationTemperature;
    }
    else {
      //else if failed count is good and previousTemperature is good too
      if (_homeAutomationFailedCount < 6 && previousTemperatureToDisplay > 0.1) {
        _homeAutomationFailedCount++;
        _homeAutomationTemperatureUsed = true;
        _homeAutomationTemperature = previousTemperatureToDisplay;
        temperatureToDisplay = previousTemperatureToDisplay;
      }
      //otherwise fail back to oneWire
      else temperatureToDisplay = _owTemperature;
    }
  }
  else temperatureToDisplay = _owTemperature; //HA not enable

  //if connectionBox is enabled
  if (_appData1->connectionBox.enabled) {

    //if _stoveTemperature is correct so failed counter reset
    if (_stoveTemperature > 0.1) _stoveRequestFailedCount = 0;
    else if (_stoveRequestFailedCount < 6) _stoveRequestFailedCount++; //else increment failed counter

    //if failed counter reached 5 then reset calculated delta
    if (_stoveRequestFailedCount >= 5) _stoveDelta = 0;
    //if stoveTemp is ok and previousTemperatureToDisplay also so adjust delta
    if (_stoveTemperature > 0.1 && previousTemperatureToDisplay > 0.1) _stoveDelta += (previousTemperatureToDisplay - _stoveTemperature) / 2.5F;
  }

  //Set DigiPot position according to resistance calculated from temperature to display with delta
  SetDualDigiPot(temperatureToDisplay + _stoveDelta);

  _pushedTemperature = temperatureToDisplay + _stoveDelta;
}


//------------------------------------------
//return WebPalaSensor Status in JSON
String WebPalaSensor::GetStatus() {

  String statusJSON('{');
  if (_appData1->HA.enabled) statusJSON = statusJSON + F("\"lhar\":") + _homeAutomationRequestResult + F(",\"lhat\":") + _homeAutomationTemperature + F(",\"hafc\":") + _homeAutomationFailedCount; //Home Automation infos
  else statusJSON = statusJSON + F("\"lhar\":\"NA\",\"lhat\":\"NA\",\"hafc\":\"NA\"");
  if (_appData1->connectionBox.enabled) statusJSON = statusJSON + F(",\"lcr\":") + _stoveRequestResult + F(",\"lct\":") + _stoveTemperature + F(",\"cfc\":") + _stoveRequestFailedCount; //stove(ConnectionBox)) infos
  else statusJSON = statusJSON + F(",\"lcr\":\"NA\",\"lct\":\"NA\",\"cfc\":\"NA\"");
  statusJSON = statusJSON + F(",\"low\":") + _owTemperature + F(",\"owu\":\"") + (_homeAutomationTemperatureUsed ? F("Not ") : F("")) + '"';
  statusJSON = statusJSON + F(",\"pt\":") + _pushedTemperature + F(",\"p50\":") + _mcp4151_50k.getPosition(0) + F(",\"p5\":") + _mcp4151_5k.getPosition(0);
  statusJSON += '}';

  return statusJSON;
}





//------------------------------------------
//Constructor
WebPalaSensor::WebPalaSensor(): _ds18b20(ONEWIRE_BUS_PIN), _mcp4151_5k(MCP4151_5k_SSPIN), _mcp4151_50k(MCP4151_50k_SSPIN) {
  //Init SPI for DigiPot
  SPI.begin();
  //Init DigiPots @20°C
  _mcp4151_50k.setPosition(0, 61);
  _mcp4151_5k.setPosition(0, 5);
}

//------------------------------------------
//Function to initiate WebPalaSensor with Config
void WebPalaSensor::Init(AppData1 &appData1) {

  Serial.print(F("Start WebPalaSensor"));

  _appData1 = &appData1;

  //first call
  TimerTick();

  //then next will be done by SimpleTimer object
  _refreshTimer.setInterval(REFRESH_PERIOD, [this]() {
    this->TimerTick();
  });

  if (_ds18b20.GetReady()) Serial.println(F(" : OK"));
  else Serial.println(F("FAILED"));
}

//------------------------------------------
void WebPalaSensor::InitWebServer(AsyncWebServer &server) {

  server.on("/gs1", HTTP_GET, [this](AsyncWebServerRequest * request) {
    request->send(200, F("text/json"), GetStatus());
  });

  server.on("/calib", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), (const uint8_t*)calibhtmlgz, sizeof(calibhtmlgz));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  //GetDigiPot
  server.on("/gdp", HTTP_GET, [this](AsyncWebServerRequest * request) {

    String dpJSON('{');
    dpJSON = dpJSON + F("\"r\":") + (_mcp4151_50k.getPosition(0) * _appData1->digipotsNTC.rBW50KStep + _mcp4151_5k.getPosition(0) * _appData1->digipotsNTC.rBW5KStep + _appData1->digipotsNTC.rWTotal);
#if DEVELOPPER_MODE
    dpJSON = dpJSON + F(",\"dp5k\":") + _mcp4151_5k.getPosition(0);
    dpJSON = dpJSON + F(",\"dp50k\":") + _mcp4151_50k.getPosition(0);
#endif
    dpJSON += '}';

    request->send(200, F("text/json"), dpJSON);
  });

  //SetDigiPot
  server.on("/sdp", HTTP_POST, [this](AsyncWebServerRequest * request) {

#define TICK_TO_SKIP 20

    char parseBuffer[6] = {0}; //6 because of 5 numbers value + end for resistance

    //look for temperature to apply
    if (request->hasParam(F("temperature"), true)) {
      //convert and set it
      SetDualDigiPot(request->getParam(F("temperature"), true)->value().toFloat());
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

    //look for increase of digipots
    if (request->hasParam(F("up"), true)) {
      //go one step up
      SetDualDigiPot((int)(_mcp4151_50k.getPosition(0) * _appData1->digipotsNTC.rBW50KStep + _mcp4151_5k.getPosition(0) * _appData1->digipotsNTC.rBW5KStep + _appData1->digipotsNTC.rWTotal + _appData1->digipotsNTC.rBW5KStep ));
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

    //look for decrease of digipots
    if (request->hasParam(F("down"), true)) {
      //go one step down
      SetDualDigiPot((int)(_mcp4151_50k.getPosition(0) * _appData1->digipotsNTC.rBW50KStep + _mcp4151_5k.getPosition(0) * _appData1->digipotsNTC.rBW5KStep + _appData1->digipotsNTC.rWTotal - _appData1->digipotsNTC.rBW5KStep ));
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

#if DEVELOPPER_MODE
    //look for 5k digipot requested position
    if (request->hasParam(F("dp5k"), true)) {
      //convert and set it
      _mcp4151_5k.setPosition(0, request->getParam(F("dp5k"), true)->value().toInt());
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

    //look for 50k digipot requested position
    if (request->hasParam(F("dp50k"), true)) {
      //convert and set it
      _mcp4151_50k.setPosition(0, request->getParam(F("dp50k"), true)->value().toInt());
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

    //look for resistance to apply
    if (request->hasParam(F("resistance"), true)) {
      //convert resistance value and call right function
      SetDualDigiPot(0, request->getParam(F("resistance"), true)->value().toInt());
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }
#endif

    request->send(200);
  });
}

//------------------------------------------
//Run for timer
void WebPalaSensor::Run() {

  _refreshTimer.run();
}
