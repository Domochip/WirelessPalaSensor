#include "WirelessPalaSensor.h"

//-----------------------------------------------------------------------
// Steinhart–Hart reverse function
//-----------------------------------------------------------------------
void WebPalaSensor::setDualDigiPot(float temperature)
{
  // convert temperature from Celsius to Kevin degrees
  float temperatureK = temperature + 273.15;

  // calculate and return resistance value based on provided temperature
  double x = (1 / _digipotsNTC.steinhartHartCoeffs[2]) * (_digipotsNTC.steinhartHartCoeffs[0] - (1 / temperatureK));
  double y = sqrt(pow(_digipotsNTC.steinhartHartCoeffs[1] / (3 * _digipotsNTC.steinhartHartCoeffs[2]), 3) + pow(x / 2, 2));
  setDualDigiPot((int)(exp(pow(y - (x / 2), 1.0F / 3) - pow(y + (x / 2), 1.0F / 3))));
}
//-----------------------------------------------------------------------
// Set Dual DigiPot resistance (serial rBW)
//-----------------------------------------------------------------------
void WebPalaSensor::setDualDigiPot(int resistance)
{
  float adjustedResistance = resistance - _digipotsNTC.rWTotal - (_digipotsNTC.rBW5KStep * _digipotsNTC.dp5kOffset);

  // DigiPot positions calculation
  int digiPot50k_position = floor((adjustedResistance) / (_digipotsNTC.rBW50KStep * _digipotsNTC.dp50kStepSize)) * _digipotsNTC.dp50kStepSize;
  int digiPot5k_position = round((adjustedResistance - (digiPot50k_position * _digipotsNTC.rBW50KStep)) / _digipotsNTC.rBW5KStep);
  setDualDigiPot(digiPot50k_position, digiPot5k_position + _digipotsNTC.dp5kOffset);
}

void WebPalaSensor::setDualDigiPot(unsigned int dp50kPosition, unsigned int dp5kPosition)
{
  // Set DigiPot position
  if (_mcp4151_50k.getPosition(0) != dp50kPosition)
    _mcp4151_50k.setPosition(0, dp50kPosition);
  if (_mcp4151_5k.getPosition(0) != dp5kPosition)
    _mcp4151_5k.setPosition(0, dp5kPosition);
}

