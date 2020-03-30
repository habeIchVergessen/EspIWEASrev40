
#define PROGNAME "EspIWEASrev40"
#define PROGVERS "0.1"
#define PROGBUILD String(__DATE__) + " " + String(__TIME__)

int         Time_VOLTAGE  =  120;                       // ALife-Intervall für Abfrage der Betriebsspannung in Sekunden

#define VOLTAGE   16

#define RELAY_1_SUPPORT
//#define RELAY_2_SUPPORT

// Zuweisung der GPIO´s muss nur für andere Board´s (Wemos, ESP01 usw.) geaendert werden
#ifdef RELAY_1_SUPPORT
  #define PIN_LOAD_1     4
  #define PIN_RELAY_1   14
  #define PIN_BUTTON_1  12
#endif  // RELAY_1_SUPPORT

#ifdef RELAY_2_SUPPORT
  #define PIN_LOAD_2     5
  #define PIN_RELAY_2   15
  #define PIN_BUTTON_2  13
#endif  // RELAY_2_SUPPORT

#define _MQTT_SUPPORT
#define _ESPIWEAS_SUPPORT

#ifdef ESP8266
  #include <ESP8266WiFi.h>
  #include "ESP8266WebServer.h"
#endif

#ifdef ESP32
  #include <WiFi.h>
  #include "WebServer.h"
#endif

//#define _DEBUG
#define _DEBUG_SETUP
//#define _DEBUG_TIMING
//#define _DEBUG_MQTT
//#define _DEBUG_HEAP

#include "EspConfig.h"
#include "EspDebug.h"

//#define _DEBUG_HTTP
#include "EspWifi.h"
#include "IWEAS_v40.h"

#ifdef _MQTT_SUPPORT
  #include <PubSubClient.h>
  #include "EspMqtt.h"
#endif

// KeyValueProtocol with full format keys
//#define KVP_LONG_KEY_FORMAT 1
#include "KVPSensors.h"

bool httpRequestProcessed     = false;
bool optionsChanged           = false;

bool sendKeyValueProtocol = true;
#ifdef _MQTT_SUPPORT
  bool sendMqtt = true;
#endif

// global objects
EspConfig espConfig(PROGNAME);
EspWiFi espWiFi;
EspDebug espDebug;

#ifdef _MQTT_SUPPORT
  EspMqtt espMqtt(PROGNAME);

  enum MqttTargets : byte {
    MqttClientID = 0x00
  , MqttZiel01 = 0x01
  , MqttZiel02 = 0x02
  , MqttZiel03 = 0x03
  , MqttQuelle = 0x10
  , MqttBattery = 0x20
  };
#endif

// prototypes
void readVoltage(bool force=false);
#ifdef _MQTT_SUPPORT
  bool pusblishToMqttTarget(MqttTargets mqttTarget, String state, String relayName="");
  String getMqttTarget(MqttTargets mqttTarget, String relayName="");
#endif

unsigned long lastVoltage = 0;

void setup() {

  Serial.begin(115200);
  yield();

  espDebug.enableSerialOutput();
  
#if defined(RELAY_1_SUPPORT) || defined(RELAY_2_SUPPORT)

  IWEAS_v40 *iweasV40;

  // R1
  #ifdef RELAY_1_SUPPORT
    iweasV40 = new IWEAS_v40(PIN_RELAY_1, PIN_LOAD_1);
    iweasV40->registerPowerStateCallback(PowerState);
    iweasV40->registerButtonPressCallback(ButtonPressed, PIN_BUTTON_1);
  #endif  // RELAY_1_SUPPORT
    
  // R2
  #ifdef RELAY_2_SUPPORT
    iweasV40 = new IWEAS_v40(PIN_RELAY_2, PIN_LOAD_2);
    iweasV40->registerPowerStateCallback(PowerState);
    iweasV40->registerButtonPressCallback(ButtonPressed, PIN_BUTTON_2);
  #endif  // RELAY_2_SUPPORT
  
#endif  // RELAY_1_SUPPORT || RELAY_2_SUPPORT

  setupEspTools();
  EspWiFi::setup();

  // deviceConfig handler
  espWiFi.registerDeviceConfigCallback(handleDeviceConfig);
  espWiFi.registerDeviceListCallback(handleDeviceList);

#ifdef _MQTT_SUPPORT
  espMqtt.subscribe(getMqttTarget(MqttQuelle, "+"), mqttSubscribeCallback);
#endif

  // read options from config
  readOptions();

  espDebug.begin();
  espDebug.registerInputCallback(handleInputStream);

  // send initial state
  while (IWEAS_v40::hasNext() != NULL)
    PowerState(IWEAS_v40::getNextInstance());
}

