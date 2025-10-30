# Template Changes from MobiFlight CommunityTemplate

This document describes the modifications made to the original [MobiFlight CommunityTemplate](https://github.com/MobiFlight/CommunityTemplate) to support ESP32-S3 hardware and the CC_G5 project requirements.

## Context

The CC_G5 project started from a fork chain:
- **Original**: MobiFlight/CommunityTemplate (supports Arduino Mega, Raspberry Pi Pico)
- **ESP32 Fork**: Added ESP32-S3 support (source: ESP32_support branch)
- **CC_G5**: This project - specialized G5 glass cockpit implementation

ESP32 support in the base CommunityTemplate is incomplete/experimental. Key features like firmware upload via MobiFlight Connector do not work with ESP32 boards.

## Major Template Modifications

### 1. ESP32-S3 Platform Support

**Files Modified:**
- `platformio.ini`
- `CC_G5/CC_G5_platformio.ini`
- `get_CoreFiles.py`

**Changes:**
- Added `espressif32` platform support
- Added ESP32-S3 DevKitC-1 board configuration
- Added ESP32-specific library dependencies:
  ```ini
  custom_lib_deps_esp32 =
      ricaun/ArduinoUniqueID @ ^1.3.0
      madhephaestus/ESP32Servo
  ```
- Added PSRAM support with `-DBOARD_HAS_PSRAM`
- Custom memory buffer sizes optimized for ESP32:
  ```ini
  -DMEMLEN_CONFIG=1496
  -DMEMLEN_NAMES_BUFFER=1000
  -DMF_MAX_DEVICEMEM=1600
  ```

### 2. Second Core Processing (ESP32 Dual-Core)

**Files Modified:**
- `MFCustomDevice.cpp`
- `CC_G5_platformio.ini`

**Changes:**
- Added optional `-DUSE_2ND_CORE` build flag
- Enables running custom device update loop on second ESP32 core
- **Currently disabled** for CC_G5 due to framerate issues with graphics
- Code exists but is commented out:
  ```cpp
  ;-DUSE_2ND_CORE  ; Using second core routines kills the framerate. Don't use.
  ```

### 3. Configuration Storage Options

**Files Modified:**
- `MFCustomDevice.cpp`
- `MFCustomDevice.h`

**Changes:**
- Added support for reading config from Flash memory (not just EEPROM)
- Added conditional compilation with `HAS_CONFIG_IN_FLASH`
- Modified `getStringFromMem()` to support both EEPROM and Flash sources
- Useful for ESP32 where EEPROM is emulated in Flash

### 4. Board Definition for ESP32-S3

**Files Added:**
- `CC_G5/Community/boards/CC_G5_ESP32_S3.board.json`

**Key Differences from Template:**
- Hardware IDs for ESP32-S3 USB serial:
  ```json
  "HardwareIds": [
    "^VID_303A&PID_1001",
    "^VID_1A86&PID_7523"
  ]
  ```
- Firmware extension changed from `.hex` to `.bin`
- Different connection timing for ESP32:
  ```json
  "ConnectionDelay": 1250,
  "DelayAfterFirmwareUpdate": 1250
  ```
- **Firmware upload disabled** in board.json:
  ```json
  "CanInstallFirmware": false,
  "CanResetBoard": false
  ```
  *(This is a known limitation - manual upload via PlatformIO required)*
- Custom device limit = 1 (only one custom device per board)
- Different pin configuration (ESP32 GPIO pins vs Arduino digital pins)

### 5. Build Scripts Modifications

**Files Modified:**
- `get_CoreFiles.py`
- `copy_fw_files.py`
- `get_version.py`

**Changes:**
- `get_CoreFiles.py`: Added logic to handle ESP32 branch checkout
  ```python
  custom_core_firmware_version = ESP32_support  # Branch instead of version tag
  ```
- `copy_fw_files.py`: Added `.bin` file support for ESP32 firmware
- Build process now generates `.bin` files instead of/in addition to `.hex`

### 6. USB Serial Configuration

**Files Modified:**
- `CC_G5_platformio.ini`

**Changes:**
- Added USB mode flags (conditionally enabled):
  ```ini
  ;-DARDUINO_USB_MODE=1          ; Comment out for Guition screen
  ;-DARDUINO_USB_CDC_ON_BOOT=1   ; Comment out for Guition screen
  ```
- These control whether ESP32-S3 uses native USB or UART-to-USB chip
- Depends on specific ESP32-S3 dev board variant

### 7. Custom Device Features Disabled

**Changes in `CC_G5_platformio.ini`:**

Disabled most standard MobiFlight features to save memory for graphics:
```ini
-DMF_STEPPER_SUPPORT=0
-DMF_SERVO_SUPPORT=0
-DMF_LCD_SUPPORT=0
-DMF_ANALOG_SUPPORT=0
-DMF_DIGIN_MUX_SUPPORT=0
-DMF_INPUT_SHIFTER_SUPPORT=0
-DMF_OUTPUT_SHIFTER_SUPPORT=0
-DMF_SEGMENT_SUPPORT=0
-DMF_MUX_SUPPORT=0
```

This is **CC_G5 specific** - ESP32 template could support these features.

### 8. Additional Library Support

**Files Modified:**
- `CC_G5_platformio.ini`

**CC_G5-Specific Libraries Added:**
- `lovyan03/LovyanGFX@^1.2.7` - High-performance graphics library

This is not a template change but shows how to add custom libraries.

## Known Limitations & Workarounds

### Firmware Upload Not Supported via Connector
**Issue**: `"CanInstallFirmware": false` in board.json

**Workaround**:
- Use PlatformIO to upload: `pio run -t upload`
- Or use esptool.py directly
- Or use web-based ESP32 flasher

**Root Cause**: MobiFlight Connector expects Arduino-style bootloader, ESP32 uses different upload protocol

### EEPROM Emulation
**Issue**: ESP32 doesn't have true EEPROM, uses Flash emulation

**Workaround**:
- Template already handles this in MobiFlight core
- Keep config sizes reasonable (1496 bytes max)
- Consider Flash config option for larger configs

### Serial Monitor Issues
**Issue**: ESP32 serial can be unreliable during heavy graphics operations

**Workaround**:
- Use `-DDEBUG2CMDMESSENGER` to route debug to command messenger
- Or reduce serial baud rate
- Or disable debug output in production

## Files Safe to Delete

These files are template-related and not needed for CC_G5:

- `Template/` folder (if present) - original template example code
- `.github/` workflows - if not using CI/CD
- `Community/firmware/reset.*.hex` files for Arduino boards
- `Community/boards/mobiflight_template_*.board.json` - template examples

## Divergence from Upstream

**Important**: These changes make CC_G5 **incompatible** with merging updates from CommunityTemplate upstream without manual conflict resolution.

### Files That Will Conflict:
- `platformio.ini` - heavily modified for ESP32
- `get_CoreFiles.py` - uses ESP32_support branch
- `copy_fw_files.py` - handles .bin files
- `MFCustomDevice.cpp` - dual-core and Flash config additions

### Safe to Merge:
- Documentation updates
- GitHub Actions improvements (if you use them)
- Bug fixes to core MobiFlight code (may need manual application)

## Recommendations for Future

1. **Stay Independent**: Continue as standalone project, don't try to merge upstream
2. **Monitor CommunityTemplate**: Watch for ESP32 support improvements in main template
3. **Document Changes**: Keep this file updated if you modify template-level code
4. **Consider Upstreaming**: If ESP32 support matures, your changes could help upstream

## References

- Original Template: https://github.com/MobiFlight/CommunityTemplate
- ESP32 Support Discussion: *(Add link to MobiFlight forums if available)*
- ESP32-S3 TRM: https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf
- PlatformIO ESP32: https://docs.platformio.org/en/latest/platforms/espressif32.html

---

**Last Updated**: 2025-01-XX
**Template Base Version**: Unknown (pre-ESP32 support)
**ESP32 Fork Author**: elral (based on git history)
**CC_G5 Modifications**: CCrawford