//-----------------------------------------------------------------------
// Main Timer Tick (aka this should be done every 30sec)
//-----------------------------------------------------------------------
void WebPalaSensor::timerTick()
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

  // LOG
  LOG_SERIAL.println(F("TimerTick"));

  // read temperature from the local sensor
  _owTemperature = _ds18b20.readTemp();
  if (_owTemperature == 12.3456F)
    _owTemperature = 20.0; // if reading of local sensor failed so push 20°C
  else
  {
    // round it to tenth
    _owTemperature *= 10;
    _owTemperature = round(_owTemperature);
    _owTemperature /= 10;
  }

  // if HomeAutomation protocol is HTTP and WiFi is connected
  if (_ha.protocol == HA_PROTO_HTTP && WiFi.isConnected())
  {

    // if Jeedom type in http config
    if (_ha.http.type == HA_HTTP_JEEDOM)
    {
      WiFiClient client;
      WiFiClientSecure clientSecure;

      HTTPClient http;

      // set timeOut
      http.setTimeout(5000);

      // try to get house automation sensor value -----------------
      String completeURI = String(F("http")) + (_ha.http.tls ? F("s") : F("")) + F("://") + _ha.http.hostname + F("/core/api/jeeApi.php?apikey=") + _ha.http.jeedom.apiKey + F("&type=cmd&id=") + _ha.http.temperatureId;
      if (!_ha.http.tls)
        http.begin(client, completeURI);
      else
      {
        if (Utils::isFingerPrintEmpty(_ha.http.fingerPrint))
          clientSecure.setInsecure();
        else
        {
#ifdef ESP8266
          clientSecure.setFingerprint(_ha.http.fingerPrint);
#else
          // TODO: First expand fingerprint to 32 bytes
          // TODO: connect, verify with fingerprint, close and then setInsecure
          clientSecure.setInsecure();
#endif
        }
        http.begin(clientSecure, completeURI);
      }
      // send request
      _homeAutomationRequestResult = http.GET();
      // if we get successfull HTTP answer
      if (_homeAutomationRequestResult == 200)
      {
        WiFiClient *stream = http.getStreamPtr();

        // get the answer content
        char payload[6];
        int nb = stream->readBytes(payload, sizeof(payload) - 1);
        payload[nb] = 0;

        if (nb)
        {
          // convert
          _homeAutomationTemperature = atof(payload);
          // round it to tenth
          _homeAutomationTemperature *= 10;
          _homeAutomationTemperature = round(_homeAutomationTemperature);
          _homeAutomationTemperature /= 10;
        }
      }
      http.end();
    }

    // if Fibaro type in http config
    if (_ha.http.type == HA_HTTP_FIBARO)
    {
      WiFiClient client;
      WiFiClientSecure clientSecure;

      HTTPClient http;

      // set timeOut
      http.setTimeout(5000);

      // try to get house automation sensor value -----------------
      String completeURI = String(F("http")) + (_ha.http.tls ? F("s") : F("")) + F("://") + _ha.http.hostname + F("/api/devices?id=") + _ha.http.temperatureId;
      // String completeURI = String(F("http")) + (_ha.tls ? F("s") : F("")) + F("://") + _ha.hostname + F("/devices.json");
      if (!_ha.http.tls)
        http.begin(client, completeURI);
      else
      {
        if (Utils::isFingerPrintEmpty(_ha.http.fingerPrint))
          clientSecure.setInsecure();
        else
        {
#ifdef ESP8266
          clientSecure.setFingerprint(_ha.http.fingerPrint);
#else
          // TODO: First expand fingerprint to 32 bytes
          // TODO: connect, verify with fingerprint, close and then setInsecure
          clientSecure.setInsecure();
#endif
        }
        http.begin(clientSecure, completeURI);
      }

      // Pass authentication if specified in configuration
      if (_ha.http.fibaro.username[0])
        http.setAuthorization(_ha.http.fibaro.username, _ha.http.fibaro.password);

      // send request
      _homeAutomationRequestResult = http.GET();

      // if we get successfull HTTP answer
      if (_homeAutomationRequestResult == 200)
      {
        WiFiClient *stream = http.getStreamPtr();

        while (http.connected() && stream->find("\"value\""))
        {
          // go to first next double quote (or return false if a comma appears first)
          if (stream->findUntil("\"", ","))
          {
            char payload[60];
            // read value (read until next doublequote)
            int nb = stream->readBytesUntil('"', payload, sizeof(payload) - 1);
            payload[nb] = 0;
            if (nb)
            {
              // convert
              _homeAutomationTemperature = atof(payload);
              // round it to tenth
              _homeAutomationTemperature *= 10;
              _homeAutomationTemperature = round(_homeAutomationTemperature);
              _homeAutomationTemperature /= 10;
            }
          }
        }
      }
      http.end();
    }
  }

  // if HomeAutomation protocol is MQTT
  if (_ha.protocol == HA_PROTO_MQTT)
  {
    // if mqtt received value is still valid (not too old)
    if (_lastMqttHATemperatureMillis + (1000 * (unsigned long)_refreshPeriod) >= millis())
    {
      // then use it
      _homeAutomationTemperature = _lastMqttHATemperature;
    }
  }

  // if ConnectionBox protocol is HTTP and WiFi is connected
  if (_ha.cboxProtocol == CBOX_PROTO_HTTP && WiFi.isConnected())
  {
    WiFiClient client;

    HTTPClient http;

    // set timeOut
    http.setTimeout(5000);

    // try to get current stove temperature info ----------------------
    http.begin(client, String(F("http://")) + IPAddress(_ha.http.cboxIp).toString() + F("/cgi-bin/sendmsg.lua?cmd=GET%20TMPS"));

    // send request
    _stoveRequestResult = http.GET();
    // if we get successfull HTTP answer
    if (_stoveRequestResult == 200)
    {
      WiFiClient *stream = http.getStreamPtr();

      // if we found T1 in answer
      if (stream->find("\"T1\""))
      {
        char payload[8];
        // read until the comma into payload variable
        int nb = stream->readBytesUntil(',', payload, sizeof(payload) - 1);
        payload[nb] = 0; // end payload char[]
        // if we readed some bytes
        if (nb)
        {
          // look for start position of T1 value
          byte posTRW = 0;
          while ((payload[posTRW] == ' ' || payload[posTRW] == ':' || payload[posTRW] == '\t') && posTRW < nb)
            posTRW++;

          _stoveTemperature = atof(payload + posTRW); // convert
        }
      }
    }
    http.end();
  }

  // if ConnectionBox protocol is MQTT
  if (_ha.cboxProtocol == CBOX_PROTO_MQTT)
  {
    // if mqtt received value is still valid (not too old)
    if (_lastMqttStoveTemperatureMillis + (1000 * (unsigned long)_refreshPeriod) >= millis())
    {
      // then use it
      _stoveTemperature = _lastMqttStoveTemperature;
    }
  }

  // select temperature source
  if (_ha.protocol != HA_PROTO_DISABLED)
  {
    // if we got an HA temperature
    if (_homeAutomationTemperature > 0.1)
    {
      _homeAutomationFailedCount = 0;
      _homeAutomationTemperatureUsed = true;
      temperatureToDisplay = _homeAutomationTemperature;
    }
    else
    {
      // else if failed count is good and previousTemperature is good too
      if (_homeAutomationFailedCount <= _ha.maxFailedRequest && previousTemperatureToDisplay > 0.1)
      {
        _homeAutomationFailedCount++;
        _homeAutomationTemperatureUsed = true;
        _homeAutomationTemperature = previousTemperatureToDisplay;
        temperatureToDisplay = previousTemperatureToDisplay;
      }
      // otherwise failover to oneWire
      else
        temperatureToDisplay = _owTemperature;
    }
  }
  else
    temperatureToDisplay = _owTemperature; // HA not enable

  // if connectionBox is enabled, make delta adjustment calculation
  if (_ha.cboxProtocol != CBOX_PROTO_DISABLED)
  {

    // if _stoveTemperature is correct so failed counter reset
    if (_stoveTemperature > 0.1)
      _stoveRequestFailedCount = 0;
    else if (_stoveRequestFailedCount <= _ha.maxFailedRequest)
      _stoveRequestFailedCount++; // else increment failed counter

    // if failed counter went over maxFailedRequest then reset calculated delta
    if (_stoveRequestFailedCount > _ha.maxFailedRequest)
      _stoveDelta = 0;
    // if stoveTemp is ok and previousTemperatureToDisplay also so adjust delta
    if (_stoveTemperature > 0.1 && previousTemperatureToDisplay > 0.1)
      _stoveDelta += (previousTemperatureToDisplay - _stoveTemperature) / 2.5F;
  }

  // Set DigiPot position according to resistance calculated from temperature to display with delta
  setDualDigiPot(temperatureToDisplay + _stoveDelta);

  _pushedTemperature = temperatureToDisplay + _stoveDelta;
}

