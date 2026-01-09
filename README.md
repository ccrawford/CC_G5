# CC_G5 - Glass Cockpit Display for Flight Simulation

  A MobiFlight custom firmware device for ESP32 that implements a Garmin G5-style glass cockpit Primary Flight Display
  (PFD) and Horizontal Situation Indicator (HSI) for MSFS 2020 or 2024. This device provides a high-fidelity,
  physical glass cockpit display using an ESP32-S3 microcontroller with a 4-inch LCD display. Based on real
  world G5 manual, this is a completely stand alone device that only requires MobiFlight to interface with
  MSFS. The display and logic is completely contained on the ESP32.

  This code supports both HSI and PFD in a single code base. Use MF to select display configuration. Ability
  to switch between displays at run-time (like the real device) is not implemented.

  UPDATE Nov 2025: You may now select the device from the menu. There is only one custom device type (CCs CC_G5). It will
  remember which device type it is when selected from the menu, or use the MF Message "Device Type" to control it from 
  your MF config.
  NOTE: The message IDs have now changed to support the single device/dual mode setup. Your old .mcc files will not 
  work. There is a new Community file set in the Community folder and new sample MF .mcc files in the MF folder. 
  Use G5 ROOT CONFIG.mcc for a single device or G5 AIO.mcc for duals. The create_dual_g5_config.bat can be used to 
  duplicate the G5 ROOT CONFIG.mcc for dual device. However you will need to update with the SN of your devices. 

  Both displays get around 16fps. I initially thought this was unacceptably slow, but in reality it's fine.
  Smoothing of input values and sub-pixel rendering allows smooth, realistic movement of dials and tapes. 

  This project is pushing the bounds of the memory on the ESP32-S3. While the device has ample PSRAM, using it
  is much slower than main memory and the fps drops quickly. Currently we are out of room for new sprites. 

  <img src="CC_G5/Photos/Turning to intercept Localizer.jpg" width="400" alt="CC_G5 intercepting localizer">
  <br>
  <strong>PFD (top) HSI (bottom) as the C172 intercepts the Localizer on an ILS approach. <BR> More photos and video below!</strong>

  ## Features

  ### Primary Flight Display (PFD)
  - **Attitude Indicator**: Real-time pitch and bank display with artificial horizon
  - **Flight Director**: FD command bars showing autopilot guidance
  - **Airspeed Indicator**: Tape-style airspeed display with V-speed markers (Vr, Vx, Vy, Vg, Vno, Vne)
  - **Altitude Indicator**: Tape-style altitude display with autopilot target bug
  - **Vertical Speed Indicator**: Real-time VSI 
  - **Slip/Skid Indicator**: Turn coordination ball with turn rate indicator
  - **CDI (Course Deviation Indicator)**: Horizontal and vertical deviation bars
  - **Glide Slope Indicator**: ILS/GPS approach guidance
  - **Autopilot Status**: Visual indication of active and armed autopilot modes and parameters
  - **V-Speed Configuration**: Customizable V-speeds via on-screen settings menu
  - **On Screen Message Indicator** Identifies if connection to MF is lost
  - **Encoder click knob adjusts baro by default or click to enter menu and adjust other parameters
  - **TO DO**
    - Target AP VS bug on VS scale.
    - Target AP speed for FLC on speed tape
    - ~~Improve blinking of AP and Altitude on status change~~ Done.
    - ~~Add battery status and logic~~Done.
    - ~~Add startup/shutdown sequences~~Done.
    - ~~Add "Out of range" chevrons that point to horizon in unusual attitudes~~ Done. Also fixed inverted flight displays
    - ~~Swap between HSI and PFD at runtime~~Done.

  ### Horizontal Situation Indicator (HSI)
  - **Compass Rose**: 360° rotating compass with true heading display
  - **Heading Bug**: User-adjustable heading target with indicator
  - **Course Deviation Indicator**: Full CDI with to/from indicator
  - **Glide Slope Display**: Vertical approach guidance
  - **Bearing Pointers**: Dual bearing pointers supporting:
    - GPS waypoint bearing
    - VOR1/VOR2 radials
    - ADF bearing
    - Configurable via internal menu system
  - **Navigation Source Display**: GPS/NAV1/NAV2 indicators with color coding
  - **Ground Speed & Track**: Current groundspeed and GPS track
  - **Distance to Waypoint**: Nautical miles to next GPS waypoint
  - **Wind Display**: Wind speed and direction arrow
  - **Approach Indicators**: ILS/LOC/GPS approach type display
  - **OBS Mode**: Manual course selection support
  - **TO DO**
    - Add battery status/logic
    - Add startup/shutdown sequences

  ### Menu System
  - **On-Screen Interactive Menu**: Rotary encoder navigation
  - **Adjustable Parameters**:
    - Heading bug
    - Course/OBS selector
    - Bearing pointer 1 & 2 sources
    - V-speed settings (Vs0, Vs1, Vr, Vx, Vy, Vg, Vfe, Vno, Vne)
    - HSI or MFD display
  - **Virtual MobiFlight Encoders and Buttons**
    - Allows dynamic use of the rotary encoder without messy state management in MF
  
  ### Display Features
  - **High-Performance Graphics**: LovyanGFX library, 480x480 display.
  - **Accurate colors**: Magenta for GPS, Green for VOR/ILS
  - **Multi-Layer Sprites**: Complex overlays for no-flicker refresh
  - **Smooth Animations**: Low-pass filtering for heading and deviation displays

