#pragma once

// Define your input custom devices and uncomment -DHAS_CONFIG_IN_FLASH
// in your MFCustomDevice_platformio.ini
const char CustomDeviceConfig[] PROGMEM =
    {
        "17.CC_G5.1..CCs CC_G5:"
        "8.2.3.0.encHeading:"
        "8.4.5.0.encCourse:"
        "1.6.btnHsiEncoder:"
        "8.2.3.0.encKohls:"
        "8.4.5.0.encTargetAlt:"
        "8.6.7.0.encTrack:"
        "1.8.btnPfdEncoder:"
        "1.8.btnPfdPower:"
        "1.8.btnHsiPower:"
        "1.8.btnPfdDevice:"
        "1.8.btnHsiDevice:"};

//"17.CC_G5_AIO.1..CCs CC_G5 XXX:8.2.3.0.encHeading:8.4.5.0.encCourse:1.6.btnHsiEncoder:8.2.3.0.encKohls:8.4.5.0.encTargetAlt:8.6.7.0.encTrack:1.8.btnPfdEncoder:"
// "17.CC_G5_HSI.1..CCs CC_G5 XXX:8.2.3.0.encHeading:8.4.5.0.encCourse:1.6.btnHsiEncoder:"
// 17.CC_G5_PFD.1..CCs CC_G5 PFD:8.2.3.0.encKohls:8.4.5.0.encTargetAlt:8.6.7.0.encTrack:1.8.btnPfdEncoder:

// 17.CC_G5_PFD.1..CCs CC_G5 PFD:8.2.3.0.encKohls:8.4.5.0.encTargetAlt:8.6.7.0.encTrack:1.8.btnPfdEncoder: