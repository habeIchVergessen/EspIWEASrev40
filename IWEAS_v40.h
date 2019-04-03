#ifndef _IWEAS_V40_H
#define _IWEAS_V40_H

#include "Arduino.h"

#ifdef _DEBUG_TIMING
  #define elapTime(start) (String)"(" + (String)((micros() - start) / (float)1000.0) + " ms)"
#else
  #define elapTime(start) (String)""
#endif

class IWEAS_v40 {
  public:
    typedef void (*ButtonPressCallback) (IWEAS_v40 *iweasV40);
    typedef void (*PowerStateCallback) (IWEAS_v40 *iweasV40);

    typedef struct __attribute__((packed)) IWEAS_v40List
    {
      IWEAS_v40      *iweasV40;
      IWEAS_v40List  *next;
    };

    enum LoadState : byte {
      LoadOff     = 0x00
    , LoadOn      = 0x01
    };
    enum PowerState : byte {
      PowerOff    = 0x00
    , PowerOn     = 0x01
    };
    enum RelayState : byte {
      RelayPos1   = 0x00
    , RelayPos2   = 0x01
    };
    
    IWEAS_v40(byte relayPin=14, byte loadPin=4);
    bool initialize(PowerState powerState=PowerOff);
    void registerPowerStateCallback(PowerStateCallback powerStateCallback);
    void registerButtonPressCallback(ButtonPressCallback buttonPressCallback, byte inputPin);
    PowerState getPowerState();
    bool setPowerState(PowerState powerState);
    bool togglePowerState();
    String getName() { return mName; };

    static void loop();
    static IWEAS_v40 *getInstance(String name);
    static bool hasNext();
    static IWEAS_v40* getNextInstance();

    String getStatus() { return String("pins(") + String(mRelayPin) + "," + String(mLoadPin) + (mInputPin != 0xFF ? "," + String(mInputPin) : "") + ") "
        ", power = " + String(getPowerState()) + 
        ", relay = " + String(getRelayState()) + 
        ", load = " + String(getLoadState());
    };

  protected:
    String mName;
    byte mRelayPin, mLoadPin, mInputPin;
    LoadState mLastLoadState = LoadOff;
    PowerStateCallback mPowerStateCallback = NULL;
    ButtonPressCallback mButtonPressCallback = NULL;
    bool mButtonPressed = false, mButtonPressedLong = false;
    unsigned long mButtonPressedMillis;

    LoadState getLoadState();
    RelayState getRelayState();
    bool setRelayState(RelayState relayState);
    void setName(String name) { mName = name; };
    void loopInstance();
    void loopButton();
    void loopPower();
    static IWEAS_v40* getNextInstanceIntern(bool readNext=false);

    static IWEAS_v40 *registerInstance(IWEAS_v40 *iweasV40);
    static IWEAS_v40 *handleInstance(IWEAS_v40 *iweasV40, bool processOnly=false, bool searchOnly=false, String searchForName="", bool iterateOnly=false);
};

#endif  // _IWEAS_V40_H
