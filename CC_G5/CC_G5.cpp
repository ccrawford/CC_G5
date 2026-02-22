#include "CC_G5.h"
#include "esp_log.h"

#include <Wire.h>


#include "allocateMem.h"
#include "commandmessenger.h"
#include "Sprites\cdiPointer.h"
#include "Sprites\planeIcon_1bit.h"
#include "Sprites\cdiBar.h"
#include "Sprites\currentTrackPointer.h"
#include "Sprites\headingBug.h"
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

#include "Sprites\gsBox.h"
#include "Sprites\distBox.h"
#include "Sprites\headingBox.h"

// Global sprites for display

LGFX_Sprite compass(&lcd);
LGFX_Sprite curHdg(&lcd);
LGFX_Sprite curDME(&lcd);
LGFX_Sprite plane(&lcd);              // The airplane silhouette
LGFX_Sprite deviationScale(&compass); // The four circles
LGFX_Sprite glideDeviationScale(&lcd);
LGFX_Sprite deviationDiamond(&glideDeviationScale);
LGFX_Sprite cdiPtr(&lcd);
LGFX_Sprite currentTrackPtr(&lcd);
LGFX_Sprite cdiBar(&lcd);
LGFX_Sprite headingBug(&compass);
LGFX_Sprite toFrom(&compass);
LGFX_Sprite distBox(&lcd);
LGFX_Sprite dtkBox(&lcd);
LGFX_Sprite bearingPointer1(&compass);
LGFX_Sprite bearingPointer2(&compass);
LGFX_Sprite bearingPointerBox(&lcd);
LGFX_Sprite bearingPointerBox2(&lcd);