//------------------------------------------
// Connect then Subscribe to MQTT
void WebPalaSensor::mqttConnectedCallback(MQTTMan *mqttMan, bool firstConnection)
{

  // Subscribe to needed topic
  // if Home Automation is configured for MQTT
  if (_ha.protocol == HA_PROTO_MQTT)
  {
    mqttMan->subscribe(_ha.mqtt.temperatureTopic);
  }

  // if Connection Box/PalaControl is configured for MQTT
  if (_ha.cboxProtocol == CBOX_PROTO_MQTT)
  {
    mqttMan->subscribe(_ha.mqtt.cboxT1Topic);
  }
}

//------------------------------------------
// Callback used when an MQTT message arrived
void WebPalaSensor::mqttCallback(char *topic, uint8_t *payload, unsigned int length)
{
  // if Home Automation is configured for MQTT and topic match
  if (_ha.protocol == HA_PROTO_MQTT && !strcmp(topic, _ha.mqtt.temperatureTopic))
  {
    String strHomeAutomationTemperature;
    strHomeAutomationTemperature.reserve(length + 1);

    // convert payload to string
    for (unsigned int i = 0; i < length; i++)
      strHomeAutomationTemperature += (char)payload[i];

    // convert
    _lastMqttHATemperature = strHomeAutomationTemperature.toFloat();

    // remove all 0 from str for conversion verification
    // (remove all 0 from the string, then only one dot should remain like "0.00")
    strHomeAutomationTemperature.replace("0", "");

    if (_lastMqttHATemperature != 0.0F || strHomeAutomationTemperature == ".")
    {
      // round it to tenth
      _lastMqttHATemperature *= 10;
      _lastMqttHATemperature = round(_lastMqttHATemperature);
      _lastMqttHATemperature /= 10;

      _lastMqttHATemperatureMillis = millis();
    }
  }

  // if Home Automation is configured for MQTT and topic match
  if (_ha.cboxProtocol == CBOX_PROTO_MQTT && !strcmp(topic, _ha.mqtt.cboxT1Topic))
  {
    String strStoveTemperature;
    strStoveTemperature.reserve(length + 1);

    // convert payload to string
    for (unsigned int i = 0; i < length; i++)
      strStoveTemperature += (char)payload[i];

    // if payload contains JSON, isolate T1 value
    if (strStoveTemperature[0] == '{')
    {
      strStoveTemperature = strStoveTemperature.substring(strStoveTemperature.indexOf(F("\"T1\":")) + 5);
      strStoveTemperature.trim();
      strStoveTemperature = strStoveTemperature.substring(0, strStoveTemperature.indexOf(','));
      strStoveTemperature.trim();
    }

    // convert
    _lastMqttStoveTemperature = strStoveTemperature.toFloat();

    // remove all 0 from str for conversion verification
    // (remove all 0 from the string, then only one dot should remain like "0.00")
    strStoveTemperature.replace("0", "");

    if (_lastMqttStoveTemperature != 0.0F || strStoveTemperature == ".")
      _lastMqttStoveTemperatureMillis = millis();
  }
}

//------------------------------------------
// Used to initialize configuration properties to default values
void WebPalaSensor::setConfigDefaultValues()
{
  _refreshPeriod = 30;

  _digipotsNTC.rWTotal = 240.0;
  _digipotsNTC.steinhartHartCoeffs[0] = 0.001067860568;
  _digipotsNTC.steinhartHartCoeffs[1] = 0.0002269969431;
  _digipotsNTC.steinhartHartCoeffs[2] = 0.0000002641627999;
  _digipotsNTC.rBW5KStep = 19.0;
  _digipotsNTC.rBW50KStep = 190.0;
  _digipotsNTC.dp50kStepSize = 1;
  _digipotsNTC.dp5kOffset = 10;

  _ha.maxFailedRequest = 10;
  _ha.protocol = HA_PROTO_DISABLED;
  _ha.cboxProtocol = CBOX_PROTO_DISABLED;

  _ha.http.type = HA_HTTP_JEEDOM;
  _ha.http.hostname[0] = 0;
  _ha.http.tls = false;
  memset(_ha.http.fingerPrint, 0, 20);
  _ha.http.temperatureId = 0;
  _ha.http.jeedom.apiKey[0] = 0;
  _ha.http.fibaro.username[0] = 0;
  _ha.http.fibaro.password[0] = 0;
  _ha.http.cboxIp = 0;

  _ha.mqtt.hostname[0] = 0;
  _ha.mqtt.port = 1883;
  _ha.mqtt.username[0] = 0;
  _ha.mqtt.password[0] = 0;
  _ha.mqtt.baseTopic[0] = 0;
  _ha.mqtt.temperatureTopic[0] = 0;
  _ha.mqtt.cboxT1Topic[0] = 0;
}

