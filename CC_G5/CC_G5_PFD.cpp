#include "CC_G5_PFD.h"
#include "esp_log.h"

#include <Wire.h>
// #include <esp_cache.h>
// #include <esp32s3/rom/cache.h>

#include "allocateMem.h"
#include "commandmessenger.h"
#include "Sprites\cdiPointer.h"
#include "Sprites\planeIcon_1bit.h"
#include "Sprites\cdiBar.h"
#include "Sprites\currentTrackPointer.h"
#include "Sprites\headingBug.h"
#include "Sprites\headingBug_1bit.h"
#include "Sprites\deviationScale.h"
#include "Sprites\gsDeviation.h"
#include "Sprites\deviationDiamond.h"
#include "Sprites\diamondBitmap.h"
#include "Sprites\deviationDiamondGreen.h"
#include "Sprites\cdiPointerGreen.h"
#include "Sprites\cdiBarGreen.h"
#include "Sprites\toFromGreen.h"
#include "Sprites\toFrom.h"
#include "Sprites\horizonMarker.h"
#include "Sprites\vsScale.h"
#include "Sprites\vsPointer.h"
#include "Sprites\bankAngleScale.h"
#include "Sprites\bankAnglePointer.h"
#include "Sprites\ball.h"
#include "Sprites\hScale.h"
#include "Sprites\pointer.h"
#include "Sprites\horizontalDeviationScale.h"
#include "Sprites\speedPointer.h"
#include "Sprites\stdBankIndicator.h"

#include "Sprites\gsBox.h"
#include "Sprites\distBox.h"
#include "Sprites\headingBox.h"

#include "Sprites\fdTriangles.h"
#include "Sprites\fdTrianglesNoAP.h"
// #include "Images\PrimaSans16.h"

LGFX_Sprite attitude(&lcd);
LGFX_Sprite speedUnit(&attitude);
LGFX_Sprite speedTens(&attitude);
LGFX_Sprite altUnit(&attitude);
LGFX_Sprite altTens(&attitude);
LGFX_Sprite horizonMarker(&attitude);
LGFX_Sprite stdBankIndicator(&attitude);

LGFX_Sprite altScaleNumber(&attitude);
LGFX_Sprite altBug(&attitude);
LGFX_Sprite altBugBitmap(&attitude);
LGFX_Sprite vsScale(&attitude);
LGFX_Sprite vsPointer(&attitude);
LGFX_Sprite baScale(&attitude);
LGFX_Sprite turnBar(&lcd);
LGFX_Sprite ballSprite(&turnBar);
LGFX_Sprite messageIndicator(&turnBar);

LGFX_Sprite headingTape(&attitude);
LGFX_Sprite hScale(&headingTape);
LGFX_Sprite horizontalDeviationScale(&attitude);

LGFX_Sprite fdTriangle(&attitude);
LGFX_Sprite speedPointer(&attitude);

LGFX_Sprite kohlsBox(&lcd);
LGFX_Sprite targetAltBox(&attitude);

LGFX_Sprite apBox(&lcd);

uint8_t *attBuffer;

extern CC_G5_Settings g5Settings;

CC_G5_PFD::CC_G5_PFD()
{
}

// Read data from RP2040
void CC_G5_PFD::read_rp2040_data()
{
    static bool encButtonPrev   = false;
    static bool extraButtonPrev = false;
    static int  encCount        = 0;

    int8_t delta, enc_btn, ext_btn;

    // If there is any sort of interaction and we're in shutting down mode, put us in battery mode
    if (g5State.powerState == PowerState::SHUTTING_DOWN) {
        powerStateSet(PowerState::BATTERY_POWERED);
        // read the data, but don't do anything with it to prevent normal action.
        g5Hardware.readEncoderData(delta, enc_btn, ext_btn);
        return;
    }

    // Read data from hardware interface
    if (g5Hardware.readEncoderData(delta, enc_btn, ext_btn)) {
        // Serial.printf("Data back from RP2040. Enc delta: %d, enc_btn: %d, ext_btn: %d\n", delta, enc_btn, ext_btn);

        if (enc_btn == ButtonEventType::BUTTON_CLICKED) {
            if (pfdMenu.menuActive) {
                // Route input to menu when active
                pfdMenu.handleEncoderButton(true);
            } else if (!brightnessMenu.active()) {
                // Open menu when not active
                pfdMenu.setActive(true);
            }
        }

        // if (enc_btn == ButtonEventType::BUTTON_LONG_PRESSED) {
        //     //  Serial.println("Long press on PFD. Send button to MF");
        //     pfdMenu.sendButton("btnPfdEncoder", 0);
        // }

        // POWER BUTTON
        if (ext_btn && g5State.powerState == PowerState::POWER_OFF) {
            powerStateSet(PowerState::POWER_ON);
            return;
        }
        if (ext_btn == ButtonEventType::BUTTON_LONG_PRESSED) {
            // Shutdown.
            powerStateSet(PowerState::SHUTTING_DOWN);
        }

        if (ext_btn == ButtonEventType::BUTTON_CLICKED) {
            if (brightnessMenu.active()) {
                g5Settings.lcdBrightness = g5State.lcdBrightness;
                saveSettings();
                brightnessMenu.hide();
                // Save the brightness setting.

            } else {
                brightnessMenu.show();
            }
        }

        if (delta) {
            if (brightnessMenu.active()) {
                brightnessMenu.adjustBrightness(delta);
            } else if (pfdMenu.menuActive) {
                // Route encoder turns to menu when active
                pfdMenu.handleEncoder(delta);
            } else {
                // Normal heading adjustment when menu not active
                pfdMenu.sendEncoder("encKohls", abs(delta), delta > 0 ? 0 : 2);
            }
        }
    }
}

void CC_G5_PFD::begin()
{
    loadSettings();

    // g5Hardware.setLedState(true);
    // Serial.printf("PowerSetting: %d\n", g5Settings.powerControl);

    lcd.setColorDepth(8);
    lcd.init();

    lcd.setBrightness(brightnessGamma(g5Settings.lcdBrightness));
    g5State.lcdBrightness = g5Settings.lcdBrightness;
    //    lcd.initDMA();

    // Setup menu structure

    pfdMenu.initializeMenu();

    // Configure i2c pins

    pinMode(INT_PIN, INPUT_PULLUP);

    // Configure I2C master
    if (!Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 40000)) {
    //    ESP_LOGE(TAG_I2C, "i2c setup failed");
    } else {
    //    ESP_LOGI(TAG_I2C, "i2c setup successful");
    }

    //    Serial.printf("Attaching interrupts.\n");
    // Test the bus.
    // ESP_LOGD(TAG_I2C, "Scanning I2C bus...");
    // for (byte addr = 1; addr < 127; addr++) {
    //     Wire.beginTransmission(addr);
    //     byte error = Wire.endTransmission();
    //     if (error == 0) {
    //        ESP_LOGD(TAG_I2C, "I2C device found at address 0x%02X", addr);
    //     }
    // }
    // ESP_LOGD(TAG_I2C, "I2C scan complete");

    // Configure interrupt handler with lambda
    attachInterrupt(digitalPinToInterrupt(INT_PIN), []() { g5Hardware.setDataAvailable(); }, FALLING);

#ifdef USE_GUITION_SCREEN
    lcd.setRotation(3); // Puts the USB jack at the bottom on Guition screen.
#else
    lcd.setRotation(0); // Orients the Waveshare screen with FPCB connector at bottom.
#endif

    lcd.fillScreen(TFT_BLACK);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    // lcd.setBrightness(255); // This doesn't work :-( I'm not sure if we can turn off the backlight or control brightness.
    lcd.loadFont(PrimaSans32);

    setupSprites();

    restoreState();

    //   // Get info about memory usage
    //   size_t internal_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    //   size_t psram_heap = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    //   Serial.printf("Internal free heap: %d bytes\n", internal_heap);
    //   Serial.printf("PSRAM free heap: %d bytes\n", psram_heap);
}

