
#ifndef _ESP_MQTT_H
#define _ESP_MQTT_H

#include "ESP8266WiFi.h"
#include "WiFiClient.h"

//#define MQTT_KEEPALIVE 120
#define MQTT_SOCKET_TIMEOUT 3

#include "PubSubClient.h"

#include "EspConfig.h"
#include "EspWiFi.h"

class EspMqtt {
public:
  EspMqtt(String mqttClientName);

  bool          isConnected();
  bool          connect();
  void          disconnect();
  bool          publish(String topic, String value, bool keepConnection=true);
  bool          publish(String deviceName, String attributeName, String attributeValue, bool keepConnection=true);
  bool          subscribe(String topic, MQTT_CALLBACK_SIGNATURE, bool keepConnection=true);
  bool          sendAlive();

  void          setup() { espWiFi.registerExternalRequestHandler(&espMqttRequestHandler); };
  void          loop() { if (!mEspMqttInitDone && (WiFi.status() == WL_CONNECTED)) { mEspMqttInitDone = true; sendAlive(); } };

protected:
  class EspMqttRequestHandler : public EspWiFiRequestHandler {
  public:
#ifdef ESP8266
    bool handle(ESP8266WebServer& server, HTTPMethod method, String uri) override;
#endif
#ifdef ESP32
    bool handle(WebServer& server, HTTPMethod method, String uri) override;
#endif

  protected:
#ifdef ESP8266
    bool canHandle(ESP8266WebServer& server) override;
#endif
#ifdef ESP32
    bool canHandle(WebServer& server) override;
#endif
    String menuHtml() override;
    uint8_t menuIdentifiers() override;
    String menuIdentifiers(uint8_t identifier) override;

    friend class EspMqtt;
  } espMqttRequestHandler;
  
  bool          mLastConnectAttempFailed = false, mEspMqttInitDone = false;
  String        mMqttClientName;
  String        mMqttTopicPrefix;
  String        mqttMenuId() { return "mqtt"; };
  
  WiFiClient    wifiClient;
  PubSubClient  mqttClient;

  bool          testConfig(String server, String port, String user, String password);
  bool          connect(String server, String port, String user, String password, bool reconnect=false);
  IPAddress     parseIP(String ip);

  friend class EspMqttRequestHandler;
};

extern EspMqtt espMqtt;

#endif  // _ESP_MQTT_H