//------------------------------------------
// Parse JSON object into configuration properties
void WebPalaSensor::parseConfigJSON(JsonDocument &doc)
{
  if (!doc[F("rp")].isNull())
    _refreshPeriod = doc[F("rp")];

  if (!doc[F("sha")].isNull())
    _digipotsNTC.steinhartHartCoeffs[0] = doc[F("sha")];
  if (!doc[F("shb")].isNull())
    _digipotsNTC.steinhartHartCoeffs[1] = doc[F("shb")];
  if (!doc[F("shc")].isNull())
    _digipotsNTC.steinhartHartCoeffs[2] = doc[F("shc")];

  // Parse Home Automation config
  if (!doc[F("hamfr")].isNull())
    _ha.maxFailedRequest = doc[F("hamfr")];
  if (!doc[F("haproto")].isNull())
    _ha.protocol = doc[F("haproto")];

  if (!doc[F("hahtype")].isNull())
    _ha.http.type = doc[F("hahtype")];
  if (!doc[F("hahhost")].isNull())
    strlcpy(_ha.http.hostname, doc["hahhost"], sizeof(_ha.http.hostname));
  if (!doc[F("hahtls")].isNull())
    _ha.http.tls = doc[F("hahtls")];
  if (!doc[F("hahfp")].isNull())
    Utils::fingerPrintS2A(_ha.http.fingerPrint, doc[F("hahfp")]);
  if (!doc[F("hahtempid")].isNull())
    _ha.http.temperatureId = doc[F("hahtempid")];

  if (!doc[F("hahjak")].isNull())
    strlcpy(_ha.http.jeedom.apiKey, doc[F("hahjak")], sizeof(_ha.http.jeedom.apiKey));

  if (!doc[F("hahfuser")].isNull())
    strlcpy(_ha.http.fibaro.username, doc[F("hahfuser")], sizeof(_ha.http.fibaro.username));
  if (!doc[F("hahfpass")].isNull())
    strlcpy(_ha.http.fibaro.password, doc[F("hahfpass")], sizeof(_ha.http.fibaro.password));

  if (!doc[F("hamtemptopic")].isNull())
    strlcpy(_ha.mqtt.temperatureTopic, doc[F("hamtemptopic")], sizeof(_ha.mqtt.temperatureTopic));

  if (!doc[F("cbproto")].isNull())
    _ha.cboxProtocol = doc[F("cbproto")];

  if (!doc[F("cbhip")].isNull())
    _ha.http.cboxIp = doc[F("cbhip")];

  if (!doc[F("cbmt1topic")].isNull())
    strlcpy(_ha.mqtt.cboxT1Topic, doc[F("cbmt1topic")], sizeof(_ha.mqtt.cboxT1Topic));

  if (!doc[F("hamhost")].isNull())
    strlcpy(_ha.mqtt.hostname, doc["hamhost"], sizeof(_ha.mqtt.hostname));
  if (!doc[F("hamport")].isNull())
    _ha.mqtt.port = doc[F("hamport")];
  if (!doc[F("hamu")].isNull())
    strlcpy(_ha.mqtt.username, doc[F("hamu")], sizeof(_ha.mqtt.username));
  if (!doc[F("hamp")].isNull())
    strlcpy(_ha.mqtt.password, doc[F("hamp")], sizeof(_ha.mqtt.password));
  if (!doc[F("hambt")].isNull())
    strlcpy(_ha.mqtt.baseTopic, doc[F("hambt")], sizeof(_ha.mqtt.baseTopic));
}

