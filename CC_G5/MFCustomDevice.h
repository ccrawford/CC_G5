#pragma once

#include <Arduino.h>
#include "CC_G5.h"
#include "CC_G5_PFD.h"
#include "CC_ISIS.h"
#include "MFCustomDeviceTypes.h"

class MFCustomDevice
{
public:
    MFCustomDevice();
    void attach(uint16_t adrPin, uint16_t adrType, uint16_t adrConfig, bool configFromFlash = false);
    void detach();
    void update();
    void set(int16_t messageID, char *setPoint);

private:
    bool       getStringFromMem(uint16_t addreeprom, char *buffer, bool configFromFlash);
    bool       _initialized = false;
    CC_G5_HSI *_hsiDevice;
    CC_G5_PFD *_pfdDevice;
    CC_ISIS *_isisDevice;
    uint8_t    _customType = CUSTOM_HSI_DEVICE;
};