void loop() {
  // apply options changes
  if (optionsChanged) {
    readOptions();
    optionsChanged = false;
  }
  
#ifdef _MQTT_SUPPORT
  EspMqtt::loop();
#endif

  // scheduler
//  scheduler.runSchedules();
  
  // handle wifi
  EspWiFi::loop();

  // tools
  loopEspTools();

  // handle input
  handleInputStream(&Serial);

#if defined(RELAY_1_SUPPORT) || defined(RELAY_2_SUPPORT)
  IWEAS_v40::loop();
#endif  // RELAY_1_SUPPORT || RELAY_2_SUPPORT

  // send debug data
  espDebug.loop();
}

void readOptions() {
  espWiFi.setupHttp((toCheckboxValue(espConfig.getValue("http")) == "1"));
  sendKeyValueProtocol = (toCheckboxValue(espConfig.getValue("kvpudp")) == "1");
  sendMqtt = (toCheckboxValue(espConfig.getValue("mqtt")) == "1");
}

void ButtonPressed(IWEAS_v40 *iweasV40) {
  iweasV40->togglePowerState();
  DBG_PRINTLN(String("ButtonPressed: ") + iweasV40->getName() + " " + iweasV40->getStatus());
}

void PowerState(IWEAS_v40 *iweasV40) {
  unsigned long powerStart = micros();

  String message = SensorDataHeader(PROGNAME, getChipID());
  String value = (iweasV40->getPowerState() == IWEAS_v40::PowerOff ? "off" : "on");
  
  message += SensorDataValuePort(Load, iweasV40->getName(), value);

  if (sendKeyValueProtocol)
    sendMessage(message, powerStart);
#ifdef _MQTT_SUPPORT
  if (sendMqtt)
    pusblishToMqttTarget(MqttZiel01, value, iweasV40->getName());
#endif
}

void readVoltage(bool force) {
  unsigned long curr = millis();

  if (curr < lastVoltage)
    lastVoltage = 0;
    
  if (!force && curr < lastVoltage + Time_VOLTAGE * 1000)
    return;

// TODO: ESP32 impl
#ifdef ESP8266

  float Spannung = ESP.getVcc() / 20000.0;
  lastVoltage = curr;
  Serial.println("readVoltage: Voltage = " + String(Spannung) + " @ " + String(lastVoltage));
#ifdef _MQTT_SUPPORT
  pusblishToMqttTarget(MqttBattery, String(Spannung));
#endif

#endif
}

#ifdef _MQTT_SUPPORT
void mqttSubscribeCallback(char* kanal, byte* nachrichtInBytes, unsigned int length) {
  // channel
  String chan = String(kanal), subDevice = "", cmd = "";

  String clientID = getMqttTarget(MqttClientID);
  if (!chan.startsWith(clientID)) {
    DBG_PRINTLN(String("callback: wrong device ") + chan);
    return;
  }
  int idx = chan.indexOf("/", clientID.length()), idx2;
  if (idx > 0 && (idx2 = chan.indexOf("/", idx + 1)) > 0) {
    subDevice = chan.substring(idx + 1, idx2);
    cmd = chan.substring(idx2 + 1);
  } else
  // malformed topic
    return;

  // search sub device
  IWEAS_v40 *iweasV40;
  if ((iweasV40 = IWEAS_v40::getInstance(subDevice)) == NULL) {
    DBG_PRINTLN(String("callback: wrong sub device ") + subDevice);
    return;
  }

  // message
  char buffer[length+1];
  memcpy(buffer, nachrichtInBytes, length);
  buffer[length] = 0;
  String msg = String(buffer);

  IWEAS_v40::PowerState powerState = iweasV40->getPowerState();

  if ((powerState == IWEAS_v40::PowerOff && msg == "On") ||
      (powerState == IWEAS_v40::PowerOn && msg == "Off")
  ) {
    iweasV40->togglePowerState();
    DBG_PRINTLN(String("callback: ") + " sub " + subDevice + ", cmd " + msg + ", status " + iweasV40->getStatus());
  }
}

bool pusblishToMqttTarget(MqttTargets mqttTarget, String state, String relayName) {
  espMqtt.publish(getMqttTarget(mqttTarget, relayName).c_str(), state.c_str());
}