//------------------------------------------
// Parse HTTP POST parameters in request into configuration properties
bool WebPalaSensor::parseConfigWebRequest(WebServer &server)
{
  if (server.hasArg(F("rp")))
    _refreshPeriod = server.arg(F("rp")).toInt();

  // Find Steinhart-Hart coeff then convert to double
  // AND handle scientific notation
  if (server.hasArg(F("sha")))
    _digipotsNTC.steinhartHartCoeffs[0] = server.arg(F("sha")).toFloat();
  if (server.hasArg(F("shb")))
    _digipotsNTC.steinhartHartCoeffs[1] = server.arg(F("shb")).toFloat();
  if (server.hasArg(F("shc")))
    _digipotsNTC.steinhartHartCoeffs[2] = server.arg(F("shc")).toFloat();

  if (server.hasArg(F("hamfr")))
    _ha.maxFailedRequest = server.arg(F("hamfr")).toInt();

  // Parse common MQTT param
  if (server.hasArg(F("hamhost")) && server.arg(F("hamhost")).length() < sizeof(_ha.mqtt.hostname))
    strcpy(_ha.mqtt.hostname, server.arg(F("hamhost")).c_str());
  if (server.hasArg(F("hamport")))
    _ha.mqtt.port = server.arg(F("hamport")).toInt();
  if (server.hasArg(F("hamu")) && server.arg(F("hamu")).length() < sizeof(_ha.mqtt.username))
    strcpy(_ha.mqtt.username, server.arg(F("hamu")).c_str());
  char tempPassword[64 + 1] = {0};
  // put MQTT password into temporary one for predefpassword
  if (server.hasArg(F("hamp")) && server.arg(F("hamp")).length() < sizeof(tempPassword))
    strcpy(tempPassword, server.arg(F("hamp")).c_str());
  // check for previous password (there is a predefined special password that mean to keep already saved one)
  if (strcmp_P(tempPassword, appDataPredefPassword))
    strcpy(_ha.mqtt.password, tempPassword);
  if (server.hasArg(F("hambt")) && server.arg(F("hambt")).length() < sizeof(_ha.mqtt.baseTopic))
    strcpy(_ha.mqtt.baseTopic, server.arg(F("hambt")).c_str());

  // Parse HA protocol
  if (server.hasArg(F("haproto")))
    _ha.protocol = server.arg(F("haproto")).toInt();

  // Now get specific param
  switch (_ha.protocol)
  {
  case HA_PROTO_HTTP:

    if (server.hasArg(F("hahtype")))
      _ha.http.type = server.arg(F("hahtype")).toInt();
    if (server.hasArg(F("hahhost")) && server.arg(F("hahhost")).length() < sizeof(_ha.http.hostname))
      strcpy(_ha.http.hostname, server.arg(F("hahhost")).c_str());
    if (server.hasArg(F("hahtls")))
      _ha.http.tls = (server.arg(F("hahtls")) == F("on"));
    else
      _ha.http.tls = false;
    if (server.hasArg(F("hahfp")))
      Utils::fingerPrintS2A(_ha.http.fingerPrint, server.arg(F("hahfp")).c_str());
    if (server.hasArg(F("hahtempid")))
      _ha.http.temperatureId = server.arg(F("hahtempid")).toInt();

    switch (_ha.http.type)
    {
    case HA_HTTP_JEEDOM:

      char tempApiKey[48 + 1];
      // put apiKey into temporary one for predefpassword
      if (server.hasArg(F("hahjak")) && server.arg(F("hahjak")).length() < sizeof(tempApiKey))
        strcpy(tempApiKey, server.arg(F("hahjak")).c_str());
      // check for previous apiKey (there is a predefined special password that mean to keep already saved one)
      if (strcmp_P(tempApiKey, appDataPredefPassword))
        strcpy(_ha.http.jeedom.apiKey, tempApiKey);
      if (!_ha.http.hostname[0] || !_ha.http.jeedom.apiKey[0])
        _ha.protocol = HA_PROTO_DISABLED;
      break;
    case HA_HTTP_FIBARO:

      if (server.hasArg(F("hahfuser")) && server.arg(F("hahfuser")).length() < sizeof(_ha.http.fibaro.username))
        strcpy(_ha.http.fibaro.username, server.arg(F("hahfuser")).c_str());

      char tempFibaroPassword[64 + 1];
      // put Fibaropassword into temporary one for predefpassword
      if (server.hasArg(F("hahfpass")) && server.arg(F("hahfpass")).length() < sizeof(_ha.http.fibaro.password))
        strcpy(tempFibaroPassword, server.arg(F("hahfpass")).c_str());
      // check for previous fibaro password (there is a predefined special password that mean to keep already saved one)
      if (strcmp_P(tempFibaroPassword, appDataPredefPassword))
        strcpy(_ha.http.fibaro.password, tempFibaroPassword);
      if (!_ha.http.hostname[0])
        _ha.protocol = 0;
      break;
    }
    break;

  case HA_PROTO_MQTT:

    if (server.hasArg(F("hamtemptopic")) && server.arg(F("hamtemptopic")).length() < sizeof(_ha.mqtt.temperatureTopic))
      strcpy(_ha.mqtt.temperatureTopic, server.arg(F("hamtemptopic")).c_str());

    if (!_ha.mqtt.hostname[0] || !_ha.mqtt.baseTopic[0] || !_ha.mqtt.temperatureTopic[0])
      _ha.protocol = HA_PROTO_DISABLED;
    break;
  }

  // Parse CBox protocol
  if (server.hasArg(F("cbproto")))
    _ha.cboxProtocol = server.arg(F("cbproto")).toInt();

  // Now get specific param
  switch (_ha.cboxProtocol)
  {
  case CBOX_PROTO_HTTP:

    if (server.hasArg(F("cbhip")))
    {
      IPAddress ipParser;
      if (ipParser.fromString(server.arg(F("cbhip"))))
        _ha.http.cboxIp = static_cast<uint32_t>(ipParser);
      else
      {
        _ha.http.cboxIp = 0;
        _ha.cboxProtocol = CBOX_PROTO_DISABLED;
      }
    }
    break;

  case CBOX_PROTO_MQTT:

    if (server.hasArg(F("cbmt1topic")) && server.arg(F("cbmt1topic")).length() < sizeof(_ha.mqtt.cboxT1Topic))
      strcpy(_ha.mqtt.cboxT1Topic, server.arg(F("cbmt1topic")).c_str());

    if (!_ha.mqtt.hostname[0] || !_ha.mqtt.baseTopic[0] || !_ha.mqtt.cboxT1Topic[0])
      _ha.cboxProtocol = CBOX_PROTO_DISABLED;
    break;
  }

  return true;
}

