#include "CC_G5_HSI.h"
#include "esp_log.h"

#include <Wire.h>

#include "allocateMem.h"
#include "commandmessenger.h"
#include "Sprites\cdiPointer.h"
#include "Sprites\planeIcon_1bit.h"
#include "Sprites\cdiBar.h"
#include "Sprites\currentTrackPointer.h"
#include "Sprites\headingBug.h"
#include "Sprites\headingBugSmall.h"
#include "Sprites\headingBugHSI.h"
#include "Sprites\deviationScale.h"
#include "Sprites\gsDeviation.h"
#include "Sprites\deviationDiamond.h"
#include "Sprites\diamondBitmap.h"
#include "Sprites\deviationDiamondBitmap.h"
#include "Sprites\deviationDiamondGreen.h"
#include "Sprites\cdiPointerGreen.h"
#include "Sprites\cdiBarGreen.h"
#include "Sprites\toFromGreen.h"
#include "Sprites\toFrom.h"
#include "Sprites\windArrow.h"
#include "Sprites\bearingPointer.h"
#include "Sprites\bearingPointer2.h"
#include "Sprites\bearingPointerBoxLeft.h"
#include "Sprites\bearingPointerBoxRight.h"

// #include "Sprites\courseBox.h"
#include "Sprites\distBox.h"
#include "Sprites\headingBox.h"

// Global sprites for display

LGFX_Sprite compass(&lcd); // Large center compass sprite plus the outer reference markers.
LGFX_Sprite cdiPtr(&lcd);  // Large magenta or green arrow points current course.
LGFX_Sprite curHdg(&lcd);  // Current Heading in the box with triangle at top of compass
LGFX_Sprite curDME(&lcd);
LGFX_Sprite deviationScale(&compass); // The four circles
LGFX_Sprite glideDeviationScale(&lcd);
LGFX_Sprite deviationDiamond(&glideDeviationScale);
LGFX_Sprite currentTrackPtr(&lcd);
LGFX_Sprite cdiBar(&lcd);
LGFX_Sprite headingBug(&compass);
LGFX_Sprite toFrom(&compass);
LGFX_Sprite distBox(&lcd);
LGFX_Sprite dtkBox(&lcd);
LGFX_Sprite bearingPointer1(&compass);
LGFX_Sprite bearingPointer2(&compass);
LGFX_Sprite bearingPointerBox1(&lcd);
LGFX_Sprite bearingPointerBox2(&lcd);

LGFX_Sprite batteryHolderSprite(&lcd);
LGFX_Sprite courseBox(&lcd);

LGFX_Sprite menuSprite(&lcd);

static LGFX_Sprite windBox(&lcd);
static LGFX_Sprite windArrow(&windBox);

CC_G5_HSI::CC_G5_HSI()
{
}

// Read data from RP2040
void CC_G5_HSI::read_rp2040_data()
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
        if (enc_btn == ButtonEventType::BUTTON_CLICKED) {
            if (hsiMenu.menuActive) {
                // Route input to menu when active
                hsiMenu.handleEncoderButton(true);
            } else if (!brightnessMenu.active()) { // Don't open the setting menu when brightness menu open.
                // Open menu when not active
                hsiMenu.setActive(true);
            }
        }

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
                lcd.clearDisplay(); // CAC CLEAR TEST
                g5State.forceRedraw = true;
                // Save the brightness setting.
            } else {
                brightnessMenu.show();
            }
        }

        if (delta) {
            if (brightnessMenu.active()) {
                brightnessMenu.adjustBrightness(delta);
            } else if (hsiMenu.menuActive) {
                // Route encoder turns to menu when active
                hsiMenu.handleEncoder(delta);
            } else {
                // Normal heading adjustment when menu not active
                sendEncoder("encHeading", abs(delta), delta > 0 ? 0 : 2);
            }
        }
    }
}

void CC_G5_HSI::begin()
{

    loadSettings();

    lcd.init();
    lcd.setBrightness(brightnessGamma(g5Settings.lcdBrightness));
    g5State.lcdBrightness = g5Settings.lcdBrightness;
#ifdef USE_GUITION_SCREEN
    lcd.setRotation(3); // Puts the USB jack at the bottom on Guition screen.
#else
    lcd.setRotation(0); // Orients the Waveshare screen with FPCB connector at bottom.
#endif
    lcd.setColorDepth(8);
    lcd.fillScreen(TFT_BACKGROUND_COLOR);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.loadFont(PrimaSans18);
    // lcd.setClipRect(X_OFFSET, Y_OFFSET, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Setup menu structure

    hsiMenu.initializeMenu();
    // Configure i2c pins and interrupt
    pinMode(INT_PIN, INPUT_PULLUP);

    // Configure I2C master
    if (!Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN)) {
        // ESP_LOGE(TAG_I2C, "i2c setup failed");
    } else {
        // ESP_LOGI(TAG_I2C, "i2c setup successful");
    }

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

    g5State.forceRedraw = true;

    setupSprites();

    if (restoreState()) {
        setNavSource();
        // Call any other "Set" functions.
    }

    updateCommon();
}