void CC_G5_PFD::setupSprites()
{

    attBuffer = (uint8_t *)heap_caps_malloc(ATTITUDE_WIDTH * ATTITUDE_HEIGHT * (ATTITUDE_COLOR_BITS / 8), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);

    if (attBuffer == nullptr) {
        Serial.println("Failed to allocate DMA-capable internal SRAM buffer!");
        while (1)   // Intentional hang for critical stop.
            ;
    }

    attitude.setColorDepth(ATTITUDE_COLOR_BITS);
    attitude.setBuffer(attBuffer, ATTITUDE_WIDTH, ATTITUDE_HEIGHT);
    attitude.fillSprite(TFT_MAIN_TRANSPARENT);
    attitude.loadFont(PrimaSans32);
    attitude.setTextColor(TFT_LIGHTGRAY);

    speedUnit.setColorDepth(8);
    speedUnit.createSprite(26, 82);
    speedUnit.loadFont(PrimaSans32);
    speedUnit.setTextSize(1.0);
    speedUnit.setTextColor(TFT_WHITE, TFT_BLACK);
    speedUnit.setTextDatum(CR_DATUM);

    speedTens.setColorDepth(8);
    speedTens.createSprite(50, 40);
    speedTens.loadFont(PrimaSans32);
    speedTens.setTextSize(1.0);
    speedTens.setTextColor(TFT_WHITE, TFT_BLACK);
    speedTens.setTextDatum(BR_DATUM);

    altUnit.setColorDepth(8);
    altUnit.createSprite(40, 80);
    altUnit.loadFont(PrimaSans32);
    altUnit.setTextSize(0.8);
    altUnit.setTextColor(TFT_WHITE, TFT_BLACK);

    altTens.setColorDepth(8);
    altTens.createSprite(60, 40);
    altTens.loadFont(PrimaSans32);
    altTens.setTextSize(0.8);
    altTens.setTextColor(TFT_WHITE, TFT_BLACK);

    speedPointer.setColorDepth(8);
    speedPointer.createSprite(SPEEDPOINTER_IMG_WIDTH, SPEEDPOINTER_IMG_HEIGHT);
    speedPointer.setBitmapColor(TFT_BLACK, TFT_WHITE);
    speedPointer.drawBitmap(0, 0, SPEEDPOINTER_IMG_DATA, SPEEDPOINTER_IMG_WIDTH, SPEEDPOINTER_IMG_HEIGHT, TFT_BLACK);
    speedPointer.setTextColor(TFT_CYAN);
    speedPointer.loadFont(PrimaSans32);
    speedPointer.setTextSize(0.5);
    speedPointer.setTextDatum(CC_DATUM);

    horizonMarker.setColorDepth(8);
    horizonMarker.createSprite(HORIZONMARKER_IMG_WIDTH, HORIZONMARKER_IMG_HEIGHT);
    horizonMarker.pushImage(0, 0, HORIZONMARKER_IMG_WIDTH, HORIZONMARKER_IMG_HEIGHT, HORIZONMARKER_IMG_DATA);

    stdBankIndicator.setColorDepth(8);
    stdBankIndicator.createSprite(STDBANKINDICATOR_IMG_WIDTH, STDBANKINDICATOR_IMG_HEIGHT);
    stdBankIndicator.pushImage(0, 0, STDBANKINDICATOR_IMG_WIDTH, STDBANKINDICATOR_IMG_HEIGHT, STDBANKINDICATOR_IMG_DATA);
    stdBankIndicator.setPivot(STDBANKINDICATOR_IMG_WIDTH / 2, 143); // CAC NEEDS TUNING.

    // GS Is Ground speed, NOT glide slope.
    gsBox.setColorDepth(8);
    gsBox.createSprite(SPEED_COL_WIDTH, BOTTOM_BAR_HEIGHT);
    // gsBox.pushImage(0,0, SPEED_COL_WIDTH, GSBOX_IMG_HEIGHT, GSBOX_IMG_DATA);
    // gsBox.setTextColor(TFT_MAGENTA);
    // gsBox.setTextDatum(BR_DATUM);
    gsBox.loadFont(PrimaSans32);

    kohlsBox.setColorDepth(8);
    kohlsBox.createSprite(ALTITUDE_COL_WIDTH, BOTTOM_BAR_HEIGHT);
    // if(kohlsBox.getBuffer() == nullptr) while(1);
    kohlsBox.setTextColor(TFT_CYAN);
    kohlsBox.setTextDatum(CC_DATUM);
    kohlsBox.loadFont(PrimaSans32);

    headingBox.setColorDepth(8);
    headingBox.createSprite(HEADINGBOX_IMG_WIDTH, HEADINGBOX_IMG_HEIGHT);
    headingBox.pushImage(0, 0, HEADINGBOX_IMG_WIDTH, HEADINGBOX_IMG_HEIGHT, HEADINGBOX_IMG_DATA);
    headingBox.setTextColor(TFT_CYAN);
    headingBox.setTextDatum(BR_DATUM);
    headingBox.loadFont(PrimaSans32);

    altBug.setColorDepth(8);
    altBug.createSprite(HEADINGBUG_IMG_WIDTH, HEADINGBUG_IMG_HEIGHT);
    // altBug.pushImage(0,0,HEADINGBUG_IMG_WIDTH, HEADINGBUG_IMG_HEIGHT, HEADINGBOX_IMG_DATA);
    altBug.setBuffer(const_cast<std::uint16_t *>(HEADINGBUG_IMG_DATA), HEADINGBUG_IMG_WIDTH, HEADINGBUG_IMG_HEIGHT, 16);
    altBug.setPivot(HEADINGBOX_IMG_WIDTH / 2, HEADINGBUG_IMG_HEIGHT / 2); // THIS IS WRONG CONSTANT! but i don't feel like redoing the functions that depend on the error.

    // altBugBitmap.setColorDepth(1);
    // altBugBitmap.createSprite(HEADINGBUG_1BIT_IMG_WIDTH, HEADINGBUG_1BIT_IMG_HEIGHT);
    // altBugBitmap.setBuffer(const_cast<std::uint8_t *>(HEADINGBUG_1BIT_IMG_DATA), HEADINGBUG_1BIT_IMG_WIDTH, HEADINGBUG_1BIT_IMG_HEIGHT);
    // altBugBitmap.setPivot(HEADINGBUG_1BIT_IMG_WIDTH / 2, HEADINGBUG_1BIT_IMG_HEIGHT / 2);

    targetAltBox.setColorDepth(8);
    targetAltBox.createSprite(130, TOP_BAR_HEIGHT);
    targetAltBox.setTextColor(TFT_CYAN);
    targetAltBox.loadFont(PrimaSans32);
    targetAltBox.setTextDatum(CR_DATUM);
    targetAltBox.fillSprite(TFT_BLACK);
    targetAltBox.pushImageRotateZoom(14, 4, 0, 0, 90, 0.6, 0.6, HEADINGBUG_IMG_WIDTH, HEADINGBUG_IMG_HEIGHT, HEADINGBUG_IMG_DATA, TFT_WHITE);
    targetAltBox.drawRect(0, 0, ALTITUDE_COL_WIDTH, TOP_BAR_HEIGHT, TFT_DARKGREY);
    targetAltBox.drawRect(1, 1, ALTITUDE_COL_WIDTH - 2, TOP_BAR_HEIGHT - 2, TFT_DARKGREY);
    targetAltBox.setTextSize(0.5);
    targetAltBox.drawString("f", ALTITUDE_COL_WIDTH - 10, 12);
    targetAltBox.drawString("t", ALTITUDE_COL_WIDTH - 10, 26);
    targetAltBox.setTextSize(0.9);

    altScaleNumber.setColorDepth(8);
    altScaleNumber.createSprite(23, 23);
    altScaleNumber.setPivot(8, 7);
    altScaleNumber.setTextColor(TFT_WHITE, TFT_BLACK);
    altScaleNumber.setTextDatum(MC_DATUM);
    altScaleNumber.loadFont(PrimaSans32);
    altScaleNumber.setTextSize(0.6);

    vsScale.setColorDepth(8);
    vsScale.createSprite(VSSCALE_IMG_WIDTH, VSSCALE_IMG_HEIGHT);
    vsScale.setBuffer(const_cast<std::uint16_t *>(VSSCALE_IMG_DATA), VSSCALE_IMG_WIDTH, VSSCALE_IMG_HEIGHT, 16);

    vsPointer.setColorDepth(8);
    vsPointer.createSprite(VSPOINTER_IMG_WIDTH, VSPOINTER_IMG_HEIGHT);
    vsPointer.setBuffer(const_cast<std::uint16_t *>(VSPOINTER_IMG_DATA), VSPOINTER_IMG_WIDTH, VSPOINTER_IMG_HEIGHT, 16);

    baScale.setColorDepth(1);
    baScale.createSprite(BANKANGLESCALE_IMG_WIDTH, BANKANGLESCALE_IMG_HEIGHT);
    baScale.setBuffer(const_cast<std::uint8_t *>(BANKANGLESCALE_IMG_DATA), BANKANGLESCALE_IMG_WIDTH, BANKANGLESCALE_IMG_HEIGHT);
    baScale.setPivot(BANKANGLESCALE_IMG_WIDTH / 2, 143); // From image.

    turnBar.setColorDepth(8);
    turnBar.createSprite(CENTER_COL_WIDTH, 40);

    ballSprite.setColorDepth(8);
    ballSprite.createSprite(BALL_IMG_WIDTH, BALL_IMG_HEIGHT);
    ballSprite.setBuffer(const_cast<std::uint16_t *>(BALL_IMG_DATA), BALL_IMG_WIDTH, BALL_IMG_HEIGHT, 16);

    headingTape.setColorDepth(8);
    headingTape.createSprite(CENTER_COL_WIDTH, 40);
    headingTape.fillSprite(DARK_SKY_COLOR);
    headingTape.setTextDatum(CC_DATUM);
    headingTape.loadFont(PrimaSans32);
    headingTape.setTextColor(TFT_WHITE);

    if (headingTape.getBuffer() == nullptr) {
        while (1)
            Serial.printf("No room!\n");    // Intentional hang for non-recoverable error.
    }

    hScale.setColorDepth(1);
    hScale.setBitmapColor(TFT_WHITE, TFT_BLACK);
    hScale.createSprite(HSCALE_IMG_WIDTH, HSCALE_IMG_HEIGHT);
    hScale.setBuffer(const_cast<std::uint8_t *>(HSCALE_IMG_DATA), HSCALE_IMG_WIDTH, HSCALE_IMG_HEIGHT);

    glideDeviationScale.createSprite(GSDEVIATION_IMG_WIDTH, GSDEVIATION_IMG_HEIGHT);
    glideDeviationScale.pushImage(0, 0, GSDEVIATION_IMG_WIDTH, GSDEVIATION_IMG_HEIGHT, GSDEVIATION_IMG_DATA);
    glideDeviationScale.setTextColor(TFT_MAGENTA);
    glideDeviationScale.setTextSize(0.5);
    glideDeviationScale.setTextDatum(CC_DATUM);
    glideDeviationScale.loadFont(PrimaSans32);

    deviationScale.setColorDepth(8);
    horizontalDeviationScale.createSprite(HORIZONTALDEVIATIONSCALE_IMG_WIDTH, HORIZONTALDEVIATIONSCALE_IMG_HEIGHT);
    //    deviationScale.setBuffer(const_cast<std::uint16_t*>(HORIZONTALDEVIATIONSCALE_IMG_DATA), HORIZONTALDEVIATIONSCALE_IMG_WIDTH, HORIZONTALDEVIATIONSCALE_IMG_HEIGHT);
    horizontalDeviationScale.pushImage(0, 0, HORIZONTALDEVIATIONSCALE_IMG_WIDTH, HORIZONTALDEVIATIONSCALE_IMG_HEIGHT, HORIZONTALDEVIATIONSCALE_IMG_DATA);

    // Diamond for deviation scale
    deviationDiamond.setColorDepth(1);
    deviationDiamond.setBitmapColor(TFT_MAGENTA, TFT_BLACK);
    deviationDiamond.createSprite(DIAMONDBITMAP_IMG_WIDTH, DIAMONDBITMAP_IMG_HEIGHT);
    // deviationDiamond.setBuffer(const_cast<std::uint16_t*>(DEVIATIONDIAMOND_IMG_DATA), DEVIATIONDIAMOND_IMG_WIDTH, DEVIATIONDIAMOND_IMG_HEIGHT);
    deviationDiamond.setBuffer(const_cast<std::uint8_t *>(DIAMONDBITMAP_IMG_DATA), DIAMONDBITMAP_IMG_WIDTH, DIAMONDBITMAP_IMG_HEIGHT);

    fdTriangle.setColorDepth(8);
    fdTriangle.createSprite(FDTRIANGLES_IMG_WIDTH, FDTRIANGLES_IMG_HEIGHT);
    fdTriangle.pushImage(0, 0, FDTRIANGLES_IMG_WIDTH, FDTRIANGLES_IMG_HEIGHT, FDTRIANGLES_IMG_DATA);
    fdTriangle.setPivot(FDTRIANGLES_IMG_WIDTH / 2, 0);

    messageIndicator.setColorDepth(1);
    messageIndicator.createSprite(30, 30);
    messageIndicator.loadFont(PrimaSans32);
    messageIndicator.fillRoundRect(0, 0, 30, 30, 4, TFT_WHITE);
    messageIndicator.drawRoundRect(0, 0, 30, 30, 4, TFT_BLACK);
    messageIndicator.drawRoundRect(1, 1, 28, 28, 3, TFT_BLACK);
    messageIndicator.setTextDatum(CC_DATUM);
    messageIndicator.setTextSize(0.7);
    messageIndicator.setTextColor(TFT_BLACK, TFT_WHITE);
    messageIndicator.drawString("!", 15, 15);

    apBox.setColorDepth(8);
    apBox.createSprite(ATTITUDE_WIDTH, min(30, TOP_BAR_HEIGHT));
    apBox.loadFont(PrimaSans32);
    apBox.setTextSize(0.8);
    apBox.setTextDatum(BC_DATUM);

    if (apBox.getBuffer() == nullptr) {
        while (1) {
            Serial.println("Out of memory.");
            sleep(5);
        };
    }

    return;
}

void CC_G5_PFD::drawMenu()
{
    pfdMenu.drawMenu();
}

void CC_G5_PFD::drawAdjustmentPopup()
{
    // Delegate to menu's popup drawing, but using CC_G5's LCD context
    pfdMenu.drawAdjustmentPopup();
}

void CC_G5_PFD::processMenu()
{
    if (pfdMenu.menuActive) {
        if (pfdMenu.currentState == PFDMenu::MenuState::BROWSING) {
            drawMenu(); // Draws menu items on attitude sprite
        } else if (pfdMenu.currentState == PFDMenu::MenuState::ADJUSTING) {
            drawAdjustmentPopup(); // Draws popup on attitude sprite
        } else if (pfdMenu.currentState == PFDMenu::MenuState::SETTINGS_BROWSING) {
            pfdMenu.drawSettingsList();
        }
    }
}

void CC_G5_PFD::attach()
{
}

void CC_G5_PFD::detach()
{
    if (!_initialised)
        return;
    _initialised = false;
}

void CC_G5_PFD::setPFD(int16_t messageID, char *setPoint)
{
    lastMFUpdate = millis(); // Resets the MF connection alert timeout.
    switch (messageID) {
    case 60: // Airspeed
        g5State.rawAirspeed = atof(setPoint);
        break;
    case 61: // AP Active
        g5State.apActive = atoi(setPoint);
        setFDBitmap();
        break;
    case 62: // AP Alt Captured
        g5State.apAltCaptured = atoi(setPoint);
        break;
    case 63: // AP Altitude Bug
        g5State.targetAltitude = atoi(setPoint);
        break;
    case 64: // AP Armed Lateral Mode
        g5State.apLArmedMode = atoi(setPoint);
        break;
    case 65: // AP Armed Vertical Mode
        g5State.apVArmedMode = atoi(setPoint);
        break;
    case 66: // AP Lateral Mode
        g5State.apLMode = atoi(setPoint);
        break;
    case 67: // AP Speed Bug
        g5State.apTargetSpeed = atoi(setPoint);
        break;
    case 68: // AP Vertical Mode
        g5State.apVMode = atoi(setPoint);
        break;
    case 69: // AP Vertical Speed Bug
        g5State.apTargetVS = atoi(setPoint);
        break;
    case 70: // AP Yaw Damper
        g5State.apYawDamper = atoi(setPoint);
        break;
    case 71: // Ball (Slip/Skid) Position
        g5State.rawBallPos = atof(setPoint);
        break;
    case 72: // Bank Angle
        g5State.rawBankAngle = atof(setPoint);
        break;
    case 73: // FD Active
        g5State.flightDirectorActive = atoi(setPoint);
        setFDBitmap();
        break;
    case 74: // FD Bank Angle
        g5State.flightDirectorBank = atof(setPoint);
        break;
    case 75: // FD Pitch
        g5State.flightDirectorPitch = atof(setPoint);
        break;
    case 76: // GPS Course to Steer
        g5State.desiredTrack = atof(setPoint);
        break;
    case 77: // Indicated Altitude
        g5State.rawAltitude = atof(setPoint);
        break;
    case 78: // Kohlsman Value
        g5State.kohlsman = atof(setPoint);
        break;
    case 79: // OAT
        g5State.oat = atoi(setPoint);
        break;
    case 80: // Pitch Angle
        g5State.rawPitchAngle = atof(setPoint);
        break;
    case 81: // Turn Rate
        g5State.turnRate = atof(setPoint);
        break;
    case 82: // V-Speeds Array
        setVSpeeds(setPoint);
        break;
    case 83: // Vertical Speed
        g5State.rawVerticalSpeed = atoi(setPoint);
        break;
    case 84: // OBS Course Setting
        g5State.navCourse = atoi(setPoint);
        break;
    case 85: // Density
        g5State.densityAltitude = atoi(setPoint);
        break;
    case 86: // True Airspeed
        g5State.trueAirspeed = atof(setPoint);
        break;
    }
}

// OLD UNUSED Set().

void CC_G5_PFD::set(int16_t messageID, char *setPoint)
{
    // switch (messageID) {
    //     case -2: // PowerSavingMode if 1, go into power saving or 0 to wake up
    //         g5State.inShutdown = atoi(setPoint) == 1;
    //         cmdMessenger.sendCmd(kStatus, F("Shutdown message in."));
    //         break;

    //     case -1: // Stop message from MF. Device execution stops.
    //         cmdMessenger.sendCmd(kStatus, F("Stop message in."));
    //         // g5State.inShutdown = true;
    //         // Let's show some
    //         break;
    // }

    //     /* **********************************************************************************
    //         MessageID == -2 will be send from the board when PowerSavingMode is set
    //             Message will be "0" for leaving and "1" for entering PowerSavingMode
    //         MessageID == -1 will be send from the connector when Connector stops running
    //     ********************************************************************************** */
    //     lastMFUpdate = millis();

    //     int32_t data = 0;

    //     if (setPoint != NULL)
    //         data = atoi(setPoint);
    //     else
    //         return;

    //     uint16_t output;
    //     float    value;

    //     switch (messageID) {
    //     case -1:
    //         // tbd., get's called when Mobiflight shuts down
    //         break;
    //     case -2:
    //         // tbd., get's called when PowerSavingMode is entered
    //         g5Hardware.setLedState(false);
    //         break;
    //     case 0:
    //         g5Hardware.setLedState(true);
    //         value           = atof(setPoint);
    //         g5State.rawHeadingAngle = value;
    //         break;
    //     case 1:
    //         output          = (uint16_t)data;
    //         g5State.headingBugAngle = output;
    //         break;
    //     case 2:
    //         g5State.rawAirspeed = atof(setPoint);
    //         break;
    //     case 3:
    //         g5State.rawAltitude = data;
    //         break;
    //     case 4:
    //         value        = atof(setPoint);
    //         g5State.rawCdiOffset = value;
    //         break;
    //     case 5:
    //         g5State.rawPitchAngle = atof(setPoint);
    //         break;
    //     case 6:
    //         g5State.groundSpeed = data;
    //         break;
    //     case 7:
    //         g5State.groundTrack = atof(setPoint);
    //         break;
    //     case 8:
    //         g5State.rawBankAngle = atof(setPoint);
    //         break;
    //     case 9:
    //         break;
    //     case 10:
    //         g5State.targetAltitude = atoi(setPoint);
    //         break;
    //     case 11:
    //         g5State.rawBallPos = atof(setPoint);
    //         break;
    //     case 12:
    //         g5State.kohlsman = atof(setPoint);
    //         break;
    //     case 13:
    //         g5State.turnRate = atof(setPoint);
    //         break;
    //     case 14:
    //         g5State.rawVerticalSpeed = (int32_t)data; // VS can be negative!
    //         break;
    //     case 15:
    //         g5State.gsiNeedleValid = (uint16_t)data;
    //         break;
    //     case 16:
    //         g5State.cdiNeedleValid = (uint16_t)data;
    //         break;
    //     case 17:
    //         g5State.desiredTrack = atof(setPoint);
    //         break;
    //     case 18:
    //         g5State.gsiNeedle = (int)data;
    //         break;
    //     case 19:
    //         g5State.navSource = (int)data;
    //         break;
    //     case 20:
    //         g5State.flightDirectorActive = (int)data;
    //         setFDBitmap();
    //         break;
    //     case 21:
    //         g5State.flightDirectorPitch = atof(setPoint);
    //         break;
    //     case 22:
    //         g5State.flightDirectorBank = atof(setPoint);
    //         break;
    //     case 23:
    //         g5State.cdiToFrom = (int)data;
    //         break;
    //     case 24:
    //         g5State.gpsApproachType = (int)data;
    //         break;
    //     case 25:
    //         g5State.navCourse = (int)data;
    //         break;
    //     case 26:
    //         g5State.apActive = (int)data;
    //         setFDBitmap();
    //         break;
    //     case 27:
    //         g5State.apLMode = (int)data;
    //         break;
    //     case 28:
    //         g5State.apLArmedMode = (int)data;
    //         break;
    //     case 29:
    //         g5State.apVMode = (int)data;
    //         break;
    //     case 30:
    //         g5State.apVArmedMode = (int)data;
    //         break;
    //     case 31:
    //         g5State.apTargetVS = (int)data;
    //         break;
    //     case 32:
    //         g5State.apTargetSpeed = (int)data;
    //         break;
    //     case 33:
    //         g5State.apAltCaptured = (int)data;
    //         break;
    //     case 34:
    //         g5State.apYawDamper = (int)data;
    //         break;
    //     case 35:
    //         setVSpeeds(setPoint);
    //         break;
    //     case 36:
    //         g5State.oat = (int)data;
    //         break;
    //     default:
    //         break;
    //     }
}

void CC_G5_PFD::setFDBitmap()
{
    // Could probably put in some short circuits here, but MF is good about not sending extra updates.
    if (!g5State.flightDirectorActive) return;

    fdTriangle.fillSprite(TFT_WHITE);

    if (!g5State.apActive) {
        // Use the dark filled FD
        fdTriangle.pushImage(0, 0, FDTRIANGLESNOAP_IMG_WIDTH, FDTRIANGLESNOAP_IMG_HEIGHT, FDTRIANGLESNOAP_IMG_DATA);
    } else {
        // Use the magenta filled FD
        fdTriangle.pushImage(0, 0, FDTRIANGLES_IMG_WIDTH, FDTRIANGLES_IMG_HEIGHT, FDTRIANGLES_IMG_DATA);
    }
}

void CC_G5_PFD::setVSpeeds(char *vSpeedString)
{
    if (!vSpeedString || strlen(vSpeedString) == 0) return;

    // Array of pointers to the g5Settings members in order
    uint16_t *vSpeedFields[] = {
        &g5Settings.Vs0,
        &g5Settings.Vs1,
        &g5Settings.Vr,
        &g5Settings.Vx,
        &g5Settings.Vy,
        &g5Settings.Vg,
        &g5Settings.Va,
        &g5Settings.Vfe,
        &g5Settings.Vno,
        &g5Settings.Vne};

    int   fieldIndex = 0;
    char *token      = strtok(vSpeedString, "|");

    while (token != NULL && fieldIndex < 10) {
        long value = 0;

        // Check if token is not empty (handles || case)
        if (strlen(token) > 0) {
            value = atol(token); // Use atol for long int
        }
        // If token is empty, value stays 0

        // Clamp to uint8_t range (0-255)
        if (value < 0) value = 0;

        *vSpeedFields[fieldIndex] = value;

        token = strtok(NULL, "|");
        fieldIndex++;
    }

    saveSettings(); // Save the updated settings to EEPROM
}

void CC_G5_PFD::drawAttitude()
{
    // Display dimensions
    const int16_t CENTER_X = attitude.width() / 2;
    const int16_t CENTER_Y = attitude.height() / 2;

    // Colors
    const uint16_t HORIZON_COLOR    = 0xFFFF; // White horizon line
    const uint16_t PITCH_LINE_COLOR = 0xFFFF; // White pitch lines
    const uint16_t TEXT_COLOR       = 0xFFFF; // White text color

    // Convert angles to radians
    float bankRad = g5State.bankAngle * PI / 180.0;

    // Pitch scaling factor (pixels per degree)
    const float PITCH_SCALE = 8.0;

    // --- 1. Draw Sky and Ground (Reverted to the reliable drawFastVLine method) ---

    // Determine if aircraft is inverted based on bank angle
    // Inverted when bank is beyond ±90 degrees
    bool inverted = (g5State.bankAngle > 90.0 || g5State.bankAngle < -90.0);

    // Calculate vertical offset of the horizon due to pitch.
    // When inverted, flip the pitch offset to match what pilot sees from inverted perspective
    // A negative g5State.pitchAngle (nose up) moves the horizon down (positive pixel offset).
    float horizonPixelOffset = inverted ? (g5State.pitchAngle * PITCH_SCALE) : (-g5State.pitchAngle * PITCH_SCALE);

    // Clear the sprite
    attitude.fillSprite(SKY_COLOR);

    // Pre-calculate tan of bank angle for the loop
    float tanBank = tan(bankRad);

    // For each column of pixels, calculate where the horizon intersects
    for (int16_t x = 0; x < attitude.width(); x++) {
        // Distance from center
        int16_t dx = x - CENTER_X;

        // Calculate horizon Y position for this column
        // The horizon's center is at CENTER_Y + horizonPixelOffset
        float horizonY = (CENTER_Y + horizonPixelOffset) + (dx * tanBank);

        int16_t horizonPixel = round(horizonY);

        // Drawing the dimmed sides costs an fps, but i think it's worth it.
        if (inverted) {
            // When inverted, ground is ABOVE the horizon line
            if (horizonPixel > 0) {
                attitude.drawFastVLine(x, 0, min((int16_t)ATTITUDE_HEIGHT, horizonPixel), (x < SPEED_COL_WIDTH || x > ATTITUDE_WIDTH - ALTITUDE_COL_WIDTH) ? DARK_GND_COLOR : GND_COLOR);
                if (x < SPEED_COL_WIDTH || x > (ATTITUDE_WIDTH - ALTITUDE_COL_WIDTH)) attitude.drawFastVLine(x, max((int16_t)0, horizonPixel), ATTITUDE_HEIGHT - max((int16_t)0, horizonPixel), (x < SPEED_COL_WIDTH || x > ATTITUDE_WIDTH - ALTITUDE_COL_WIDTH) ? DARK_SKY_COLOR : SKY_COLOR);
                // attitude.drawFastVLine(x, 0, min((int16_t)attitude.height(), horizonPixel), GND_COLOR);
            }
        } else {
            // When upright, ground is BELOW the horizon line
            if (horizonPixel < attitude.height()) {
                attitude.drawFastVLine(x, max((int16_t)0, horizonPixel), ATTITUDE_HEIGHT - max((int16_t)0, horizonPixel), (x < SPEED_COL_WIDTH || x > ATTITUDE_WIDTH - ALTITUDE_COL_WIDTH) ? DARK_GND_COLOR : GND_COLOR);
                if (x < SPEED_COL_WIDTH || x > (ATTITUDE_WIDTH - ALTITUDE_COL_WIDTH)) attitude.drawFastVLine(x, min((int16_t)0, horizonPixel), horizonPixel, DARK_SKY_COLOR);
            }
        }
    }

    // --- 2. Draw Pitch Ladder (with correct math) ---
    float cosBank = cos(bankRad);
    float sinBank = sin(bankRad);

    auto drawPitchLine = [&](float pitchDegrees, int lineWidth, bool showNumber, uint16_t color) {
        // Calculate the line's vertical distance from the screen center in an un-rotated frame.
        // A positive value moves the line DOWN the screen.
        // (pitchDegrees - g5State.pitchAngle) gives the correct relative position.
        float verticalOffset = (pitchDegrees - g5State.pitchAngle) * PITCH_SCALE;

        // Define the line's endpoints relative to the screen center before rotation
        float halfWidth = lineWidth / 2.0;
        float p1x_unrot = -halfWidth;
        float p1y_unrot = verticalOffset;
        float p2x_unrot = +halfWidth;
        float p2y_unrot = verticalOffset;

        // Apply the bank rotation to the endpoints
        int16_t x1 = CENTER_X + p1x_unrot * cosBank - p1y_unrot * sinBank;
        int16_t y1 = CENTER_Y + p1x_unrot * sinBank + p1y_unrot * cosBank;
        int16_t x2 = CENTER_X + p2x_unrot * cosBank - p2y_unrot * sinBank;
        int16_t y2 = CENTER_Y + p2x_unrot * sinBank + p2y_unrot * cosBank;

        attitude.drawLine(x1, y1, x2, y2, color);

        if (showNumber && abs(pitchDegrees) >= 10) {
            char pitchText[4];
            sprintf(pitchText, "%d", (int)abs(pitchDegrees));

            float textOffset   = halfWidth + 15;
            float text1x_unrot = -textOffset;
            float text2x_unrot = +textOffset;
            float texty_unrot  = verticalOffset;

            int16_t textX1 = CENTER_X + text1x_unrot * cosBank - texty_unrot * sinBank;
            int16_t textY1 = CENTER_Y + text1x_unrot * sinBank + texty_unrot * cosBank;

            int16_t textX2 = CENTER_X + text2x_unrot * cosBank - texty_unrot * sinBank;
            int16_t textY2 = CENTER_Y + text2x_unrot * sinBank + texty_unrot * cosBank;

            altScaleNumber.fillSprite(TFT_BLACK);
            altScaleNumber.drawString(pitchText, 9, 9);
            attitude.setPivot(textX1, textY1);
            altScaleNumber.pushRotated(inverted ? g5State.bankAngle + 180.0 : g5State.bankAngle, TFT_BLACK);
            attitude.setPivot(textX2, textY2);
            altScaleNumber.pushRotated(inverted ? g5State.bankAngle + 180.0 : g5State.bankAngle, TFT_BLACK);

        }
    };

    auto drawChevron = [&](float pitchDegrees, int width, uint16_t color, int thickness) {
        // Point of the V is at pitchDegrees
        // Tails extend 10 degrees away from horizon (toward more extreme pitch)
        float tailPitch = pitchDegrees + (pitchDegrees > 0 ? 10.0 : -10.0);

        float halfWidth = width / 2.0;

        // Calculate the tip position (center point of V, at the pitch line)
        float tipVerticalOffset = (pitchDegrees - g5State.pitchAngle) * PITCH_SCALE;

        // Calculate the tail positions (ends of V, 10 degrees further from horizon)
        float tailVerticalOffset = (tailPitch - g5State.pitchAngle) * PITCH_SCALE;

        // Define points before rotation
        // Left tail
        float leftTailX_unrot = -halfWidth;
        float leftTailY_unrot = tailVerticalOffset;

        // Tip (center)
        float tipX_unrot = 0;
        float tipY_unrot = tipVerticalOffset;

        // Right tail
        float rightTailX_unrot = halfWidth;
        float rightTailY_unrot = tailVerticalOffset;

        // Apply bank rotation to all three points
        int16_t leftTailX = CENTER_X + leftTailX_unrot * cosBank - leftTailY_unrot * sinBank;
        int16_t leftTailY = CENTER_Y + leftTailX_unrot * sinBank + leftTailY_unrot * cosBank;

        int16_t tipX = CENTER_X + tipX_unrot * cosBank - tipY_unrot * sinBank;
        int16_t tipY = CENTER_Y + tipX_unrot * sinBank + tipY_unrot * cosBank;

        int16_t rightTailX = CENTER_X + rightTailX_unrot * cosBank - rightTailY_unrot * sinBank;
        int16_t rightTailY = CENTER_Y + rightTailX_unrot * sinBank + rightTailY_unrot * cosBank;

        // Draw the two lines that form the V with thickness
        for (int t = 0; t < thickness; t++) {
            // Draw left line (left tail to tip) - offset perpendicular to the line
            float   angle1 = atan2(tipY - leftTailY, tipX - leftTailX);
            int16_t dx1    = -sin(angle1) * t;
            int16_t dy1    = cos(angle1) * t;
            // attitude.drawLine(leftTailX + dx1, leftTailY + dy1, tipX + dx1, tipY + dy1, color);
            attitude.drawWideLine(leftTailX + dx1, leftTailY + dy1, tipX + dx1, tipY + dy1, 6, color);

            // Draw right line (tip to right tail) - offset perpendicular to the line
            float   angle2 = atan2(rightTailY - tipY, rightTailX - tipX);
            int16_t dx2    = -sin(angle2) * t;
            int16_t dy2    = cos(angle2) * t;
            // attitude.drawLine(tipX + dx2, tipY + dy2, rightTailX + dx2, rightTailY + dy2, color);
            attitude.drawWideLine(tipX + dx2, tipY + dy2, rightTailX + dx2, rightTailY + dy2, 6, color);
        }
    };

    // Define and draw all the pitch lines
    const struct {
        float deg;
        int   width;
        bool  num;
    } pitch_lines[] = {
        {85.0, 60, false}, {80.0, 80, true}, {75.0, 60, false}, {70.0, 80, true}, {65.0, 60, false}, {60.0, 80, true}, {55.0, 60, false}, {50.0, 80, true}, {45.0, 60, false}, {40.0, 80, true}, {35.0, 60, false}, {30.0, 80, true}, {25.0, 60, false}, {20.0, 80, true}, {17.5, 40, false}, {15.0, 60, false}, {12.5, 40, false}, {10.0, 80, true}, {7.5, 40, false}, {5.0, 60, false}, {2.5, 40, false}, {-2.5, 40, false}, {-5.0, 60, false}, {-7.5, 40, false}, {-10.0, 80, true}, {-12.5, 40, false}, {-15.0, 60, false}, {-17.5, 40, false}, {-20.0, 80, true}, {-25.0, 60, false}, {-30.0, 80, true}, {-35.0, 60, false}, {-40.0, 80, true}, {-45.0, 60, false}, {-50.0, 80, true}, {-55.0, 60, false}, {-60.0, 80, true}, {-65.0, 60, false}, {-70.0, 80, true}, {-75.0, 60, false}, {-80.0, 80, true}, {-85.0, 60, false}};

    uint16_t color = TFT_RED;
    for (const auto &line : pitch_lines) {
        // Simple culling: only draw lines that are somewhat close to the screen
        float verticalPos = abs(line.deg - g5State.pitchAngle) * PITCH_SCALE;
        if (verticalPos < attitude.height() - 280) {
            // Fade the colors into the background
            // if (line.deg > 0 && verticalPos > 60) color = 0xDEDF;
            if (line.deg > 0 && verticalPos > 81) color = 0xB48A; // dark brown
            // else if (line.deg < 0 && verticalPos > 60 ) color = 0xB48A;
            else if (line.deg < 0 && verticalPos > 81)
                color = 0x949f; // Light blue
            else
                color = TFT_WHITE;
            //        Serial.printf("Line: d: %f, vPos: %d, col: %d\n", line.deg, verticalPos, color);
            drawPitchLine(line.deg, line.width, line.num, color);
        }
    }

    // Draw extreme attitude chevrons at 60, 70, and 80 degree pitch lines
    const float chevron_pitches[] = {40.0, 50.0, 60.0, 70.0, 80.0, -40.0, -50.0, -60.0, -70.0, -80.0};
    for (const auto &chevronPitch : chevron_pitches) {
        float verticalPos = abs(chevronPitch - g5State.pitchAngle) * PITCH_SCALE;
        // Only draw chevrons that are close to being on screen
        if (verticalPos < attitude.height() + 100) {
            drawChevron(chevronPitch, 80, TFT_RED, 3);
        }
    }

    // Draw the bank scale.
    attitude.setPivot(240, 200);
    baScale.pushRotated(g5State.bankAngle, TFT_BLACK);

    // Draw the static scale pointer at the top center. Use the center of the screen, but offset by the difference in column widths.
    attitude.drawBitmap(231, 79, BANKANGLEPOINTER_IMG_DATA, BANKANGLEPOINTER_IMG_WIDTH, BANKANGLEPOINTER_IMG_HEIGHT, TFT_WHITE); // 231 is 240 - half the sprite width.

    // Draw the turn rate indicators if we're above some speed tbd.
    if (g5State.trueAirspeed > 45) {
        float stdBa = g5State.trueAirspeed / 10 + 7;
        stdBankIndicator.pushRotated(g5State.bankAngle - stdBa, TFT_BLACK);
        stdBankIndicator.pushRotated(g5State.bankAngle + stdBa, TFT_BLACK);
    }

    // baScale.pushSprite(240, 100, TFT_BLACK);

    // --- 3. Draw Horizon Line ---
    // The horizon is just a pitch line at 0 degrees.
    // We draw it extra long to ensure it always crosses the screen.
    float horiz_unrot_y = (0 - g5State.pitchAngle) * PITCH_SCALE;
    float lineLength    = attitude.width() * 1.5;

    int16_t hx1 = CENTER_X + (-lineLength / 2.0) * cosBank - horiz_unrot_y * sinBank;
    int16_t hy1 = CENTER_Y + (-lineLength / 2.0) * sinBank + horiz_unrot_y * cosBank;
    int16_t hx2 = CENTER_X + (lineLength / 2.0) * cosBank - horiz_unrot_y * sinBank;
    int16_t hy2 = CENTER_Y + (lineLength / 2.0) * sinBank + horiz_unrot_y * cosBank;

    attitude.drawLine(hx1, hy1, hx2, hy2, HORIZON_COLOR);
    attitude.drawLine(hx1, hy1 + 1, hx2, hy2 + 1, HORIZON_COLOR); // Thicker line
}

void CC_G5_PFD::drawSpeedTrend()
{
    float st = speedTrend.getTrendValue();
    if (fabs(st) < 1.0f) return;
    attitude.fillRect(SPEED_COL_WIDTH, attitude.height() / 2, 4, (int)(st * -7.02), TFT_MAGENTA);
}

void CC_G5_PFD::drawSpeedTape()
{
    // Short cirucuiting here doesn't seem to help much and is complex.
    float drawSpeed = g5State.airspeed;

    if (g5State.airspeed < SPEED_ALIVE_SPEED) drawSpeed = 0.0; // The g5State.airspeed isn't displayed at low speed.

    int intDigits[7];

    // Convert to integer with one decimal place
    int scaled = (int)(drawSpeed * 10);
    // Serial.printf("a: %f s: %d 0.01s: %d\n",drawSpeed, scaled, scaled/100);

    // Extract as ints
    intDigits[4] = scaled / 100; // 100s and 10s
    intDigits[3] = scaled % 10;
    scaled /= 10; //   0.1s
    intDigits[2] = scaled % 10;
    scaled /= 10; //   1s
    intDigits[1] = scaled % 10;
    scaled /= 10;               //   10s
    intDigits[0] = scaled % 10; //   100s

    speedUnit.setTextDatum(CC_DATUM);
    int   digitHeight     = 37; // height/2 - 3
    float fractionalSpeed = drawSpeed - floor(drawSpeed);
    int   yBaseline       = 47 + (int)((fractionalSpeed - 0.1f) * digitHeight);
    //    int yBaseline   = 47 + (((intDigits[3] - 1) * (digitHeight)) / 10);
    int xOffset = speedUnit.width() / 2;

    speedUnit.fillSprite(TFT_BLACK);

    // Draw the rolling unit number on the right
    if (drawSpeed > SPEED_ALIVE_SPEED) {

        if (intDigits[3] > 7) speedUnit.drawNumber((intDigits[2] + 2) % 10, xOffset, yBaseline - digitHeight * 2);

        speedUnit.drawNumber((int)(intDigits[2] + 1) % 10, xOffset, yBaseline - digitHeight);
        speedUnit.drawNumber(intDigits[2], xOffset, yBaseline);
        speedUnit.drawNumber((int)(intDigits[2] + 9) % 10, xOffset, yBaseline + digitHeight);

    } else {
        speedUnit.drawString("-", xOffset, 44); // don't roll the -
    }

    xOffset        = speedTens.width() - 1;
    yBaseline      = speedTens.height() / 2 + 20;
    int digitWidth = 19;

    speedTens.setTextDatum(BR_DATUM);

    speedTens.fillSprite(TFT_BLACK);
    if (drawSpeed > SPEED_ALIVE_SPEED) {
        if (intDigits[2] == 9) {
            // yBaseline = yBaseline + (intDigits[3] * digitHeight) / 10; // Animate based on tenths.
            yBaseline = yBaseline + (int)(fractionalSpeed * digitHeight); // Animate based on tenths.
            speedTens.drawNumber(intDigits[4] + 1, xOffset, yBaseline - digitHeight);
            speedTens.drawNumber(intDigits[4], xOffset, yBaseline);
        } else {
            speedTens.drawNumber(intDigits[4], xOffset, yBaseline);
        }
    } else {
        speedTens.drawString("-", xOffset, yBaseline);
    }

    // Draw scrolling tape
    // relative to LCD

    digitHeight = 70;
    int yTop    = -80;
    int xRight  = SPEED_COL_WIDTH - 36;

    attitude.setTextDatum(CR_DATUM);
    attitude.setTextSize(0.9);
    attitude.setTextColor(TFT_LIGHTGRAY);

    for (int i = 0; i < 7; i++) {
        int curVal = (intDigits[4] + 4 - i) * 10; // Value to be displayed

        int tapeSpacing = digitHeight * (i) + ((intDigits[2] * 10 + intDigits[3]) * (digitHeight)) / 100;

        if (curVal <= 0) continue;

        attitude.drawNumber(curVal, xRight, yTop + tapeSpacing);
        attitude.drawFastHLine(xRight + 20, yTop + tapeSpacing, 15);      // major tick
        attitude.drawFastHLine(xRight + 25, yTop + tapeSpacing + 35, 10); // minor tick (only one on speed)
        // attitude.drawLine(xRight+20, yTop + tapeSpacing, xRight+35, yTop + tapeSpacing );  // Major tick
        // attitude.drawLine(xRight+25, yTop + tapeSpacing + 35, xRight+35, yTop + tapeSpacing + 35);  // Minor tick
    }
    // Draw the color bar...
    // Our entire display is: 70kts tall and it's 400px tall. That's 400px/70kts = 5.7px per kt. 200 = scaled.
    int barStart, barEnd, barWidth, barX;

    barWidth = 8;
    barX     = xRight + 30;

    // Too slow
    barStart = speedToY(0, drawSpeed);
    barEnd   = speedToY(g5Settings.Vs0, drawSpeed);
    attitude.fillRect(barX, barEnd, barWidth, barStart - barEnd, TFT_RED);

    // Green Arc
    barStart = speedToY(g5Settings.Vs1, drawSpeed);
    barEnd   = speedToY(g5Settings.Vno, drawSpeed);
    attitude.fillRect(barX, barEnd, barWidth, barStart - barEnd, TFT_GREEN);

    // Yellow arc
    barStart = speedToY(g5Settings.Vno, drawSpeed);
    barEnd   = speedToY(g5Settings.Vne, drawSpeed);
    attitude.fillRect(barX, barEnd, barWidth, barStart - barEnd, TFT_YELLOW);

    // Overspeed TODO Make this a barber pole
    barStart = speedToY(g5Settings.Vne, drawSpeed);
    barEnd   = speedToY(500, drawSpeed);
    attitude.fillRect(barX, barEnd, barWidth, barStart - barEnd, TFT_RED);

    // White flap arc
    barStart = speedToY(g5Settings.Vs0, drawSpeed);
    barEnd   = speedToY(g5Settings.Vfe, drawSpeed);
    attitude.fillRect(barX + 5, barEnd, barWidth - 5, barStart - barEnd, TFT_WHITE);

    // White arc (below green, not yet red)
    barStart = speedToY(g5Settings.Vs0, drawSpeed);
    barEnd   = speedToY(g5Settings.Vs1, drawSpeed);
    attitude.fillRect(barX, barEnd, barWidth, barStart - barEnd, TFT_WHITE);

    // Draw the boxes last.
    speedTens.pushSprite(SPEED_COL_WIDTH - 40 - speedTens.width(), 200 - speedTens.height() / 2); // Was 80
    speedUnit.pushSprite(SPEED_COL_WIDTH - 40, 200 - speedUnit.height() / 2);

    // Draw the true airspeed box
    attitude.fillRect(0, 0, SPEED_COL_WIDTH, 40, TFT_BLACK);
    attitude.drawRect(0, 0, SPEED_COL_WIDTH, 40, TFT_WHITE);
    attitude.drawRect(1, 1, SPEED_COL_WIDTH - 2, 40 - 2, TFT_WHITE);
    attitude.setTextColor(TFT_WHITE, TFT_BLACK);
    attitude.setTextSize(0.5);
    attitude.setTextDatum(CL_DATUM);
    attitude.drawString("TAS", 5, 19);
    attitude.setTextSize(0.8);
    attitude.setTextDatum(CR_DATUM);
    char buf[8];
    sprintf(buf, "%.0f", g5State.trueAirspeed);
    if (g5State.trueAirspeed > 10) attitude.drawString(buf, SPEED_COL_WIDTH - 5, 20);
}

void CC_G5_PFD::drawSpeedPointers()
{
    // Construct the list of vSpeed pointers.
    const struct {
        char label;
        int  speed;
        int  order;
    } speed_pointers[] = {
        {'R', g5Settings.Vr, 0}, {'X', g5Settings.Vx, 1}, {'Y', g5Settings.Vy, 2}, {'G', g5Settings.Vg, 3}, {'A', g5Settings.Va, 4}};

    for (const auto &pointer : speed_pointers) {

        if (pointer.speed == 0 || (pointer.speed > g5State.airspeed + 30 && g5State.airspeed > (speed_pointers[0].speed) - 30)) continue; // Short circuit if off screen.

        speedPointer.drawBitmap(0, 0, SPEEDPOINTER_IMG_DATA, SPEEDPOINTER_IMG_WIDTH, SPEEDPOINTER_IMG_HEIGHT, TFT_WHITE, TFT_BLACK);
        speedPointer.drawChar(pointer.label, 10, 2);
        // If the g5State.airspeed is below the first speed, then show them at the bottom.
        // Update, actually only show them this way if g5State.airspeed not alive.
        int yPos = 0;
        //        if (g5State.airspeed < (speed_pointers[0].speed - 30)) {
        if (g5State.airspeed < 20) {
            yPos = (ATTITUDE_HEIGHT - 30) - (pointer.order * 30);
            attitude.setTextColor(TFT_CYAN);
            attitude.setTextSize(0.6);
            attitude.setTextDatum(CR_DATUM);

            attitude.drawNumber(pointer.speed, SPEED_COL_WIDTH - 4, yPos);
        } else {
            yPos = speedToY(pointer.speed, g5State.airspeed) + 9;
        }
        speedPointer.pushSprite(SPEED_COL_WIDTH, yPos - 10, TFT_WHITE);
    }
}

int CC_G5_PFD::speedToY(float targetSpeed, float curSpeed)
{
    // Our entire sprite is: 57kts tall and it's 400px tall. That's 400px/57kts = 7.02px per kt. 200 = scaled value.
    return (int)(200.0 + (curSpeed - targetSpeed) * 7.02f);
}

int CC_G5_PFD::altToY(int targetAlt, int curAlt)
{
    // Our entire sprite is: 330' tall and it's 400px tall. That's 400px/330' = 1.2'/px 200 = center point.
    return (int)(248.0 + (curAlt - targetAlt) * 1.21f);
}

inline int floorMod(int a, int b)
{
    return ((a % b) + b) % b;
}

void CC_G5_PFD::drawDensityAlt()
{
    // Only displayed on the ground.
    if (g5State.airspeed > SPEED_ALIVE_SPEED) return;

    // We can reuse the kohlsbox here.
    kohlsBox.fillSprite(TFT_BLACK);
    kohlsBox.drawRect(0, 0, ALTITUDE_COL_WIDTH, 40, TFT_WHITE);
    kohlsBox.drawRect(1, 1, ALTITUDE_COL_WIDTH - 2, 38, TFT_WHITE);
    kohlsBox.setTextDatum(TC_DATUM);
    kohlsBox.setTextSize(0.5);
    kohlsBox.setTextColor(TFT_WHITE);
    kohlsBox.drawString("DENSITY ALT", ALTITUDE_COL_WIDTH / 2, 4);

    char buf[8];
    sprintf(buf, "%d", g5State.densityAltitude);
    kohlsBox.setTextDatum(BC_DATUM);
    kohlsBox.setTextSize(0.5);
    kohlsBox.drawString(buf, ALTITUDE_COL_WIDTH / 2, 37);
    kohlsBox.pushSprite(&attitude, ATTITUDE_WIDTH - ALTITUDE_COL_WIDTH, ATTITUDE_HEIGHT - kohlsBox.height());
}

void CC_G5_PFD::drawAltTape()
{
    // Need to push the sprites even if we don't recalc. Need to push the numbers too. this saves 3ms, but is a mess.
    // static int lastAlt = -999;
    // if (lastAlt == g5State.altitude) {
    //     vsScale.pushSprite(attitude.width() - vsScale.width(), 0, TFT_BLACK); // Scale drawn over the vs bar
    //     // Draw the pointer

    //     vsPointer.pushSprite(ATTITUDE_WIDTH - VSPOINTER_IMG_WIDTH, 200 - VSPOINTER_IMG_HEIGHT / 2 - (int)(g5State.verticalSpeed * 0.131f), LGFX::color565(0x20, 0x20, 0x20));

    //     // Draw the boxes last.

    //     altUnit.pushSprite(attitude.width() - altUnit.width() - 15, (attitude.height() - altUnit.height()) / 2);
    //     altTens.pushSprite(attitude.width() - altUnit.width() - altTens.width() - 14, (attitude.height() - altTens.height()) / 2);
    //     return;
    // }
    // lastAlt = g5State.altitude;

    int intDigits[7];

    // Serial.printf("a: %f s: %d 0.01s: %d\n",g5State.airspeed, scaled, scaled/100);
    int scaled = g5State.altitude;

    // Extract as ints
    intDigits[1] = scaled % 10;           //   1s
    intDigits[2] = (scaled / 10) % 10;    //   10s digit
    intDigits[3] = (scaled / 100) % 10;   // hundreds of feet digit
    intDigits[4] = (scaled / 1000) % 10;  // thousands of feet
    intDigits[5] = (scaled / 10000) % 10; // tens of thousands of feet

    int yOffset = altUnit.height() / 2;
    int xOffset = 2;

    // For the tape show every 100 feet. 200 feet greater and 200 feet less than current.
    altUnit.fillSprite(TFT_BLACK);

    altUnit.setTextDatum(BL_DATUM);
    int digitHeight = 36; // height/2 - 3
                          //    int yBaseline   = 40 + 2 + (((intDigits[1] - 1) * (digitHeight)) / 20);

    speedUnit.fillSprite(TFT_BLACK);

    // Draw the rolling unit number on the right This is the twenties with the 1s defining the top.
    // int roundUpToNext20(int n) { return ( (n + 19) / 20 ) * 20; }

    // int dispUnit = (intDigits[2] + (intDigits[2] % 2)) * 10; // make an even number.
    int dispUnit = ((int)g5State.altitude / 20) * 20; // round down to 20-ft band

    //    yBaseline = yBaseline - ((intDigits[2] % 2) * digitHeight / 2);
    // each digit height is 20'... so each foot it 36/20 = 1.8f. Offset by 1/2 the height: 54
    int  yBaseline = (int)(54 + (fmodf(g5State.altitude, 20.0f) * 1.8f));
    char buf[8];
    sprintf(buf, "%02d", abs((dispUnit + 20) % 100));
    // sprintf(buf, "%02d", floorMod(dispUnit + 20, 100));
    // Serial.printf("scaled: %d, intDigits[2]: %d, dispUnit: %d, buf: %s\n", scaled, intDigits[2], dispUnit, buf);
    altUnit.drawString(buf, xOffset, yBaseline - digitHeight);
    sprintf(buf, "%02d", abs(dispUnit % 100));
    // sprintf(buf, "%02d", floorMod(dispUnit, 100));
    altUnit.drawString(buf, xOffset, yBaseline);
    sprintf(buf, "%02d", abs((dispUnit - 20) % 100));
    // sprintf(buf, "%02d", floorMod(dispUnit -20, 100));
    altUnit.drawString(buf, xOffset, yBaseline + digitHeight);

    int digitWidth = 19;
    altTens.fillSprite(TFT_BLACK);

    altTens.setTextSize(g5State.altitude < 1000 ? 1.0 : 0.8);
    xOffset = altTens.width() - 1;
    yOffset = altTens.height() / 2; // Base offset without rolling!
    altTens.setTextDatum(CR_DATUM);

    // Roll the hundreds.
    if (g5State.altitude >= 80 || g5State.altitude < -90) { // Don't draw a leading 0
                                                            //  if ((g5State.altitude>0 && intDigits[2] >= 8) || (g5State.altitude<0 && intDigits[2] <= 1)) {
        if (abs(intDigits[2]) >= 8) {
            yOffset = yOffset - (int)((20.0f - fmodf(g5State.altitude, 20.0f)) * 1.8f); // 1.8f is height/2
            altTens.drawNumber(intDigits[3] + 1, xOffset, yOffset);
            if (g5State.altitude > 100) altTens.drawNumber(intDigits[3], xOffset, yOffset + digitHeight);
            altTens.drawNumber(intDigits[3] - 1, xOffset, yOffset + digitHeight * 2);
        } else
            altTens.drawNumber(intDigits[3], xOffset, yOffset);
    }

    xOffset = altTens.width() - 1 - digitWidth;

    // roll the thousands
    if (g5State.altitude / 1000 > 0) {
        altTens.setTextSize(1.0);
        if (intDigits[3] == 9 && intDigits[2] >= 8) {
            //          yOffset = yOffset - (20 - (g5State.altitude % 20))*(1.8f);  // 1.8f is height/2
            altTens.drawNumber(intDigits[4] + 1, xOffset, yOffset);
            altTens.drawNumber(intDigits[4], xOffset, yOffset + digitHeight);
        } else
            altTens.drawNumber(intDigits[4], xOffset, altTens.height() / 2);
    }

    // roll the ten thousands
    xOffset = altTens.width() - 1 - digitWidth * 2;
    if (g5State.altitude / 10000 > 0) {
        if (intDigits[4] == 9 && intDigits[3] == 9 && intDigits[2] >= 8) {
            altTens.drawNumber(intDigits[5] + 1, xOffset, yOffset);
            altTens.drawNumber(intDigits[5], xOffset, yOffset);
        } else
            altTens.drawNumber(intDigits[5], xOffset, altTens.height() / 2);
    }

    // Draw scrolling tape
    // relative to LCD
    digitHeight = 120;
    int yTop    = -40;
    int xRight  = attitude.width() - altUnit.width() - altTens.width();

    attitude.setTextDatum(CL_DATUM);
    attitude.setTextSize(0.8);
    attitude.setTextColor(TFT_LIGHTGRAY);

    // Draw the background tape
    for (int i = -1; i < 4; i++) {
        int curVal = ((int)g5State.altitude / 100 + 2 - i) * 100; // Value to be displayed (100's)

        int tapeSpacing = digitHeight * (i) + ((intDigits[2] * 10 + intDigits[1]) * (digitHeight)) / 100;

        // TODO If alt is above 1000, then the hundreds and tens should be smaller.

        // If target alt is on screen, we'll draw it below.
        if (curVal != g5State.targetAltitude) attitude.drawNumber(curVal, xRight, yTop + tapeSpacing);
        // attitude.drawLine(xRight - 30,yTop + tapeSpacing, xRight-15, yTop + tapeSpacing); // Major Tick
        attitude.drawFastHLine(xRight - 30, yTop + tapeSpacing, 15, TFT_WHITE); // Major Tick
        for (int j = 1; j < 5; j++) {
            attitude.drawFastHLine(xRight - 30, yTop + tapeSpacing + j * 24, 10, TFT_WHITE); // Minor Tick
            // attitude.drawLine(xRight - 30,yTop + tapeSpacing + j*24, xRight-20, yTop + tapeSpacing+ j*24); // Minor Tick
        }
    }

    // Draw the bug over the tape.
    // If the target g5State.altitude is off the scale, draw it at the boundary.
    int bugPos = altToY(g5State.targetAltitude, (int)g5State.altitude) - 5;
    // but that's in terms of the attitude sprite,
    // Serial.printf("target: %d, alt: %d, bugpos: %d\n", g5State.targetAltitude, g5State.altitude, bugPos);
    int offset = 61;
    if (bugPos < HEADINGBUG_IMG_HEIGHT / 2) bugPos = HEADINGBUG_IMG_WIDTH / 2 + offset; // Yes, width. image is sideways.
    if (bugPos > ATTITUDE_HEIGHT - HEADINGBUG_IMG_WIDTH / 2) bugPos = ATTITUDE_HEIGHT - HEADINGBUG_IMG_HEIGHT / 2 + offset;
    // Serial.printf("target: %d, alt: %d, bugpos: %d\n", g5State.targetAltitude, g5State.altitude, bugPos);

    attitude.setPivot(xRight - 23, bugPos);
    if (g5State.targetAltitude != 0) altBug.pushRotated(90, TFT_WHITE);
    attitude.setTextColor(TFT_CYAN);
    attitude.drawNumber(g5State.targetAltitude, xRight, altToY(g5State.targetAltitude, (int)g5State.altitude) - 49);

    // Draw the vertical speed scale
    yTop = 200;
    // 131 pixels is 1000fpm so 1fpm is 0.131 pixel
    int barHeight = abs((int)(g5State.verticalSpeed * 0.131f));
    if (g5State.verticalSpeed > 0) yTop = 200 - barHeight;
    attitude.fillRect(475, yTop, 5, barHeight, TFT_MAGENTA);              // push to attitude to avoid a refill of vsScale.
    vsScale.pushSprite(attitude.width() - vsScale.width(), 0, TFT_BLACK); // Scale drawn over the vs bar
    // Draw the pointer

    vsPointer.pushSprite(ATTITUDE_WIDTH - VSPOINTER_IMG_WIDTH, 200 - VSPOINTER_IMG_HEIGHT / 2 - (int)(g5State.verticalSpeed * 0.131f), LGFX::color565(0x20, 0x20, 0x20));

    // Draw the boxes last.

    altUnit.pushSprite(attitude.width() - altUnit.width() - 15, (attitude.height() - altUnit.height()) / 2);
    altTens.pushSprite(attitude.width() - altUnit.width() - altTens.width() - 14, (attitude.height() - altTens.height()) / 2);
}

void CC_G5_PFD::drawHorizonMarker()
{
    horizonMarker.pushSprite(240 - HORIZONMARKER_IMG_WIDTH / 2, 194, LGFX::color332(0x20, 0x20, 0x20));
}

// This is the box in the lower right.
void CC_G5_PFD::drawKohlsman()
{
    // Cyan box in lower right
    // Should move some of this to setup, but the value doesn't change often.

    // static float lastVal = 0.0;
    // if (g5State.kohlsman == lastVal || g5State.forceRedraw) return;
    // lastVal = g5State.kohlsman;
    kohlsBox.fillSprite(TFT_BLACK);
    kohlsBox.drawRect(0, 0, ALTITUDE_COL_WIDTH, 40, TFT_CYAN);
    kohlsBox.drawRect(1, 1, ALTITUDE_COL_WIDTH - 2, 38, TFT_CYAN);
    kohlsBox.setTextDatum(CR_DATUM);
    kohlsBox.setTextSize(0.5);
    kohlsBox.setTextColor(TFT_CYAN);

    if (g5State.kohlsman > 100) {
        // kohlsBox.drawString("m", 122, 12);
        kohlsBox.drawString("hPa", 124, 25);

        char buf[8];
        sprintf(buf, "%.0f", g5State.kohlsman);
        kohlsBox.setTextSize(0.9);
        kohlsBox.drawString(buf, 90, 23); // units in hPa
    } else {

        kohlsBox.drawString("i", 119, 13);
        kohlsBox.drawString("n", 122, 25);

        char buf[8];
        sprintf(buf, "%.2f", g5State.kohlsman);
        kohlsBox.setTextSize(0.9);
        kohlsBox.drawString(buf, 100, 20);
    }
    kohlsBox.pushSprite(ATTITUDE_WIDTH - ALTITUDE_COL_WIDTH, 440);

    // Serial.printf("Pushing kohlsBox to x:%d, y:%d\n",lcd.width()-kohlsBox.width(), lcd.height() - kohlsBox.height());
}

// This is the box in the upper right.
void CC_G5_PFD::drawAltTarget()
{
    // Altitude Alerting per G5 Manual - State Machine Implementation
    // Flash pattern: 0.8 sec on, 0.2 sec off
    //
    // States:
    //   IDLE: Far from target (>1000')
    //   WITHIN_1000: Within 1000' but not within 200' (flash cyan once)
    //   WITHIN_200: Within 200' but not captured (flash cyan once)
    //   CAPTURED: Within ±100' of target (altitude captured)
    //   DEVIATED: Was captured, now outside ±200' (flash yellow, stay yellow)

    // We should be able to short cirucuit this one, but the changing altitude makes it more of a challenge.

    static int lastTargetAlt = -9999;

    // If target altitude changes, reset state machine
    if (lastTargetAlt != g5State.targetAltitude) {
        altAlertState  = ALT_IDLE;
        altAlertActive = false;
        lastTargetAlt  = g5State.targetAltitude;
    }

    int altDiff = (int)fabsf(g5State.altitude - (float)g5State.targetAltitude);

    // State machine transitions
    AltAlertState previousState = altAlertState;

    switch (altAlertState) {
    case ALT_IDLE:
        if (altDiff <= ALT_ALERT_1000_THRESHOLD) {
            altAlertState = ALT_WITHIN_1000;
        }
        break;

    case ALT_WITHIN_1000:
        if (altDiff > ALT_ALERT_1000_THRESHOLD) {
            altAlertState = ALT_IDLE;
        } else if (altDiff <= ALT_ALERT_200_THRESHOLD) {
            altAlertState = ALT_WITHIN_200;
        }
        break;

    case ALT_WITHIN_200:
        if (altDiff > ALT_DEVIATION_THRESHOLD) {
            // Left 200' band before capturing - return to appropriate state
            altAlertState = (altDiff > ALT_ALERT_1000_THRESHOLD) ? ALT_IDLE : ALT_WITHIN_1000;
        } else if (altDiff <= ALT_CAPTURE_THRESHOLD) {
            altAlertState = ALT_CAPTURED;
        }
        break;

    case ALT_CAPTURED:
        if (altDiff > ALT_DEVIATION_THRESHOLD) {
            // Deviated from captured altitude
            altAlertState = ALT_DEVIATED;
        }
        // Stay in CAPTURED even if we drift within 100-200' range
        break;

    case ALT_DEVIATED:
        if (altDiff <= ALT_DEVIATION_THRESHOLD) {
            // Returned within deviation band - recaptured
            altAlertState = (altDiff <= ALT_CAPTURE_THRESHOLD) ? ALT_CAPTURED : ALT_WITHIN_200;
        }
        break;
    }

    // Trigger alerts on state transitions
    if (altAlertState != previousState) {
        switch (altAlertState) {
        case ALT_WITHIN_1000:
            // Crossed 1000' threshold - flash cyan
            altAlertActive = true;
            alertStartTime = millis();
            alertColor     = TFT_CYAN;
            break;

        case ALT_WITHIN_200:
            if (previousState == ALT_WITHIN_1000) {
                // Crossed 200' threshold - flash cyan
                altAlertActive = true;
                alertStartTime = millis();
                alertColor     = TFT_CYAN;
            } else if (previousState == ALT_DEVIATED) {
                // Returned within 200' after deviation - flash cyan
                altAlertActive = true;
                alertStartTime = millis();
                alertColor     = TFT_CYAN;
            }
            break;

        case ALT_CAPTURED:
            if (previousState == ALT_DEVIATED) {
                // Returned to capture after deviation - flash cyan
                altAlertActive = true;
                alertStartTime = millis();
                alertColor     = TFT_CYAN;
            }
            // No alert when first capturing from WITHIN_200
            break;

        case ALT_DEVIATED:
            // Deviated from captured altitude - flash yellow
            altAlertActive = true;
            alertStartTime = millis();
            alertColor     = TFT_YELLOW;
            break;

        default:
            break;
        }
    }

    // Stop alert after 5 seconds
    if (altAlertActive && (millis() - alertStartTime) > 5000) {
        altAlertActive = false;
    }

    // Determine display color
    uint16_t textColor = TFT_CYAN; // Default color

    // Yellow steady state when deviated
    if (altAlertState == ALT_DEVIATED) {
        textColor = TFT_YELLOW;
    }

    // Flash override (0.8s on, 0.2s off)
    if (altAlertActive) {
        bool isVisible = ((millis() - alertStartTime) % 1000) < 800;
        if (!isVisible) {
            textColor = TFT_BLACK; // Blink off
        } else {
            textColor = alertColor; // Use alert color (CYAN or YELLOW)
        }
    }

    // Format the altitude text
    char buf[10];
    if (g5State.targetAltitude != 0) {
        sprintf(buf, "%d", g5State.targetAltitude);
    } else {
        strcpy(buf, "- - - -");
    }

    // targetAltBox.fillSprite(TFT_BLACK);
    // targetAltBox.drawRect(0, 0, targetAltBox.width(), targetAltBox.height(), TFT_LIGHTGRAY);
    // targetAltBox.drawRect(1, 1, targetAltBox.width()-2, targetAltBox.height()-2, TFT_LIGHTGRAY);
    // targetAltBox.drawBitmap()

    // Clear previous text and draw new
    targetAltBox.fillRect(22, 3, 88, 34, TFT_BLACK);
    targetAltBox.setTextColor(textColor);
    targetAltBox.drawString(buf, 110, 18);

    targetAltBox.pushSprite(&attitude, ATTITUDE_WIDTH - ALTITUDE_COL_WIDTH, 0);
}

void CC_G5_PFD::drawGroundSpeed()
{
    // Magenta in box in lower left.

    static int lastGs = 399;

    if (g5State.groundSpeed > 500) return;

    //    if (lastGs == g5State.groundSpeed) return;

    lastGs = g5State.groundSpeed;

    char buf[8];

    gsBox.fillSprite(TFT_BLACK);
    gsBox.drawRect(0, 0, SPEED_COL_WIDTH, gsBox.height(), TFT_LIGHTGREY);
    gsBox.drawRect(1, 1, SPEED_COL_WIDTH - 2, gsBox.height() - 2, TFT_LIGHTGREY);

    /*
    gsBox.setTextDatum(CR_DATUM);
    gsBox.setTextSize(0.5);
    gsBox.setTextColor(TFT_DARKGRAY);
    gsBox.drawString("GS", 28, 28);
    gsBox.setTextColor(TFT_MAGENTA);
    gsBox.drawString("k", 110, 13);
    gsBox.drawString("t", 109, 26);
    gsBox.setTextSize(0.9);
    gsBox.drawString(buf, 90, 21);
    gsBox.pushSprite(0, 440);
    */

    // Separator line
    gsBox.drawFastHLine(2, gsBox.height() / 2, SPEED_COL_WIDTH - 4);
    gsBox.drawFastHLine(2, gsBox.height() / 2 - 1, SPEED_COL_WIDTH - 4);

    gsBox.setTextDatum(CL_DATUM);
    gsBox.setTextSize(0.5);
    gsBox.setTextColor(TFT_LIGHTGRAY);
    gsBox.drawString("GS", 10, 10);
    gsBox.drawString("OAT", 12, 30);
    gsBox.setTextDatum(CR_DATUM);
    sprintf(buf, "%d\xB0", g5State.oat);
    gsBox.setTextSize(0.5);
    gsBox.drawString(buf, 90, 30);

    sprintf(buf, "%d", g5State.groundSpeed);
    gsBox.setTextColor(TFT_MAGENTA);
    gsBox.drawString(buf, 80, 10);

    gsBox.pushSprite(0, 440);

    return;
}

void CC_G5_PFD::drawBall()
{
    // Draw the ball and the turn rate. The g5State.ballPos goes from -1.0 (far right) to 1.0 (far left)
    int turnBarCenter = CENTER_COL_CENTER;
    turnBar.fillSprite(GND_COLOR);

    int ballXOffset = (int)(g5State.ballPos * BALL_IMG_WIDTH * 1.8f);                     // This 1.8 factor can vary by plane. The comanche is backwards!
    ballSprite.pushSprite(turnBarCenter - ballSprite.width() / 2 + ballXOffset, 0, 0xC2); // The transparent color is an odd one here. 0xC2 works

    // Draw the ball cage
    turnBar.drawRect(turnBarCenter - 20 - 3, 0, 6, 32, TFT_BLACK);
    turnBar.fillRect(turnBarCenter - 20 - 2, 0, 4, 30, TFT_WHITE);
    turnBar.fillRect(turnBarCenter + 20 - 2, 0, 4, 30, TFT_WHITE);
    turnBar.drawRect(turnBarCenter + 20 - 3, 0, 6, 32, TFT_BLACK);

    // if(! millis() % 100) Serial.printf("g5State.ballPos: %f ballXOffset: %d\n", g5State.ballPos, ballXOffset);

    // Draw the turn rate bar and markers.
    // Turn rate is in degrees per sec. +3 (right) or -3 (left) is std turn. (full 360 in 120 seconds)
    turnBar.fillRect(turnBarCenter - 1 - 70, 34, 2, 6, TFT_WHITE);
    turnBar.fillRect(turnBarCenter - 1 + 70, 34, 2, 6, TFT_WHITE);
    turnBar.fillRect(turnBarCenter - 1, 34, 3, 6, TFT_DARKGREY);

    // 3 degrees is 69 pixels: 23 pix per degree
    int turnRateWidth = (int)(g5State.turnRate * 23);
    turnBar.fillRect(min(turnRateWidth + turnBarCenter, turnBarCenter), 34, abs(turnRateWidth), 6, TFT_MAGENTA);

    // Draw the message indicator.
    if (lastMFUpdate < (millis() - 3000)) messageIndicator.pushSprite(20, 3);

    turnBar.pushSprite(SPEED_COL_WIDTH, 440);
}

int CC_G5_PFD::headingToX(float targetHeading, float currentHeading)
{

    // Serial.printf("htoX: %f %f: %d", targetHeading, currentHeading, (int)(CENTER_COL_CENTER + (targetHeading - currentHeading) * 6.6f));
    // return (int)(CENTER_COL_CENTER + ((targetHeading  - currentHeading) * 6.6f);

    // return (int)(CENTER_COL_CENTER + (((int)(targetHeading - currentHeading + 180) % 360) - 180) * 7.8f);
    float diff = targetHeading - currentHeading;
    while (diff > 180.0f)
        diff -= 360.0f;
    while (diff < -180.0f)
        diff += 360.0f;
    return (int)(CENTER_COL_CENTER + diff * 6.8f);
}

static inline int incrementHeading(int heading, int delta)
{
    return ((heading + delta - 1) % 360 + 360) % 360 + 1;
}

void CC_G5_PFD::drawHeadingTape()
{

    // Our entire sprite is: CENTER_COL_WIDTH and covers 36.8 degrees That's 250/32' = 6.8f px per degree
    const float DEGREES_DISPLAYED = 36.8f;
    const float PX_PER_DEGREE     = 6.8f;

    // static int lastHeading = -1;
    // if (lastHeading == (int) g5State.headingAngle) return;
    // lastHeading = g5State.headingAngle;
    int tapeCenter  = CENTER_COL_CENTER;
    int scaleOffset = (int)(fmod(g5State.headingAngle, 5.0f) * PX_PER_DEGREE - 8);
    // int xOffset     = (int)g5State.headingAngle % 10 * 7 + 17;
    int xOffset = (int)(fmod(g5State.headingAngle, 10.0f) * PX_PER_DEGREE) + 17;
    headingTape.fillSprite(DARK_SKY_COLOR);

    // Draw the tape scale
    hScale.pushSprite(0 - scaleOffset, 40 - HSCALE_IMG_HEIGHT, TFT_BLACK);
    int baseHeading = ((int)g5State.headingAngle / 10) * 10;

    headingTape.setTextSize(0.5);
    char buf[5];
    sprintf(buf, "%03d", incrementHeading(baseHeading, -10));
    headingTape.drawString(buf, CENTER_COL_CENTER - xOffset - 53, 20);
    sprintf(buf, "%03d", incrementHeading(baseHeading, 0));
    headingTape.drawString(buf, CENTER_COL_CENTER - xOffset + 15, 20);
    sprintf(buf, "%03d", incrementHeading(baseHeading, 10));
    headingTape.drawString(buf, CENTER_COL_CENTER - xOffset + 83, 20);
    sprintf(buf, "%03d", incrementHeading(baseHeading, 20));
    headingTape.drawString(buf, CENTER_COL_CENTER - xOffset + 151, 20);

    // Serial.printf("h: %f, bH: %d, sO: %d, xO: %d, v1: %d x1: %d v2: %d x2: %d\n",g5State.headingAngle, baseHeading, scaleOffset, xOffset, (baseHeading + 350) % 360, 115 - xOffset - 20, (baseHeading + 10) % 360,115 - xOffset + 82 );

    // Draw Nav Course to Steer... but only  if in gps mode. Otherwise draw the CRS.
    // FIX: This should be written to the LCD, not the headingTape.
    if (g5State.navSource == NAVSOURCE_GPS)
        headingTape.fillRect(headingToX(g5State.desiredTrack, g5State.headingAngle) - 2, 30, 4, 10, TFT_GREEN);
    else
        headingTape.fillRect(headingToX(g5State.navCourse, g5State.headingAngle) - 2, 30, 4, 10, TFT_GREEN);

    // Serial.printf("ncs %d, x: %d\n",g5State.desiredTrack, headingToX(g5State.desiredTrack, g5State.headingAngle));

    // Draw the Ground Course triangle
    headingTape.drawBitmap(headingToX(g5State.groundTrack, g5State.headingAngle) - 9, 32, POINTER_IMG_DATA, POINTER_IMG_WIDTH, POINTER_IMG_HEIGHT, TFT_MAGENTA);

    // Draw the Heading Bug
    int headingBugOffset = headingToX((float)g5State.headingBugAngle, g5State.headingAngle) - HEADINGBUG_IMG_WIDTH / 2;
    if (headingBugOffset < 0 - HEADINGBUG_IMG_WIDTH / 2) headingBugOffset = 0 - HEADINGBUG_IMG_WIDTH / 2;
    if (headingBugOffset > headingTape.width() - HEADINGBUG_IMG_WIDTH / 2) headingBugOffset = headingTape.width() - HEADINGBUG_IMG_WIDTH / 2;
    //  Serial.printf("Offset: %d\n", headingBugOffset);
    //    altBug.pushSprite(&headingTape, headingBugOffset, headingTape.height() - HEADINGBUG_IMG_HEIGHT, TFT_WHITE);
    altBug.pushRotateZoom(&headingTape, headingBugOffset + 51, headingTape.height() - HEADINGBUG_IMG_HEIGHT + 10, 0, 0.7, 0.7, TFT_WHITE);

    // Draw the heading box and current heading
    headingTape.drawRect(tapeCenter - 24, 0, 48, 27, TFT_LIGHTGRAY);
    headingTape.fillRect(tapeCenter - 23, 1, 46, 25, TFT_BLACK);
    sprintf(buf, "%03d", (int)roundf(g5State.headingAngle));
    headingTape.setTextSize(0.7);
    headingTape.drawString(buf, tapeCenter, 14);
    headingTape.pushSprite(&attitude, SPEED_COL_WIDTH, 0);
}

void CC_G5_PFD::drawGlideSlope()
{

    if (!g5State.gsiNeedleValid) return;

    // To Do: Set appropriate bug position
    //        Figure out if we skip this because wrong mode.
    //        Set right text and colors for ILS vs GPS.

    // Positive number, needle deflected down. Negative, up.

    // Change of direction. Use HSI GSI NEEDLE:1 simvar. -127 to 127.

    const float scaleMax    = 127.0;
    const float scaleMin    = -127.0;
    const float scaleOffset = 20.0; // Distance the scale starts from top of sprite.

    // Refill the sprite to overwrite old diamond.
    glideDeviationScale.fillSprite(TFT_BLACK);
    glideDeviationScale.pushImage(0, 0, GSDEVIATION_IMG_WIDTH, GSDEVIATION_IMG_HEIGHT, GSDEVIATION_IMG_DATA);

    int markerCenterPosition = (int)(scaleOffset + ((g5State.gsiNeedle + scaleMax) * (190.0 / (scaleMax - scaleMin))) - (deviationDiamond.height() / 2.0));

    if (g5State.navSource == NAVSOURCE_GPS) {
        glideDeviationScale.setTextColor(TFT_MAGENTA);
        glideDeviationScale.drawString("G", glideDeviationScale.width() / 2, 12);
        glideDeviationScale.drawBitmap(1, markerCenterPosition, DIAMONDBITMAP_IMG_DATA, DIAMONDBITMAP_IMG_WIDTH, DIAMONDBITMAP_IMG_HEIGHT, TFT_MAGENTA);
    } else {
        glideDeviationScale.setTextColor(TFT_GREEN);
        glideDeviationScale.drawString("L", glideDeviationScale.width() / 2, 12);
        glideDeviationScale.drawBitmap(1, markerCenterPosition, DIAMONDBITMAP_IMG_DATA, DIAMONDBITMAP_IMG_WIDTH, DIAMONDBITMAP_IMG_HEIGHT, TFT_GREEN);
    }

    glideDeviationScale.pushSprite(&attitude, SPEED_COL_WIDTH + CENTER_COL_WIDTH - glideDeviationScale.width() - 1, ATTITUDE_HEIGHT / 2 - glideDeviationScale.height() / 2);
}

void CC_G5_PFD::drawCDIBar()
{
    // Full scale offset can be configured on the real unit. Here we will use 5 miles full scale
    // either side, except in terminal mode when it is 1 mile full deflection.

    // This code works for GPS or NAV. We set the color in setNavSource.
    // Needle deflection (+/- 127)
    // Total needle deflection value: 254.
    // Scale width cdiBar.width(); (pixels)

    if (!g5State.cdiNeedleValid) return;

    const float scaleMax    = 127.0;
    const float scaleMin    = -127.0;
    const float scaleOffset = 0.0; // Distance the scale starts from middle of sprite.

    // Refill the sprite to overwrite old diamond.

    // GPS Mode: Magenta triangle always pointing to.
    // ILS Mode: Green diamond.
    // VOR Mode: Green triangle To/From

    horizontalDeviationScale.fillSprite(TFT_BLACK);
    horizontalDeviationScale.pushImage(0, 0, HORIZONTALDEVIATIONSCALE_IMG_WIDTH, HORIZONTALDEVIATIONSCALE_IMG_HEIGHT, HORIZONTALDEVIATIONSCALE_IMG_DATA);
    int markerCenterPosition = (int)(scaleOffset + ((g5State.cdiOffset + scaleMax) * (190.0 / (scaleMax - scaleMin))) - (BANKANGLEPOINTER_IMG_WIDTH / 2));

    if (g5State.navSource == NAVSOURCE_GPS) {
        // always the magenta triangle
        horizontalDeviationScale.drawBitmap(markerCenterPosition, 2, BANKANGLEPOINTER_IMG_DATA, BANKANGLEPOINTER_IMG_WIDTH, BANKANGLEPOINTER_IMG_HEIGHT, TFT_MAGENTA);
        horizontalDeviationScale.pushSprite(&attitude, CENTER_COL_CENTER, 360);

    } else {
        // ILS or VOR or LOC is a green diamond
        if (g5State.gpsApproachType == 4 || g5State.gpsApproachType == 2 || g5State.gpsApproachType == 5) {

            // diamond
            horizontalDeviationScale.drawBitmap(markerCenterPosition, 2, DIAMONDBITMAP_IMG_DATA, DIAMONDBITMAP_IMG_WIDTH, DIAMONDBITMAP_IMG_HEIGHT, TFT_GREEN);
            horizontalDeviationScale.pushSprite(&attitude, CENTER_COL_CENTER, 360);
        } else {
            // Triangle with to/from
            if (g5State.cdiToFrom == 1) {
                // To
                horizontalDeviationScale.drawBitmap(markerCenterPosition, 2, BANKANGLEPOINTER_IMG_DATA, BANKANGLEPOINTER_IMG_WIDTH, BANKANGLEPOINTER_IMG_HEIGHT, TFT_GREEN);
                horizontalDeviationScale.pushSprite(&attitude, CENTER_COL_CENTER, 360);
            } else {
                // From
                horizontalDeviationScale.drawBitmap(HORIZONTALDEVIATIONSCALE_IMG_WIDTH - markerCenterPosition, 2, BANKANGLEPOINTER_IMG_DATA, BANKANGLEPOINTER_IMG_WIDTH, BANKANGLEPOINTER_IMG_HEIGHT, TFT_GREEN);
                lcd.setPivot(CENTER_COL_CENTER, 360 - HORIZONMARKER_IMG_HEIGHT / 2);
                horizontalDeviationScale.setPivot(HORIZONMARKER_IMG_WIDTH / 2, HORIZONMARKER_IMG_HEIGHT / 2);
                horizontalDeviationScale.pushRotated(180);
            }
        }
    }
}

void CC_G5_PFD::drawAp()
{

    static bool lastAPState = false;

    apBox.fillSprite(TFT_BLACK);
    // Draw Lateral Mode (ROL, HDG, TRK, GPS/VOR/LOC (nav mode), GPS/LOC/BC (Nav movde), TO/GA (Toga))

    apBox.drawFastVLine(152, 0, apBox.height(), TFT_LIGHTGRAY);
    apBox.drawFastVLine(240, 0, apBox.height(), TFT_LIGHTGRAY);

    // if (!g5State.apActive && !g5State.flightDirectorActive) {
    //     // Nothing on the screen.
    //     apBox.pushSprite(0, 5);
    //     return;
    // }

    int yBaseline = 29;

    // Draw the Green things.
    char buf[10] = "";
    switch (g5State.apLMode) {
    case 0:
        strcpy(buf, "");
        break;
    case 1:
        strcpy(buf, "ROL");
        break;
    case 2:
        strcpy(buf, "HDG");
        break;
    case 3:
        strcpy(buf, "GPS");
        break;
    case 4:
        strcpy(buf, "VOR");
        break;
    case 5:
        strcpy(buf, "LOC");
        break;
    case 6:
        strcpy(buf, "BC");
        break;
    default:
        sprintf(buf, "%d", g5State.apLMode);
        break;
    }
    apBox.setTextDatum(BC_DATUM);
    apBox.setTextColor(TFT_GREEN);
    apBox.drawString(buf, 108, yBaseline);

    switch (g5State.apVMode) {
    case 0:
        strcpy(buf, "");
        break;
    case 1:
        strcpy(buf, "ALT");
        break;
    case 2:
        strcpy(buf, "VS");
        break;
    case 3:
        strcpy(buf, "PIT");
        break;
    case 4:
        strcpy(buf, "IAS");
        break;
    case 5:
        strcpy(buf, "ALTS");
        break;
    case 6:
        strcpy(buf, "GS");
        break;
    case 7:
        strcpy(buf, "GP");
        break;
    }
    apBox.setTextDatum(BL_DATUM);
    apBox.drawString(buf, 246, yBaseline);

    // If alt mode, print the captured g5State.altitude (nearst 10')
    strcpy(buf, "");
    char unitsBuf[5] = "";

    if (g5State.apVMode == 1) {
        sprintf(buf, "%d", g5State.apAltCaptured);
        strcpy(unitsBuf, "ft");
    }
    // If vs mode, print the VS
    if (g5State.apVMode == 2) {
        sprintf(buf, "%d", g5State.apTargetVS);
        strcpy(unitsBuf, "fpm");
    }

    // If  IAS mode, print target speed
    if (g5State.apVMode == 4) {
        sprintf(buf, "%d", g5State.apTargetSpeed);
        strcpy(unitsBuf, "kts");
    }

    apBox.setTextDatum(BR_DATUM);
    apBox.drawString(buf, 355, yBaseline);
    apBox.setTextDatum(BL_DATUM);
    apBox.setTextSize(0.3);
    apBox.drawString(unitsBuf, 357, yBaseline - 4);

    apBox.setTextSize(0.7);

    // Draw Armed modes in white.
    switch (g5State.apLArmedMode) {
    case 0:
        strcpy(buf, "");
        break;
    case 1:
        strcpy(buf, "rol"); // shouldn't come up
        break;
    case 2:
        strcpy(buf, "hdg"); // shouldn't come up
        break;
    case 3:
        strcpy(buf, "GPS");
        break;
    case 4:
        strcpy(buf, "VOR");
        break;
    case 5:
        strcpy(buf, "LOC");
        break;
    case 6:
        strcpy(buf, "BC");
        break;
    default:
        sprintf(buf, "%d", g5State.apLArmedMode);
        break;
    }
    apBox.setTextColor(TFT_WHITE);
    apBox.setTextDatum(BL_DATUM);
    apBox.drawString(buf, 7, yBaseline);

    switch (g5State.apVArmedMode) {
    case 0:
        strcpy(buf, "");
        break;
    case 1:
        strcpy(buf, "ALTS");
        break;
    case 2:
        strcpy(buf, "ALT");
        break;
    case 4:
        strcpy(buf, "GS");
        break;
    case 5:
        strcpy(buf, "ALTS  GS");
        break;
    case 6:
        strcpy(buf, "ALT  GS");
        break;
    case 8:
        strcpy(buf, "GP");
        break;
    case 9:
        strcpy(buf, "ALTS  GP");
        break;
    case 10:
        strcpy(buf, "ALT  GP");
        break;
    default:
        sprintf(buf, "%d", g5State.apVArmedMode);
        break;
    }
    apBox.setTextDatum(BR_DATUM);
    apBox.drawString(buf, 465, yBaseline);

    apBox.setTextDatum(BC_DATUM);
    apBox.setTextColor(TFT_GREEN);

    if (lastAPState == 1 && g5State.apActive == 0) {
        // Blink it.
        apBlinkEnd = millis() + 5000;
    }

    lastAPState = g5State.apActive;
    //    apBox.fillRoundRect(160, 2, 35, yBaseline + 2, 3, TFT_RED);

    if (millis() < apBlinkEnd && g5State.apActive == 0) {
        if (millis() % 1000 < 200) { // 800ms on, 200ms off.
            // Off
        } else {
            apBox.setTextColor(TFT_BLACK, TFT_YELLOW);
            apBox.fillRoundRect(158, 2, 42, yBaseline - 2, 4, TFT_YELLOW);
            apBox.drawString("AP", 178, yBaseline);
            apBox.setTextColor(TFT_GREEN);
        }
    } else if (g5State.apActive) {
        // TODO add blink on change.
        apBox.drawString("AP", 178, yBaseline);
    }

    if (g5State.apYawDamper) {
        apBox.drawString("YD", 220, yBaseline);
    }

    drawBattery(&apBox, 0, 0);

    apBox.pushSprite(0, 5); // Center the Y
}

void CC_G5_PFD::drawMessageIndicator()
{
    // Only message we're going to indicate is that we've lost connection to MobiFlight.
    if (lastMFUpdate > (millis() - 3000)) return;

    messageIndicator.pushSprite(60, 10);
}

void CC_G5_PFD::drawFlightDirector()
{
    static bool isApOn = true;
    if (!g5State.flightDirectorActive) return;

    // Use the hollow if AP is off.

    // Set the pitch with the attitude pivot point.
    // Need to set max values for flight director.
    attitude.setPivot(ATTITUDE_WIDTH / 2, 200 + (g5State.flightDirectorPitch - g5State.pitchAngle) * 8);
    fdTriangle.pushRotated(g5State.bankAngle - g5State.flightDirectorBank, TFT_WHITE);

    return;
}

void CC_G5_PFD::updateInputValues()
{
    // This gives the cool, smooth value transitions rather than fake looking ones.

    g5State.headingAngle = smoothDirection(g5State.rawHeadingAngle, g5State.headingAngle, 0.15f, 0.02f);
    g5State.altitude     = smoothInput(g5State.rawAltitude, g5State.altitude, 0.1f, 1.0f);
    g5State.airspeed     = smoothInput(g5State.rawAirspeed, g5State.airspeed, 0.1f, 0.005f);
    g5State.gsiNeedle    = smoothInput(g5State.rawGsiNeedle, g5State.gsiNeedle, 0.15f, 1.0f);

    speedTrend.update(g5State.rawAirspeed);

    g5State.ballPos       = smoothInput(g5State.rawBallPos, g5State.ballPos, 0.2f, 0.005f);
    g5State.cdiOffset     = smoothInput(g5State.rawCdiOffset, g5State.cdiOffset, 0.3f, 1.0f);
    g5State.bankAngle     = smoothAngle(g5State.rawBankAngle, g5State.bankAngle, 0.3f, 0.05f);
    g5State.pitchAngle    = smoothInput(g5State.rawPitchAngle, g5State.pitchAngle, 0.3f, 0.05f);
    g5State.verticalSpeed = smoothInput(g5State.rawVerticalSpeed, g5State.verticalSpeed, 0.03, 1);
}

void CC_G5_PFD::update()
{

    // Update the smoothing values. Needs to run in the update loop, not the set.

    uint32_t draw_start = millis();
    updateInputValues();

    static unsigned long lastFrameUpdate = millis() + 1000;
    unsigned long        startDraw       = millis();
    char                 buf[128];

    // Check if data is available from RP2040
    if (g5Hardware.hasData()) {
        read_rp2040_data();
    }

    drawAttitude();
    drawSpeedTape();
    drawAltTape();
    drawHeadingTape();
    drawAltTarget();
    drawHorizonMarker();
    drawGlideSlope();
    drawCDIBar();

    drawFlightDirector();
    drawSpeedPointers();
    drawSpeedTrend();

    unsigned long drawTime  = millis() - startDraw;
    unsigned long pushStart = millis();

    drawDensityAlt();

    processMenu();
    brightnessMenu.draw(&attitude); // Draws on attitude sprite!

    drawShutdown(&attitude);

    attitude.pushSprite(0, TOP_BAR_HEIGHT, TFT_MAIN_TRANSPARENT);

    drawGroundSpeed();
    drawKohlsman();
    //    drawMessageIndicator();
    drawBall();
    drawAp();

    g5State.forceRedraw = false;

    unsigned long pushEnd = millis();
    // lcd.setTextSize(0.5);
    // sprintf(buf, "%4.1f %lu/%lu", 1000.0 / (pushEnd - lastFrameUpdate), drawTime, pushEnd - pushStart);
    // lcd.fillRect(0, 0, SPEED_COL_WIDTH, 40, TFT_BLACK);

    //    lcd.drawString(buf, 0, 55);

    //    lcd.drawNumber(data_available, 400, 10);
    // sprintf(buf, "b:%3.0f p:%3.0f ias:%5.1f alt:%d", g5State.bankAngle, g5State.pitchAngle, g5State.airspeed, g5State.altitude);
    // lcd.drawString(buf, 0, 10);
    // lastFrameUpdate = millis();

    return;
}

void CC_G5_PFD::saveState()
{
    CC_G5_Base::saveState();  // saves common g5State fields (switching flag, IDs 0-10)

    // PFD-specific fields (IDs 60-84)
    Preferences prefs;
    prefs.begin("g5state", false);
    prefs.putFloat("airspd", g5State.rawAirspeed);
    prefs.putInt("apAct", g5State.apActive);
    prefs.putInt("apAltC", g5State.apAltCaptured);
    prefs.putInt("tgtAlt", g5State.targetAltitude);
    prefs.putInt("apLArm", g5State.apLArmedMode);
    prefs.putInt("apVArm", g5State.apVArmedMode);
    prefs.putInt("apLMd", g5State.apLMode);
    prefs.putInt("apSpd", g5State.apTargetSpeed);
    prefs.putInt("apVMd", g5State.apVMode);
    prefs.putInt("apVS", g5State.apTargetVS);
    prefs.putInt("apYaw", g5State.apYawDamper);
    prefs.putFloat("ballP", g5State.rawBallPos);
    prefs.putFloat("bankA", g5State.rawBankAngle);
    prefs.putInt("fdAct", g5State.flightDirectorActive);
    prefs.putFloat("fdBank", g5State.flightDirectorBank);
    prefs.putFloat("fdPtch", g5State.flightDirectorPitch);
    prefs.putFloat("desTrk", g5State.desiredTrack);
    prefs.putFloat("alt", g5State.rawAltitude);
    prefs.putFloat("kohl", g5State.kohlsman);
    prefs.putInt("oat", g5State.oat);
    prefs.putFloat("pitch", g5State.rawPitchAngle);
    prefs.putFloat("trnRt", g5State.turnRate);
    prefs.putInt("vSpd", g5State.rawVerticalSpeed);
    prefs.putFloat("navCrs", g5State.navCourse);

    prefs.end();
}

bool CC_G5_PFD::restoreState()
{
    if (!CC_G5_Base::restoreState())
        return false;

    // PFD-specific fields
    Preferences prefs;
    prefs.begin("g5state", false);
    g5State.rawAirspeed          = prefs.getFloat("airspd", 0);
    g5State.apActive             = prefs.getInt("apAct", 0);
    g5State.apAltCaptured        = prefs.getInt("apAltC", 0);
    g5State.targetAltitude       = prefs.getInt("tgtAlt", 0);
    g5State.apLArmedMode         = prefs.getInt("apLArm", 0);
    g5State.apVArmedMode         = prefs.getInt("apVArm", 0);
    g5State.apLMode              = prefs.getInt("apLMd", 0);
    g5State.apTargetSpeed        = prefs.getInt("apSpd", 0);
    g5State.apVMode              = prefs.getInt("apVMd", 0);
    g5State.apTargetVS           = prefs.getInt("apVS", 0);
    g5State.apYawDamper          = prefs.getInt("apYaw", 0);
    g5State.rawBallPos           = prefs.getFloat("ballP", 0);
    g5State.rawBankAngle         = prefs.getFloat("bankA", 0);
    g5State.flightDirectorActive = prefs.getInt("fdAct", 0);
    g5State.flightDirectorBank   = prefs.getFloat("fdBank", 0);
    g5State.flightDirectorPitch  = prefs.getFloat("fdPtch", 0);
    g5State.desiredTrack         = prefs.getFloat("desTrk", 0);
    g5State.rawAltitude          = prefs.getFloat("alt", 0.0f);
    g5State.kohlsman             = prefs.getFloat("kohl", 29.92);
    g5State.oat                  = prefs.getInt("oat", 15);
    g5State.rawPitchAngle        = prefs.getFloat("pitch", 0);
    g5State.turnRate             = prefs.getFloat("trnRt", 0);
    g5State.rawVerticalSpeed     = prefs.getInt("vSpd", 0);
    g5State.navCourse            = prefs.getFloat("navCrs", 0);
    prefs.end();
    return true;
}