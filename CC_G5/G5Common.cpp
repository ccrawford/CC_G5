#include "G5Common.h"

// Hardware interface instance
G5_Hardware g5Hardware;

LGFX lcd = LGFX();

// Used in both:
LGFX_Sprite headingBox(&lcd);
LGFX_Sprite gsBox(&lcd);

BrightnessMenu brightnessMenu;

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

// Send a psedo button press event to MF.
void sendButton(String name, int pressType = 0)
{
    cmdMessenger.sendCmdStart(kButtonChange);
    cmdMessenger.sendCmdArg(name);
    cmdMessenger.sendCmdArg(pressType);
    cmdMessenger.sendCmdEnd();
}

// Set a new global power state. Returns false if it was already set, true if there was a change.

bool powerStateSet(PowerState ps)
{
    static PowerState lastPs = PowerState::INVALID;

    // Serial.printf("Set state: %d Control state: %d lastPs %d\n", (int)ps, (int)g5Settings.powerControl, lastPs);

    // No change? Short circuit.
    if (ps == lastPs) return false;

    if (g5Settings.powerControl == PowerControl::ALWAYS_ON) {
        ps = PowerState::POWER_ON;
    }

    // // if the lastPs was shutting down, we need to redraw the HSI.
    // if (lastPs == PowerState::SHUTTING_DOWN) {
    //     Serial.printf("refresh screen\n");
    //     lcd.fillScreen(TFT_BLACK);
    //     g5State.forceRedraw = true;
    // }

    if (ps == PowerState::POWER_OFF && g5Settings.powerControl != PowerControl::ALWAYS_ON) {
        // turn off the display.
        lcd.setBrightness(0);
    }

    if (ps == PowerState::POWER_ON) {
        // turn the display back on
        lcd.setBrightness(brightnessGamma(g5State.lcdBrightness));

//        Serial.printf("refresh screen 2\n");
        lcd.fillScreen(TFT_BLACK);
        g5State.forceRedraw = true;
    }

    // Start the shutdown timer if we're starting a shtudown.
    if (ps == PowerState::SHUTTING_DOWN) {
        g5State.shutdownStartMs = millis();
    }

    if (ps == PowerState::BATTERY_POWERED) {
        g5State.batteryStartMs = millis();
    }

    // default is just to set the new global state and move on.
    g5State.powerState = ps;
    lastPs             = ps;
    return true;
}