//------------------------------------------
// Generate JSON from configuration properties
String WebPalaSensor::generateConfigJSON(bool forSaveFile = false)
{
  String gc('{');

  char fpStr[60];

  gc = gc + F("\"rp\":") + _refreshPeriod;

  gc = gc + F(",\"sha\":") + String(_digipotsNTC.steinhartHartCoeffs[0], 16);
  gc = gc + F(",\"shb\":") + String(_digipotsNTC.steinhartHartCoeffs[1], 16);
  gc = gc + F(",\"shc\":") + String(_digipotsNTC.steinhartHartCoeffs[2], 16);

  gc = gc + F(",\"hamfr\":") + _ha.maxFailedRequest;
  gc = gc + F(",\"haproto\":") + _ha.protocol;

  // if for WebPage or protocol selected is HTTP
  if (!forSaveFile || _ha.protocol == HA_PROTO_HTTP)
  {
    gc = gc + F(",\"hahtype\":") + _ha.http.type;
    gc = gc + F(",\"hahhost\":\"") + _ha.http.hostname + '"';
    gc = gc + F(",\"hahtls\":") + _ha.http.tls;
    gc = gc + F(",\"hahfp\":\"") + Utils::fingerPrintA2S(fpStr, _ha.http.fingerPrint, forSaveFile ? 0 : ':') + '"';
    gc = gc + F(",\"hahtempid\":") + _ha.http.temperatureId;

    if (forSaveFile)
      gc = gc + F(",\"hahjak\":\"") + _ha.http.jeedom.apiKey + '"';
    else
      gc = gc + F(",\"hahjak\":\"") + (__FlashStringHelper *)appDataPredefPassword + '"'; // predefined special password (mean to keep already saved one)

    gc = gc + F(",\"hahfuser\":\"") + _ha.http.fibaro.username + '"';
    if (forSaveFile)
      gc = gc + F(",\"hahfpass\":\"") + _ha.http.fibaro.password + '"';
    else
      gc = gc + F(",\"hahfpass\":\"") + (__FlashStringHelper *)appDataPredefPassword + '"'; // predefined special password (mean to keep already saved one)
  }

  // if for WebPage or protocol selected is MQTT
  if (!forSaveFile || _ha.protocol == HA_PROTO_MQTT)
  {
    gc = gc + F(",\"hamtemptopic\":\"") + _ha.mqtt.temperatureTopic + '"';
  }

  gc = gc + F(",\"cbproto\":") + _ha.cboxProtocol;

  // if for WebPage or CBox protocol selected is HTTP
  if (!forSaveFile || _ha.cboxProtocol == CBOX_PROTO_HTTP)
  {
    if (forSaveFile)
      gc = gc + F(",\"cbhip\":") + _ha.http.cboxIp;
    else if (_ha.http.cboxIp)
      gc = gc + F(",\"cbhip\":\"") + IPAddress(_ha.http.cboxIp).toString() + '"';
  }

  // if for WebPage or CBox protocol selected is MQTT
  if (!forSaveFile || _ha.cboxProtocol == CBOX_PROTO_MQTT)
  {
    gc = gc + F(",\"cbmt1topic\":\"") + _ha.mqtt.cboxT1Topic + '"';
  }

  if (!forSaveFile || _ha.protocol == HA_PROTO_MQTT || _ha.cboxProtocol == CBOX_PROTO_MQTT)
  {
    gc = gc + F(",\"hamhost\":\"") + _ha.mqtt.hostname + '"';
    gc = gc + F(",\"hamport\":") + _ha.mqtt.port;
    gc = gc + F(",\"hamu\":\"") + _ha.mqtt.username + '"';
    if (forSaveFile)
      gc = gc + F(",\"hamp\":\"") + _ha.mqtt.password + '"';
    else
      gc = gc + F(",\"hamp\":\"") + (__FlashStringHelper *)appDataPredefPassword + '"'; // predefined special password (mean to keep already saved one)
    gc = gc + F(",\"hambt\":\"") + _ha.mqtt.baseTopic + '"';
  }

  gc += '}';

  return gc;
}