String getMqttTarget(MqttTargets mqttTarget, String relayName) {
  String res = ""; //String(PROGNAME) + "@" + getChipID();
  if (mqttTarget != MqttClientID && relayName != "")
    res += relayName + "/";
  
  switch (mqttTarget) {
    case MqttClientID:
      res = String(PROGNAME) + "/" + getChipID();
      break;
    case MqttZiel01:
      res += SensorName(Load);  //"Last_Status";
      break;
    case MqttQuelle:
      res += "Befehl";
      break;
    case MqttBattery:
      res += SensorName(Voltage);  //"Spannung";
      break;
  }
  
  return res;
}
#endif

void printHeapFree() {
#ifdef _DEBUG_HEAP
  DBG_PRINTLN((String)F("heap: ") + (String)(ESP.getFreeHeap()));
#endif
}

String getDictionary() {
  String result = "";

#ifndef KVP_LONG_KEY_FORMAT
  while (IWEAS_v40::hasNext() != NULL)
    result += DictionaryValuePort(Load, IWEAS_v40::getNextInstance()->getName());

  result += DictionaryValue(Voltage);
#endif

  return result;
}

void handleInput(char r, bool hasValue, unsigned long value, bool hasValue2, unsigned long value2) {
  switch (r) {
#ifdef _MQTT_SUPPORT
    case 'm':
      if (hasValue) {
        espConfig.setValue("mqtt", (value == 1 ? "1" : "0"));
        if (espConfig.hasChanged()) {
          espConfig.saveToFile();
          readOptions();
        }
      }
      DBG_PRINTLN(String("MQTT: ") + (sendMqtt ? "on" : "off"));
      break;
#endif  // _MQTT_SUPPORT
    case 'k':
      if (hasValue) {
        espConfig.setValue("kvpudp", (value == 1 ? "1" : "0"));
        if (espConfig.hasChanged()) {
          espConfig.saveToFile();
          readOptions();
        }
      }
      DBG_PRINTLN(String("KVPUDP: ") + (sendKeyValueProtocol ? "on" : "off"));
      break;
    case 'h':
      if (hasValue) {
        espConfig.setValue("http", (value == 1 ? "1" : "0"));
        if (espConfig.hasChanged()) {
          espConfig.saveToFile();
          readOptions();
        }
      }
      break;
    case 'r':
      DBG_PRINTLN("reset: " + uptime());
#ifdef ESP8266
    ESP.reset();
#endif
#ifdef ESP32
    ESP.restart();
#endif
      break;
    case 'u':
      DBG_PRINTLN("uptime: " + uptime());
      printHeapFree();
      break;
    case 'v':
      // Version info
      handleCommandV();
      print_config();
      break;
    case ' ':
    case '\n':
    case '\r':
      break;
    case '?':
      DBG_PRINTLN();
      DBG_PRINTLN(F("usage:"));
      DBG_PRINTLN(F("# "));
      DBG_PRINTLN(F("u - uptime"));
      DBG_PRINTLN(F("k - KVPUDP on/off"));
#ifdef _MQTT_SUPPORT
      DBG_PRINTLN(F("m - MQTT on/off"));
#endif  // _MQTT_SUPPORT
      DBG_PRINTLN();
      break;
    default:
      handleCommandV();
      DBG_PRINTLN("uptime: " + uptime());
/*
#ifndef NOHELP
      Help::Show();
#endif
*/      break;
    }
}

void handleInputStream(Stream *input) {
  if (input->available() <= 0)
    return;

  static long value, value2;
  bool hasValue, hasValue2;
  char r = input->read();

  // reset variables
  value = 0; hasValue = false;
  value2 = 0; hasValue2 = false;
  
  byte sign = 0;
  // char is a number
  if ((r >= '0' && r <= '9') || r == '-'){
    byte delays = 2;
    while ((r >= '0' && r <= '9') || r == ',' || r == '-') {
      if (r == '-') {
        sign = 1;
      } else {
        // check value separator
        if (r == ',') {
          if (!hasValue || hasValue2) {
            print_warning(2, "format");
            return;
          }
          
          hasValue2 = true;
          if (sign == 0) {
            value = value * -1;
            sign = 0;
          }
        } else {
          if (!hasValue || !hasValue2) {
            value = value * 10 + (r - '0');
            hasValue = true;
          } else {
            value2 = value2 * 10 + (r - '0');
            hasValue2 = true;
          }
        }
      }
            
      // wait a little bit for more input
      while (input->available() == 0 && delays > 0) {
        delay(20);
        delays--;
      }

      // more input available
      if (delays == 0 && input->available() == 0) {
        return;
      }

      r = input->read();
    }
  }

  // Vorzeichen
  if (sign == 1) {
    if (hasValue && !hasValue2)
      value = value * -1;
    if (hasValue && hasValue2)
      value2 = value2 * -1;
  }

  handleInput(r, hasValue, value, hasValue2, value2);
}