void CC_G5_HSI::setupSprites()
{
    // Use centralized compass sprite setup
    setupCompassSprites();

    curHdg.setColorDepth(8);
    curHdg.createSprite(67, 26); // 67x26
    curHdg.setTextColor(TFT_WHITE, TFT_BLACK);

    curHdg.fillSprite(TFT_BLACK);
    curHdg.setColor(DATA_BOX_OUTLINE_COLOR);
    curHdg.setTextColor(TFT_WHITE);
    curHdg.setTextDatum(MC_DATUM);
    curHdg.loadFont(PrimaSans18);

    curHdg.drawRect(0, 0, curHdg.width(), curHdg.height(), DATA_BOX_OUTLINE_COLOR);
    curHdg.drawRect(1, 1, curHdg.width() - 2, curHdg.height() - 2, DATA_BOX_OUTLINE_COLOR);
    // Black out the part of the border for the triangle pointer. Pointer is drawn by compass, so leaving it intact causes flicker.
    curHdg.drawFastHLine(curHdg.width() / 2 - HEADING_POINTER_WIDTH / 2 - 2, curHdg.height() - 1, HEADING_POINTER_WIDTH - 6, TFT_BLACK);
    curHdg.drawFastHLine(curHdg.width() / 2 - HEADING_POINTER_WIDTH / 2 - 3, curHdg.height() - 2, HEADING_POINTER_WIDTH - 4, TFT_BLACK);

    glideDeviationScale.createSprite(GSDEVIATION_IMG_WIDTH, GSDEVIATION_IMG_HEIGHT);
    glideDeviationScale.pushImage(0, 0, GSDEVIATION_IMG_WIDTH, GSDEVIATION_IMG_HEIGHT, GSDEVIATION_IMG_DATA);
    glideDeviationScale.setTextColor(TFT_MAGENTA);
    glideDeviationScale.setTextDatum(CC_DATUM);
    glideDeviationScale.loadFont(PrimaSans12);

    // Diamond for deviation scale
    deviationDiamond.setColorDepth(1);
    deviationDiamond.setBitmapColor(TFT_MAGENTA, TFT_BLACK);
    deviationDiamond.createSprite(DIAMONDBITMAP_IMG_WIDTH, DIAMONDBITMAP_IMG_HEIGHT);
    // deviationDiamond.setBuffer(const_cast<std::uint16_t*>(DEVIATIONDIAMOND_IMG_DATA), DEVIATIONDIAMOND_IMG_WIDTH, DEVIATIONDIAMOND_IMG_HEIGHT);
    deviationDiamond.setBuffer(const_cast<std::uint8_t *>(DIAMONDBITMAP_IMG_DATA), DIAMONDBITMAP_IMG_WIDTH, DIAMONDBITMAP_IMG_HEIGHT);

    // Cyan bearing pointer 1
    bearingPointer1.setColorDepth(1);
    bearingPointer1.setBitmapColor(TFT_CYAN, TFT_BLACK);
    bearingPointer1.createSprite(BEARINGPOINTER_IMG_WIDTH, BEARINGPOINTER_IMG_HEIGHT);
    bearingPointer1.drawBitmap(0, 0, BEARINGPOINTER_IMG_DATA, BEARINGPOINTER_IMG_WIDTH, BEARINGPOINTER_IMG_HEIGHT, TFT_CYAN, TFT_BLACK);
    bearingPointer1.setPivot(BEARINGPOINTER_IMG_WIDTH / 2, BEARINGPOINTER_IMG_HEIGHT / 2);

    // Cyan bearing pointer 2
    bearingPointer2.setColorDepth(1);
    bearingPointer2.setBitmapColor(TFT_CYAN, TFT_BLACK);
    bearingPointer2.createSprite(BEARINGPOINTER2_IMG_WIDTH, BEARINGPOINTER2_IMG_HEIGHT);
    bearingPointer2.drawBitmap(0, 0, BEARINGPOINTER2_IMG_DATA, BEARINGPOINTER2_IMG_WIDTH, BEARINGPOINTER2_IMG_HEIGHT, TFT_CYAN, TFT_BLACK);
    bearingPointer2.setPivot(BEARINGPOINTER2_IMG_WIDTH / 2, BEARINGPOINTER2_IMG_HEIGHT / 2);

    bearingPointerBox1.setColorDepth(8);
    bearingPointerBox1.createSprite(BEARINGPOINTERBOXLEFT_IMG_WIDTH, BEARINGPOINTERBOXLEFT_IMG_HEIGHT);
    bearingPointerBox1.pushImage(0, 0, BEARINGPOINTERBOXLEFT_IMG_WIDTH, BEARINGPOINTERBOXLEFT_IMG_HEIGHT, BEARINGPOINTERBOXLEFT_IMG_DATA);
    bearingPointerBox1.loadFont(PrimaSans16);
    bearingPointerBox1.setTextDatum(BL_DATUM);
    bearingPointerBox1.setTextColor(TFT_CYAN);

    bearingPointerBox2.setColorDepth(8);
    bearingPointerBox2.createSprite(BEARINGPOINTERBOXRIGHT_IMG_WIDTH, BEARINGPOINTERBOXRIGHT_IMG_HEIGHT);
    bearingPointerBox2.pushImage(0, 0, BEARINGPOINTERBOXRIGHT_IMG_WIDTH, BEARINGPOINTERBOXRIGHT_IMG_HEIGHT, BEARINGPOINTERBOXRIGHT_IMG_DATA);
    bearingPointerBox2.loadFont(PrimaSans16);
    bearingPointerBox2.setTextDatum(BR_DATUM);
    bearingPointerBox2.setTextColor(TFT_CYAN);

    // // GS Is Ground speed, NOT glide slope. THIS IS USED FOR MORE THAN JUST GROUND SPEED
    courseBox.setColorDepth(8);
    courseBox.createSprite(DATA_BOX_WIDTH_LEFT, DATA_BOX_HEIGHT_MED);

    distBox.setColorDepth(8);
    distBox.createSprite(DIST_BOX_WIDTH, DATA_BOX_HEIGHT_MED);
    distBox.setColor(DATA_BOX_OUTLINE_COLOR);
    distBox.drawRect(0, 0, distBox.width(), distBox.height());
    distBox.drawRect(1, 1, distBox.width() - 2, distBox.height() - 2);

    distBox.setTextDatum(BL_DATUM);
    distBox.setTextColor(TFT_WHITE);
    distBox.loadFont(PrimaSans12);
    distBox.drawString("DIST", 4, distBox.height());

    distBox.setTextColor(TFT_MAGENTA);
    distBox.setTextDatum(BR_DATUM);
    distBox.loadFont(PrimaSans18);

    dtkBox.setColorDepth(8);
    dtkBox.createSprite(DATA_BOX_WIDTH_LEFT, DATA_BOX_HEIGHT_MED);
    dtkBox.setColor(DATA_BOX_OUTLINE_COLOR);

    headingBox.setColorDepth(8);
    headingBox.createSprite(DATA_BOX_WIDTH_LEFT, DATA_BOX_HEIGHT_MED);
    headingBox.setColor(TFT_CYAN);
    headingBox.drawRect(0, 0, headingBox.width(), headingBox.height());
    headingBox.drawRect(1, 1, headingBox.width() - 2, headingBox.height() - 2);
    headingBox.pushImage(5, (headingBox.height() - HEADINGBUGSMALL_IMG_HEIGHT) / 2, HEADINGBUGSMALL_IMG_WIDTH, HEADINGBUGSMALL_IMG_HEIGHT, HEADINGBUGSMALL_IMG_DATA);
    headingBox.setTextColor(TFT_CYAN);
    headingBox.setTextDatum(BR_DATUM);
    headingBox.loadFont(PrimaSans18);

    batteryHolderSprite.setColorDepth(8);
    batteryHolderSprite.createSprite(100, 40);

    windBox.setColorDepth(8);
    windBox.setBitmapColor(TFT_WHITE, TFT_BLACK);
    windBox.createSprite(DATA_BOX_WIDTH_LEFT, DATA_BOX_HEIGHT_TALL); // Make the same width and height as the courseBox.
    windBox.setTextColor(TFT_WHITE);
    windBox.loadFont(PrimaSans12);
    windBox.setTextDatum(CC_DATUM);
    windBox.fillSprite(TFT_BLACK);
    windBox.drawRect(0, 0, windBox.width(), windBox.height(), DATA_BOX_OUTLINE_COLOR);
    windBox.drawRect(1, 1, windBox.width() - 2, windBox.height() - 2, DATA_BOX_OUTLINE_COLOR);
    windBox.setPivot(WINDARROW_IMG_WIDTH + 4, windBox.height() / 2);

    windArrow.setColorDepth(1);
    windArrow.setBitmapColor(TFT_WHITE, TFT_BLACK);
    windArrow.createSprite(WINDARROW_IMG_WIDTH, WINDARROW_IMG_HEIGHT);
    windArrow.setBuffer(const_cast<std::uint8_t *>(WINDARROW_IMG_DATA), WINDARROW_IMG_WIDTH, WINDARROW_IMG_HEIGHT);
    windArrow.setPivot(WINDARROW_IMG_WIDTH / 2, WINDARROW_IMG_HEIGHT / 2);
}