//------------------------------------------
// Generate JSON of application status
String WebPalaSensor::generateStatusJSON()
{
  String gs('{');

  // Home Automation infos
  switch (_ha.protocol)
  {
  case HA_PROTO_DISABLED:
    gs = gs + F("\"has1\":\"Disabled\"");
    break;
  case HA_PROTO_HTTP:
    gs = gs + F("\"has1\":\"Last Home Automation HTTP Result : ") + _homeAutomationRequestResult + '"';
    gs = gs + F(",\"has2\":\"Last Home Automation Temperature : ") + _homeAutomationTemperature + '"';
    break;
  case HA_PROTO_MQTT:
    gs = gs + F("\"has1\":\"MQTT Connection State : ");
    switch (_mqttMan.state())
    {
    case MQTT_CONNECTION_TIMEOUT:
      gs = gs + F("Timed Out");
      break;
    case MQTT_CONNECTION_LOST:
      gs = gs + F("Lost");
      break;
    case MQTT_CONNECT_FAILED:
      gs = gs + F("Failed");
      break;
    case MQTT_CONNECTED:
      gs = gs + F("Connected");
      break;
    case MQTT_CONNECT_BAD_PROTOCOL:
      gs = gs + F("Bad Protocol Version");
      break;
    case MQTT_CONNECT_BAD_CLIENT_ID:
      gs = gs + F("Incorrect ClientID ");
      break;
    case MQTT_CONNECT_UNAVAILABLE:
      gs = gs + F("Server Unavailable");
      break;
    case MQTT_CONNECT_BAD_CREDENTIALS:
      gs = gs + F("Bad Credentials");
      break;
    case MQTT_CONNECT_UNAUTHORIZED:
      gs = gs + F("Connection Unauthorized");
      break;
    }
    gs = gs + F("\",\"has2\":\"Last Home Automation Temperature : ") + _lastMqttHATemperature;
    if (_lastMqttHATemperatureMillis + (1000 * (unsigned long)_refreshPeriod) < millis())
      gs = gs + F(" (") + ((millis() - _lastMqttHATemperatureMillis) / 1000) + F(" seconds ago)");
    gs += '"';
    break;
  }

  gs = gs + F(",\"hafc\":") + _homeAutomationFailedCount;

  // stove(ConnectionBox) infos
  switch (_ha.cboxProtocol)
  {
  case CBOX_PROTO_DISABLED:
    gs = gs + F(",\"cbs1\":\"Disabled\"");
    break;
  case CBOX_PROTO_HTTP:
    gs = gs + F(",\"cbs1\":\"Last ConnectionBox HTTP Result : ") + _stoveRequestResult + '"';
    gs = gs + F(",\"cbs2\":\"Last ConnectionBox Temperature : ") + _stoveTemperature + '"';
    break;
  case CBOX_PROTO_MQTT:
    gs = gs + F(",\"cbs1\":\"MQTT Connection State : ");
    switch (_mqttMan.state())
    {
    case MQTT_CONNECTION_TIMEOUT:
      gs = gs + F("Timed Out");
      break;
    case MQTT_CONNECTION_LOST:
      gs = gs + F("Lost");
      break;
    case MQTT_CONNECT_FAILED:
      gs = gs + F("Failed");
      break;
    case MQTT_CONNECTED:
      gs = gs + F("Connected");
      break;
    case MQTT_CONNECT_BAD_PROTOCOL:
      gs = gs + F("Bad Protocol Version");
      break;
    case MQTT_CONNECT_BAD_CLIENT_ID:
      gs = gs + F("Incorrect ClientID ");
      break;
    case MQTT_CONNECT_UNAVAILABLE:
      gs = gs + F("Server Unavailable");
      break;
    case MQTT_CONNECT_BAD_CREDENTIALS:
      gs = gs + F("Bad Credentials");
      break;
    case MQTT_CONNECT_UNAUTHORIZED:
      gs = gs + F("Connection Unauthorized");
      break;
    }
    gs = gs + F("\",\"cbs2\":\"Last ConnectionBox Temperature : ") + _lastMqttStoveTemperature;
    if (_lastMqttStoveTemperatureMillis + (1000 * (unsigned long)_refreshPeriod) < millis())
      gs = gs + F(" (") + ((millis() - _lastMqttStoveTemperatureMillis) / 1000) + F(" seconds ago)");
    gs += '"';
    break;
  }

  gs = gs + F(",\"cbfc\":") + _stoveRequestFailedCount;

  gs = gs + F(",\"low\":") + _owTemperature;
  gs = gs + F(",\"owu\":\"") + (_homeAutomationTemperatureUsed ? F("Not ") : F("")) + '"';
  gs = gs + F(",\"pt\":") + _pushedTemperature;
  gs = gs + F(",\"p50\":") + _mcp4151_50k.getPosition(0);
  gs = gs + F(",\"p5\":") + _mcp4151_5k.getPosition(0);

  gs += '}';

  return gs;
}