// G5_Hardware class implementation
bool G5_Hardware::readEncoderData(int8_t &outDelta, int8_t &outEncButton, int8_t &outExtraButton)
{
    if (!dataAvailable) {
        return false;
    }

    dataAvailable = false;

    size_t retSize = Wire.requestFrom(RP2040_ADDR, 3); // Request 3 bytes

    if (Wire.available() >= 3) {
        outDelta       = (int8_t)Wire.read();
        outEncButton   = (int8_t)Wire.read();
        outExtraButton = (int8_t)Wire.read();

        // Update internal state
        encoderValue += outDelta;
        encoderButton = outEncButton;
        extraButton   = outExtraButton;

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
// The snapThreshold is the same unit/magnitude as the input. At a minimum it should be the smallest changable value.
int smoothInput(int input, int current, float alpha, int threshold = 1)
{
    int diff = input - current;
    if (abs(diff) <= threshold) return input;

    int update = (int)(alpha * diff);
    if (update == 0 && abs(diff) > threshold) update = (diff > 0) ? 1 : -1;
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

float smoothAngle(float input, float current, float alpha, float threshold = 0.5f)
{
    // Handle angle wrapping for -180 to +180 range (for bank angle, etc.)
    float diff = input - current;
    if (diff > 180.0f) diff -= 360.0f;
    if (diff < -180.0f) diff += 360.0f;

    // Snap to target when close enough
    if (abs(diff) < threshold) {
        return input;
    }

    float result = current + alpha * diff;

    // Normalize to -180 to +180
    while (result > 180.0f)
        result -= 360.0f;
    while (result < -180.0f)
        result += 360.0f;

    return result;
}

CC_G5_Settings g5Settings;
G5State        g5State;

LGFX_Sprite batterySprite;

void drawBattery(LGFX_Sprite *targetSprite, int x, int y)
{

    if (g5State.powerState != PowerState::BATTERY_POWERED ) return;
    // const long int batLifeSec = 3 * 60 * 60;  // 3 hours
    const long int batLifeSec = 3 * 60 * 60; // 3 hours
    long int       secOnBat   = (int)(millis() - g5State.batteryStartMs) / 1000;

    long int batPct = (int)(100 * (batLifeSec - secOnBat)) / batLifeSec;

    if (batterySprite.bufferLength() == 0) {
        batterySprite.setColorDepth(8);
        batterySprite.createSprite(100, 40);
        batterySprite.loadFont(PrimaSans32);
        batterySprite.setTextSize(0.5);
        batterySprite.setTextDatum(CL_DATUM);
        batterySprite.setTextColor(TFT_WHITE);
    }

    batterySprite.fillSprite(TFT_BLACK);

    if (batPct <= 0) return;

    batterySprite.drawRect(0, 0, batterySprite.width(), batterySprite.height(), TFT_LIGHTGRAY);
    batterySprite.drawRect(1, 1, batterySprite.width() - 2, batterySprite.height() - 2, TFT_LIGHTGRAY);


    char buf[7];
    sprintf(buf, "%d%%", batPct);
    batterySprite.drawString(buf, 5, 20);

    int bX = 55, bY = 10;
    batterySprite.drawBitmap(bX, bY, BATTERY_IMG_DATA, BATTERY_IMG_WIDTH, BATTERY_IMG_HEIGHT, TFT_LIGHTGRAY);

    if (batPct > 85) {
        batterySprite.fillRect(bX + 3, bY + 3, 6, 10, TFT_GREEN);
        batterySprite.fillRect(bX + 10, bY + 3, 6, 10, TFT_GREEN);
        batterySprite.fillRect(bX + 18, bY + 3, 6, 10, TFT_GREEN);
        batterySprite.fillRect(bX + 26, bY + 3, 6, 10, TFT_GREEN);
    } else if (batPct > 70) {
        batterySprite.fillRect(bX + 3, bY + 3, 6, 10, TFT_GREEN);
        batterySprite.fillRect(bX + 10, bY + 3, 6, 10, TFT_GREEN);
        batterySprite.fillRect(bX + 18, bY + 3, 6, 10, TFT_GREEN);
    } else if (batPct > 45) {
        batterySprite.fillRect(bX + 3, bY + 3, 6, 10, TFT_GREEN);
        batterySprite.fillRect(bX + 10, bY + 3, 6, 10, TFT_GREEN);
    } else if (batPct > 25) {
        batterySprite.fillRect(bX + 3, bY + 3, 6, 10, TFT_YELLOW);
        batterySprite.fillRect(bX + 10, bY + 3, 6, 10, TFT_YELLOW);
    } else {

        batterySprite.fillRect(bX + 3, bY + 3, 6, 10, TFT_RED);
    }

    batterySprite.pushSprite(targetSprite, x, y);
    if (batPct <= 0) powerStateSet(PowerState::POWER_OFF);
}

void drawShutdown(LGFX_Sprite *targetSprite)
{
    // This code only fires when we are in the process of shutting down.
    if (g5State.powerState != PowerState::SHUTTING_DOWN || g5Settings.powerControl == PowerControl::ALWAYS_ON) return;

    const int timeOut   = 45;
    int       secRemain = (int)(timeOut - (millis() - g5State.shutdownStartMs) / 1000);
    if (secRemain < 0) {
        g5State.powerState = PowerState::POWER_OFF;
        lcd.setBrightness(0);
        return;
    }
    int tw = targetSprite->width();
    int th = targetSprite->height();
    int ww = 380;
    int wh = 260;

    int topY = (th - wh) / 2;

    targetSprite->fillRect((tw - ww) / 2, topY, ww, wh, TFT_BLACK);
    targetSprite->drawRect((tw - ww) / 2, topY, ww, wh, TFT_WHITE);
    targetSprite->drawRect((tw - ww) / 2 + 1, topY + 1, ww - 2, wh - 2, TFT_WHITE);
    targetSprite->setTextDatum(TC_DATUM);
    targetSprite->setTextColor(TFT_WHITE);

    targetSprite->setTextSize(0.8);
    targetSprite->drawString("External Power Lost", tw / 2, topY + 6);
    targetSprite->drawFastHLine((tw - ww) / 2, topY + 34, ww, TFT_WHITE);
    targetSprite->setTextColor(TFT_YELLOW);
    targetSprite->drawString("Shutting down in:", tw / 2, topY + 40);

    targetSprite->setTextSize(2.0);
    char buf[6];
    sprintf(buf, "%d", secRemain);
    targetSprite->drawString(buf, tw / 2, topY + 100);

    targetSprite->setTextSize(0.8);
    targetSprite->setTextColor(TFT_WHITE);
    targetSprite->drawString("Press any key to continue", tw / 2, topY + 200);
    targetSprite->drawString("on battery power", tw / 2, topY + 226);
}

bool loadSettings()
{
    if (MFeeprom.read_block(CC_G5_SETTINGS_OFFSET, g5Settings)) {
        if (g5Settings.version != SETTINGS_VERSION) {
            g5Settings = CC_G5_Settings(); // Reset to defaults
                                           //            ESP_LOGE("PREF", "Settings Version mismatch\n");
            saveSettings();
        }
        ESP_LOGV("PREF", "Settings back. Device: %d\n", g5Settings.deviceType);
        return true;
    } else {
        // EEPROM read failed, defaults already set
        //    ESP_LOGE("PREF", "Load settings failed.\n");
        return false;
    }
}

bool saveSettings()
{
    bool retval;

    retval = MFeeprom.write_block(CC_G5_SETTINGS_OFFSET, g5Settings);
    if (retval) MFeeprom.commit();
    //  ESP_LOGV("PREF", "Settings saved. retval: %d\n", retval);
    return retval;
}

//   uint8_t brightnessGamma(int percent)
//   {
//       if (percent < 1) percent = 1;
//       if (percent > 100) percent = 100;

//       // Gamma 2.2 curve for perceptual linearity
//       float normalized = percent / 100.0f;
//       float corrected = pow(normalized, 2.2f);
//       return (uint8_t)(corrected * 255.0f);
//   }
uint8_t brightnessGamma(int percent)
{
    if (percent < 1) percent = 1;
    if (percent > 100) percent = 100;

    float normalized = percent / 100.0f;
    float corrected  = normalized * normalized * 0.75f + normalized * 0.25f;

    return (uint8_t)(40 + corrected * 215.0f);
}