void CC_G5_HSI::setupCompassSprites()
{

    // compass.setPsram(true); //Gets way too jittery when we try this.

    compass.setColorDepth(8);
    void *buffer = compass.createSprite(COMPASS_WIDTH, COMPASS_HEIGHT); // Sprite s/b big enough to hold compass and outer markers
    // void *buffer = compass.createSprite(406, 406); // Sprite s/b big enough to hold compass and outer markers

    if (buffer != nullptr) {
        compass.loadFont(PrimaSans18);
        compass.setTextColor(TFT_WHITE, TFT_BLACK);
        // Compass center coordinates are calculated in setupSprites() after curHdg is created

    } else {
        while (1)
            Serial.printf("Compass sprite creation failed. \n");
    }

    cdiPtr.setBuffer(const_cast<std::uint16_t *>(CDIPOINTER_IMG_DATA), CDIPOINTER_IMG_WIDTH, CDIPOINTER_IMG_HEIGHT, 16);
    cdiPtr.setPivot(CDIPOINTER_IMG_WIDTH >> 1, CDIPOINTER_IMG_HEIGHT >> 1);

    cdiBar.setBuffer(const_cast<std::uint16_t *>(CDIBAR_IMG_DATA), CDIBAR_IMG_WIDTH, CDIBAR_IMG_HEIGHT, 16);
    cdiBar.setPivot(CDIBAR_IMG_WIDTH >> 1, CDIBAR_IMG_HEIGHT >> 1);

    currentTrackPtr.setBuffer(const_cast<std::uint16_t *>(CURRENTTRACKPOINTER_IMG_DATA), CURRENTTRACKPOINTER_IMG_WIDTH, CURRENTTRACKPOINTER_IMG_HEIGHT, 16);
    currentTrackPtr.setPivot(CURRENTTRACKPOINTER_IMG_WIDTH >> 1, CURRENTTRACKPOINTER_IMG_HEIGHT);

    // Create child sprites that depend on compass
    deviationScale.setColorDepth(1);
    deviationScale.setBitmapColor(TFT_WHITE, TFT_BLACK);
    void *devBuffer = deviationScale.createSprite(DEVIATIONSCALE_IMG_WIDTH, DEVIATIONSCALE_IMG_HEIGHT);

    if (devBuffer != nullptr) {
        deviationScale.setBuffer(const_cast<std::uint8_t *>(DEVIATIONSCALE_IMG_DATA), DEVIATIONSCALE_IMG_WIDTH, DEVIATIONSCALE_IMG_HEIGHT);
        deviationScale.setPivot(deviationScale.width() / 2, deviationScale.height() / 2);
    }

    headingBug.setBuffer(const_cast<std::uint16_t *>(HEADINGBUGHSI_IMG_DATA), HEADINGBUGHSI_IMG_WIDTH, HEADINGBUGHSI_IMG_HEIGHT, 16);
    headingBug.setPivot(HEADINGBUGHSI_IMG_WIDTH / 2, HEADINGBUGHSI_IMG_HEIGHT + COMPASS_OUTER_RADIUS);

    toFrom.setBuffer(const_cast<std::uint16_t *>(TOFROM_IMG_DATA), TOFROM_IMG_WIDTH, TOFROM_IMG_HEIGHT, 16);
    toFrom.setPivot(TOFROM_IMG_WIDTH / 2, 86);
}

void CC_G5_HSI::updateCommon()
{
    // Swap sprite buffers between magenta (GPS) and green (NAV) when navSource changes.
    // setNavSource() has an internal guard so it's a no-op when the source hasn't changed.
    setNavSource();

    // Clear the compass sprite. Use MAIN_TRANSPARENAT as the transparent color. It's not used in the display.
    compass.fillSprite(TFT_MAIN_TRANSPARENT);

    compass.fillCircle(COMPASS_CENTER_X, COMPASS_CENTER_Y, COMPASS_OUTER_RADIUS + 15, TFT_BLACK); // Try this to reduce heading box flicker

    drawHeadingBug();
    drawCompass();

    drawDeviationScale();
    drawCDIScaleLabel();
    drawCDISource();
    drawNavCDILabel();
    drawCDIBar(); 

    drawWPTAlert();
    drawCurrentTrack();
    drawCDIPointer();

    drawCurrentHeading();

    // drawHeadingBug();
    drawBearingPointer1();
    drawBearingPointer2();

    // Draw adjustment popup onto compass before pushing to LCD
    // if (menu.currentState == MenuState::ADJUSTING) {
    //     drawAdjustmentPopup();
    // }

    drawCompassOuterMarkers();
    processMenu();
    brightnessMenu.draw(&compass);
    drawPlaneIcon();
    drawOrangeCenterTick();

    drawShutdown(&compass);

    compass.drawRect(0,0,COMPASS_WIDTH, COMPASS_HEIGHT, TFT_RED);

    compass.pushSprite(&lcd, X_OFFSET + COMPASS_X_OFFSET, Y_OFFSET + COMPASS_Y_OFFSET, TFT_MAIN_TRANSPARENT);

    drawGlideSlope();

    drawDistNextWaypoint();
    drawHeadingBugValue();
    drawDesiredTrack();

    drawWind();
    drawBattery(&batteryHolderSprite, 0, 0);
    batteryHolderSprite.pushSprite(X_OFFSET, Y_OFFSET + windBox.height());
}

void CC_G5_HSI::processMenu()
{
    static bool menuWasActive = false;
    static auto lastMenuState = HSIMenu::MenuState::BROWSING;

    if (hsiMenu.menuActive) {

        // Detect state transition to ADJUSTING or SELECTING to clear browsing menu from screen.
        if (lastMenuState == HSIMenu::MenuState::BROWSING &&
            (hsiMenu.currentState == HSIMenu::MenuState::ADJUSTING ||
             hsiMenu.currentState == HSIMenu::MenuState::SELECTING)) {
            // Clear the browsing menu area
            compass.fillRect(hsiMenu.menuXpos, hsiMenu.menuYpos, hsiMenu.menuWidth, hsiMenu.menuHeight, TFT_BLACK);
        }

        if (hsiMenu.currentState == HSIMenu::MenuState::BROWSING) {
            hsiMenu.drawMenu(); // Draws menu items on sprite
        } else if (hsiMenu.currentState == HSIMenu::MenuState::ADJUSTING) {
            hsiMenu.drawAdjustmentPopup(); // Draws popup on sprite
        } else if (hsiMenu.currentState == HSIMenu::MenuState::SELECTING) {
            hsiMenu.drawSelectionPopup();
        }

        lastMenuState = hsiMenu.currentState;
        menuWasActive = true;
    } else if (menuWasActive) {
        menuWasActive = false;
        lastMenuState = HSIMenu::MenuState::BROWSING;
        lcd.clearDisplay(TFT_BLACK);
        g5State.forceRedraw = true;
    }
}

void CC_G5_HSI::attach()
{
    _initialised = true;
}

void CC_G5_HSI::detach()
{
    if (!_initialised)
        return;
    _initialised = false;
}

void CC_G5_HSI::setHSI(int16_t messageID, char *setPoint)
{
    switch (messageID) {
    case 30: // ADF Bearing
        g5State.bearingAngleADF = atoi(setPoint);
        break;
    case 31: // ADF Valid
        g5State.adfValid = atoi(setPoint);
        break;
    case 32: // CDI Bearing
        g5State.rawCdiDirection = atoi(setPoint);
        break;
    case 33: // CDI Scale Label
        g5State.cdiScaleLabel = atoi(setPoint);
        break;
    case 34: // Desired Track
        g5State.desiredTrack = atof(setPoint);
        break;
    case 35: // Desired Track Valid
        g5State.desiredTrackValid = atoi(setPoint);
        break;
    case 36: // Distance to Next GPS Waypoint
        g5State.distNextWaypoint = atof(setPoint);
        break;
    case 37: // GPS Bearing to Next Station
        g5State.bearingAngleGPS = atoi(setPoint);
        break;
    case 38: // NAV CDI Label
        g5State.navCDILabelIndex = atoi(setPoint);
        break;
    case 39: // Nav1 Bearing Angle
        g5State.bearingAngleVLOC1 = atoi(setPoint);
        break;
    case 40: // Nav1 Bearing Source
        g5State.vloc1Type = atoi(setPoint);
        break;
    case 41: // Nav2 Bearing Angle
        g5State.bearingAngleVLOC2 = atoi(setPoint);
        break;
    case 42: // Nav2 Bearing Source
        g5State.vloc2Type = atoi(setPoint);
        break;
    case 43: // OBS Active
        g5State.obsModeOn = atoi(setPoint);
        break;
    case 44: // OBS Heading
        g5State.obsAngle = atoi(setPoint);
        break;
    case 45: // Wind Direction (Magnetic)
        g5State.rawWindDir = atoi(setPoint);
        break;
    case 46: // Wind Speed
        g5State.rawWindSpeed = atoi(setPoint);
        break;
    case 47: // ETE
        g5State.gpsEteWp = atoi(setPoint);
        break;
    case 48: // Bearing point source for use without buttons.
        g5State.bearing1Source           = atoi(setPoint);
        g5Settings.bearingPointer1Source = atoi(setPoint);
        break;
    case 49: // Bearing point source for use without buttons.
        g5State.bearing2Source           = atoi(setPoint);
        g5Settings.bearingPointer2Source = atoi(setPoint);
        break;
    }
}