LGFX_Sprite batteryHolderSprite(&lcd);

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

    // Setup menu structure

    hsiMenu.initializeMenu();

    ESP_LOGI(TAG_I2C, "Setting up i2c");
    // Configure i2c pins
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

    lcd.fillScreen(TFT_BACKGROUND_COLOR);
    g5State.forceRedraw = true;

    // lcd.setBrightness(255);  // Works with Guition.  not sure about Waveshare.

    lcd.setColorDepth(8);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.loadFont(PrimaSans32);

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
    curHdg.createSprite(83, 40);
    curHdg.setTextColor(TFT_WHITE, TFT_BLACK);

    {
        const int hdgWidth           = curHdg.width() - 1;
        const int hdgHeight          = curHdg.height() - 1;
        const int triangleWidth      = 25;
        const int triangleHeight     = 15;
        const int triangleLeftOffset = (hdgWidth - triangleWidth) / 2;
        const int rectangleHeight    = hdgHeight;

        curHdg.fillSprite(TFT_BLACK);
        curHdg.setColor(TFT_WHITE);
        curHdg.setTextColor(TFT_WHITE);
        curHdg.setTextDatum(MC_DATUM);
        curHdg.loadFont(PrimaSans32);
        curHdg.setTextSize(1.0);

        curHdg.drawLine(0, 0, hdgWidth, 0, TFT_WHITE);                                                   // ------
        curHdg.drawLine(0, 0, 0, rectangleHeight);                                                       //  |
        curHdg.drawLine(0, rectangleHeight, triangleLeftOffset, rectangleHeight);                        // -
        curHdg.drawLine(triangleLeftOffset + triangleWidth, rectangleHeight, hdgWidth, rectangleHeight); // -
        curHdg.drawLine(hdgWidth, rectangleHeight, hdgWidth, 1);                                         // |
    }

    // Calculate compass center coordinates (needs curHdg.height())
    const int compassLeftShift = (lcd.width() - compass.width()) / 2;
    const int compassTopShift  = curHdg.height();
    compassCenterX             = compassLeftShift + (compass.width() / 2);
    compassCenterY             = compassTopShift + (compass.height() / 2);

    glideDeviationScale.createSprite(GSDEVIATION_IMG_WIDTH, GSDEVIATION_IMG_HEIGHT);
    glideDeviationScale.pushImage(0, 0, GSDEVIATION_IMG_WIDTH, GSDEVIATION_IMG_HEIGHT, GSDEVIATION_IMG_DATA);
    glideDeviationScale.setTextColor(TFT_MAGENTA);
    glideDeviationScale.setTextSize(0.5);
    glideDeviationScale.setTextDatum(CC_DATUM);
    glideDeviationScale.loadFont(PrimaSans32);

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

    bearingPointerBox.setColorDepth(8);
    bearingPointerBox.createSprite(BEARINGPOINTERBOXLEFT_IMG_WIDTH, BEARINGPOINTERBOXLEFT_IMG_HEIGHT);
    bearingPointerBox.pushImage(0, 0, BEARINGPOINTERBOXLEFT_IMG_WIDTH, BEARINGPOINTERBOXLEFT_IMG_HEIGHT, BEARINGPOINTERBOXLEFT_IMG_DATA);
    bearingPointerBox.loadFont(PrimaSans32);
    bearingPointerBox.setTextDatum(BL_DATUM);
    bearingPointerBox.setTextColor(TFT_CYAN, TFT_BLACK);
    bearingPointerBox.setTextSize(0.7);

    bearingPointerBox2.setColorDepth(8);
    bearingPointerBox2.createSprite(BEARINGPOINTERBOXRIGHT_IMG_WIDTH, BEARINGPOINTERBOXRIGHT_IMG_HEIGHT);
    bearingPointerBox2.pushImage(0, 0, BEARINGPOINTERBOXRIGHT_IMG_WIDTH, BEARINGPOINTERBOXRIGHT_IMG_HEIGHT, BEARINGPOINTERBOXRIGHT_IMG_DATA);
    bearingPointerBox2.loadFont(PrimaSans32);
    bearingPointerBox2.setTextDatum(BR_DATUM);
    bearingPointerBox2.setTextColor(TFT_CYAN, TFT_BLACK);
    bearingPointerBox2.setTextSize(0.7);

    plane.setColorDepth(1);
    plane.setBitmapColor(TFT_WHITE, TFT_BLACK);
    plane.createSprite(PLANEICON_IMG_WIDTH, PLANEICON_IMG_HEIGHT);
    plane.setBuffer(const_cast<std::uint8_t *>(PLANEICON_IMG_DATA), PLANEICON_IMG_WIDTH, PLANEICON_IMG_HEIGHT);

    // // GS Is Ground speed, NOT glide slope. THIS IS USED FOR MORE THAN JUST GROUND SPEED
    gsBox.setColorDepth(8);
    gsBox.createSprite(135, 45);
    // // gsBox.pushImage(0, 0, GSBOX_IMG_WIDTH, GSBOX_IMG_HEIGHT, GSBOX_IMG_DATA);
    // //gsBox.setTextColor(TFT_MAGENTA);
    // //gsBox.setTextDatum(BR_DATUM);
    gsBox.loadFont(PrimaSans32);

    distBox.setColorDepth(8);
    distBox.createSprite(DISTBOX_IMG_WIDTH, DISTBOX_IMG_HEIGHT);
    distBox.pushImage(0, 0, DISTBOX_IMG_WIDTH, DISTBOX_IMG_HEIGHT, DISTBOX_IMG_DATA);
    distBox.setTextColor(TFT_MAGENTA);
    distBox.setTextDatum(BR_DATUM);
    distBox.loadFont(PrimaSans32);

    headingBox.setColorDepth(8);
    headingBox.createSprite(HEADINGBOX_IMG_WIDTH, HEADINGBOX_IMG_HEIGHT);
    headingBox.pushImage(0, 0, HEADINGBOX_IMG_WIDTH, HEADINGBOX_IMG_HEIGHT, HEADINGBOX_IMG_DATA);
    headingBox.setTextColor(TFT_CYAN);
    headingBox.setTextDatum(BR_DATUM);
    headingBox.loadFont(PrimaSans32);

    batteryHolderSprite.setColorDepth(8);
    batteryHolderSprite.createSprite(100, 40);

    windBox.setColorDepth(8);
    windBox.setBitmapColor(TFT_WHITE, TFT_BLACK);
    windBox.createSprite(GSBOX_IMG_WIDTH, GSBOX_IMG_HEIGHT); // Make the same width and height as the gsBox.
    windBox.setTextColor(TFT_WHITE);
    windBox.loadFont(PrimaSans32);
    windBox.setTextSize(0.5);
    windBox.setTextDatum(CC_DATUM);
    windBox.fillSprite(TFT_BLACK);
    windBox.drawRect(0, 0, windBox.width(), windBox.height(), TFT_WHITE);
    windBox.drawRect(1, 1, windBox.width() - 2, windBox.height() - 2, TFT_WHITE);
    windBox.setPivot(WINDARROW_IMG_WIDTH, windBox.height() / 2);

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
    void *buffer = compass.createSprite(406, 406);

    if (buffer != nullptr) {
        compass.loadFont(PrimaSans32);
        compass.setTextColor(TFT_WHITE, TFT_BLACK);

        // Compass center coordinates are calculated in setupSprites() after curHdg is created

    } else {
//        ESP_LOGE(TAG_SPRITES, "ERROR: Compass sprite creation FAILED!");
    }

    // Add missing sprite setup that was in original
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

    headingBug.setBuffer(const_cast<std::uint16_t *>(HEADINGBUG_IMG_DATA), HEADINGBUG_IMG_WIDTH, HEADINGBUG_IMG_HEIGHT, 16);
    headingBug.setPivot(HEADINGBUG_IMG_WIDTH / 2, HEADINGBUG_IMG_HEIGHT + COMPASS_OUTER_RADIUS);

    toFrom.setBuffer(const_cast<std::uint16_t *>(TOFROM_IMG_DATA), TOFROM_IMG_WIDTH, TOFROM_IMG_HEIGHT, 16);
    toFrom.setPivot(TOFROM_IMG_WIDTH / 2, 86);
}