## More Photos
<p align="center">
    <img src="CC_G5/Photos/Established on Localizer.jpg" width="400" alt="HSI Display">
    <br>
    <em>Established on the Localizer</em>
  </p>
  <p align="center">
    <img src="CC_G5/Photos/RNAV approach.jpg" width="400" alt="PFD Display">
    <br>
    <em>RNAV approach</em>
  </p>
  <p align="center">
    <img src="CC_G5/Photos/PFD Menu.jpg" width="400" alt="PFD Display">
    <br>
    <em>PFD Menu</em>
  </p>
    <p align="center">
    <img src="CC_G5/Photos/HSI Menu.jpg" width="400" alt="PFD Display">
    <br>
    <em>HSI menu</em>
  </p>
 <p align="center">
    <a href="https://www.youtube.com/watch?v=kaV-mxJTDQc">
      <img src="https://img.shields.io/badge/▶-Watch%20Video-red?style=for-the-badge&logo=youtube" alt="Watch
  Video">
    </a>
    <br><br>
    <a href="https://www.youtube.com/watch?v=kaV-mxJTDQc">
      <img src="https://img.youtube.com/vi/kaV-mxJTDQc/maxresdefault.jpg" width="400" alt="CC_G5 demonstration">
    </a>
  </p>

  ## Hardware Requirements

  ### Primary Components
  - **ESP32-S3 DevKitC-1** with PSRAM (4MB or 8MB recommended)
  - **4-inch LCD Display** (480x480 or compatible resolution)
    - Guition ESP32-S3 4827S043C (recommended--built in ESP32-S3 with clean wiring)
    - Search AliExpress for "guition 480x480". Should be around $25 US.
    - Alternative: Standard 4-inch SPI LCD with proper pin configuration
  - **RP2040 Microcontroller** (Raspberry Pi Pico or similar) - for rotary encoder interface
    - See this repository for more information https://github.com/ccrawford/CC_G5_Slave
    - This can be skipped if you are using the Guition screen and don't need the power button
    - LED and power button are operational ~~but startup/shutown logic and battery are not implemented~~  
  - **Rotary Encoder** with push button. Whatever EC11 variant you have will work.
  - **Ring-lit push button** I'm using a 9mm ring-lit white led momentary push button. 

  ### Connections

  #### ESP32-S3 to LCD
  The LCD connection depends on your display module. For the Guition screen, SPI and control pins are
  pre-configured in `4inchLCDConfig_Guition.h` and `4inchLCDConfig.h`

  #### ESP32-S3 to RP2040 (I2C)
  - **Guition Screen Configuration** This is more fully described in the https://github.com/ccrawford/CC_G5_Slave repo
    - SDA: GPIO1
    - SCL: GPIO2
    - INT: GPIO40 (interrupt from RP2040) 
    - GND
  - **Standard Configuration**:
    - SDA: GPIO15
    - SCL: GPIO7
    - INT: GPIO16 (interrupt from RP2040)
    - GND

  #### RP2040 Setup
  - RP2040 code is at https://github.com/ccrawford/CC_G5_Slave
  - Pins are in that code.
  - Rotary encoder, and LED+Button connects to RP2040
  - RP2040 acts as I2C slave at address `0x08`
  - Sends 3-byte protocol: encoder delta, encoder button, extra button
  - Receives LED status indication code.

  ### Power Requirements
  - 5V USB power sufficient for most configurations. USE A POWERED USB HUB.
  - LCD backlight may require up to 300mA at full brightness

  ## Software Requirements

  ### Development Environment
  - **VSCode** with PlatformIO extension
  - **Git** (for cloning repository)
  - **Python 3.x** (for build scripts)

  ### MobiFlight Connector
  - **MobiFlight Connector** v10.x or later
  - Custom community device configuration (see Installation)


  
  ## Building the Firmware

  Display Configuration

  - CC_G5/4inchLCDConfig_Guition.h - Guition screen settings
  - CC_G5/4inchLCDConfig.h - Standard LCD settings
  - CC_G5/G5Common.h - Common definitions and settings structure

  Build Configuration

  - platformio.ini - Core MobiFlight configuration
  - CC_G5/CC_G5_platformio.ini - CC_G5 specific environment
  - get_version.py - Version management script
  - copy_fw_files.py - Distribution package creation

  Helper Scripts (in Scripts directory)
  - deploy.py to copy the board and device files to your MF Community directory
  - various .png -> .h helper scripts to create sprite bitmap header files


  Project Structure

  CC_G5/
  ├── CC_G5/                      # Main device code
  │   ├── CC_G5.h/cpp            # HSI implementation
  │   ├── CC_G5_PFD.h/cpp        # PFD implementation
  │   ├── MFCustomDevice.h/cpp   # MobiFlight integration
  │   ├── G5Common.h             # Shared definitions and menu system
  │   ├── CCi2c.h/cpp            # I2C encoder interface
  │   ├── Community/             # MobiFlight connector files
  │   │   ├── boards/           # Board definitions
  │   │   └── devices/          # Device message definitions
  │   ├── Images/               # Font and bitmap resources
  │   └── Sprites/              # 16-bit color sprite images
  ├── platformio.ini            # Build configuration
  └── _build/                   # Generated firmware and packages

  Adding New Features

  1. Modify device classes in CC_G5/CC_G5.cpp or CC_G5_PFD.cpp
  2. Add message handlers in set() method
  3. Update corresponding .device.json with new message definitions
  4. Test with PlatformIO
  5. Generate distribution package

  Links

  - GitHub Repository: https://github.com/ccrawford/CC_G5
  - MobiFlight: https://www.mobiflight.com





