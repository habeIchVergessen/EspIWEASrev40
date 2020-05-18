#ifdef _MQTT_SUPPORT

#include "EspMqtt.h"

void loopEspMqtt() {
}

EspMqtt::EspMqtt(String mqttClientName) {
  mMqttClientName   = mqttClientName + "@" + getChipID();
  mMqttTopicPrefix  = mqttClientName + "/" + getChipID() + "/";

  mqttClient.setClient(wifiClient);
}

bool EspMqtt::testConfig(String server, String port, String user, String password) {
  bool result = (server == "" && port == "");
  
  if ((server == "" && port == "") || ((result = connect(server, port, user, password, true)) && sendAlive())) {
    espConfig.setValue(F("mqttServer"), server);
    espConfig.setValue(F("mqttPort"), port);
    espConfig.setValue(F("mqttUser"), user);
    espConfig.setValue(F("mqttPassword"), password);
    espConfig.saveToFile();
  }

  return result;
}

bool EspMqtt::isConnected() {
  return mqttClient.connected();
}

bool EspMqtt::connect() {
  return connect(espConfig.getValue(F("mqttServer")), espConfig.getValue(F("mqttPort")), espConfig.getValue(F("mqttUser")), espConfig.getValue(F("mqttPassword")));
}

bool EspMqtt::connect(String server, String port, String user, String password, bool reconnect) {
  if (server == "" || port == "" || (mLastConnectAttempFailed && !reconnect))
    return false;

  if (!reconnect && isConnected())
    return true;

  if (reconnect)
    disconnect();

#ifdef _DEBUG_MQTT
  DBG_PRINT("connecting to MQTT-Broker: ");
  unsigned long connStart = micros();
#endif

  bool conn = false;
  mqttClient.setServer(parseIP(server), atoi(port.c_str()));
  if (!(conn = mqttClient.connect(mMqttClientName.c_str()))) {
    mLastConnectAttempFailed = true;
  }

#ifdef _DEBUG_MQTT
  DBG_PRINTLN(String(conn ? "connected " : "failed") + elapTime(connStart));
#endif

  return conn;
}

void EspMqtt::disconnect() {
  mLastConnectAttempFailed = false;
  mqttClient.disconnect();
}

bool EspMqtt::publish(String deviceName, String attributeName, String attributeValue, bool keepConnection) {
  return publish(deviceName + "/" + attributeName, attributeValue, keepConnection);
}

bool EspMqtt::publish(String topic, String value, bool keepConnection) {
  if (!isConnected() && !connect())
    return false;

#if defined(_DEBUG_MQTT) || defined(_DEBUG_TIMING)
  DBG_PRINT("EspMqtt::publish: topic = '" + mMqttTopicPrefix + topic + "' value '" + value + "'");
  unsigned long pubStart = micros();
#endif

  bool result = mqttClient.publish(String(mMqttTopicPrefix + topic).c_str(), value.c_str());

#if defined(_DEBUG_MQTT) || defined(_DEBUG_TIMING)
  DBG_PRINTLN(" " + elapTime(pubStart));
#endif

  if (!keepConnection)
    disconnect();

  return result;
}

bool EspMqtt::sendAlive() {
  return publish(F("Status"), F("Alive"));
}

bool EspMqtt::subscribe(String topic, MQTT_CALLBACK_SIGNATURE, bool keepConnection) {
  if (!isConnected() && !connect())
    return false;

  mqttClient.subscribe(String(mMqttTopicPrefix + topic).c_str());
  mqttClient.setCallback(callback);

  if (!keepConnection)
    disconnect();
}

IPAddress EspMqtt::parseIP(String ip) {
  IPAddress MyIP;
  for (int i = 0; i < 4; i++){
    String x = ip.substring(0, ip.indexOf("."));
    MyIP[i] = x.toInt();
    ip.remove(0, ip.indexOf(".")+1); 
  }
  return MyIP;
}

#ifdef ESP8266
bool EspMqtt::EspMqttRequestHandler::handle(ESP8266WebServer& server, HTTPMethod method, String uri) {
#endif
#ifdef ESP32
bool EspMqtt::EspMqttRequestHandler::handle(WebServer& server, HTTPMethod method, String uri) {
#endif
  if (server.method() == HTTP_POST && server.uri() == getConfigUri() && server.hasArg(espMqtt.mqttMenuId())) {
    String mqttArg = server.arg(espMqtt.mqttMenuId());
  
    DBG_PRINT("mqtt ");
    if (mqttArg == "") {
      String result = F("<h4>MQTT</h4>");
      result += mqttForm();
      server.client().setNoDelay(true);
      server.send(200, "text/html", result);
      
      return (httpRequestProcessed = true);
    }
    
    if (mqttArg == "submit") {
      if (espMqtt.testConfig(server.arg("server"), server.arg("port"), server.arg("user"), server.arg("password"))) {
        server.client().setNoDelay(true);
        server.send(200, "text/plain", "ok");
      } else {
        server.client().setNoDelay(true);
        server.send(304, "text/plain", "MQTT connection test failed!");
      }

      return (httpRequestProcessed = true);
    }
  }

  return false;
}

#ifdef ESP8266
bool EspMqtt::EspMqttRequestHandler::canHandle(ESP8266WebServer& server) {
#endif
#ifdef ESP32
bool EspMqtt::EspMqttRequestHandler::canHandle(WebServer& server) {
#endif
  if (server.method() == HTTP_POST && server.uri() == getConfigUri() && server.hasArg(espMqtt.mqttMenuId()))
    return true;

  return false;
}


String EspMqtt::EspMqttRequestHandler::menuHtml() {
  return htmlMenuItem(espMqtt.mqttMenuId(), "MQTT");
}

uint8_t EspMqtt::EspMqttRequestHandler::menuIdentifiers() {
  return 1;
}

String EspMqtt::EspMqttRequestHandler::menuIdentifiers(uint8_t identifier) {
  switch (identifier) {
    case 0: return espMqtt.mqttMenuId();break;
  }
  
  return "";
}

#endif  // _MQTT_SUPPORT