void CC_G5_HSI::updateCommon()
{
    // Swap sprite buffers between magenta (GPS) and green (NAV) when navSource changes.
    // setNavSource() has an internal guard so it's a no-op when the source hasn't changed.
    setNavSource();

    // Clear the compass sprite. Use RED as the transparent color. It's not used in the display.
    // compass.fillSprite(TFT_BLACK);
    compass.fillSprite(TFT_MAIN_TRANSPARENT);

    compass.fillCircle(compass.width() / 2, compass.height() / 2, COMPASS_OUTER_RADIUS + 16, TFT_BLACK);
    // curHdg.fillSprite(TFT_BLACK);

    drawCompass();

    // if (g5State.navSource == NAVSOURCE_GPS)
    //     updateGps();
    // else
    //     updateNav();

    drawCDIScaleLabel();
    drawCDISource();
    drawNavCDILabel();

    drawDeviationScale();
    drawCurrentTrack();
    drawCDIPointer();
    drawCDIBar();
    drawPlaneIcon();
    drawWPTAlert();

    drawCurrentHeading();
    drawHeadingBug();
    drawBearingPointer1();
    drawBearingPointer2();

    // Draw adjustment popup onto compass before pushing to LCD
    // if (menu.currentState == MenuState::ADJUSTING) {
    //     drawAdjustmentPopup();
    // }

    drawCompassOuterMarkers();
    processMenu();
    brightnessMenu.draw(&compass);

    drawShutdown(&compass);

    compass.pushSprite(&lcd, (lcd.width() - compass.width()) / 2, curHdg.height(), TFT_MAIN_TRANSPARENT);
    curHdg.pushSprite(&lcd, (480 / 2) - curHdg.width() / 2, CUR_HEADING_Y_OFFSET);

    drawGlideSlope();

    drawDistNextWaypoint();
    drawHeadingBugValue();
    drawDesiredTrack();

    drawWind();
    drawBattery(&batteryHolderSprite, 0, 0);
    batteryHolderSprite.pushSprite(0, windBox.height());
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
        // compass.fillRect(hsiMenu.menuXpos, hsiMenu.menuYpos, hsiMenu.menuWidth, hsiMenu.menuHeight, TFT_BLACK);
        lcd.clearDisplay(TFT_BLACK);
        g5State.forceRedraw = true;
    }
}

void CC_G5_HSI::updateNav()
{
    drawCDIScaleLabel();
    drawRadioNavApproachType();
    drawNavCDILabel();

    drawDeviationScale();
    drawCDIPointer();
    drawCDIBar();

    drawCurrentTrack();

    drawPlaneIcon();

    drawCurrentHeading();
    drawBearingPointer1();
    drawBearingPointer2();
    drawHeadingBug();

    // Draw adjustment popup onto compass before pushing to LCD
    // if (menu.currentState == MenuState::ADJUSTING) {
    //     drawAdjustmentPopup();
    // }

    drawCompassOuterMarkers();
    processMenu(); // writes to the compass sprite. Must do before pushing compass.
    brightnessMenu.draw(&compass);
    drawShutdown(&compass);

    compass.pushSprite(&lcd, (lcd.width() - compass.width()) / 2, curHdg.height(), TFT_MAIN_TRANSPARENT);
    // curHdg.pushSprite(&lcd, (480 / 2) - curHdg.width() / 2, CUR_HEADING_Y_OFFSET);

    drawGlideSlope();

    drawHeadingBugValue();
    drawVORCourseBox();
    drawWind();
}

void CC_G5_HSI::updateGps()
{

    drawCDIScaleLabel();
    drawCDISource();
    drawWPTAlert();

    drawDeviationScale();
    drawCurrentTrack();
    drawCDIPointer();
    drawCDIBar();
    drawPlaneIcon();

    drawCurrentHeading();
    drawHeadingBug();
    drawBearingPointer1();
    drawBearingPointer2();

    // Draw adjustment popup onto compass before pushing to LCD
    // if (menu.currentState == MenuState::ADJUSTING) {
    //     drawAdjustmentPopup();
    // }

    drawCompassOuterMarkers();
    processMenu();
    brightnessMenu.draw(&compass);

    drawShutdown(&compass);

    compass.pushSprite(&lcd, (lcd.width() - compass.width()) / 2, curHdg.height(), TFT_MAIN_TRANSPARENT);
    curHdg.pushSprite(&lcd, (480 / 2) - curHdg.width() / 2, CUR_HEADING_Y_OFFSET);

    drawGlideSlope();

    drawDistNextWaypoint();
    drawHeadingBugValue();
    drawDesiredTrack();
    drawWind();
}

void CC_G5_HSI::attach()
{
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
    }
}

