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
  if (_haTemperatureUsed)
    previousTemperatureToDisplay = _haTemperature;
  else
    previousTemperatureToDisplay = _owTemperature;
  _stoveTemperature = 0.0;
  _haTemperature = 0.0;
  _haTemperatureUsed = false;

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

    WiFiClient client;
    WiFiClientSecure clientSecure;

    HTTPClient http;

    // set timeOut
    http.setTimeout(5000);

    // try to get house automation sensor value -----------------

    // build the complete URI
    String completeURI = String(F("http")) + (_ha.http.tls ? F("s") : F("")) + F("://") + _ha.http.hostname;
    switch (_ha.http.type)
    {
    case HA_HTTP_JEEDOM:
      completeURI = completeURI + F("/core/api/jeeApi.php?apikey=") + _ha.http.jeedom.apiKey + F("&type=cmd&id=") + _ha.http.temperatureId;
      break;
    case HA_HTTP_FIBARO:
      completeURI = completeURI + F("/api/devices?id=") + _ha.http.temperatureId;
      break;
    }

    // if not TLS then use client, else use clientSecure
    if (!_ha.http.tls)
      http.begin(client, completeURI);
    else
    {
      clientSecure.setInsecure();
      http.begin(clientSecure, completeURI);
    }

    // For Fibaro, Pass authentication if specified in configuration
    if (_ha.http.type == HA_HTTP_FIBARO && _ha.http.fibaro.username[0])
      http.setAuthorization(_ha.http.fibaro.username, _ha.http.fibaro.password);

    // send request
    _haRequestResult = http.GET();

    if (_haRequestResult == 200)
    {
      WiFiClient *stream = http.getStreamPtr();

      char payload[60] = {0};
      int nb;
      JsonDocument doc;

      switch (_ha.http.type)
      {
      case HA_HTTP_JEEDOM:

        nb = stream->readBytes(payload, 5);
        payload[nb] = 0;

        if (nb) // convert
          _haTemperature = atof(payload);
        break;

      case HA_HTTP_FIBARO:

        while (http.connected() && stream->find("\"value\""))
        {
          // go to first next double quote (or return false if a comma appears first)
          if (stream->findUntil("\"", ","))
          {
            // read value (read until next doublequote)
            nb = stream->readBytesUntil('"', payload, sizeof(payload) - 1);
            payload[nb] = 0;

            if (nb) // convert
              _haTemperature = atof(payload);
          }
        }
        break;
      }

      // round it to tenth
      _haTemperature *= 10;
      _haTemperature = round(_haTemperature);
      _haTemperature /= 10;
    }
    http.end();
  }

  // if HomeAutomation protocol is MQTT
  if (_ha.protocol == HA_PROTO_MQTT)
  {
    // if mqtt received value is still valid (not too old)
    if (_lastMqttHATemperatureMillis + (1000 * (unsigned long)_refreshPeriod) >= millis())
    {
      // then use it
      _haTemperature = _lastMqttHATemperature;
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
    if (_haTemperature > 0.1)
    {
      _haFailedCount = 0;
      _haTemperatureUsed = true;
      temperatureToDisplay = _haTemperature;
    }
    else
    {
      // else if failed count is good and previousTemperature is good too
      if (_haFailedCount <= _ha.maxFailedRequest && previousTemperatureToDisplay > 0.1)
      {
        _haFailedCount++;
        _haTemperatureUsed = true;
        _haTemperature = previousTemperatureToDisplay;
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

  // raise flag to publish Home Assistant discovery data
  _needPublishHassDiscovery = true;
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
bool WebPalaSensor::publishHassDiscoveryToMqtt()
{
  // if MQTT is not connected then return false
  if (!_mqttMan.connected())
    return false;

  LOG_SERIAL.println(F("Publish Home Assistant Discovery data"));

  // variables
  JsonDocument jsonDoc;
  String device, availability, payload;
  String baseTopic;
  String uniqueIdPrefixWPalaSensor;
  String uniqueId;
  String topic;

  // prepare base topic
  baseTopic = _ha.mqtt.baseTopic;
  MQTTMan::prepareTopic(baseTopic);

  // prepare unique id prefix for WPalaSensor
  uniqueIdPrefixWPalaSensor = F("WPalaSensor_");
  uniqueIdPrefixWPalaSensor += WiFi.macAddress();
  uniqueIdPrefixWPalaSensor.replace(":", "");

  // ---------- WPalaSensor Device ----------

  // prepare WPalaSensor device JSON
  jsonDoc["configuration_url"] = F("http://wpalasensor.local");
  jsonDoc["identifiers"][0] = uniqueIdPrefixWPalaSensor;
  jsonDoc["manufacturer"] = F("Domochip");
  jsonDoc["model"] = F("WPalaSensor");
  jsonDoc["name"] = WiFi.getHostname();
  jsonDoc["sw_version"] = VERSION;
  serializeJson(jsonDoc, device); // serialize to device String
  jsonDoc.clear();                // clean jsonDoc

  // ----- WPalaSensor Entities -----

  //
  // Connectivity entity
  //

  // prepare uniqueId, topic and payload for WPalaSensor connectivity sensor
  uniqueId = uniqueIdPrefixWPalaSensor;
  uniqueId += F("_Connectivity");

  topic = _ha.mqtt.hassDiscoveryPrefix;
  topic += F("/binary_sensor/");
  topic += uniqueId;
  topic += F("/config");

  // prepare payload for WPalaSensor connectivity sensor
  jsonDoc["~"] = baseTopic.substring(0, baseTopic.length() - 1); // remove ending '/'
  jsonDoc["device_class"] = F("connectivity");
  jsonDoc["device"] = serialized(device);
  jsonDoc["entity_category"] = F("diagnostic");
  jsonDoc["object_id"] = F("wpalasensor_connectivity");
  jsonDoc["state_topic"] = F("~/connected");
  jsonDoc["unique_id"] = uniqueId;
  jsonDoc["value_template"] = F("{{ iif(int(value) > 0, 'ON', 'OFF') }}");

  jsonDoc.shrinkToFit();
  serializeJson(jsonDoc, payload);

  // publish
  _mqttMan.publish(topic.c_str(), payload.c_str(), true);

  // clean
  jsonDoc.clear();
  payload = "";

  return true;
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
  _ha.http.temperatureId = 0;
  _ha.http.jeedom.apiKey[0] = 0;
  _ha.http.fibaro.username[0] = 0;
  _ha.http.fibaro.password[0] = 0;
  _ha.http.cboxIp = 0;

  _ha.mqtt.hostname[0] = 0;
  _ha.mqtt.port = 1883;
  _ha.mqtt.username[0] = 0;
  _ha.mqtt.password[0] = 0;
  strcpy_P(_ha.mqtt.baseTopic, PSTR("$model$"));
  _ha.mqtt.hassDiscoveryEnabled = true;
  strcpy_P(_ha.mqtt.hassDiscoveryPrefix, PSTR("homeassistant"));
  _ha.mqtt.temperatureTopic[0] = 0;
  _ha.mqtt.cboxT1Topic[0] = 0;
}

//------------------------------------------
// Parse JSON object into configuration properties
bool WebPalaSensor::parseConfigJSON(JsonDocument &doc, bool fromWebPage = false)
{
  JsonVariant jv;
  char tempPassword[150 + 1] = {0};

  if ((jv = doc["rp"]).is<JsonVariant>())
    _refreshPeriod = jv;

  if ((jv = doc["sha"]).is<JsonVariant>())
    _digipotsNTC.steinhartHartCoeffs[0] = jv;
  if ((jv = doc["shb"]).is<JsonVariant>())
    _digipotsNTC.steinhartHartCoeffs[1] = jv;
  if ((jv = doc["shc"]).is<JsonVariant>())
    _digipotsNTC.steinhartHartCoeffs[2] = jv;

  // Parse Home Automation config

  if ((jv = doc["haproto"]).is<JsonVariant>())
    _ha.protocol = jv;
  if ((jv = doc["cbproto"]).is<JsonVariant>())
    _ha.cboxProtocol = jv;

  // if an home Automation protocol has been selected then get common param
  if (_ha.protocol != HA_PROTO_DISABLED)
  {
    if ((jv = doc["hamfr"]).is<JsonVariant>())
      _ha.maxFailedRequest = jv;
  }

  // if home automation or CBox protocol is MQTT then get common mqtt params
  if (_ha.protocol == HA_PROTO_MQTT || _ha.cboxProtocol == CBOX_PROTO_MQTT)
  {

    if ((jv = doc["hamhost"]).is<const char *>())
      strlcpy(_ha.mqtt.hostname, jv, sizeof(_ha.mqtt.hostname));
    if ((jv = doc["hamport"]).is<JsonVariant>())
      _ha.mqtt.port = jv;
    if ((jv = doc["hamu"]).is<const char *>())
      strlcpy(_ha.mqtt.username, jv, sizeof(_ha.mqtt.username));

    // put MQTT password into tempPassword
    if ((jv = doc["hamp"]).is<const char *>())
    {
      strlcpy(tempPassword, jv, sizeof(_ha.mqtt.password));

      // if not from web page or password is not the predefined one then copy it to _ha.mqtt.password
      if (!fromWebPage || strcmp_P(tempPassword, appDataPredefPassword))
        strcpy(_ha.mqtt.password, tempPassword);
    }
    if ((jv = doc["hambt"]).is<const char *>())
      strlcpy(_ha.mqtt.baseTopic, jv, sizeof(_ha.mqtt.baseTopic));

    _ha.mqtt.hassDiscoveryEnabled = doc["hamhassde"];
    if ((jv = doc["hamhassdp"]).is<const char *>())
      strlcpy(_ha.mqtt.hassDiscoveryPrefix, jv, sizeof(_ha.mqtt.hassDiscoveryPrefix));
  }

  // Now get Home Automation specific params
  switch (_ha.protocol)
  {
  case HA_PROTO_HTTP:

    if ((jv = doc["hahtype"]).is<JsonVariant>())
      _ha.http.type = jv;
    if ((jv = doc["hahhost"]).is<const char *>())
      strlcpy(_ha.http.hostname, jv, sizeof(_ha.http.hostname));
    _ha.http.tls = doc["hahtls"];
    if ((jv = doc["hahtempid"]).is<JsonVariant>())
      _ha.http.temperatureId = jv;

    switch (_ha.http.type)
    {
    case HA_HTTP_JEEDOM:

      // put apiKey into tempPassword
      if ((jv = doc["hahjak"]).is<const char *>())
      {
        strlcpy(tempPassword, jv, sizeof(_ha.http.jeedom.apiKey));

        // if not from web page or received apiKey is not the predefined one then copy it to _ha.http.jeedom.apiKey
        if (!fromWebPage || strcmp_P(tempPassword, appDataPredefPassword))
          strcpy(_ha.http.jeedom.apiKey, tempPassword);
      }

      if (!_ha.http.hostname[0] || !_ha.http.jeedom.apiKey[0])
        _ha.protocol = HA_PROTO_DISABLED;
      break;

    case HA_HTTP_FIBARO:

      if ((jv = doc["hahfuser"]).is<const char *>())
        strlcpy(_ha.http.fibaro.username, jv, sizeof(_ha.http.fibaro.username));

      // put Fibaropassword into tempPassword
      if ((jv = doc["hahfpass"]).is<const char *>())
      {
        strlcpy(tempPassword, jv, sizeof(_ha.http.fibaro.password));

        // if not from web page or password is not the predefined one then copy it to _ha.http.fibaro.password
        if (!fromWebPage || strcmp_P(tempPassword, appDataPredefPassword))
          strcpy(_ha.http.fibaro.password, tempPassword);
      }

      if (!_ha.http.hostname[0])
        _ha.protocol = HA_PROTO_DISABLED;
      break;
    }

    break;
  case HA_PROTO_MQTT:

    if ((jv = doc["hamtemptopic"]).is<const char *>())
      strlcpy(_ha.mqtt.temperatureTopic, jv, sizeof(_ha.mqtt.temperatureTopic));

    if (!_ha.mqtt.hostname[0] || !_ha.mqtt.baseTopic[0] || !_ha.mqtt.temperatureTopic[0])
      _ha.protocol = HA_PROTO_DISABLED;
    break;
  }

  // Now get ConnectionBox specific params
  switch (_ha.cboxProtocol)
  {
  case CBOX_PROTO_HTTP:

    if ((jv = doc["cbhip"]).is<const char *>())
    {
      IPAddress ipParser;
      if (ipParser.fromString(jv.as<const char *>()))
        _ha.http.cboxIp = static_cast<uint32_t>(ipParser);
      else
        _ha.http.cboxIp = 0;
    }

    if (!_ha.http.cboxIp)
      _ha.cboxProtocol = CBOX_PROTO_DISABLED;
    break;

  case CBOX_PROTO_MQTT:

    if ((jv = doc["cbmt1topic"]).is<const char *>())
      strlcpy(_ha.mqtt.cboxT1Topic, jv, sizeof(_ha.mqtt.cboxT1Topic));

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
  JsonDocument doc;

  doc["rp"] = _refreshPeriod;

  doc["sha"] = serialized(String(_digipotsNTC.steinhartHartCoeffs[0], 16));
  doc["shb"] = serialized(String(_digipotsNTC.steinhartHartCoeffs[1], 16));
  doc["shc"] = serialized(String(_digipotsNTC.steinhartHartCoeffs[2], 16));

  doc["hamfr"] = _ha.maxFailedRequest;
  doc["haproto"] = _ha.protocol;

  // if for WebPage or protocol selected is HTTP
  if (!forSaveFile || _ha.protocol == HA_PROTO_HTTP)
  {
    doc["hahtype"] = _ha.http.type;
    doc["hahhost"] = _ha.http.hostname;
    doc["hahtls"] = _ha.http.tls;
    doc["hahtempid"] = _ha.http.temperatureId;

    if (forSaveFile)
      doc["hahjak"] = _ha.http.jeedom.apiKey;
    else
      doc["hahjak"] = (const __FlashStringHelper *)appDataPredefPassword; // predefined special password (mean to keep already saved one)

    doc["hahfuser"] = _ha.http.fibaro.username;
    if (forSaveFile)
      doc["hahfpass"] = _ha.http.fibaro.password;
    else
      doc["hahfpass"] = (const __FlashStringHelper *)appDataPredefPassword; // predefined special password (mean to keep already saved one)
  }

  // if for WebPage or protocol selected is MQTT
  if (!forSaveFile || _ha.protocol == HA_PROTO_MQTT)
  {
    doc["hamtemptopic"] = _ha.mqtt.temperatureTopic;
  }

  doc["cbproto"] = _ha.cboxProtocol;

  // if for WebPage or CBox protocol selected is HTTP
  if (!forSaveFile || _ha.cboxProtocol == CBOX_PROTO_HTTP)
  {
    if (_ha.http.cboxIp)
      doc["cbhip"] = IPAddress(_ha.http.cboxIp).toString();
  }

  // if for WebPage or CBox protocol selected is MQTT
  if (!forSaveFile || _ha.cboxProtocol == CBOX_PROTO_MQTT)
  {
    doc["cbmt1topic"] = _ha.mqtt.cboxT1Topic;
  }

  if (!forSaveFile || _ha.protocol == HA_PROTO_MQTT || _ha.cboxProtocol == CBOX_PROTO_MQTT)
  {
    doc["hamhost"] = _ha.mqtt.hostname;
    doc["hamport"] = _ha.mqtt.port;
    doc["hamu"] = _ha.mqtt.username;
    if (forSaveFile)
      doc["hamp"] = _ha.mqtt.password;
    else
      doc["hamp"] = (const __FlashStringHelper *)appDataPredefPassword; // predefined special password (mean to keep already saved one)
    doc["hambt"] = _ha.mqtt.baseTopic;
    doc["hamhassde"] = _ha.mqtt.hassDiscoveryEnabled;
    doc["hamhassdp"] = _ha.mqtt.hassDiscoveryPrefix;
  }

  String gc;
  doc.shrinkToFit();
  serializeJson(doc, gc);

  return gc;
}

//------------------------------------------
// Generate JSON of application status
String WebPalaSensor::generateStatusJSON()
{
  JsonDocument doc;

  // Home Automation infos
  String has1, has2;
  switch (_ha.protocol)
  {
  case HA_PROTO_DISABLED:
    has1 = F("Disabled");
    break;
  case HA_PROTO_HTTP:
    doc["has1"] = String(F("Last Home Automation HTTP Result : ")) + _haRequestResult;
    doc["has2"] = String(F("Last Home Automation Temperature : ")) + _haTemperature;
    break;
  case HA_PROTO_MQTT:
    has1 = F("MQTT Connection State : ");
    switch (_mqttMan.state())
    {
    case MQTT_CONNECTION_TIMEOUT:
      has1 = has1 + F("Timed Out");
      break;
    case MQTT_CONNECTION_LOST:
      has1 = has1 + F("Lost");
      break;
    case MQTT_CONNECT_FAILED:
      has1 = has1 + F("Failed");
      break;
    case MQTT_CONNECTED:
      has1 = has1 + F("Connected");
      break;
    case MQTT_CONNECT_BAD_PROTOCOL:
      has1 = has1 + F("Bad Protocol Version");
      break;
    case MQTT_CONNECT_BAD_CLIENT_ID:
      has1 = has1 + F("Incorrect ClientID ");
      break;
    case MQTT_CONNECT_UNAVAILABLE:
      has1 = has1 + F("Server Unavailable");
      break;
    case MQTT_CONNECT_BAD_CREDENTIALS:
      has1 = has1 + F("Bad Credentials");
      break;
    case MQTT_CONNECT_UNAUTHORIZED:
      has1 = has1 + F("Connection Unauthorized");
      break;
    }
    doc["has1"] = has1;
    has2 = String(F("Last Home Automation Temperature : ")) + _lastMqttHATemperature;
    if (_lastMqttHATemperatureMillis + (1000 * (unsigned long)_refreshPeriod) < millis())
      has2 = has2 + F(" (") + ((millis() - _lastMqttHATemperatureMillis) / 1000) + F(" seconds ago)");
    doc["has2"] = has2;
    break;
  }

  doc["hafc"] = _haFailedCount;

  // stove(ConnectionBox) infos
  String cbs1, cbs2;
  switch (_ha.cboxProtocol)
  {
  case CBOX_PROTO_DISABLED:
    cbs1 = F("Disabled");
    break;
  case CBOX_PROTO_HTTP:
    doc["cbs1"] = String(F("Last ConnectionBox HTTP Result : ")) + _stoveRequestResult;
    doc["cbs2"] = String(F("Last ConnectionBox Temperature : ")) + _stoveTemperature;
    break;
  case CBOX_PROTO_MQTT:
    cbs1 = F("MQTT Connection State : ");
    switch (_mqttMan.state())
    {
    case MQTT_CONNECTION_TIMEOUT:
      cbs1 = cbs1 + F("Timed Out");
      break;
    case MQTT_CONNECTION_LOST:
      cbs1 = cbs1 + F("Lost");
      break;
    case MQTT_CONNECT_FAILED:
      cbs1 = cbs1 + F("Failed");
      break;
    case MQTT_CONNECTED:
      cbs1 = cbs1 + F("Connected");
      break;
    case MQTT_CONNECT_BAD_PROTOCOL:
      cbs1 = cbs1 + F("Bad Protocol Version");
      break;
    case MQTT_CONNECT_BAD_CLIENT_ID:
      cbs1 = cbs1 + F("Incorrect ClientID ");
      break;
    case MQTT_CONNECT_UNAVAILABLE:
      cbs1 = cbs1 + F("Server Unavailable");
      break;
    case MQTT_CONNECT_BAD_CREDENTIALS:
      cbs1 = cbs1 + F("Bad Credentials");
      break;
    case MQTT_CONNECT_UNAUTHORIZED:
      cbs1 = cbs1 + F("Connection Unauthorized");
      break;
    }
    doc["cbs1"] = cbs1;
    cbs2 = String(F("Last ConnectionBox Temperature : ")) + _lastMqttStoveTemperature;
    if (_lastMqttStoveTemperatureMillis + (1000 * (unsigned long)_refreshPeriod) < millis())
      cbs2 = cbs2 + F(" (") + ((millis() - _lastMqttStoveTemperatureMillis) / 1000) + F(" seconds ago)");
    doc["cbs2"] = cbs2;
    break;
  }

  doc["cbfc"] = _stoveRequestFailedCount;

  doc["low"] = serialized(String(_owTemperature));
  doc["owu"] = (_haTemperatureUsed ? F("Not ") : F(""));
  doc["pt"] = serialized(String(_pushedTemperature));
  doc["p50"] = _mcp4151_50k.getPosition(0);
  doc["p5"] = _mcp4151_5k.getPosition(0);

  String gs;
  doc.shrinkToFit();
  serializeJson(doc, gs);

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
    _mqttMan.setBufferSize(600); // max JSON size (Connectivity HAss discovery ~450)
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
    _haRequestResult = 0;
    _haTemperature = 0.0;
    _haFailedCount = 0;
    _stoveRequestResult = 0;
    _stoveTemperature = 0.0;
    _stoveRequestFailedCount = 0;
    _owTemperature = 0.0;
    _haTemperatureUsed = false;
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

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error)
    {
      server.send(400, F("text/html"), F("Malformed JSON"));
      return;
    }

    JsonVariant jv;

    //look for temperature to apply
    if ((jv = doc["temperature"]).is<JsonVariant>())
    {
      //convert and set it
      setDualDigiPot(jv.as<float>());
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

    //look for increase of digipots
    if (doc["up"].is<JsonVariant>())
    {
      //go one step up
      setDualDigiPot((int)(_mcp4151_50k.getPosition(0) * _digipotsNTC.rBW50KStep + _mcp4151_5k.getPosition(0) * _digipotsNTC.rBW5KStep + _digipotsNTC.rWTotal + _digipotsNTC.rBW5KStep));
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

    //look for decrease of digipots
    if (doc["down"].is<JsonVariant>())
    {
      //go one step down
      setDualDigiPot((int)(_mcp4151_50k.getPosition(0) * _digipotsNTC.rBW50KStep + _mcp4151_5k.getPosition(0) * _digipotsNTC.rBW5KStep + _digipotsNTC.rWTotal - _digipotsNTC.rBW5KStep));
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

#if DEVELOPPER_MODE

    //look for 5k digipot requested position
    if ((jv = doc["dp5k"]).is<JsonVariant>())
    {
      //convert and set it
      _mcp4151_5k.setPosition(0, jv);
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

    //look for 50k digipot requested position
    if ((jv = doc["dp50k"]).is<JsonVariant>())
    {
      //convert and set it
      _mcp4151_50k.setPosition(0, jv);
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

    //look for resistance to apply
    if ((jv = doc["resistance"]).is<JsonVariant>())
    {
      //convert resistance value and call right function
      setDualDigiPot(0, jv);
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
  {
    _mqttMan.loop();

    // if Home Assistant discovery enabled and publish is needed
    if (_ha.mqtt.hassDiscoveryEnabled && _needPublishHassDiscovery)
    {
      if (publishHassDiscoveryToMqtt()) // publish discovery
      {
        _needPublishHassDiscovery = false;
        _needTick = true;
      }
    }
  }

  if (_needTick)
  {
    _needTick = false;
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