float CC_G5_HSI::getBearingPointerAngle(uint8_t source)
{
    switch (source) {
    case 0: // Off
        return 0.0f;
    case 1: // GPS
        return g5State.bearingAngleGPS;
    case 2: // VLOC1
        return g5State.bearingAngleVLOC1;
    case 3: // VLOC2
        return g5State.bearingAngleVLOC2;
    case 4: // ADF
        return fmod((g5State.bearingAngleADF + g5State.headingAngle + 360), 360);
    default:
        return 0.0f;
    }
}

void CC_G5_HSI::updateInputValues()
{
    g5State.headingAngle = smoothDirection(g5State.rawHeadingAngle, g5State.headingAngle, 0.15f, 0.2f);
    g5State.cdiDirection = smoothDirection(g5State.rawCdiDirection, g5State.cdiDirection, 0.15f, 0.5f);
    g5State.gsiNeedle    = smoothInput(g5State.rawGsiNeedle, g5State.gsiNeedle, 0.15f, 0.1f);
    g5State.cdiOffset    = smoothInput(g5State.rawCdiOffset, g5State.cdiOffset, 0.15f, 1.0f);
    g5State.windDir      = smoothInput(g5State.rawWindDir, g5State.windDir, 0.15f, 5.0f);
    g5State.windSpeed    = smoothInput(g5State.rawWindSpeed, g5State.windSpeed, 0.15f, 0.2f);
    bearingPointer1Angle = smoothDirection(getBearingPointerAngle(g5Settings.bearingPointer1Source), bearingPointer1Angle, 0.25f, 0.1f);
    bearingPointer2Angle = smoothDirection(getBearingPointerAngle(g5Settings.bearingPointer2Source), bearingPointer2Angle, 0.25f, 0.1f);
}

unsigned long testLastUpdate = 0;

void CC_G5_HSI::update()
{
    static unsigned long lastFrameUpdate = millis() + 1000;
    unsigned long        now             = millis();
    char                 buf[16];

    // Check if data is available from RP2040
    if (g5Hardware.hasData()) {
        read_rp2040_data();
    }

    updateInputValues();

    updateCommon();

    // Confirm screen size
    //     lcd.drawRect(X_OFFSET,Y_OFFSET,SCREEN_WIDTH,SCREEN_HEIGHT,TFT_RED);

    g5State.forceRedraw = false;
    /*
        if (millis() - testLastUpdate > 500) {
            g5State.rawHeadingAngle -= 0.5f;
            if (g5State.rawHeadingAngle < 0) g5State.rawHeadingAngle = 359;

            g5State.headingBugAngle += 1;
            if (g5State.headingBugAngle > 359) g5State.headingBugAngle = 0;
            testLastUpdate = millis();
        }

        sprintf(buf, "HSI %4.1f f/s", 1000.0 / (now - lastFrameUpdate));
        //   lcd.drawString(buf, 360, 0);
        lastFrameUpdate = now;
        */
}

void CC_G5_HSI::setNavSource()
{
    static int lastNavSource = 99;
    if (g5State.navSource == lastNavSource) return;

    // Green if NAV, Magenta if GPS.
    // Fill the screen with black because we will redraw all the boxes.
    lcd.fillScreen(TFT_BACKGROUND_COLOR);
    g5State.forceRedraw = true;

    if (g5State.navSource == NAVSOURCE_GPS) {
        cdiPtr.setBuffer(const_cast<std::uint16_t *>(CDIPOINTER_IMG_DATA), CDIPOINTER_IMG_WIDTH, CDIPOINTER_IMG_HEIGHT, 16);
        cdiBar.setBuffer(const_cast<std::uint16_t *>(CDIBAR_IMG_DATA), CDIBAR_IMG_WIDTH, CDIBAR_IMG_HEIGHT, 16);
        // deviationDiamond.setBuffer(const_cast<std::uint8_t*>(DIAMONDBITMAP_IMG_DATA), DIAMONDBITMAP_IMG_WIDTH, DIAMONDBITMAP_IMG_HEIGHT, );
        toFrom.setBuffer(const_cast<std::uint16_t *>(TOFROM_IMG_DATA), TOFROM_IMG_WIDTH, TOFROM_IMG_HEIGHT, 16);

    } else if (g5State.navSource == NAVSOURCE_NAV) {
        cdiPtr.setBuffer(const_cast<std::uint16_t *>(CDIPOINTERGREEN_IMG_DATA), CDIPOINTERGREEN_IMG_WIDTH, CDIPOINTERGREEN_IMG_HEIGHT, 16);
        cdiBar.setBuffer(const_cast<std::uint16_t *>(CDIBARGREEN_IMG_DATA), CDIBARGREEN_IMG_WIDTH, CDIBARGREEN_IMG_HEIGHT, 16);
        // deviationDiamond.setBuffer(const_cast<std::uint8_t*>(DIAMONDBITMAP_IMG_DATA), DIAMONDBITMAP_IMG_WIDTH, DIAMONDBITMAP_IMG_HEIGHT);
        toFrom.setBuffer(const_cast<std::uint16_t *>(TOFROMGREEN_IMG_DATA), TOFROMGREEN_IMG_WIDTH, TOFROMGREEN_IMG_HEIGHT, 16);
    }

    lastNavSource = g5State.navSource;
}

void CC_G5_HSI::drawCompassOuterMarkers()
{
    // There are six fixed outer markers outside the compass at 0, 45, 135, 180, 225, and 315
    // Precomputing this made 0 difference in  speed.
    static int tickLength = COMPASS_OUTER_TICK_SIZE;
    static int radius     = COMPASS_OUTER_RADIUS + 14;

    int angles[] = {0, 45, 135, 180, 225, 315};

    for (int angle : angles) {

        // float innerX = compassCenterX + (radius) * (float)cos(angle * PIf / 180.0f);
        float innerX = COMPASS_CENTER_X + (radius) * (float)cos(angle * PIf / 180.0f);
        // float innerY = compassCenterY + (radius) * (float)sin(angle * PIf / 180.0f);
        float innerY = COMPASS_CENTER_Y + (radius) * (float)sin(angle * PIf / 180.0f);

        float outerX = COMPASS_CENTER_X + (radius + tickLength) * (float)cos(angle * PIf / 180.0f);
        float outerY = COMPASS_CENTER_Y + (radius + tickLength) * (float)sin(angle * PIf / 180.0f);

        // Draw the tick mark
        compass.drawWideLine(outerX, outerY, innerX, innerY, 2, 0xFFFFFF);
    }
}

void CC_G5_HSI::drawNavCDILabel()
{
    //  This is drawn at the 10 o'clock position on the compass.
    if (g5State.navSource == NAVSOURCE_GPS) return;
    compass.setTextColor(TFT_GREEN);
    compass.loadFont(PrimaSans16);
    char mode[10];

    const char *const CDI_LABELS[] = {
        "GPS",
        "LOC1",
        "VOR1",
        "DME1",
        "LOC2",
        "VOR2",
        "DME2",
        "VOR1", // NAV1 No station
        "VOR2", // Nav2 No station
        "",     // reserved
        "",     // Unknown
    };

    if (g5State.navCDILabelIndex >= 0 && g5State.navCDILabelIndex < sizeof(CDI_LABELS) / sizeof(CDI_LABELS[0])) {
        strcpy(mode, CDI_LABELS[g5State.navCDILabelIndex]);
    } else {
        sprintf(mode, "?%d?", g5State.navCDILabelIndex); // Unknown number.
    }

    compass.setTextDatum(BR_DATUM);
    compass.drawString(mode, COMPASS_CENTER_X - 17, COMPASS_CENTER_Y- 40);
}