// OLD and UNUSED
void CC_G5_HSI::set(int16_t messageID, char *setPoint)
{
    /* **********************************************************************************
        MessageID == -2 will be send from the board when PowerSavingMode is set
            Message will be "0" for leaving and "1" for entering PowerSavingMode
        MessageID == -1 will be send from the connector when Connector stops running
    *********************   ************************************************************* */
    Serial.printf("Wait. set was just called. NFG\n");
    while (1) {
        Serial.printf("Wait. set was just called. NFG\n");
        delay(1000);
    }

    // int32_t data = 0;

    // if (setPoint != NULL) data = atoi(setPoint);

    // uint16_t output;
    // float    value;

    // switch (messageID) {
    // case -1:
    //     // tbd., get's called when Mobiflight shuts down
    //     break;
    // case -2:
    //     // tbd., get's called when PowerSavingMode is entered
    //     break;
    // case 0:
    //     g5State.rawHeadingAngle = atof(setPoint);
    //     break;
    // case 1:
    //     output                  = (uint16_t)data;
    //     g5State.headingBugAngle = output;
    //     break;
    // case 2:
    //     output               = (uint16_t)data;
    //     g5State.cdiDirection = output;
    //     break;
    // case 3:
    //     output            = (uint16_t)data;
    //     g5State.cdiToFrom = output;
    //     break;
    // case 4:
    //     g5State.rawCdiOffset = atof(setPoint);
    //     break;
    // case 5:
    //     g5State.rawGsiNeedle = atof(setPoint);
    //     break;
    // case 6:
    //     output              = (uint16_t)data;
    //     g5State.groundSpeed = output;
    //     break;
    // case 7:
    //     output              = (uint16_t)data;
    //     g5State.groundTrack = output;
    //     break;
    // case 8:
    //     value                    = atof(setPoint);
    //     g5State.distNextWaypoint = value;
    //     break;
    // case 9:
    //     output            = (uint16_t)data;
    //     g5State.navSource = output;
    //     setNavSource();
    //     break;
    // case 10:
    //     output                = (uint16_t)data;
    //     g5State.cdiScaleLabel = output;
    //     break;
    // case 11:
    //     output                  = (uint16_t)data;
    //     g5State.gpsApproachType = output;
    //     break;
    // case 12:
    //     output                   = (uint16_t)data;
    //     g5State.navCDILabelIndex = output;
    //     break;
    // case 13:
    //     g5State.obsModeOn = (uint16_t)data;
    //     break;
    // case 14:
    //     g5State.obsAngle = atof(setPoint);
    //     break;
    // case 15:
    //     g5State.gsiNeedleValid = (int)data;
    //     break;
    // case 16:
    //     g5State.cdiNeedleValid = (uint16_t)data;
    //     break;
    // case 17:
    //     g5State.rawWindDir = (uint16_t)data;
    //     break;
    // case 18:
    //     g5State.rawWindSpeed = atof(setPoint);
    //     break;
    // case 19:
    //     g5State.desiredTrack = atof(setPoint);
    //     break;
    // case 20:
    //     g5State.desiredTrackValid = (int)data;
    //     break;
    // case 21:
    //     g5State.bearingAngleGPS = atof(setPoint);
    //     break;
    // case 22:
    //     g5State.bearingAngleVLOC1 = atof(setPoint);
    //     break;
    // case 23:
    //     g5State.bearingAngleVLOC2 = atof(setPoint);
    //     break;
    // case 24:
    //     g5State.vloc1Type = data;
    //     break;
    // case 25:
    //     g5State.vloc2Type = data;
    //     break;
    // case 26:
    //     g5State.bearingAngleADF = atof(setPoint);
    //     break;
    // case 27:
    //     g5State.adfValid = data;
    //     break;
    // default:
    //     break;
    // }
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
    case 3: // VLOC2dry
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
    g5State.gsiNeedle    = smoothInput(g5State.rawGsiNeedle, g5State.gsiNeedle, 0.15f, 1.0f);
    g5State.cdiOffset    = smoothInput(g5State.rawCdiOffset, g5State.cdiOffset, 0.15f, 1.0f);
    g5State.windDir      = smoothInput(g5State.rawWindDir, g5State.windDir, 0.15f, 5.0f);
    g5State.windSpeed    = smoothInput(g5State.rawWindSpeed, g5State.windSpeed, 0.15f, 0.2f);
    bearingPointer1Angle = smoothDirection(getBearingPointerAngle(g5Settings.bearingPointer1Source), bearingPointer1Angle, 0.25f, 0.1f);
    bearingPointer2Angle = smoothDirection(getBearingPointerAngle(g5Settings.bearingPointer2Source), bearingPointer2Angle, 0.25f, 0.1f);
}

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

    g5State.forceRedraw = false;

    lcd.setTextSize(0.5);
    sprintf(buf, "HSI %4.1f f/s", 1000.0 / (now - lastFrameUpdate));
    //   lcd.drawString(buf, 360, 0);
    lastFrameUpdate = now;
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
    static int tickLength = 16;
    static int radius     = COMPASS_OUTER_RADIUS + 17;

    int angles[] = {0, 45, 135, 180, 225, 315};

    for (int angle : angles) {

        // float innerX = compassCenterX + (radius) * (float)cos(angle * PIf / 180.0f);
        float innerX = compass.width() / 2 + (radius) * (float)cos(angle * PIf / 180.0f);
        // float innerY = compassCenterY + (radius) * (float)sin(angle * PIf / 180.0f);
        float innerY = compass.height() / 2 + (radius) * (float)sin(angle * PIf / 180.0f);

        float outerX = compass.width() / 2 + (radius + tickLength) * (float)cos(angle * PIf / 180.0f);
        float outerY = compass.height() / 2 + (radius + tickLength) * (float)sin(angle * PIf / 180.0f);

        // Draw the tick mark
        compass.drawWideLine(outerX, outerY, innerX, innerY, 2, 0xFFFFFF);
    }
}

void CC_G5_HSI::drawNavCDILabel()
{
    //  This is drawn at the 10 o'clock position on the compass.
    if (g5State.navSource == NAVSOURCE_GPS) return;
    compass.setTextColor(TFT_GREEN, TFT_BLACK);
    compass.setTextSize(0.5);
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
    compass.drawString(mode, compass.width() / 2 - 17, compass.height() / 2 - 40);
}

