#include "MFCustomDevice.h"
#include "commandmessenger.h"
#include "allocateMem.h"
#include "MFEEPROM.h"

#ifdef HAS_CONFIG_IN_FLASH
#include "MFCustomDevicesConfig.h"
#else
const char CustomDeviceConfig[] PROGMEM = {};
#endif

extern MFEEPROM MFeeprom;

/* **********************************************************************************
    The custom device pins, type and configuration is stored in the EEPROM
    While loading the config the adresses in the EEPROM are transferred to the constructor
    Within the constructor you have to copy the EEPROM content to a buffer
    and evaluate him. The buffer is used for all 3 types (pins, type configuration),
    so do it step by step.
    The max size of the buffer is defined here. It must be the size of the
    expected max length of these strings.

    E.g. 6 pins are required, each pin could have two characters (two digits),
    each pins are delimited by "|" and the string is NULL terminated.
    -> (6 * 2) + 5 + 1 = 18 bytes is the maximum.
    The custom type is "CC_G5", which means 14 characters plus NULL = 15
    The configuration is "myConfig", which means 8 characters plus NULL = 9
    The maximum characters to be expected is 18, so MEMLEN_STRING_BUFFER has to be at least 18
********************************************************************************** */
#define MEMLEN_STRING_BUFFER 40

// reads a string from EEPROM or Flash at given address which is '.' terminated and saves it to the buffer
bool MFCustomDevice::getStringFromMem(uint16_t addrMem, char *buffer, bool configFromFlash)
{
    // Serial.printf("GetString\n");

    char     temp    = 0;
    uint8_t  counter = 0;
    uint16_t length  = MFeeprom.get_length();
    do {
        if (configFromFlash) {
            Serial.println("config from flash.");
            temp = pgm_read_byte_near(CustomDeviceConfig + addrMem++);
            if (addrMem > sizeof(CustomDeviceConfig))
            return false;
        } else {
            Serial.println("config not from flash.");
            temp = MFeeprom.read_byte(addrMem++);
            if (addrMem > length)
                return false;
        }
        buffer[counter++] = temp;              // save character and locate next buffer position
        if (counter >= MEMLEN_STRING_BUFFER) { // nameBuffer will be exceeded
            return false;                      // abort copying to buffer
        }
    } while (temp != '.'); // reads until limiter '.' and locates the next free buffer position
    buffer[counter - 1] = 0x00; // replace '.' by NULL, terminates the string

    // Serial.printf("GotString '%s'\n", buffer);

    return true;
}

MFCustomDevice::MFCustomDevice()
{
    _initialized = false;
}

/* **********************************************************************************
    Within the connector pins, a device name and a config string can be defined
    These informations are stored in the EEPROM or Flash like for the other devices.
    While reading the config from the EEPROM or Flash this function is called.
    It is the first function which will be called for the custom device.
    If it fits into the memory buffer, the constructor for the customer device
    will be called
********************************************************************************** */

void MFCustomDevice::attach(uint16_t adrPin, uint16_t adrType, uint16_t adrConfig, bool configFromFlash)
{
    if (adrPin == 0) return;

    /* **********************************************************************************
        Do something which is required to setup your custom device
    ********************************************************************************** */

    // char parameter[MEMLEN_STRING_BUFFER];

    /* **********************************************************************************
        Read the Type from the EEPROM or Flash, copy it into a buffer and evaluate it
        The string get's NOT stored as this would need a lot of RAM, instead a variable
        is used to store the type
    ********************************************************************************** */

    // Let's not figure out the type from config, but instead from the device.
    loadSettings(); // From G5Common

    _customType = g5Settings.deviceType;
//    Serial.printf("****In custom device attach. _customType is %d, settings type: %d and the HSI type is: %d\n", _customType, g5Settings.deviceType, CUSTOM_HSI_DEVICE);

    /*
    getStringFromMem(adrType, parameter, configFromFlash);
    if (strcmp(parameter, "CC_G5_HSI") == 0)
        _customType = CUSTOM_HSI_DEVICE;
    if (strcmp(parameter, "CC_G5_PFD") == 0)
        _customType = CUSTOM_PFD_DEVICE;
        */

    if (_customType == CUSTOM_HSI_DEVICE) {
        /* **********************************************************************************
            Check if the device fits into the device buffer
        ********************************************************************************** */
        if (!FitInMemory(sizeof(CC_G5_HSI))) {
            // Error Message to Connector
            cmdMessenger.sendCmd(kStatus, F("Custom Device does not fit in Memory"));
            return;
        }

        _hsiDevice = new (allocateMemory(sizeof(CC_G5_HSI))) CC_G5_HSI();
  //      Serial.printf("*****Attaching an HSI\n");
        _hsiDevice->attach();
        _hsiDevice->begin();
        _initialized = true;
    } else if (_customType == CUSTOM_PFD_DEVICE) {

        // Serial.printf("*****Attaching a PFD\n");
        /* **********************************************************************************
            Check if the device fits into the device buffer
        ********************************************************************************** */
        if (!FitInMemory(sizeof(CC_G5_PFD))) {
            // Error Message to Connector
            cmdMessenger.sendCmd(kStatus, F("Custom Device does not fit in Memory"));
            return;
        }

        _pfdDevice = new (allocateMemory(sizeof(CC_G5_PFD))) CC_G5_PFD();
        _pfdDevice->attach();
        // if your custom device does not need a separate begin() function, delete the following
        // or this function could be called from the custom constructor or attach() function
        //       Serial.printf("mydevice2 attached");
        _pfdDevice->begin();
        _initialized = true;
    } else if (_customType == CUSTOM_ISIS_DEVICE) {

        // Serial.printf("*****Attaching a PFD\n");
        /* **********************************************************************************
            Check if the device fits into the device buffer
        ********************************************************************************** */
        if (!FitInMemory(sizeof(CC_ISIS))) {
            // Error Message to Connector
            cmdMessenger.sendCmd(kStatus, F("Custom Device does not fit in Memory"));
            return;
        }

        _isisDevice = new (allocateMemory(sizeof(CC_ISIS))) CC_ISIS();
        _isisDevice->attach();
        // if your custom device does not need a separate begin() function, delete the following
        // or this function could be called from the custom constructor or attach() function
        //       Serial.printf("mydevice2 attached");
        _isisDevice->begin();
        _initialized = true;
    } else {
        cmdMessenger.sendCmd(kStatus, F("Custom Device is not supported by this firmware version"));
    }
}

/* **********************************************************************************
    The custom devives gets unregistered if a new config gets uploaded.
    Keep it as it is, mostly nothing must be changed.
    It gets called from CustomerDevice::Clear()
********************************************************************************** */
void MFCustomDevice::detach()
{
    _initialized = false;
    if (_customType == CUSTOM_HSI_DEVICE) {
        _hsiDevice->detach();
    } else if (_customType == CUSTOM_PFD_DEVICE) {
        _pfdDevice->detach();
    }else if (_customType == CUSTOM_ISIS_DEVICE) {
        _isisDevice->detach();
    }
}

/* **********************************************************************************
    Within in loop() the update() function is called regularly
    Within the loop() you can define a time delay where this function gets called
    or as fast as possible. See comments in loop().
    It is only needed if you have to update your custom device without getting
    new values from the connector.
    Otherwise just make a return; in the calling function.
    It gets called from CustomerDevice::update()
********************************************************************************** */
void MFCustomDevice::update()
{
    if (!_initialized) return;
    /* **********************************************************************************
        Do something if required
    ********************************************************************************** */
    if (_customType == CUSTOM_HSI_DEVICE) {
        _hsiDevice->update();
    } else if (_customType == CUSTOM_PFD_DEVICE) {
        _pfdDevice->update();
    } else if (_customType == CUSTOM_ISIS_DEVICE) {
        _isisDevice->update();
    }
}

/* **********************************************************************************
    If an output for the custom device is defined in the connector,
    this function gets called when a new value is available.
    It gets called from CustomerDevice::OnSet()

    CAC UPDATE:
    We will use this as a router. Some messages go to both devcies, some to the specific one.
    I have organized the IDs into bunches for easy routing.
********************************************************************************** */
void MFCustomDevice::set(int16_t messageID, char *setPoint)
{
    if (!_initialized) return;
    
    _isisDevice->set(messageID, setPoint);

     if (messageID < MSG_HSI_MIN) {
         // Common messages - route to active device
         if (_customType == CUSTOM_HSI_DEVICE)
             _hsiDevice->setCommon(messageID, setPoint);
         else if (_customType == CUSTOM_PFD_DEVICE)
             _pfdDevice->setCommon(messageID, setPoint);
         else if (_customType == CUSTOM_ISIS_DEVICE)
             _isisDevice->setCommon(messageID, setPoint);
     }
     else if (messageID < MSG_PFD_MIN) {
         // HSI-specific - only process in HSI mode
         if (_customType == CUSTOM_HSI_DEVICE)
             _hsiDevice->setHSI(messageID, setPoint);
     }
     else if (messageID < MSG_ISIS_MIN) {
         // PFD-specific - only process in PFD mode
         if (_customType == CUSTOM_PFD_DEVICE)
             _pfdDevice->setPFD(messageID, setPoint);
     }
     else {
        _isisDevice->set(messageID, setPoint);
     }
}
