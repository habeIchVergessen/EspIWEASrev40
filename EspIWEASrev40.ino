#include "IWEAS.h"

IWEAS_v40::IWEAS_v40(byte relayPin, byte loadPin) {
  mRelayPin = relayPin;
  mLoadPin = loadPin;

  pinMode(mRelayPin, OUTPUT); 
  pinMode(mLoadPin, INPUT_PULLUP);

  IWEAS_v40::registerInstance(this);
}

bool IWEAS_v40::initialize(IWEAS_v40::PowerState powerState) {
  if (getLoadState() == LoadOff && powerState == PowerOff)
    return true;

    bool result = setPowerState(powerState);
    mLastLoadState = getLoadState();
    
    return result;
}

IWEAS_v40::LoadState IWEAS_v40::getLoadState() {
  return (digitalRead(mLoadPin) == HIGH ? LoadOn : LoadOff);
}

IWEAS_v40::PowerState IWEAS_v40::getPowerState() {
  return (getLoadState() == LoadOn ? PowerOn : PowerOff);
}

bool IWEAS_v40::setPowerState(IWEAS_v40::PowerState powerState) {
  if (getPowerState() == powerState)
    return false;
    
  return setRelayState((getRelayState() == RelayPos1 ? RelayPos2 : RelayPos1));
}

bool IWEAS_v40::togglePowerState() {
  return setPowerState((getLoadState() == LoadOn ? PowerOff : PowerOn));
}

IWEAS_v40::RelayState IWEAS_v40::getRelayState() {
  return (digitalRead(mRelayPin) == HIGH ? RelayPos1 : RelayPos2);
}

bool IWEAS_v40::setRelayState(IWEAS_v40::RelayState relayState) {
  digitalWrite(mRelayPin, (relayState == RelayPos1 ? HIGH : LOW));

  delayMicroseconds(10000);

  return (getRelayState() == relayState);
}

void IWEAS_v40::registerPowerStateCallback(PowerStateCallback powerStateCallback) {
  mPowerStateCallback = powerStateCallback;
}

void IWEAS_v40::registerButtonPressCallback(ButtonPressCallback buttonPressCallback, byte inputPin) {
  mInputPin = inputPin;
  pinMode(mInputPin, INPUT_PULLUP); 
  mButtonPressCallback = buttonPressCallback;
  mButtonPressed = false;
}

void IWEAS_v40::loop() {
  // call the function to process instance list (stored in the static variable)
  // for each instance call function loopInstance
  IWEAS_v40::handleInstance(NULL, true);
}

void IWEAS_v40::loopInstance() {
  loopButton();
  loopPower();
}

void IWEAS_v40::loopButton() {
  if (mButtonPressCallback == NULL)
    return;
    
  bool pinState = (digitalRead(mInputPin) == LOW);
  
  if (!mButtonPressed && pinState) {
    mButtonPressed = true;
    mButtonPressedLong = false;
    mButtonPressedMillis = millis();
    return;
  }

  if (mButtonPressed && !pinState && mButtonPressedMillis + 50 > millis()) {
    mButtonPressed = mButtonPressedLong = false;
    return;
  }
  
  if (mButtonPressed && pinState && !mButtonPressedLong && (mButtonPressedMillis + 1000 < millis())) {
    mButtonPressedLong = true;
    mButtonPressCallback(this);
  }

  if (mButtonPressed && !pinState) {
    if (!mButtonPressedLong)
      mButtonPressCallback(this);
    mButtonPressed = mButtonPressedLong = false;
  }
}

void IWEAS_v40::loopPower() {
  LoadState loadState = getLoadState();
    
  if (mLastLoadState != loadState) {
    mLastLoadState = loadState;
    if (mPowerStateCallback != NULL)
      mPowerStateCallback(this);
  }
}

// functions to handle list of instances
IWEAS_v40* IWEAS_v40::getInstance(String name) {
  return IWEAS_v40::handleInstance(NULL, false, true, name);
}

IWEAS_v40* IWEAS_v40::registerInstance(IWEAS_v40 *iweasV40) {
  IWEAS_v40::handleInstance(iweasV40);
}

bool IWEAS_v40::hasNext() {
  return (getNextInstanceIntern(true) != NULL);
}

IWEAS_v40* IWEAS_v40::getNextInstance() {
  return getNextInstanceIntern(false);
}

IWEAS_v40* IWEAS_v40::getNextInstanceIntern(bool readNext) {
  static IWEAS_v40 *nextItem = NULL;
  
  if (readNext)
    nextItem = handleInstance(NULL, false, false, "", true);
    
  return nextItem;
}

// static class members doesn't work properly
// so we need a function with static variable to register all class instances in one list
// we also need to process this list from the same function where the static variable is defined
IWEAS_v40* IWEAS_v40::handleInstance(IWEAS_v40 *iweasV40, bool processOnly, bool searchOnly, String searchForName, bool iterateOnly) {
  static IWEAS_v40List *firstInstance = NULL, *iterItem = NULL;

  // iterate
  if (!processOnly && !searchOnly && iterateOnly) {
    if (firstInstance == NULL)
      return NULL;

    if (iterItem == NULL) {
      iterItem = firstInstance;
      return iterItem->iweasV40;
    }

    if (iterItem != NULL && iterItem->next == NULL) {
      iterItem = NULL;
      return NULL;
    }

    iterItem = iterItem->next;
    return iterItem->iweasV40;
  }
  
  // process (loop)
  if (processOnly && !searchOnly && !iterateOnly) {
    IWEAS_v40List *currItem = firstInstance;
    while (currItem != NULL) {
      currItem->iweasV40->loopInstance();
      currItem = currItem->next;
    }
  }
  
  // search by name
  if (!processOnly && searchOnly && !iterateOnly) {
    IWEAS_v40List *currItem = firstInstance;
    while (currItem != NULL) {
      if (currItem->iweasV40->getName() == searchForName)
        return currItem->iweasV40;
      currItem = currItem->next;
    }
  }

  if (processOnly || searchOnly || iterateOnly)
    return NULL;

  // add new instance to list
  IWEAS_v40List *newItem = NULL;

  byte cnt = 0;
  if (firstInstance == NULL) {
    cnt++;
    firstInstance = newItem = new IWEAS_v40List;
    newItem->iweasV40 = iweasV40;
    newItem->next = NULL;
  } else {
    IWEAS_v40List *currItem = firstInstance;
    while (currItem != NULL) {
      cnt++;
      if (currItem->iweasV40->getName() == iweasV40->getName()) {
        DBG_PRINTLN("IWEAS_v40 instances needs unique names!");
        return NULL;
      }

      if (currItem->next == NULL)
        break;

      currItem = currItem->next;
    }
  
    currItem->next = newItem = new IWEAS_v40List;
    newItem->iweasV40 = iweasV40;
    newItem->next = NULL;
  }

  newItem->iweasV40->setName("R" + String(cnt));

  return newItem->iweasV40;
}