void CC_G5_HSI::drawRadioNavApproachType()
{
    // This is drawn at the 2 o'clock position on the compass.
    if (g5State.navSource == NAVSOURCE_GPS) return;

    compass.setTextColor(TFT_GREEN, TFT_BLACK);
    compass.setTextSize(0.5);
    char mode[10];

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
    } else {
        sprintf(mode, "", g5State.gpsApproachType); // Unknown number.
    }

    compass.setTextDatum(BL_DATUM);
    // Clear the old text
    compass.fillRect(compass.width() / 2 + 3, compass.height() / 2 - 60, 50, 20, TFT_BLACK);
    compass.drawString(mode, compass.width() / 2 + 10, compass.height() / 2 - 40);
}

void CC_G5_HSI::drawCDIScaleLabel()
{

    if (g5State.navSource == NAVSOURCE_GPS) {
        compass.setTextColor(TFT_MAGENTA, TFT_BLACK);
    } else {
        compass.setTextColor(TFT_GREEN, TFT_BLACK);
    }

    compass.setTextSize(0.5);
    char mode[10];

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
        sprintf(mode, "xxx", g5State.cdiScaleLabel); // Unknown number.
    }

    compass.setTextDatum(BL_DATUM);
    compass.fillRect(compass.width() / 2 + 10, compass.height() / 2 - 60, 50, 20, TFT_BLACK);
    compass.drawString(mode, compass.width() / 2 + 10, compass.height() / 2 - 40);

    // if obs is on, put indicator up in lower right of inner circle.
    if (g5State.obsModeOn) {
        compass.setTextDatum(TL_DATUM);
        compass.drawString("OBS", compass.width() / 2 + 17, compass.height() / 2 + 40);
    }
}

void CC_G5_HSI::drawCDISource()
{
    if (g5State.navSource != NAVSOURCE_GPS) return;

    compass.setTextColor(TFT_MAGENTA, TFT_BLACK);
    compass.setTextSize(0.5);
    compass.setTextDatum(BR_DATUM);
    compass.drawString("GPS", compass.width() / 2 - 20, compass.height() / 2 - 40);
}

void CC_G5_HSI::drawWPTAlert()
{
    static int lastETE = -999;

    if (g5State.navSource != NAVSOURCE_GPS) return;

    // If the ete to next wp is less than 30 sec and getting lower, then show WPT reminder
    if (g5State.gpsEteWp < 30 && g5State.gpsEteWp > 10 && g5State.gpsEteWp <= lastETE 
            && (millis() % 1000) > 300 ) 
        {
        compass.setTextColor(TFT_WHITE, TFT_BLACK);
        compass.setTextSize(0.5);
        compass.setTextDatum(BR_DATUM);
        compass.drawString("WPT", compass.width() / 2 - 20, compass.height() / 2 + 40);

        // Turn on the WPT blinker.
    }

    lastETE = g5State.gpsEteWp;
}

void CC_G5_HSI::drawCompass()
{

    int   centerX          = compass.width() / 2;
    int   centerY          = compass.height() / 2;
    int   outerRadius      = COMPASS_OUTER_RADIUS; // Leave some margin
    int   innerRadius      = COMPASS_INNER_RADIUS; // Inner circle radius
    int   majorTickLength  = 22;
    int   mediumTickLength = 15;
    int   smallTickLength  = 9;
    int   majorTickWidth   = 2;
    int   mediumTickWidth  = 1;
    int   smallTickWidth   = 1;
    float offsetAngle      = g5State.headingAngle - 0; // Initial offset angle in degrees

    // Draw the outer circle
    //    lcd.drawCircle(centerX, centerY, outerRadius, TFT_WHITE);

    // Draw the inner circle NOT SURE WHEN TO DRAW THIS...MAYBE IF NO CDI?
    //     compass.drawCircle(centerX, centerY, innerRadius, TFT_WHITE);

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
            compass.setTextSize(0.9);

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

    // Draw the V of the pointer. Gotta do it here to prevent flicker.
    compass.drawLine(centerX - 12, 0 + CUR_HEADING_Y_OFFSET, centerX, 9 + CUR_HEADING_Y_OFFSET, TFT_LIGHTGRAY);
    compass.drawLine(centerX, 9 + CUR_HEADING_Y_OFFSET, centerX + 12, 0 + CUR_HEADING_Y_OFFSET, TFT_LIGHTGRAY);

    // Draw the big orange/yellow tick at the top
    compass.drawWideLine(centerX, 42, centerX, majorTickLength + 42, 2, TFT_ORANGE);

    // draw the inner circle
    compass.drawCircle(centerX, centerY, innerRadius + 2, TFT_WHITE);
    compass.drawCircle(centerX, centerY, innerRadius + 1, TFT_WHITE);
}