void handleCommandV() {
  DBG_PRINT(F("["));
  DBG_PRINT(PROGNAME);
  DBG_PRINT(F("."));
  DBG_PRINT(PROGVERS);

  DBG_PRINT(F("] "));
#if defined(_MQTT_SUPPORT)
  DBG_PRINT(F("(mqtt) "));
#endif
  DBG_PRINT("compiled at " + PROGBUILD + " ");
}

#ifdef ESP8266
String handleDeviceConfig(ESP8266WebServer *server, uint16_t *resultCode) {
#endif
#ifdef ESP32
String handleDeviceConfig(WebServer *server, uint16_t *resultCode) {
#endif
  String result = "";
  String reqAction = server->arg(F("action")), deviceID = server->arg(F("deviceID"));
  
  if (reqAction != F("form") && reqAction != F("submit"))
    return result;

  if (reqAction == F("form")) {
    String action = F("/config?ChipID=");
    action += getChipID();
    action += F("&deviceID=");
    action += deviceID;
    action += F("&action=submit");

    String html = "";

    bool togglePower = false;
    if (deviceID.endsWith(".togglePower")) {
      deviceID = deviceID.substring(0, deviceID.length() - 12);
      togglePower = true;
    }
    
    IWEAS_v40 *iweasV40 = IWEAS_v40::getInstance(deviceID);

    if (togglePower) {
      if (iweasV40 != NULL) {
        IWEAS_v40::PowerState powerState = iweasV40->getPowerState();
        iweasV40->togglePowerState();

        if (powerState == iweasV40->getPowerState()) {
          // no reload
          *resultCode = 204;
          return F("No Content");
        }
      }

      // force reload
      *resultCode = 205;
      return F("Reset Content");
    }

    // config form
    if (iweasV40 != NULL) {
      html += String("Power: ") + (iweasV40->getPowerState() == IWEAS_v40::PowerOff ? "Off" : "On") + "<br>";
      html += String("Status: ") + iweasV40->getStatus();
    }
    
    if (html != "") {
      *resultCode = 200;
//      html = "<h4>" + device->getName() + " (" + deviceID + ")" + "</h4>" + html;
      result = htmlForm(html, action, F("post"), F("configForm"), "", "");
    }
  }

  if (reqAction == F("submit")) {
    *resultCode = 200;
    result = F("ok");
  }
  
  return result;
}

String handleDeviceList() {
  String result = "";

  // search device
  while (IWEAS_v40::hasNext() != NULL) {
    IWEAS_v40 *iweasV40 = IWEAS_v40::getNextInstance();
    String value = (iweasV40->getPowerState() == IWEAS_v40::PowerOff ? "Off" : "On");
    result += String("<tr><td>") + iweasV40->getName() + "</td><td>" + "Power: ";
    result += htmlAnker(iweasV40->getName() + ".togglePower", F("dc"), value);
    result += "</td><td>";
    result += htmlAnker(iweasV40->getName(), F("dc"), F("..."));
    result += "</td></tr>";
  }
  
  return result;
}

void sendMessage(String message, unsigned long startTime) {
  DBG_PRINTLN(message + " " + elapTime(startTime));
#ifdef _DEBUG_TIMING_UDP
  unsigned long multiTime = micros();
#endif
  espWiFi.sendMultiCast(message);
#ifdef _DEBUG_TIMING_UDP
  DBG_PRINTLN("udp-multicast: " + elapTime(multiTime));
#endif
}

// helper
void print_config() {
  String blank = F(" ");
  
  DBG_PRINT(F("config:"));
  DBG_PRINTLN();
}

void print_warning(byte type, String msg) {
  return;
  DBG_PRINT(F("\nwarning: "));
  if (type == 1)
    DBG_PRINT(F("skipped incomplete command "));
  if (type == 2)
    DBG_PRINT(F("wrong parameter "));
  if (type == 3)
    DBG_PRINT(F("failed: "));
  DBG_PRINTLN(msg);
}

