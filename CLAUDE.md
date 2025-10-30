# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the CC_G5 project - a MobiFlight custom firmware device that implements a glass cockpit G5-style attitude indicator display for flight simulation. The device combines an ESP32-S3 microcontroller with an LCD display to show compass headings, CDI (Course Deviation Indicator), glide slope information, and navigation data.

## Architecture

### Hardware Platform
- **Primary MCU**: ESP32-S3 DevKitC-1 with PSRAM
- **Display**: 4-inch LCD with LovyanGFX graphics library
- **Secondary MCU**: RP2040 for rotary encoder interface via I2C
- **Communication**: I2C between ESP32 and RP2040, Serial/USB to PC

### Core Components

#### Main Classes
- **CC_G5**: Primary display controller (`CC_G5/CC_G5.h|.cpp`) - handles all display rendering, navigation data processing, and graphics updates
- **MFCustomDevice**: MobiFlight integration layer (`CC_G5/MFCustomDevice.h|.cpp`) - manages device lifecycle and message handling
- **CC_EncoderInterface**: I2C encoder interface (`CC_G5/CCi2c.h|.cpp`) - handles rotary encoder input from RP2040

#### Display System
- Uses LovyanGFX library for high-performance graphics
- Multiple sprite layers for complex overlays:
  - Compass background with rotating tick marks
  - CDI pointer and deviation bars
  - Glide slope indicators
  - Navigation mode indicators
  - Text overlays for headings and distances

#### Navigation Modes
- **GPS Mode**: Shows GPS navigation data (magenta indicators)
- **NAV Mode**: Shows VOR/ILS navigation data (green indicators)
- **CDI Scale Types**: Multiple approach types (ENR, TERM, VNAV, LP, LOC, etc.)

## Build System

### PlatformIO Configuration
- **Primary config**: `platformio.ini` - includes core MobiFlight dependencies
- **Custom config**: `CC_G5/CC_G5_platformio.ini` - device-specific environment

### Build Commands
```bash
# Build all environments
pio run

# Build specific CC_G5 environment
pio run -e ccrawford_cc_g5_esp32s3

# Clean build
pio run -t clean

# Upload firmware
pio run -t upload
```

### Build Scripts
- `get_version.py`: Sets firmware version from environment variables
- `get_CoreFiles.py`: Downloads MobiFlight core firmware source
- `copy_fw_files.py`: Packages firmware files and creates distribution ZIP

### Dependencies
- **LovyanGFX**: High-performance graphics library for ESP32
- **MobiFlight Core**: Base firmware framework (version controlled via `custom_core_firmware_version`)
- **ESP32 Arduino Core**: Platform framework

## Development Workflow

### Message Handling
The device receives navigation data via MobiFlight's message system. Key message IDs in `CC_G5::set()`:
- Message 0: Heading angle
- Message 1: Heading bug angle  
- Message 2: CDI direction
- Message 3: CDI to/from indicator
- Message 4: CDI offset
- Message 5: Glide slope indicator
- Message 6: Ground speed
- Message 9: Navigation source (GPS=1, NAV=0)

### Display Updates
- `updateCommon()`: Main display refresh
- `updateGps()`: GPS mode specific displays
- `updateNav()`: NAV mode specific displays
- Individual draw functions for each display element

### I2C Encoder Interface
- ESP32 acts as I2C master, RP2040 as slave at address 0x08
- Interrupt-driven data reading on GPIO17
- 3-byte protocol: encoder delta, encoder button, extra button

## Key File Locations

### Source Code
- `CC_G5/CC_G5.h|.cpp`: Main device implementation
- `CC_G5/MFCustomDevice.h|.cpp`: MobiFlight integration
- `CC_G5/CCi2c.h|.cpp`: I2C encoder interface

### Configuration
- `CC_G5/4inchLCDConfig.h`: Display configuration
- `CC_G5/MFCustomDevicesConfig.h`: Device type definitions

### Assets
- `CC_G5/Sprites/`: 16-bit color sprite images
- `CC_G5/Images/`: Font and 1-bit images
- `CC_G5/Community/`: MobiFlight connector configuration files

### Scripts
- `CC_G5/Scripts/`: Python image conversion utilities

## Common Development Tasks

### Adding New Navigation Data
1. Add message ID case in `CC_G5::set()`
2. Add corresponding member variable
3. Update appropriate draw function
4. Test in both GPS and NAV modes

### Display Modifications
1. Modify draw functions in `CC_G5.cpp`
2. Update sprite definitions if needed
3. Consider both color modes (GPS=magenta, NAV=green)
4. Test rotation and positioning calculations

### I2C Communication Changes
1. Update protocol in both ESP32 (`CC_G5.cpp`) and RP2040 code
2. Modify interrupt handling if needed
3. Update `CCi2c.h|.cpp` for interface changes

## Branch Information
- Current branch: `i2c-support` - implementing I2C encoder interface
- Main development typically done on feature branches
- Many modified files indicate active development phase