void CC_G5_HSI::drawBearingPointer1()
{
    // draw the Bearing Pointer
    if (g5Settings.bearingPointer1Source == 0) return;

    // draw the Bearing Pointer label
    // Skip this if there's a menu up. I just can't deal with it.
    if (hsiMenu.menuActive && hsiMenu.currentState == HSIMenu::MenuState::BROWSING) return;

    int navType;
    if (g5Settings.bearingPointer1Source == 2)
        navType = g5State.vloc1Type;
    else if (g5Settings.bearingPointer1Source == 3)
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
    if (navType == 2 || g5Settings.bearingPointer1Source == 1 || g5Settings.bearingPointer1Source == 4 && g5State.adfValid) {
        bearingPointer1.setBitmapColor(TFT_CYAN, TFT_BLACK);
        compass.setPivot(compass.width() / 2, compass.height() / 2);
        bearingPointer1.pushRotated(bearingPointer1Angle - g5State.headingAngle + 180, TFT_BLACK);
    }

    // Draw the info box in the lower left
    if (g5Settings.bearingPointer1Source == 1) strcpy(buf, "GPS");
    if (g5Settings.bearingPointer1Source == 4) strcpy(buf, "ADF");
    bearingPointerBox.fillRect(4, 23, 80, 26, TFT_BLACK); // numbers from inkscape
    bearingPointerBox.drawString(buf, 12, bearingPointerBox.height() - 6);
    bearingPointerBox.pushSprite(0, lcd.height() - gsBox.height() - bearingPointerBox.height(), TFT_MAIN_TRANSPARENT);
}

void CC_G5_HSI::drawBearingPointer2()
{
    // Is display turned off?
    if (g5Settings.bearingPointer2Source == 0) return;

    // Don't draw during menu to avoid collision.
    if (hsiMenu.menuActive && hsiMenu.currentState == HSIMenu::MenuState::BROWSING) return;

    // Figure out correct labels.

    int navType;
    if (g5Settings.bearingPointer2Source == 2)
        navType = g5State.vloc1Type;
    else if (g5Settings.bearingPointer2Source == 3)
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

    if (g5Settings.bearingPointer2Source == 1) strcpy(buf, "GPS");
    if (g5Settings.bearingPointer2Source == 4) strcpy(buf, "ADF");

    // draw the actual Bearing Pointer, but only if it's a type that draws and is valid.
    if (navType == 2 || g5Settings.bearingPointer2Source == 1 || (g5Settings.bearingPointer2Source == 4 && g5State.adfValid)) {
        bearingPointer2.setBitmapColor(TFT_CYAN, TFT_BLACK);
        compass.setPivot(compass.width() / 2, compass.height() / 2);
        bearingPointer2.pushRotated(bearingPointer2Angle - g5State.headingAngle + 180, TFT_BLACK);
    }

    // Draw info box in lower right
    bearingPointerBox2.fillRect(46, 23, 80, 26, TFT_BLACK); // numbers from inkscape
    bearingPointerBox2.drawString(buf, bearingPointerBox2.width() - 12, bearingPointerBox2.height() - 6);
    bearingPointerBox2.pushSprite(lcd.width() - bearingPointerBox2.width(), lcd.height() - gsBox.height() - bearingPointerBox2.height(), TFT_MAIN_TRANSPARENT);
}

void CC_G5_HSI::drawGlideSlope()
{

    if (!g5State.gsiNeedleValid) return;

    // To Do: Set appropriate bug position
    //        Figure out if we skip this because wrong mode.
    //        Set right text and colors for ILS vs GPS.

    // Positive number, needle deflected down. Negative, up.

    // Change of direction. Use HSI GSI NEEDLE:1 simvar. -127 to 127.

    const float scaleMax    = 127.0;
    const float scaleMin    = -127.0;
    const float scaleOffset = 22.0; // Distance the scale starts from top of sprite.

    // Refill the sprite to overwrite old diamond.
    glideDeviationScale.fillSprite(TFT_BLACK);
    glideDeviationScale.pushImage(0, 0, GSDEVIATION_IMG_WIDTH, GSDEVIATION_IMG_HEIGHT, GSDEVIATION_IMG_DATA);

    int markerCenterPosition = (int)(scaleOffset + ((g5State.gsiNeedle + scaleMax) * (190.0 / (scaleMax - scaleMin))) - (deviationDiamond.height() / 2.0));
    // int markerCenterPosition = (int)(20.0 + ((g5State.gsiNeedle + 119.0) * (190.0/238.0)) - (19.0/2.0));
    // int markerCenterPosition = (int)(scaleOffset + (g5State.gsiNeedle + scaleMax) * ((GSDEVIATION_IMG_HEIGHT-scaleOffset)/(scaleMax - scaleMin)) - (deviationDiamond.height()/2));
    // deviationDiamond.pushSprite(1, markerCenterPosition, TFT_BLACK);

    if (g5State.navSource == NAVSOURCE_GPS) {
        glideDeviationScale.setTextColor(TFT_MAGENTA);
        glideDeviationScale.drawString("G", glideDeviationScale.width() / 2, 10);
        glideDeviationScale.drawBitmap(1, markerCenterPosition, DIAMONDBITMAP_IMG_DATA, DIAMONDBITMAP_IMG_WIDTH, DIAMONDBITMAP_IMG_HEIGHT, TFT_MAGENTA);

    } else {
        glideDeviationScale.setTextColor(TFT_GREEN);
        glideDeviationScale.drawString("L", glideDeviationScale.width() / 2, 10);
        glideDeviationScale.drawBitmap(1, markerCenterPosition, DIAMONDBITMAP_IMG_DATA, DIAMONDBITMAP_IMG_WIDTH, DIAMONDBITMAP_IMG_HEIGHT, TFT_GREEN);
    }

    glideDeviationScale.pushSprite(&lcd, lcd.width() - glideDeviationScale.width() - 2, lcd.height() / 2 - glideDeviationScale.height() / 2);
}

