#pragma once

// The MobiFlight core (Config.cpp) expects CustomDeviceConfig to exist when
// HAS_CONFIG_IN_FLASH is defined. We keep it as an empty stub so that
// configLengthFlash == 0 and configStoredInFlash() returns false — meaning
// the core will not try to load the full config from flash. Instead, the
// device type comes from EEPROM (set in MobiFlight Connector), and the
// firmware supplies per-type configs from the constants below.
const char CustomDeviceConfig[] PROGMEM = {};

// Pin numbers at or above this threshold are "virtual" — they exist only in
// firmware flash (see the VirtualConfig arrays below) and must never be
// written to EEPROM or passed to GPIO setup routines.  The MobiFlight
// Connector cannot delete devices whose pins are not in the board's pin
// list, so keeping them out of EEPROM is critical.
#define VIRTUAL_PIN_MIN 100

// Virtual encoder/button records appended to kGetConfig responses.
// These devices have no physical pins — Encoder::Add() and MFButton::attach()
// are short-circuited in the core to skip GPIO setup. They appear in the
// Connector so users can map sim variables/events to them by name.
//
// Wire format matches the MobiFlight config string protocol:
//   Encoder: 8.pin1.pin2.type.name:
//   Button:  1.pin.name:
//
// Pin numbers are arbitrary (chosen to not clash with real board pins).
// Unique per-type ranges: HSI=100-109, PFD=110-119, Switchable=100-119

const char HSI_VirtualConfig[] PROGMEM =
    "8.100.101.0.encHeading:"
    "8.102.103.0.encCourse:"
    "1.104.btnHsiEncoder:"
    "1.105.btnHsiPower:"
    "1.106.btnHsiDevice:";

const char PFD_VirtualConfig[] PROGMEM =
    "8.110.111.0.encKohls:"
    "8.112.113.0.encTargetAlt:"
    "8.114.115.0.encTrack:"
    "1.116.btnPfdEncoder:"
    "1.117.btnPfdPower:"
    "1.118.btnPfdDevice:";

// Switchable device exposes all inputs from both HSI and PFD.
const char Switchable_VirtualConfig[] PROGMEM =
    "8.100.101.0.encHeading:"
    "8.102.103.0.encCourse:"
    "1.104.btnHsiEncoder:"
    "1.105.btnHsiPower:"
    "1.106.btnHsiDevice:"
    "8.110.111.0.encKohls:"
    "8.112.113.0.encTargetAlt:"
    "8.114.115.0.encTrack:"
    "1.116.btnPfdEncoder:"
    "1.117.btnPfdPower:"
    "1.118.btnPfdDevice:";