void CC_G5_HSI::drawRadioNavApproachType()
{
    // This is drawn at the 2 o'clock position on the compass.
    if (g5State.navSource == NAVSOURCE_GPS) return;

    compass.setTextColor(TFT_GREEN);
    compass.loadFont(PrimaSans16);
    char mode[10] = "\0";

    const char *const CDI_LABELS[] = {
        "", // 0: None
        "GPS",
        "VOR",
        "NDB",
        "ILS",
        "LOC",
        "SDF",
        "LDA",
        "L/VNAV",
        "VOR/D",
        "NDB/D",
        "RNAV",
        "BC"};

    if (g5State.gpsApproachType >= 0 && g5State.gpsApproachType < sizeof(CDI_LABELS) / sizeof(CDI_LABELS[0])) {
        strcpy(mode, CDI_LABELS[g5State.gpsApproachType]);
    }

    compass.setTextDatum(BL_DATUM);
    // Clear the old text
    compass.fillRect(COMPASS_CENTER_X + 3, COMPASS_CENTER_Y - 60, 50, 20, TFT_BLACK);
    compass.drawString(mode, COMPASS_CENTER_X + 10, COMPASS_CENTER_Y - 40);
}

void CC_G5_HSI::drawCDIScaleLabel()
{

    if (g5State.navSource == NAVSOURCE_GPS) {
        compass.setTextColor(TFT_MAGENTA);
    } else {
        compass.setTextColor(TFT_GREEN);
    }

    compass.loadFont(PrimaSans16);
    char mode[10];

    // There's no real good way to determine DEP vs TERM vs TDEP vs TARR vs ENR vs OCN.
    // Really, all we have is a scaling factor.
    const char *const CDI_LABELS[] = {
        "DEP",    // 0: Departure
        "TERM",   // 1: Terminal
        "TDEP",   // 2: TerminalDeparture
        "TARR",   // 3: TerminalArrival
        "ENR",    // 4: Enroute
        "OCN",    // 5: Oceanic
        "LNAV",   // 6: LNav
        "LNAV+V", // 7: LNavPlusV
        "VIS",    // 8: Visual
        "L/VNAV", // 9: LNavVNav
        "LP",     // 10: LP
        "LP+V",   // 11: LPPlusV
        "LPV",    // 12: LPV
        "RNP",    // 13: RNP
        "APR",    // 14: Approach
        "MISS",   // 15: MissedApproach
        "VFRE",   // 16: VfrEnroute
        "VFRT",   // 17: VfrTerminal
        "VFRA",   // 18: VfrApproach
        "   "     // 19: Inactive.
    };

    if (g5State.cdiScaleLabel >= 0 && g5State.cdiScaleLabel < 20) {
        strcpy(mode, CDI_LABELS[g5State.cdiScaleLabel]);
    } else {
        sprintf(mode, "xxx"); // Unknown number.
    }

    compass.setTextDatum(BL_DATUM);
    compass.fillRect(COMPASS_CENTER_X + 10, COMPASS_CENTER_Y - 63, 57, 20, TFT_BLACK);
    compass.drawString(mode, COMPASS_CENTER_X + 10, COMPASS_CENTER_Y - 40);

    // if obs is on, put indicator up in lower right of inner circle.
    if (g5State.obsModeOn) {
        compass.setTextDatum(TL_DATUM);
        compass.drawString("OBS", COMPASS_CENTER_X + 17, COMPASS_CENTER_Y + 40);
    }
}

void CC_G5_HSI::drawCDISource()
{
    if (g5State.navSource != NAVSOURCE_GPS) return;

    compass.setTextColor(TFT_MAGENTA);
    compass.loadFont(PrimaSans16);
    compass.setTextDatum(BR_DATUM);
    compass.drawString("GPS", COMPASS_CENTER_X - 20, COMPASS_CENTER_Y - 40);
}

void CC_G5_HSI::drawWPTAlert()
{
    static int lastETE = -999;

    if (g5State.navSource != NAVSOURCE_GPS) return;

    // If the ete to next wp is less than 30 sec and getting lower, then show WPT reminder
    if (g5State.gpsEteWp < 30 && g5State.gpsEteWp > 10 && g5State.gpsEteWp <= lastETE && (millis() % 1000) > 300) {
        compass.setTextColor(TFT_WHITE);
        compass.loadFont(PrimaSans16);
        compass.setTextDatum(BR_DATUM);
        compass.drawString("WPT", COMPASS_CENTER_X - 20, COMPASS_CENTER_Y + 40);

        // Turn on the WPT blinker.
    }

    lastETE = g5State.gpsEteWp;
}

void CC_G5_HSI::drawCompass()
{

    int   centerX          = COMPASS_CENTER_X;
    int   centerY          = COMPASS_CENTER_Y;
    int   outerRadius      = COMPASS_OUTER_RADIUS; // Leave some margin
    int   innerRadius      = COMPASS_INNER_RADIUS; // Inner circle radius
    int   majorTickLength  = 22;
    int   mediumTickLength = 18;
    int   smallTickLength  = 9;
    int   majorTickWidth   = 2;
    int   mediumTickWidth  = 1;
    int   smallTickWidth   = 1;
    float offsetAngle      = g5State.headingAngle - 0; // Initial offset angle in degrees

    // Draw ticks for all 360 degrees
    for (int i = 0; i < 360; i += 5) {
        // Apply offset to angle
        float angle = (i - offsetAngle + 270);

        // Calculate the starting point of the tick mark (outer edge of compass)
        float outerX = centerX + outerRadius * (float)cos(angle * PIf / 180.0f);
        float outerY = centerY + outerRadius * (float)sin(angle * PIf / 180.0f);

        // Determine tick type and length based on the angle
        int tickLength;
        int tickWidth;

        if (i % 30 == 0) {
            // Major tick marks (every 30 degrees)
            tickLength = majorTickLength;
            tickWidth  = majorTickWidth;
        } else if (i % 10 == 0) {
            // Medium tick marks (every 10 degrees)
            tickLength = mediumTickLength;
            tickWidth  = mediumTickWidth;
        } else {
            // Small tick marks (every 5 degrees)
            tickLength = smallTickLength;
            tickWidth  = smallTickWidth;
        }

        // Calculate inner point of the tick mark
        float innerX = centerX + (outerRadius - tickLength) * cos(angle * PIf / 180.0f);
        float innerY = centerY + (outerRadius - tickLength) * sin(angle * PIf / 180.0f);

        // Draw the tick mark
        compass.drawWideLine(outerX, outerY, innerX, innerY, tickWidth, 0xFFFFFF);

        // Add number labels for each 30 degrees and cardinal directions
        if (i % 30 == 0) {
            // Position for the label (inside the circle)
            float labelDistance = outerRadius - majorTickLength - 21; // Place inside the outer ring ... just a little farther than -25
            float labelX        = centerX + labelDistance * cos(angle * PIf / 180.0f);
            float labelY        = centerY + 2 + labelDistance * sin(angle * PIf / 180.0f);

            // Set text properties
            compass.setTextDatum(MC_DATUM); // Middle-center alignment
            compass.setTextColor(TFT_WHITE);
            compass.loadFont(PrimaSans18);

            // Determine which label to draw
            String label = "";

            if (i == 0) {
                label = "N";
            } else if (i == 90) {
                label = "E";
            } else if (i == 180) {
                label = "S";
            } else if (i == 270) {
                label = "W";
            } else {
                // For non-cardinal directions, show the degrees/10
                label = String(i / 10);
            }

            // Draw the label
            compass.drawString(label, labelX, labelY);
        }
    }

    // Draw the V of the pointer. Gotta do it here to prevent flicker. FIXX
    // Draw pointer triangle
    const int tx2 = COMPASS_CENTER_X;
    const int tx1 = tx2 - HEADING_POINTER_WIDTH / 2;
    const int tx3 = tx2 + HEADING_POINTER_WIDTH / 2;
    const int ty1 = 6;
    const int ty2 = 17;
    const int ty3 = ty1;
    compass.fillTriangle(tx1, ty1, tx2, ty2, tx3, ty3, DATA_BOX_OUTLINE_COLOR);
    compass.fillTriangle(tx1 + 2, ty1, tx2, ty2 - 2, tx3 - 2, ty3, TFT_BLACK);

    // draw the inner circle
    compass.drawCircle(centerX, centerY, innerRadius + 2, TFT_WHITE);
    compass.drawCircle(centerX, centerY, innerRadius + 1, TFT_WHITE);
}

void CC_G5_HSI::drawOrangeCenterTick()
{
    // Draw the big orange/yellow tik at the top about 2 px outside outer radius. FIXX needs to be drawn
    compass.drawWideLine(COMPASS_CENTER_X, 18, COMPASS_CENTER_X, 32 + 18, 2, TFT_ORANGE);
}

void CC_G5_HSI::drawBearingPointer1()
{
    // draw the Bearing Pointer if it's on
    if (g5Settings.bearingPointer1Source == 0) return;

    // draw the Bearing Pointer label
    // Skip this if there's a menu up. I just can't deal with it.
    if (hsiMenu.menuActive && hsiMenu.currentState == HSIMenu::MenuState::BROWSING) return;

    int navType = 0;
    if (g5Settings.bearingPointer1Source == 1)
        navType = g5State.vloc1Type;
    else if (g5Settings.bearingPointer1Source == 2)
        navType = g5State.vloc2Type;

    char buf[10];
    switch (navType) {
    case 1: // NOTE: If the type is LOC/ILS, then the Bearing Pointer is NOT displayed. But do display box and type.
        strcpy(buf, "ILS");
        break;
    case 2:
        strcpy(buf, "VOR");
        break;
    case 3:
        strcpy(buf, "DME");
        break;
    case 7:
        strcpy(buf, "- - -");
        break;
    default:
        strcpy(buf, "???");
        break;
    }

    // Only draw the bearing pointer if it's VOR or GPS: 1 (or ADF and the ADF is valid 4)
    if (navType == 2 || g5Settings.bearingPointer1Source == 3 || (g5Settings.bearingPointer1Source == 4 && g5State.adfValid)) {
        bearingPointer1.setBitmapColor(TFT_CYAN, TFT_BLACK);
        compass.setPivot(COMPASS_CENTER_X, COMPASS_CENTER_Y);
        bearingPointer1.pushRotated(bearingPointer1Angle - g5State.headingAngle + 180, TFT_BLACK);
    }

    // Draw the info box in the lower left
    if (g5Settings.bearingPointer1Source == 3) strcpy(buf, "GPS");
    if (g5Settings.bearingPointer1Source == 4) strcpy(buf, "ADF");
    bearingPointerBox1.fillRect(4, 25, 60, 22, TFT_BLACK); // numbers from inkscape
    bearingPointerBox1.drawString(buf, 12, bearingPointerBox1.height() - 1);
    bearingPointerBox1.pushSprite(X_OFFSET, Y_OFFSET + SCREEN_HEIGHT - dtkBox.height() - bearingPointerBox1.height() + 2, TFT_TRANSPARENT_LIGHTBLACK);
}

void CC_G5_HSI::drawBearingPointer2()
{
    // Is display turned off?
    if (g5Settings.bearingPointer2Source == 0) return;

    // Don't draw during menu to avoid collision.
    if (hsiMenu.menuActive && hsiMenu.currentState == HSIMenu::MenuState::BROWSING) return;

    // Figure out correct labels.

    int navType;
    if (g5Settings.bearingPointer2Source == 1)
        navType = g5State.vloc1Type;
    else if (g5Settings.bearingPointer2Source == 2)
        navType = g5State.vloc2Type;

    char buf[10];
    switch (navType) {
    case 1:
        strcpy(buf, "ILS");
        break;
    case 2:
        strcpy(buf, "VOR");
        break;
    case 3:
        strcpy(buf, "DME");
        break;
    case 4:
        strcpy(buf, "ADF");
        break;
    case 7:
        strcpy(buf, "- - -");
        break;
    default:
        strcpy(buf, "???");
        break;
    }

    if (g5Settings.bearingPointer2Source == 3) strcpy(buf, "GPS");
    if (g5Settings.bearingPointer2Source == 4) strcpy(buf, "ADF");

    // draw the actual Bearing Pointer, but only if it's a type that draws and is valid.
    if (navType == 2 || g5Settings.bearingPointer2Source == 3 || (g5Settings.bearingPointer2Source == 4 && g5State.adfValid)) {
        bearingPointer2.setBitmapColor(TFT_CYAN, TFT_BLACK);
        compass.setPivot(COMPASS_CENTER_X, COMPASS_CENTER_Y);
        bearingPointer2.pushRotated(bearingPointer2Angle - g5State.headingAngle + 180, TFT_BLACK);
    }

    // Draw info box in lower right
    // But don't draw it if the glideslope is active.
    if (!g5State.gsiNeedleValid) {
        bearingPointerBox2.fillRect(15, 25, 75, 22, TFT_BLACK); // numbers from inkscape
        bearingPointerBox2.drawString(buf, bearingPointerBox2.width() - 12, bearingPointerBox2.height() - 1);
        bearingPointerBox2.pushSprite(X_OFFSET + SCREEN_WIDTH - bearingPointerBox2.width(), Y_OFFSET + SCREEN_HEIGHT - headingBox.height() - bearingPointerBox2.height() + 2, TFT_TRANSPARENT_LIGHTBLACK);
    }
}

void CC_G5_HSI::drawGlideSlope()
{

    static bool wasValid = false;

    if (wasValid && !g5State.gsiNeedleValid) {
        // clear and push sprite.
        glideDeviationScale.fillSprite(TFT_BLACK);
        glideDeviationScale.pushSprite(&lcd, lcd.width() - glideDeviationScale.width() - 24, Y_OFFSET + 7 + COMPASS_CENTER_Y - glideDeviationScale.height() / 2);
        wasValid = false;
    }

    if (!g5State.gsiNeedleValid) return;

    wasValid = true;

    // Since this is drawn on the LCD, we need to erase it if it was valid and is no longer.

    // To Do: Set appropriate bug position
    //        Figure out if we skip this because wrong mode.
    //        Set right text and colors for ILS vs GPS.

    // Positive number, needle deflected down. Negative, up.

    // Change of direction. Use HSI GSI NEEDLE:1 simvar. -127 to 127.

    const float scaleMax    = 127.0;
    const float scaleMin    = -127.0;
    const float scaleOffset = 22.0;    // Distance the scale starts from top of sprite.
    const int   centerY     = 132;     // center point of deviation scale sprite.
    const float scaleFactor = 0.84375; // 216px/256deviation 0.84 pix per deviation

    // Refill the sprite to overwrite old diamond.
    glideDeviationScale.fillSprite(TFT_BLACK);
    glideDeviationScale.pushImage(0, 0, GSDEVIATION_IMG_WIDTH, GSDEVIATION_IMG_HEIGHT, GSDEVIATION_IMG_DATA);

    int markerCenterPosition = centerY + (int)(g5State.gsiNeedle * scaleFactor) - (deviationDiamond.height() / 2.0);

    if (g5State.navSource == NAVSOURCE_GPS) {
        glideDeviationScale.setTextColor(TFT_MAGENTA);
        glideDeviationScale.drawString("G", glideDeviationScale.width() / 2, 13);
        glideDeviationScale.drawBitmap(1, markerCenterPosition, DIAMONDBITMAP_IMG_DATA, DIAMONDBITMAP_IMG_WIDTH, DIAMONDBITMAP_IMG_HEIGHT, TFT_MAGENTA);

    } else {
        glideDeviationScale.setTextColor(TFT_GREEN);
        glideDeviationScale.drawString("L", glideDeviationScale.width() / 2, 13);
        glideDeviationScale.drawBitmap(1, markerCenterPosition, DIAMONDBITMAP_IMG_DATA, DIAMONDBITMAP_IMG_WIDTH, DIAMONDBITMAP_IMG_HEIGHT, TFT_GREEN);
    }

    glideDeviationScale.pushSprite(&lcd, lcd.width() - glideDeviationScale.width() - 24, Y_OFFSET + 7 + COMPASS_CENTER_Y - glideDeviationScale.height() / 2);
}

void CC_G5_HSI::drawPlaneIcon()
{
    compass.drawBitmap((COMPASS_CENTER_X) - (PLANEICON_IMG_WIDTH / 2), (COMPASS_CENTER_Y) - (PLANEICON_IMG_HEIGHT / 2), PLANEICON_IMG_DATA, PLANEICON_IMG_WIDTH, PLANEICON_IMG_HEIGHT, TFT_WHITE);
}

void CC_G5_HSI::drawDeviationScale()
{
    if (!g5State.cdiNeedleValid) return;
    compass.setPivot(COMPASS_CENTER_X, COMPASS_CENTER_Y);
    deviationScale.pushRotated(&compass, (g5State.cdiDirection - g5State.headingAngle), TFT_BLACK);
}

void CC_G5_HSI::drawCurrentHeading()
{
    // Box with direction at top of compass
    static int lastHeading = -999;

    int hdgInteger = round(g5State.headingAngle);
    // if (hdgInteger == lastHeading) return;
    lastHeading = hdgInteger;

    char hdgStr[6];

    curHdg.fillRect(3, 2, curHdg.width() - 6, curHdg.height() - 4, TFT_BLACK);
    sprintf(hdgStr, "%03d\xB0", hdgInteger);

    curHdg.drawString(hdgStr, curHdg.width() / 2, curHdg.height() / 2 + 1);
    curHdg.pushSprite(&lcd, (SCREEN_WIDTH / 2) - curHdg.width() / 2 + HEADINGBOX_LEFT_SHIFT, Y_OFFSET);
}

void CC_G5_HSI::drawVORCourseBox()
{
    // Draw the lower left course heading box. We reuse the ground speed sprite.

    int boxWidth = courseBox.width(), boxHeight = courseBox.height();
    int borderWidth = 2;

    courseBox.setTextColor(TFT_WHITE, TFT_BLACK);
    courseBox.fillSprite(TFT_BLACK);
    courseBox.drawRect(0, 0, boxWidth, boxHeight, DATA_BOX_OUTLINE_COLOR);
    courseBox.drawRect(1, 1, boxWidth - 2, boxHeight - 2, DATA_BOX_OUTLINE_COLOR);
    courseBox.setTextDatum(BL_DATUM);
    courseBox.loadFont(PrimaSans12);
    courseBox.drawString(g5State.obsModeOn ? "OBS" : "CRS", borderWidth + 1, boxHeight);
    courseBox.loadFont(PrimaSans18);
    courseBox.setTextColor(TFT_GREEN, TFT_BLACK);
    char buf[6];
    if (g5State.cdiNeedleValid)
        sprintf(buf, "%3.0f °", g5State.obsAngle);
    else
        sprintf(buf, "---°");
    courseBox.setTextDatum(BR_DATUM);
    courseBox.drawString(buf, boxWidth - (borderWidth + 1), boxHeight);
    courseBox.pushSprite(X_OFFSET, Y_OFFSET + SCREEN_HEIGHT - courseBox.height());
}

void CC_G5_HSI::drawCDIPointer()
{
    if (!g5State.cdiNeedleValid) return;
    compass.setPivot(COMPASS_CENTER_X, COMPASS_CENTER_Y);
    if (g5State.navSource == NAVSOURCE_GPS) {
        cdiPtr.pushRotated(&compass, g5State.cdiDirection - g5State.headingAngle, TFT_WHITE);
    } else {
        cdiPtr.pushRotated(&compass, g5State.obsAngle - g5State.headingAngle, TFT_WHITE);
    }
}

void CC_G5_HSI::drawCurrentTrack()
{
    // Draw the magenta triangle at the end of the dashed line
    // I think this is valid as long as our airspeed is decent.
    if (g5State.groundSpeed < 20) return;
    compass.setPivot(COMPASS_CENTER_X, COMPASS_CENTER_Y);
    currentTrackPtr.pushRotated(&compass, g5State.groundTrack - g5State.headingAngle, TFT_BLACK);
}

void CC_G5_HSI::drawCDIBar()
{
    // Full scale offset can be configured on the real unit. Here we will use 5 miles full scale
    // either side, except in terminal mode when it is 1 mile full deflection.

    // This code works for GPS or NAV. We set the color in setNavSource.
    // Needle deflection (+/- 127)
    // Total needle deflection value: 254.
    // Scale width cdiBar.width(); (pixels)

    if (!g5State.cdiNeedleValid) return;
    if (g5State.cdiToFrom == 0 && g5State.navSource == NAVSOURCE_NAV) return; // We're not tuned to a station

    float barAngle;
    if (g5State.navSource == NAVSOURCE_GPS && !g5State.obsModeOn)
        barAngle = g5State.cdiDirection;
    else
        barAngle = g5State.obsAngle;

    float pixelOffset = g5State.cdiOffset * (float)DEVIATIONSCALE_IMG_WIDTH / 248; // needs to move a little more
    compass.setPivot(COMPASS_CENTER_X + (pixelOffset * (float)cos((barAngle - g5State.headingAngle) * PIf / 180.0f)),
                     (COMPASS_CENTER_Y + (pixelOffset * (float)sin((barAngle - g5State.headingAngle) * PIf / 180.0f))));
    cdiBar.setPivot(cdiBar.width() / 2, cdiBar.height() / 2); // Pivot around the middle of the sprite.
    cdiBar.pushRotated(&compass, barAngle - g5State.headingAngle);

    compass.setPivot(COMPASS_CENTER_X, COMPASS_CENTER_Y);

    toFrom.setPivot(TOFROM_IMG_WIDTH / 2, 46); // It's actually really close to the pivot. Was 86
    // Fix: If we're on an ILS or localizer approach, we don't draw this.
    if (g5State.gpsApproachType == 99)
        toFrom.pushRotated(&compass, barAngle - g5State.headingAngle + (g5State.cdiToFrom == 2 ? 180 : 0), TFT_WHITE);
}

void CC_G5_HSI::drawHeadingBug()
{
    compass.setPivot(COMPASS_CENTER_X, COMPASS_CENTER_Y); // Center of Compass
    headingBug.pushRotated(&compass, g5State.headingBugAngle - g5State.headingAngle, TFT_WHITE);
}

// This is the cyan box in the lower right.
void CC_G5_HSI::drawHeadingBugValue()
{
    static int lastHeadingbug = -999;
    if (lastHeadingbug == g5State.headingBugAngle && !g5State.forceRedraw) return;
    lastHeadingbug = g5State.headingBugAngle;

    char buf[5];
    sprintf(buf, "%03d\xB0", g5State.headingBugAngle);
    headingBox.fillRect(HEADINGBUGSMALL_IMG_WIDTH + 8, 3, headingBox.width() - (HEADINGBUGSMALL_IMG_WIDTH + 10), headingBox.height() - 6, TFT_BLACK);
    headingBox.drawString(buf, headingBox.width() - 7, headingBox.height());
    headingBox.pushSprite(X_OFFSET + SCREEN_WIDTH - headingBox.width(), Y_OFFSET + SCREEN_HEIGHT - headingBox.height());
}

