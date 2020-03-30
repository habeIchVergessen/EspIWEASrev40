
#ifndef _ESP_MQTT_H
#define _ESP_MQTT_H

#ifdef ESP8266
  #include <ESP8266WiFi.h>
#endif

#ifdef ESP32
  #include "WiFi.h"
#endif

#include "WiFiClient.h"

//#define MQTT_KEEPALIVE 120
#define MQTT_SOCKET_TIMEOUT 3

#include "PubSubClient.h"

#include "EspConfig.h"

class EspMqtt {
public:
  EspMqtt(String mqttClientName);

  bool          testConfig(String server, String port, String user, String password);
  bool          isConnected();
  bool          connect();
  void          disconnect();
  bool          publish(String topic, String value, bool keepConnection=true);
  bool          publish(String deviceName, String attributeName, String attributeValue, bool keepConnection=true);
  bool          subscribe(String topic, MQTT_CALLBACK_SIGNATURE, bool keepConnection=true);
  bool          sendAlive();
  static void   loop();

protected:
  bool          espMqttInitDone = false;
  bool          mLastConnectAttempFailed = false;
  String        mMqttClientName;
  String        mMqttTopicPrefix;
  WiFiClient    wifiClient;
  PubSubClient  mqttClient;

  bool          connect(String server, String port, String user, String password, bool reconnect=false);
  IPAddress     parseIP(String ip);
  void          loopMqtt();
};

extern EspMqtt espMqtt;

#endif  // _ESP_MQTT_H