//------------------------------------------
// code to execute during initialization and reinitialization of the app
bool WebPalaSensor::appInit(bool reInit)
{
  // stop Ticker
  _refreshTicker.detach();

  // Stop MQTT
  _mqttMan.disconnect();

  // if MQTT used so configure it
  if (_ha.protocol == HA_PROTO_MQTT || _ha.cboxProtocol == CBOX_PROTO_MQTT)
  {
    // prepare will topic
    String willTopic = _ha.mqtt.baseTopic;
    MQTTMan::prepareTopic(willTopic);
    willTopic += F("connected");

    // setup MQTT
    _mqttMan.setClient(_wifiClient).setServer(_ha.mqtt.hostname, _ha.mqtt.port);
    _mqttMan.setConnectedAndWillTopic(willTopic.c_str());
    _mqttMan.setConnectedCallback(std::bind(&WebPalaSensor::mqttConnectedCallback, this, std::placeholders::_1, std::placeholders::_2));
    _mqttMan.setCallback(std::bind(&WebPalaSensor::mqttCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // Connect
    _mqttMan.connect(_ha.mqtt.username, _ha.mqtt.password);
  }

  if (reInit)
  {
    // reset run variables to initial values
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

    _lastMqttHATemperatureMillis = millis();
    _lastMqttStoveTemperatureMillis = millis();
  }

  // first call
  timerTick();

  // then next will be done by refreshTicker
#ifdef ESP8266
  _refreshTicker.attach(_refreshPeriod, [this]()
                        { this->_needTick = true; });
#else
  _refreshTicker.attach<typeof this>(_refreshPeriod, [](typeof this palaSensor)
                                     { palaSensor->_needTick = true; }, this);
#endif

  return _ds18b20.getReady();
}

//------------------------------------------
// Return HTML Code to insert into Status Web page
const PROGMEM char *WebPalaSensor::getHTMLContent(WebPageForPlaceHolder wp)
{
  switch (wp)
  {
  case status:
    return status1htmlgz;
    break;
  case config:
    return config1htmlgz;
    break;
  default:
    return nullptr;
    break;
  };
  return nullptr;
}

// and his Size
size_t WebPalaSensor::getHTMLContentSize(WebPageForPlaceHolder wp)
{
  switch (wp)
  {
  case status:
    return sizeof(status1htmlgz);
    break;
  case config:
    return sizeof(config1htmlgz);
    break;
  default:
    return 0;
    break;
  };
  return 0;
}

//------------------------------------------
// code to register web request answer to the web server
void WebPalaSensor::appInitWebServer(WebServer &server, bool &shouldReboot, bool &pauseApplication)
{
  server.on("/calib.html", HTTP_GET, [&server]()
            {
    server.sendHeader(F("Content-Encoding"), F("gzip"));
    server.send_P(200, PSTR("text/html"), calibhtmlgz, sizeof(calibhtmlgz)); });

  // GetDigiPot
  server.on("/gdp", HTTP_GET, [this, &server]()
            {
    String dpJSON('{');
    dpJSON = dpJSON + F("\"r\":") + (_mcp4151_50k.getPosition(0) * _digipotsNTC.rBW50KStep + _mcp4151_5k.getPosition(0) * _digipotsNTC.rBW5KStep + _digipotsNTC.rWTotal);
#if DEVELOPPER_MODE
    dpJSON = dpJSON + F(",\"dp5k\":") + _mcp4151_5k.getPosition(0);
    dpJSON = dpJSON + F(",\"dp50k\":") + _mcp4151_50k.getPosition(0);
#endif
    dpJSON += '}';

    server.sendHeader(F("Cache-Control"), F("no-cache"));
    server.send(200, F("text/json"), dpJSON); });

  // SetDigiPot
  server.on("/sdp", HTTP_POST, [this, &server]()
            {

#define TICK_TO_SKIP 20
    //look for temperature to apply
    if (server.hasArg(F("temperature")))
    {
      //convert and set it
      setDualDigiPot(server.arg(F("temperature")).toFloat());
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

    //look for increase of digipots
    if (server.hasArg(F("up")))
    {
      //go one step up
      setDualDigiPot((int)(_mcp4151_50k.getPosition(0) * _digipotsNTC.rBW50KStep + _mcp4151_5k.getPosition(0) * _digipotsNTC.rBW5KStep + _digipotsNTC.rWTotal + _digipotsNTC.rBW5KStep));
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

    //look for decrease of digipots
    if (server.hasArg(F("down")))
    {
      //go one step down
      setDualDigiPot((int)(_mcp4151_50k.getPosition(0) * _digipotsNTC.rBW50KStep + _mcp4151_5k.getPosition(0) * _digipotsNTC.rBW5KStep + _digipotsNTC.rWTotal - _digipotsNTC.rBW5KStep));
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

#if DEVELOPPER_MODE
    //look for 5k digipot requested position
    if (server.hasArg(F("dp5k")))
    {
      //convert and set it
      _mcp4151_5k.setPosition(0, server.arg(F("dp5k")).toInt());
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

    //look for 50k digipot requested position
    if (server.hasArg(F("dp50k")))
    {
      //convert and set it
      _mcp4151_50k.setPosition(0, server.arg(F("dp50k")).toInt());
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

    //look for resistance to apply
    if (server.hasArg(F("resistance")))
    {
      //convert resistance value and call right function
      setDualDigiPot(0, server.arg(F("resistance")).toInt());
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }
#endif

    server.send(200); });
}

//------------------------------------------
// Run for timer
void WebPalaSensor::appRun()
{
  if (_ha.protocol == HA_PROTO_MQTT || _ha.cboxProtocol == CBOX_PROTO_MQTT)
    _mqttMan.loop();

  if (_needTick)
  {
    // disable needTick
    _needTick = false;
    // then run
    timerTick();
  }
}

//------------------------------------------
// Constructor
WebPalaSensor::WebPalaSensor(char appId, String appName) : Application(appId, appName), _ds18b20(ONEWIRE_BUS_PIN), _mcp4151_5k(MCP4151_5k_SSPIN), _mcp4151_50k(MCP4151_50k_SSPIN)
{
  // Init SPI for DigiPot
  SPI.begin();
  // Init DigiPots @20°C
  _mcp4151_50k.setPosition(0, 61);
  _mcp4151_5k.setPosition(0, 5);
}