void CC_G5_HSI::drawPlaneIcon()
{

    plane.pushSprite(&compass, (compass.width() / 2) - (plane.width() / 2), (compass.height() / 2) - (plane.height() / 2), 0);
}

void CC_G5_HSI::drawDeviationScale()
{
    if (!g5State.cdiNeedleValid) return;
    compass.setPivot(compass.width() / 2, compass.height() / 2);
    deviationScale.pushRotated(&compass, (g5State.cdiDirection - g5State.headingAngle), TFT_BLACK);
}

void CC_G5_HSI::drawCurrentHeading()
{
    // Box with direction at top of compass
    static int lastHeading = -999;

    int hdgInteger = round(g5State.headingAngle);
    if (hdgInteger == lastHeading) return;
    lastHeading = hdgInteger;

    // const int hdgWidth           = curHdg.width() - 1;
    // const int hdgHeight          = curHdg.height() - 1;
    // const int triangleWidth      = 25;
    // const int triangleHeight     = 15;
    // const int triangleLeftOffset = (hdgWidth - triangleWidth) / 2;
    // const int rectangleHeight    = hdgHeight;

    // curHdg.fillSprite(TFT_BLACK);
    // curHdg.setColor(TFT_WHITE);
    // curHdg.drawLine(0, 0, hdgWidth, 0);                                                              // ------
    // curHdg.drawLine(0, 0, 0, rectangleHeight);                                                       //  |
    // curHdg.drawLine(0, rectangleHeight, triangleLeftOffset, rectangleHeight);                        // -
    // curHdg.drawLine(triangleLeftOffset + triangleWidth, rectangleHeight, hdgWidth, rectangleHeight); // -
    // curHdg.drawLine(hdgWidth, rectangleHeight, hdgWidth, 1);                                         // |

    char hdgStr[6];
    curHdg.fillRect(3, 3, curHdg.width() - 6, curHdg.height() - 6, TFT_BLACK);
    sprintf(hdgStr, "%03d\xB0", hdgInteger);

    curHdg.drawString(hdgStr, curHdg.width() / 2, curHdg.height() / 2);
    curHdg.pushSprite(&lcd, (480 / 2) - curHdg.width() / 2, CUR_HEADING_Y_OFFSET);
    // We used to push it later. not sure why.
}

void CC_G5_HSI::drawVORCourseBox()
{
    // Draw the lower left course heading box. We reuse the ground speed sprite.

    int boxWidth = gsBox.width(), boxHeight = gsBox.height();
    int borderWidth = 3;

    gsBox.setTextColor(TFT_WHITE, TFT_BLACK);
    gsBox.fillRect(0, 0, boxWidth, boxHeight, TFT_WHITE);
    gsBox.fillRect(borderWidth, borderWidth, boxWidth - 2 * borderWidth, boxHeight - 2 * borderWidth, TFT_BLACK);
    gsBox.setTextDatum(BL_DATUM);
    gsBox.setTextSize(0.7);
    gsBox.drawString(g5State.obsModeOn ? "OBS" : "CRS", borderWidth + 1, boxHeight - (borderWidth + 1));
    gsBox.setTextSize(1.0);
    gsBox.setTextColor(TFT_GREEN, TFT_BLACK);
    char buf[6];
    if (g5State.cdiNeedleValid)
        sprintf(buf, "%3.0f°", g5State.obsAngle);
    else
        sprintf(buf, "---°");
    gsBox.setTextDatum(BR_DATUM);
    gsBox.drawString(buf, boxWidth - (borderWidth + 1), boxHeight - (borderWidth + 1));
    gsBox.pushSprite(0, lcd.height() - gsBox.height());
}

void CC_G5_HSI::drawCDIPointer()
{
    if (!g5State.cdiNeedleValid) return;
    compass.setPivot(compass.width() / 2, compass.height() / 2);
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
    compass.setPivot(compass.width() / 2, compass.height() / 2);
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

    float pixelOffset = g5State.cdiOffset * (float)DEVIATIONSCALE_IMG_WIDTH / 254;
    compass.setPivot(compass.width() / 2 + (pixelOffset * (float)cos((barAngle - g5State.headingAngle) * PIf / 180.0f)),
                     (compass.height() / 2 + (pixelOffset * (float)sin((barAngle - g5State.headingAngle) * PIf / 180.0f))));
    cdiBar.setPivot(cdiBar.width() / 2, cdiBar.height() / 2); // Pivot around the middle of the sprite.
    cdiBar.pushRotated(&compass, barAngle - g5State.headingAngle);

    compass.setPivot(compass.width() / 2, compass.height() / 2);

    toFrom.setPivot(TOFROM_IMG_WIDTH / 2, 46); // It's actually really close to the pivot. Was 86
    // Fix: If we're on an ILS or localizer approach, we don't draw this.
    if (g5State.gpsApproachType == 99)
        toFrom.pushRotated(&compass, barAngle - g5State.headingAngle + (g5State.cdiToFrom == 2 ? 180 : 0), TFT_WHITE);
}

void CC_G5_HSI::drawHeadingBug()
{
    compass.setPivot(compass.width() / 2, compass.height() / 2); // Center of Compass
    headingBug.pushRotated(&compass, g5State.headingBugAngle - g5State.headingAngle, TFT_WHITE);
}