void CC_G5_HSI::drawDesiredTrack()
{

    if (g5State.navSource != NAVSOURCE_GPS) return;

    // Magenta in box in lower left.
    static float lastDesiredTrack = -1;
    static int   lastValid        = -1;

    if (g5State.desiredTrackValid == lastValid && lastDesiredTrack == g5State.desiredTrack && !g5State.forceRedraw) {
        return;
    }

    lastDesiredTrack = g5State.desiredTrack;
    lastValid        = g5State.desiredTrackValid;

    dtkBox.fillSprite(TFT_BLACK);
    dtkBox.drawRect(0, 0, dtkBox.width(), dtkBox.height(), DATA_BOX_OUTLINE_COLOR);
    dtkBox.drawRect(1, 1, dtkBox.width() - 2, dtkBox.height() - 2, DATA_BOX_OUTLINE_COLOR);

    dtkBox.loadFont(PrimaSans12);

    dtkBox.setTextColor(TFT_WHITE);
    dtkBox.setTextDatum(BL_DATUM);
    dtkBox.drawString("DTK", 4, dtkBox.height() - 2);

    char buf[8];
    if (g5State.desiredTrackValid == 1)
        sprintf(buf, "%03.0f\xB0", g5State.desiredTrack);
    else
        sprintf(buf, "- - - \xB0");
    dtkBox.loadFont(PrimaSans18);
    dtkBox.setTextColor(TFT_MAGENTA);
    dtkBox.setTextDatum(BR_DATUM);
    // courseBox.fillRect(42, 6, 78, 30, TFT_BLACK);
    dtkBox.drawString(buf, dtkBox.width() - 9, dtkBox.height());
    dtkBox.pushSprite(0, Y_OFFSET + SCREEN_HEIGHT - dtkBox.height());
}

void CC_G5_HSI::drawDistNextWaypoint()
{
    if (g5State.navSource != NAVSOURCE_GPS) return;
    // Magenta in box in upper right.
    // Let's put it in the upper right to make room for wind direction.
    static float lastDist  = -1.0f;
    static int   lastValid = -1;

    if (lastDist == g5State.distNextWaypoint && lastValid == g5State.cdiNeedleValid && !g5State.forceRedraw) return; // it's on LCD so no need to redraw.

    lastDist  = g5State.distNextWaypoint;
    lastValid = g5State.cdiNeedleValid;

    distBox.loadFont(PrimaSans18);
    distBox.setTextDatum(BR_DATUM);
    distBox.setTextSize(1.0);

    char buf[7];
    if (g5State.cdiNeedleValid)
        sprintf(buf, "%0.1f", g5State.distNextWaypoint);
    else
        sprintf(buf, "---.-");
    distBox.fillRect(distBox.width() - 105, 3, 100, distBox.height() - 6, TFT_BLACK);
    distBox.drawString(buf, distBox.width() - 28, distBox.height());
    distBox.loadFont(PrimaSans12);
    distBox.setTextSize(0.8);
    distBox.setTextDatum(TL_DATUM);
    distBox.drawString("n", distBox.width() - 26, 3);
    distBox.drawString("m", distBox.width() - 26, 15);
    distBox.pushSprite(SCREEN_WIDTH + X_OFFSET - distBox.width(), Y_OFFSET);
}

void CC_G5_HSI::drawWind()
{

    // grey box with arrow in upper left
    // If airspeed is less than 30 knots, wind data unavailable as it relies on gps.
    // Is it worth geting airspeed just to blank this out?

    const int x_center = 70;

    static float lastDir   = -20.0f;
    static float lastSpeed = -20.0f;

    // Ignore small changes. The MSFS values are always changing.
    if (fabs(lastDir - g5State.windDir) < 4.0f && fabs(lastSpeed - g5State.windSpeed) < 1.0f && !g5State.forceRedraw) return;
    lastDir   = g5State.windDir;
    lastSpeed = g5State.windSpeed;

    windBox.fillRect(3, 3, windBox.width() - 6, windBox.height() - 6, TFT_BLACK);

    char buf[10];
    if (g5State.windSpeed < 0) {
        windBox.drawString("NO WIND", x_center - 5, 15);
        windBox.drawString("DATA", x_center, 33);
    } else if (g5State.windSpeed < 2.0f) {
        windBox.drawString("WIND", x_center, 15);
        windBox.drawString("CALM", x_center, 33);
    } else {
        windArrow.pushRotated(((int)(g5State.windDir - g5State.headingAngle) + 180 + 360) % 360, TFT_BLACK);
        sprintf(buf, "%d KT", (int)(g5State.windSpeed + 0.5f));
        windBox.drawString(buf, x_center, 15);
        sprintf(buf, "%02d0\xB0", (int)(g5State.windDir + 0.5f) / 10);
        windBox.drawString(buf, x_center, 33);
    }
    windBox.pushSprite(X_OFFSET, Y_OFFSET);
}

void CC_G5_HSI::saveState()
{
    CC_G5_Base::saveState(); // saves common g5State fields (switching flag, IDs 0-10)

    // HSI-specific fields (IDs 30-46)
    Preferences prefs;
    prefs.begin("g5state", false);
    prefs.putFloat("adfBrg", g5State.bearingAngleADF);
    prefs.putInt("adfVal", g5State.adfValid);
    prefs.putInt("cdiDir", g5State.cdiDirection);
    prefs.putInt("cdiLbl", g5State.cdiScaleLabel);
    prefs.putFloat("desTrk", g5State.desiredTrack);
    prefs.putInt("dtVal", g5State.desiredTrackValid);
    prefs.putFloat("distWp", g5State.distNextWaypoint);
    prefs.putFloat("gpsBrg", g5State.bearingAngleGPS);
    prefs.putInt("navLbl", g5State.navCDILabelIndex);
    prefs.putFloat("v1Brg", g5State.bearingAngleVLOC1);
    prefs.putInt("v1Type", g5State.vloc1Type);
    prefs.putFloat("v2Brg", g5State.bearingAngleVLOC2);
    prefs.putInt("v2Type", g5State.vloc2Type);
    prefs.putInt("obsOn", g5State.obsModeOn);
    prefs.putFloat("obsAng", g5State.obsAngle);
    prefs.putFloat("wndDir", g5State.rawWindDir);
    prefs.putFloat("wndSpd", g5State.rawWindSpeed);

    prefs.end();
}

bool CC_G5_HSI::restoreState()
{
    if (!CC_G5_Base::restoreState())
        return false;

    // HSI-specific fields
    Preferences prefs;
    prefs.begin("g5state", false);
    g5State.bearingAngleADF   = prefs.getFloat("adfBrg", 0);
    g5State.adfValid          = prefs.getInt("adfVal", 0);
    g5State.cdiDirection      = prefs.getInt("cdiDir", 0);
    g5State.cdiScaleLabel     = prefs.getInt("cdiLbl", 0);
    g5State.desiredTrack      = prefs.getFloat("desTrk", 0);
    g5State.desiredTrackValid = prefs.getInt("dtVal", 0);
    g5State.distNextWaypoint  = prefs.getFloat("distWp", 0);
    g5State.bearingAngleGPS   = prefs.getFloat("gpsBrg", 0);
    g5State.navCDILabelIndex  = prefs.getInt("navLbl", 0);
    g5State.bearingAngleVLOC1 = prefs.getFloat("v1Brg", 0);
    g5State.vloc1Type         = prefs.getInt("v1Type", 0);
    g5State.bearingAngleVLOC2 = prefs.getFloat("v2Brg", 0);
    g5State.vloc2Type         = prefs.getInt("v2Type", 0);
    g5State.obsModeOn         = prefs.getInt("obsOn", 0);
    g5State.obsAngle          = prefs.getFloat("obsAng", 0);
    g5State.rawWindDir        = prefs.getFloat("wndDir", 0);
    g5State.rawWindSpeed      = prefs.getFloat("wndSpd", 0);
    prefs.end();
    return true;
}