#include "G5Common.h"

// Hardware interface instance
G5_Hardware g5Hardware;

LGFX lcd = LGFX();

// Used in both:
LGFX_Sprite headingBox(&lcd);
LGFX_Sprite gsBox(&lcd);

// Send a pseudo encoder event to MF.
void sendEncoder(String name, int count, bool increase)
{
    // cmdMessenger.sendCmdStart(kButtonChange);
    for (int i = 0; i < count; i++) {
        cmdMessenger.sendCmdStart(kEncoderChange);
        cmdMessenger.sendCmdArg(name);
        cmdMessenger.sendCmdArg(increase ? 0 : 2);
        cmdMessenger.sendCmdEnd();
    }
}

// G5_Hardware class implementation
bool G5_Hardware::readEncoderData(int8_t& outDelta, int8_t& outEncButton, int8_t& outExtraButton)
{
    if (!dataAvailable) {
        return false;
    }

    dataAvailable = false;

    size_t retSize = Wire.requestFrom(RP2040_ADDR, 3); // Request 3 bytes

    if (Wire.available() >= 3) {
        outDelta = (int8_t)Wire.read();
        outEncButton = (int8_t)Wire.read();
        outExtraButton = (int8_t)Wire.read();

        // Update internal state
        encoderValue += outDelta;
        encoderButton = outEncButton;
        extraButton = outExtraButton;

        return true;
    }

    return false;
}

void G5_Hardware::setLedState(bool state)
{
    Wire.beginTransmission(RP2040_ADDR);
    Wire.write(0x01); // LED control command
    Wire.write(state ? 1 : 0);
    Wire.endTransmission();
}

// This function is an exponential decay that allows changing values with a smooth motion
// Desired response time	α (approx)	Feel (at 20hz)
// 0.2 s (snappy pointer)	0.75	very responsive
// 0.5 s (analog needle)	0.3	smooth, analog feel
// 1.0 s (heavy needle)	0.15	sluggish but realistic
// 2.0 s (slow gauge)	0.075	big thermal or pressure gauge feel
// The snapThreashold is the same unit/magnitude as the input. At a minimum it should be the smallest changable value.
int smoothInput(int input, int current, float alpha, int threashold = 1)
{
    int diff = input - current;
    if (abs(diff) <= threashold) return input;

    int update = (int)(alpha * diff);
    if (update == 0 && abs(diff) > threashold) update = (diff > 0) ? 1 : -1;
    return current + update;
}

float smoothInput(float input, float current, float alpha, float snapThreshold = 0.5f)
{
    float diff   = input - current;
    float update = alpha * diff;

    // When update becomes smaller than snapThreshold * diff, snap to target
    // if (abs(update) < abs(diff) * snapThreshold) {
    if (fabs(update) < snapThreshold) {
        return input;
    }
    return current + update;
}

// Smooth a floating point directional value
float smoothDirection(float input, float current, float alpha, float threshold = 0.5f)
{
    // Handle angle wrapping (crossing 0°/360°)
    float diff = input - current;
    if (diff > 180.0f) diff -= 360.0f;
    if (diff < -180.0f) diff += 360.0f;

    // Snap to target when close enough
    if (abs(diff) < threshold) {
        return input;
    }

    float result = current + alpha * diff;

    // Normalize to 0-360
    if (result < 0.0f) result += 360.0f;
    if (result >= 360.0f) result -= 360.0f;

    return result;
}

CC_G5_Settings g5Settings;

bool loadSettings()
{
    if (MFeeprom.read_block(CC_G5_SETTINGS_OFFSET, g5Settings)) {
        if (g5Settings.version != 1) {
            g5Settings = CC_G5_Settings(); // Reset to defaults
        }
        return true;
    } else {
        // EEPROM read failed, defaults already set
        return false;
    }
}

bool saveSettings()
{
    bool retval;

    retval = MFeeprom.write_block(CC_G5_SETTINGS_OFFSET, g5Settings);
    if (retval) MFeeprom.commit();
    return retval;
}