// This is the box in the lower right.
void CC_G5_HSI::drawHeadingBugValue()
{
    static int lastHeadingbug = -999;
    if (lastHeadingbug == g5State.headingBugAngle && !g5State.forceRedraw) return;
    lastHeadingbug = g5State.headingBugAngle;

    char buf[5];
    sprintf(buf, "%03d\xB0", g5State.headingBugAngle);
    headingBox.fillRect(20, 6, 110, 30, TFT_BLACK);
    headingBox.drawString(buf, headingBox.width() - 25, headingBox.height() - 3);
    headingBox.pushSprite(lcd.width() - headingBox.width(), lcd.height() - headingBox.height());
}

// void CC_G5_HSI::drawGroundSpeed()
// {
//     // Magenta in box in lower left. UNUISED
//     char buf[5];
//     if (g5State.groundSpeed > 1000) return;
//     sprintf(buf, "%d", g5State.groundSpeed);
//     gsBox.pushImage(0, 0, GSBOX_IMG_WIDTH, GSBOX_IMG_HEIGHT, GSBOX_IMG_DATA);
//     gsBox.setTextDatum(BR_DATUM);
//     gsBox.setTextColor(TFT_MAGENTA, TFT_BLACK);
//     gsBox.fillRect(42, 6, 78, 30, TFT_BLACK);
//     gsBox.drawString(buf, gsBox.width() - 25, gsBox.height() - 4);
//     gsBox.pushSprite(0, lcd.height() - gsBox.height());
// }

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

    char buf[8];
    if (g5State.desiredTrackValid == 1)
        sprintf(buf, "%03.f\xB0", g5State.desiredTrack);
    else
        sprintf(buf, "- - -\xB0");

    gsBox.fillSprite(TFT_BLACK);
    gsBox.drawRect(0, 0, gsBox.width(), gsBox.height(), TFT_WHITE);
    gsBox.drawRect(1, 1, gsBox.width() - 2, gsBox.height() - 2, TFT_WHITE);
    gsBox.setTextColor(TFT_WHITE, TFT_BLACK);
    gsBox.setTextSize(0.6);
    gsBox.setTextDatum(BL_DATUM);
    gsBox.drawString("DTK", 5, gsBox.height() - 5);
    gsBox.setTextSize(1.0f);
    gsBox.setTextColor(TFT_MAGENTA, TFT_BLACK);
    gsBox.setTextDatum(BR_DATUM);
    // gsBox.fillRect(42, 6, 78, 30, TFT_BLACK);
    gsBox.drawString(buf, gsBox.width() - 9, gsBox.height() - 2);
    gsBox.pushSprite(0, lcd.height() - gsBox.height());
}

void CC_G5_HSI::drawDistNextWaypoint()
{
    if (g5State.navSource != NAVSOURCE_GPS) return;
    // Magenta in box in upper left.
    // Let's put it in the upper right to make room for wind direction.
    static float lastDist  = -1.0f;
    static int   lastValid = -1;

    if (lastDist == g5State.distNextWaypoint && lastValid == g5State.cdiNeedleValid && !g5State.forceRedraw) return; // it's on LCD so no need to redraw.

    lastDist  = g5State.distNextWaypoint;
    lastValid = g5State.cdiNeedleValid;

    char buf[7];
    if (g5State.cdiNeedleValid)
        sprintf(buf, "%0.1f", g5State.distNextWaypoint);
    else
        sprintf(buf, "---.-");
    distBox.fillRect(54, 6, 90, 30, TFT_BLACK);
    distBox.drawString(buf, 140, distBox.height() - 3);
    // distBox.pushSprite(0,0);
    distBox.pushSprite(480 - distBox.width(), 0);
}

void CC_G5_HSI::drawWind()
{

    // grey box with arrow in upper left
    // If airspeed is less than 30 knots, wind data unavailable as it relies on gps.
    // Is it worth geting airspeed just to blank this out?

    static float lastDir   = -20.0f;
    static float lastSpeed = -20.0f;

    // Ignore small changes. The MSFS values are always changing.
    if (fabs(lastDir - g5State.windDir) < 4.0f && fabs(lastSpeed - g5State.windSpeed) < 1.0f && !g5State.forceRedraw) return;
    lastDir   = g5State.windDir;
    lastSpeed = g5State.windSpeed;

    windBox.fillRect(3, 3, windBox.width() - 6, windBox.height() - 6, TFT_BLACK);

    char buf[10];
    if (g5State.windSpeed < 0) {
        windBox.drawString("NO WIND", 85, 15);
        windBox.drawString("DATA", 90, 33);
    } else if (g5State.windSpeed < 2.0f) {
        windBox.drawString("WIND", 90, 15);
        windBox.drawString("CALM", 90, 33);
    } else {
        windArrow.pushRotated(((int)(g5State.windDir - g5State.headingAngle) + 180 + 360) % 360, TFT_BLACK);
        sprintf(buf, "%d KT", (int)(g5State.windSpeed + 0.5f));
        windBox.drawString(buf, 90, 33);
        sprintf(buf, "%02d0\xB0", (int)(g5State.windDir + 0.5f) / 10);
        windBox.drawString(buf, 90, 15);
    }
    windBox.pushSprite(0, 0);
}

void CC_G5_HSI::saveState()
{
    CC_G5_Base::saveState();  // saves common g5State fields (switching flag, IDs 0-10)

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