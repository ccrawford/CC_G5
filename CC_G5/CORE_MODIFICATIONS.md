# Required MobiFlight Core Modifications

The CC_G5 firmware requires changes to the MobiFlight core source files
in `src/src/`. These files are downloaded by the build system — make the changes
after running the initial build (or after `get_CoreFiles.py` runs) and before
flashing.

All modifications are guarded by `#ifdef HAS_CONFIG_IN_FLASH` so they compile
out for vanilla MobiFlight builds. Without these changes, the CC_G5 display
still works — you just lose virtual button/encoder support in the Connector.

Each modification is bracketed with comments:
```
// ---- BEGIN CC_G5 MODIFICATION: <description> ----
...
// ---- END CC_G5 MODIFICATION ----
```

---

## 1. `src/src/MF_Button/MFButton.cpp` — Virtual pin guard

Prevents GPIO setup for virtual buttons (pin >= 100) while allowing real
buttons to work normally.

**Find** the top of the file after `#include "MFButton.h"` and **add:**

```cpp
// ---- BEGIN CC_G5 MODIFICATION: virtual pin guard ---------------------------
#ifdef HAS_CONFIG_IN_FLASH
#include "MFCustomDevicesConfig.h" // for VIRTUAL_PIN_MIN
#endif
// ---- END CC_G5 MODIFICATION ------------------------------------------------
```

**Find** the `MFButton::attach()` function and **add the pin guard** as the
first lines of the function body:

```cpp
void MFButton::attach(uint8_t pin, const char *name)
{
// ---- BEGIN CC_G5 MODIFICATION: virtual pin guard ---------------------------
#ifdef HAS_CONFIG_IN_FLASH
    if (pin >= VIRTUAL_PIN_MIN)
        return;
#endif
// ---- END CC_G5 MODIFICATION ------------------------------------------------
    _pin  = pin;
    ...
```

> **Why:** Virtual buttons use pin numbers >= 100 that don't correspond to
> real GPIO. Without this guard, `pinMode()` would be called on those pins,
> causing crashes or incorrect behaviour on ESP32-S3. The guard is
> selective — real buttons (pin < 100) still work.

---

## 2. `src/src/MF_Encoder/Encoder.cpp` — Virtual pin guard

Same pattern as MFButton — prevents GPIO setup for virtual encoders.

**Find** the includes at the top and **add after** `#include "Encoder.h"`:

```cpp
// ---- BEGIN CC_G5 MODIFICATION: virtual pin guard ---------------------------
#ifdef HAS_CONFIG_IN_FLASH
#include "MFCustomDevicesConfig.h" // for VIRTUAL_PIN_MIN
#endif
// ---- END CC_G5 MODIFICATION ------------------------------------------------
```

**Find** the `Encoder::Add()` function and **add the pin guard** as the
first lines of the function body (before the `maxEncoders` check):

```cpp
    void Add(uint8_t pin1, uint8_t pin2, uint8_t encoder_type, char const *name)
    {
// ---- BEGIN CC_G5 MODIFICATION: virtual pin guard ---------------------------
#ifdef HAS_CONFIG_IN_FLASH
        if (pin1 >= VIRTUAL_PIN_MIN)
            return;
#endif
// ---- END CC_G5 MODIFICATION ------------------------------------------------
        if (encodersRegistered == maxEncoders)
            return;
        ...
```

> **Why:** Virtual encoders are driven by I2C from the RP2040 co-processor.
> The MobiFlight encoder polling loop must not run on virtual pin numbers.

---

## 3. `src/src/Config.cpp` — Two modifications

### 3a. Forward declaration and virtual device injection in `OnGetConfig()`

(This modification was already present in previous versions.)

**Find** the `HAS_CONFIG_IN_FLASH` include block (near the top of `Config.cpp`):

```cpp
#ifdef HAS_CONFIG_IN_FLASH
#include "MFCustomDevicesConfig.h"
#else
const char CustomDeviceConfig[] PROGMEM = {};
#endif
```

**Replace it with:**

```cpp
#ifdef HAS_CONFIG_IN_FLASH
#include "MFCustomDevicesConfig.h"
// Implemented in MFCustomDevice.cpp — scans the EEPROM config from configStart,
// finds any '17.' custom device record, and streams the matching virtual
// encoder/button records into the kGetConfig response. Appends nothing if no
// custom device record is present.
void appendVirtualDeviceConfig(uint16_t configStart);
#else
const char CustomDeviceConfig[] PROGMEM = {};
inline void appendVirtualDeviceConfig(uint16_t) {}
#endif
```

**Find** the `OnGetConfig()` function's EEPROM streaming block and **add** the
`appendVirtualDeviceConfig()` call after the EEPROM loop:

```cpp
    if (configStoredInEEPROM()) {
        cmdMessenger.sendCmdArg((char)MFeeprom.read_byte(MEM_OFFSET_CONFIG));
        for (uint16_t i = 1; i < configLengthEEPROM; i++) {
            cmdMessenger.sendArg((char)MFeeprom.read_byte(MEM_OFFSET_CONFIG + i));
        }
        // Append virtual encoder/button records from firmware flash, based on device type.
        // Scans the config for a '17.' custom device record and appends the matching
        // virtual config. Appends nothing if no custom device record is present.
        appendVirtualDeviceConfig(MEM_OFFSET_CONFIG);
    } else if (configStoredInFlash()) {
```

> **Why:** Virtual encoder/button records live in firmware flash, not EEPROM.
> They are injected into the kGetConfig response so the Connector can see and
> map them. If the user removes the custom device, `appendVirtualDeviceConfig()`
> finds no `17.` record and appends nothing.

### 3b. Filter virtual device records in `OnSetConfig()`

When the Connector saves a config, it sends back everything it received from
`kGetConfig` — including the virtual records that were injected by
`appendVirtualDeviceConfig()`. These must NOT be written to EEPROM because the
Connector cannot delete devices whose pins aren't in the board's pin list.

**Add** the `isVirtualDeviceRecord()` helper immediately before `OnSetConfig()`:

```cpp
// ---- BEGIN CC_G5 MODIFICATION: virtual pin filtering in OnSetConfig --------
#ifdef HAS_CONFIG_IN_FLASH
static bool isVirtualDeviceRecord(const char *rec, const char *recEnd)
{
    const char *dot1 = (const char *)memchr(rec, '.', recEnd - rec);
    if (!dot1 || dot1 + 1 >= recEnd)
        return false;
    int firstPin = atoi(dot1 + 1);
    return firstPin >= VIRTUAL_PIN_MIN;
}
#endif
// ---- END CC_G5 MODIFICATION ------------------------------------------------
```

**Find** the EEPROM write block inside `OnSetConfig()`:

```cpp
        bool maxConfigLengthNotExceeded = configLengthEEPROM + cfgLen + 1 < MEM_LEN_CONFIG;
        if (maxConfigLengthNotExceeded) {
            MFeeprom.write_block(MEM_OFFSET_CONFIG + configLengthEEPROM, cfg, cfgLen + 1);
            configLengthEEPROM += cfgLen;
            cmdMessenger.sendCmd(kStatus, configLengthEEPROM);
        } else {
            cmdMessenger.sendCmd(kStatus, -1);
        }
```

**Wrap it** with the filtering code using `#ifdef HAS_CONFIG_IN_FLASH` / `#else`:

```cpp
// ---- BEGIN CC_G5 MODIFICATION: filter out virtual device records -----------
#ifdef HAS_CONFIG_IN_FLASH
        char *p = cfg;
        while (*p) {
            char *recEnd = strchr(p, ':');
            if (!recEnd) break;

            if (!isVirtualDeviceRecord(p, recEnd)) {
                uint8_t recLen = (recEnd - p) + 1;
                bool maxConfigLengthNotExceeded = configLengthEEPROM + recLen + 1 < MEM_LEN_CONFIG;
                if (maxConfigLengthNotExceeded) {
                    MFeeprom.write_block(MEM_OFFSET_CONFIG + configLengthEEPROM, p, recLen);
                    configLengthEEPROM += recLen;
                } else {
                    cmdMessenger.sendCmd(kStatus, -1);
                    return;
                }
            }
            p = recEnd + 1;
        }
        MFeeprom.write_block(MEM_OFFSET_CONFIG + configLengthEEPROM, (char *)"\0", 1);
        cmdMessenger.sendCmd(kStatus, configLengthEEPROM);
#else
// ---- END CC_G5 MODIFICATION ------------------------------------------------
        bool maxConfigLengthNotExceeded = configLengthEEPROM + cfgLen + 1 < MEM_LEN_CONFIG;
        if (maxConfigLengthNotExceeded) {
            MFeeprom.write_block(MEM_OFFSET_CONFIG + configLengthEEPROM, cfg, cfgLen + 1);
            configLengthEEPROM += cfgLen;
            cmdMessenger.sendCmd(kStatus, configLengthEEPROM);
        } else {
            cmdMessenger.sendCmd(kStatus, -1);
        }
#endif
```

> **Why:** Without this filter, virtual device records accumulate in EEPROM.
> Once there, the Connector cannot delete them (pins >= 100 are not in the
> board's pin list). The only recovery was to erase the ESP32 and reflash.
> This filter silently drops virtual records so only real devices are persisted.
