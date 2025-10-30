#include "CC_G5_PFD.h"
#include "esp_log.h"

#include <Wire.h>
#include <esp_cache.h>
#include <esp32s3/rom/cache.h>

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

#include "Sprites\gsBox.h"
#include "Sprites\distBox.h"
#include "Sprites\headingBox.h"
#include "Images\PrimaSans32.h"
#include "Sprites\fdTriangles.h"
#include "Sprites\fdTrianglesNoAP.h"
// #include "Images\PrimaSans16.h"

LGFX_Sprite attitude(&lcd);
LGFX_Sprite speedUnit(&attitude);
LGFX_Sprite speedTens(&attitude);
LGFX_Sprite altUnit(&attitude);
LGFX_Sprite altTens(&attitude);
LGFX_Sprite horizonMarker(&attitude);
LGFX_Sprite altScaleNumber(&attitude);
LGFX_Sprite altBug(&attitude);
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

    // Read data from hardware interface
    if (g5Hardware.readEncoderData(delta, enc_btn, ext_btn)) {
        // Serial.printf("Data back from RP2040. Enc delta: %d, enc_btn: %d, ext_btn: %d\n", delta, enc_btn, ext_btn);

        //     Serial.printf("enc btn: %d", enc_btn);

        if (enc_btn == ButtonEventType::BUTTON_CLICKED) {
            // Serial.println("encButton");
            if (pfdMenu.menuActive) {
                // Route input to menu when active
                pfdMenu.handleEncoderButton(true);
            } else {
                // Open menu when not active
                pfdMenu.setActive(true);
            }
        }

        if (enc_btn == ButtonEventType::BUTTON_LONG_PRESSED) {
            //  Serial.println("Long press on PFD. Send button to MF");
            pfdMenu.sendButton("btnPfdEncoder", 0);
        }

        if (ext_btn == ButtonEventType::BUTTON_LONG_PRESSED) {
            //  Serial.println("Long press on PFD. Send button to MF");
            pfdMenu.sendButton("btnPfdPower", 0);
        }

        if (delta) {
            if (pfdMenu.menuActive) {
                // Route encoder turns to menu when active
                pfdMenu.handleEncoder(delta);
            } else {
                // Normal heading adjustment when menu not active
                pfdMenu.sendEncoder("kohlsEnc", abs(delta), delta > 0 ? 0 : 2);
            }
        }
    }
}

void CC_G5_PFD::begin()
{
    loadSettings();

    g5Hardware.setLedState(true);

    lcd.setColorDepth(8);
    lcd.init();
    //    lcd.initDMA();

    //    Serial.printf("Chip revision %d\n", ESP.getChipRevision());

    //    Serial.printf("LCD Initialized.\n");

    // Setup menu structure

    pfdMenu.initializeMenu();

    // Configure i2c pins
    //    Serial.printf("Menu initialized.\n");

    pinMode(INT_PIN, INPUT_PULLUP);
    //    Serial.printf("Input pin setup.\n");
    //    Serial.flush();

    // Configure I2C master
    if (!Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 40000)) {
        //        Serial.printf("Wire begin fail.\n");
        ESP_LOGE(TAG_I2C, "i2c setup failed");
    } else {
        //        Serial.printf("Wire begin ok.\n");
        ESP_LOGI(TAG_I2C, "i2c setup successful");
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

    // lcd.setBrightness(255);  // This doesn't work :-( I'm not sure if we can turn off the backlight or control brightness.

    lcd.fillScreen(TFT_BLACK);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.loadFont(PrimaSans32);

    setupSprites();

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
        while (1)
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

    // GS Is Ground speed, NOT glide slope.
    gsBox.setColorDepth(8);
    gsBox.createSprite(SPEED_COL_WIDTH, GSBOX_IMG_HEIGHT);
    // gsBox.pushImage(0,0, SPEED_COL_WIDTH, GSBOX_IMG_HEIGHT, GSBOX_IMG_DATA);
    gsBox.setTextColor(TFT_MAGENTA);
    gsBox.setTextDatum(BR_DATUM);
    gsBox.loadFont(PrimaSans32);

    kohlsBox.setColorDepth(8);
    kohlsBox.createSprite(130, 40);
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
    altBug.setPivot(HEADINGBOX_IMG_WIDTH / 2, HEADINGBUG_IMG_HEIGHT / 2);

    targetAltBox.setColorDepth(8);
    targetAltBox.createSprite(130, 40);
    targetAltBox.setTextColor(TFT_CYAN);
    targetAltBox.loadFont(PrimaSans32);
    targetAltBox.setTextDatum(CR_DATUM);
    targetAltBox.fillSprite(TFT_BLACK);
    targetAltBox.pushImageRotateZoom(14, 4, 0, 0, 90, 0.6, 0.6, HEADINGBUG_IMG_WIDTH, HEADINGBUG_IMG_HEIGHT, HEADINGBUG_IMG_DATA, TFT_WHITE);
    targetAltBox.drawRect(0, 0, 130, 40, TFT_DARKGREY);
    targetAltBox.drawRect(1, 1, 128, 38, TFT_DARKGREY);
    targetAltBox.setTextSize(0.5);
    targetAltBox.drawString("f", 120, 12);
    targetAltBox.drawString("t", 120, 26);
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
            Serial.printf("No room!\n");
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
    apBox.createSprite(480, 30);
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

void CC_G5_PFD::set(int16_t messageID, char *setPoint)
{
    /* **********************************************************************************
        MessageID == -2 will be send from the board when PowerSavingMode is set
            Message will be "0" for leaving and "1" for entering PowerSavingMode
        MessageID == -1 will be send from the connector when Connector stops running
    ********************************************************************************** */
    lastMFUpdate = millis();

    int32_t data = 0;

    if (setPoint != NULL)
        data = atoi(setPoint);
    else
        return;

    uint16_t output;
    float    value;

    switch (messageID) {
    case -1:
        // tbd., get's called when Mobiflight shuts down
        break;
    case -2:
        // tbd., get's called when PowerSavingMode is entered
        g5Hardware.setLedState(false);
        break;
    case 0:
        g5Hardware.setLedState(true);
        value           = atof(setPoint);
        rawHeadingAngle = value;
        break;
    case 1:
        output          = (uint16_t)data;
        headingBugAngle = output;
        break;
    case 2:
        rawAirspeed = atof(setPoint);
        break;
    case 3:
        rawAltitude = data;
        break;
    case 4:
        value     = atof(setPoint);
        rawCdiOffset = value;
        break;
    case 5:
        rawPitchAngle = atof(setPoint);
        break;
    case 6:
        groundSpeed = data;
        break;
    case 7:
        groundTrack = atof(setPoint);
        break;
    case 8:
        rawBankAngle = atof(setPoint);
        break;
    case 9:
        break;
    case 10:
        targetAltitude = atoi(setPoint);
        break;
    case 11:
        rawBallPos = atof(setPoint);
        break;
    case 12:
        kohlsman = atof(setPoint);
        break;
    case 13:
        turnRate = atof(setPoint);
        break;
    case 14:
        rawVerticalSpeed = (int32_t)data; // VS can be negative!
        break;
    case 15:
        gsiNeedleValid = (uint16_t)data;
        break;
    case 16:
        cdiNeedleValid = (uint16_t)data;
        break;
    case 17:
        navCourseToSteer = atof(setPoint);
        break;
    case 18:
        gsiNeedle = (int)data;
        break;
    case 19:
        navSource = (int)data;
        break;
    case 20:
        flightDirectorActive = (int)data;
        break;
    case 21:
        flightDirectorPitch = atof(setPoint);
        break;
    case 22:
        flightDirectorBank = atof(setPoint);
        break;
    case 23:
        cdiToFrom = (int)data;
        break;
    case 24:
        gpsApproachType = (int)data;
        break;
    case 25:
        navCourse = (int)data;
        break;
    case 26:
        apActive = (int)data;
        break;
    case 27:
        apLMode = (int)data;
        break;
    case 28:
        apLArmedMode = (int)data;
        break;
    case 29:
        apVMode = (int)data;
        break;
    case 30:
        apVArmedMode = (int)data;
        break;
    case 31:
        apTargetVS = (int)data;
        break;
    case 32:
        apTargetSpeed = (int)data;
        break;
    case 33:
        apAltCaptured = (int)data;
        break;
    case 34:
        apYawDamper = (int)data;
        break;
    default:
        break;
    }
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
    float bankRad = bankAngle * PI / 180.0;

    // Pitch scaling factor (pixels per degree)
    const float PITCH_SCALE = 8.0;

    // --- 1. Draw Sky and Ground (Reverted to the reliable drawFastVLine method) ---

    // Calculate vertical offset of the horizon due to pitch.
    // A negative pitchAngle (nose up) moves the horizon down (positive pixel offset).
    float horizonPixelOffset = -pitchAngle * PITCH_SCALE;

    // Determine if aircraft is inverted
    float normalizedBank = fmod(bankAngle, 360.0);
    if (normalizedBank < 0) normalizedBank += 360.0;
    bool inverted = (normalizedBank > 90.0 && normalizedBank < 270.0);

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
        // (pitchDegrees - pitchAngle) gives the correct relative position.
        float verticalOffset = (pitchDegrees - pitchAngle) * PITCH_SCALE;

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
            altScaleNumber.pushRotated(bankAngle, TFT_BLACK);
            attitude.setPivot(textX2, textY2);
            altScaleNumber.pushRotated(bankAngle, TFT_BLACK);

            //      attitude.drawString(pitchText, textX1, textY1);
            //      attitude.drawString(pitchText, textX2, textY2);
        }
    };

    // Define and draw all the pitch lines
    const struct {
        float deg;
        int   width;
        bool  num;
    } pitch_lines[] = {
        {30.0, 80, true}, {25.0, 60, false}, {20.0, 80, true}, {17.5, 40, false}, {15.0, 60, false}, {12.5, 40, false}, {10.0, 80, true}, {7.5, 40, false}, {5.0, 60, false}, {2.5, 40, false}, {-2.5, 40, false}, {-5.0, 60, false}, {-7.5, 40, false}, {-10.0, 80, true}, {-12.5, 40, false}, {-15.0, 60, false}, {-17.5, 40, false}, {-20.0, 80, true}, {-25.0, 60, false}, {-30.0, 80, true}};

    uint16_t color = TFT_RED;
    for (const auto &line : pitch_lines) {
        // Simple culling: only draw lines that are somewhat close to the screen
        float verticalPos = abs(line.deg - pitchAngle) * PITCH_SCALE;
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

    // Draw the bank scale.
    attitude.setPivot(240, 200);
    baScale.pushRotated(bankAngle, TFT_BLACK);
    // Draw the static scale pointer at the top center. Use the center of the screen, but offset by the difference in column widths.
    attitude.drawBitmap(231, 79, BANKANGLEPOINTER_IMG_DATA, BANKANGLEPOINTER_IMG_WIDTH, BANKANGLEPOINTER_IMG_HEIGHT, TFT_WHITE); // 231 is 240 - half the sprite width.
    // baScale.pushSprite(240, 100, TFT_BLACK);

    // --- 3. Draw Horizon Line ---
    // The horizon is just a pitch line at 0 degrees.
    // We draw it extra long to ensure it always crosses the screen.
    float horiz_unrot_y = (0 - pitchAngle) * PITCH_SCALE;
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
    float drawSpeed = airspeed;

    if (airspeed < 10) drawSpeed = 0.0; // The airspeed isn't displayed at low speed.

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
    if (drawSpeed > 20) {

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
    if (drawSpeed > 20) {
        if (intDigits[2] == 9) {
            yBaseline = yBaseline + (intDigits[3] * digitHeight) / 10; // Animate based on tenths.
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
}

void CC_G5_PFD::drawSpeedPointers()
{
    // Construct the list of pointers. This will eventually go in EERAM
    const struct {
        char label;
        int  speed;
        int  order;
    } speed_pointers[] = {
        {'R', g5Settings.Vr, 0}, {'X', g5Settings.Vx, 1}, {'Y', g5Settings.Vy, 2}, {'G', g5Settings.Vg, 3}};

    for (const auto &pointer : speed_pointers) {

        if (pointer.speed > airspeed + 30 && airspeed > (speed_pointers[0].speed) - 30) continue; // Short circuit if off screen.

        speedPointer.drawBitmap(0, 0, SPEEDPOINTER_IMG_DATA, SPEEDPOINTER_IMG_WIDTH, SPEEDPOINTER_IMG_HEIGHT, TFT_WHITE, TFT_BLACK);
        speedPointer.drawChar(pointer.label, 10, 2);
        // If the airspeed is below the first speed, then show them at the bottom.
        int yPos = 0;
        if (airspeed < (speed_pointers[0].speed - 30)) {
            yPos = (ATTITUDE_HEIGHT - 30) - (pointer.order * 30);
            attitude.setTextColor(TFT_CYAN);
            attitude.setTextSize(0.6);
            attitude.setTextDatum(CR_DATUM);

            attitude.drawNumber(pointer.speed, SPEED_COL_WIDTH - 4, yPos);
        } else {
            yPos = speedToY(pointer.speed, airspeed) + 9;
        }
        speedPointer.pushSprite(SPEED_COL_WIDTH, yPos - 16, TFT_WHITE);
    }
}

int CC_G5_PFD::speedToY(float targetSpeed, float curSpeed)
{
    // Our entire sprite is: 57kts tall and it's 400px tall. That's 400px/57kts = 7.02px per kt. 200 = scaled value.
    int retVal = (int)(200.0 + (curSpeed - targetSpeed) * 7.02f);
    // if (retVal > 400) retVal = 400;
    // if (retVal < 0) retVal =0;
    return retVal;
}

int CC_G5_PFD::altToY(int targetAlt, int curAlt)
{
    // Our entire sprite is: 330' tall and it's 400px tall. That's 400px/330' = 1.2'/px 200 = center point.
    return (int)(248.0 + (curAlt - targetAlt) * 1.21f);
    // if (retVal > 400) retVal = 400;
    // if (retVal < 0) retVal =0;
}

inline int floorMod(int a, int b)
{
    return ((a % b) + b) % b;
}

void CC_G5_PFD::drawAltTape()
{
    // Need to push the sprites even if we don't recalc. Need to push the numbers too. this saves 3ms, but is a mess.
    // static int lastAlt = -999;
    // if (lastAlt == altitude) {
    //     vsScale.pushSprite(attitude.width() - vsScale.width(), 0, TFT_BLACK); // Scale drawn over the vs bar
    //     // Draw the pointer

    //     vsPointer.pushSprite(ATTITUDE_WIDTH - VSPOINTER_IMG_WIDTH, 200 - VSPOINTER_IMG_HEIGHT / 2 - (int)(verticalSpeed * 0.131f), LGFX::color565(0x20, 0x20, 0x20));

    //     // Draw the boxes last.

    //     altUnit.pushSprite(attitude.width() - altUnit.width() - 15, (attitude.height() - altUnit.height()) / 2);
    //     altTens.pushSprite(attitude.width() - altUnit.width() - altTens.width() - 14, (attitude.height() - altTens.height()) / 2);
    //     return;
    // }
    // lastAlt = altitude;

    int intDigits[7];

    // Serial.printf("a: %f s: %d 0.01s: %d\n",airspeed, scaled, scaled/100);
    int scaled = altitude;

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
    int dispUnit = (altitude / 20) * 20; // round to 20

    //    yBaseline = yBaseline - ((intDigits[2] % 2) * digitHeight / 2);
    // each digit height is 20'... so each foot it 36/20 = 1.8f. Offset by 1/2 the height: 54
    int  yBaseline = (int)(54 + ((altitude % 20) * 1.8f));
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

    altTens.setTextSize(altitude < 1000 ? 1.0 : 0.8);
    xOffset = altTens.width() - 1;
    yOffset = altTens.height() / 2; // Base offset without rolling!
    altTens.setTextDatum(CR_DATUM);

    // Roll the hundreds.
    if (altitude >= 80 || altitude < -90) { // Don't draw a leading 0
                                            //  if ((altitude>0 && intDigits[2] >= 8) || (altitude<0 && intDigits[2] <= 1)) {
        if (abs(intDigits[2]) >= 8) {
            yOffset = yOffset - (20 - (altitude % 20)) * (1.8f); // 1.8f is height/2
            altTens.drawNumber(intDigits[3] + 1, xOffset, yOffset);
            if (altitude > 100) altTens.drawNumber(intDigits[3], xOffset, yOffset + digitHeight);
            altTens.drawNumber(intDigits[3] - 1, xOffset, yOffset + digitHeight * 2);
        } else
            altTens.drawNumber(intDigits[3], xOffset, yOffset);
    }

    xOffset = altTens.width() - 1 - digitWidth;

    // roll the thousands
    if (altitude / 1000 > 0) {
        altTens.setTextSize(1.0);
        if (intDigits[3] == 9 && intDigits[2] >= 8) {
            //          yOffset = yOffset - (20 - (altitude % 20))*(1.8f);  // 1.8f is height/2
            altTens.drawNumber(intDigits[4] + 1, xOffset, yOffset);
            altTens.drawNumber(intDigits[4], xOffset, yOffset + digitHeight);
        } else
            altTens.drawNumber(intDigits[4], xOffset, altTens.height() / 2);
    }

    // roll the ten thousands
    xOffset = altTens.width() - 1 - digitWidth * 2;
    if (altitude / 10000 > 0) {
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
        int curVal = ((altitude / 100) + 2 - i) * 100; // Value to be displayed (100's)

        int tapeSpacing = digitHeight * (i) + ((intDigits[2] * 10 + intDigits[1]) * (digitHeight)) / 100;

        // TODO If alt is above 1000, then the hundreds and tens should be smaller.

        // If target alt is on screen, we'll draw it below.
        if (curVal != targetAltitude) attitude.drawNumber(curVal, xRight, yTop + tapeSpacing);
        // attitude.drawLine(xRight - 30,yTop + tapeSpacing, xRight-15, yTop + tapeSpacing); // Major Tick
        attitude.drawFastHLine(xRight - 30, yTop + tapeSpacing, 15, TFT_WHITE); // Major Tick
        for (int j = 1; j < 5; j++) {
            attitude.drawFastHLine(xRight - 30, yTop + tapeSpacing + j * 24, 10, TFT_WHITE); // Minor Tick
            // attitude.drawLine(xRight - 30,yTop + tapeSpacing + j*24, xRight-20, yTop + tapeSpacing+ j*24); // Minor Tick
        }
    }

    // Draw the bug over the tape.
    // If the target altitude is off the scale, draw it at the boundary.
    int bugPos = altToY(targetAltitude, altitude) - 5;
    // but that's in terms of the attitude sprite,
    // Serial.printf("target: %d, alt: %d, bugpos: %d\n", targetAltitude, altitude, bugPos);
    int offset = 61;
    if (bugPos < HEADINGBUG_IMG_HEIGHT / 2) bugPos = HEADINGBUG_IMG_WIDTH / 2 + offset; // Yes, width. image is sideways.
    if (bugPos > 400 - HEADINGBUG_IMG_WIDTH / 2) bugPos = ATTITUDE_HEIGHT - HEADINGBUG_IMG_HEIGHT / 2 + offset;
    // Serial.printf("target: %d, alt: %d, bugpos: %d\n", targetAltitude, altitude, bugPos);

    attitude.setPivot(xRight - 23, bugPos);
    if (targetAltitude != 0) altBug.pushRotated(90, TFT_WHITE);
    attitude.setTextColor(TFT_CYAN);
    attitude.drawNumber(targetAltitude, xRight, altToY(targetAltitude, altitude) - 49);

    // Draw the vertical speed scale
    yTop = 200;
    // 131 pixels is 1000fpm so 1fpm is 0.131 pixel
    int barHeight = abs((int)(verticalSpeed * 0.131f));
    if (verticalSpeed > 0) yTop = 200 - barHeight;
    attitude.fillRect(475, yTop, 5, barHeight, TFT_MAGENTA);              // push to attitude to avoid a refill of vsScale.
    vsScale.pushSprite(attitude.width() - vsScale.width(), 0, TFT_BLACK); // Scale drawn over the vs bar
    // Draw the pointer

    vsPointer.pushSprite(ATTITUDE_WIDTH - VSPOINTER_IMG_WIDTH, 200 - VSPOINTER_IMG_HEIGHT / 2 - (int)(verticalSpeed * 0.131f), LGFX::color565(0x20, 0x20, 0x20));

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

    static float lastVal = 0.0;
    if (kohlsman == lastVal) return;
    lastVal = kohlsman;

    kohlsBox.fillSprite(TFT_BLACK);
    kohlsBox.drawRect(0, 0, ALTITUDE_COL_WIDTH, 40, TFT_CYAN);
    kohlsBox.drawRect(1, 1, ALTITUDE_COL_WIDTH - 2, 38, TFT_CYAN);
    kohlsBox.setTextDatum(CR_DATUM);
    kohlsBox.setTextSize(0.5);
    kohlsBox.setTextColor(TFT_CYAN);
    kohlsBox.drawString("i", 119, 12);
    kohlsBox.drawString("n", 122, 26);

    char buf[8];
    sprintf(buf, "%.2f", kohlsman);
    kohlsBox.setTextSize(0.9);
    kohlsBox.drawString(buf, 100, 21);
    kohlsBox.pushSprite(ATTITUDE_WIDTH - ALTITUDE_COL_WIDTH, 440);

    // Serial.printf("Pushing kohlsBox to x:%d, y:%d\n",lcd.width()-kohlsBox.width(), lcd.height() - kohlsBox.height());
}

// This is the box in the upper right.
void CC_G5_PFD::drawAltTarget()
{

    // The altitude flashes for 5 seconds if cur altitude is within 1000' of target. Then again when within 200.
    // It only does this once until the target alt is changed.
    // Once target is reached, if we fly 200' away the altitude changes to yellow and flashes for 5 seconds.
    // It looks like 0.8 sec on, 0.2 sec off.
    static int lastTargetAlt = -9999;

    // The alert processing on this makes it too tricky to use cache for now.

    // If the targetAlt changes, reset the alerts.
    if (lastTargetAlt != targetAltitude) {
        alert1000Triggered = false;
        alert200Triggered  = false;
        altTargetReached   = false;
        alertColor         = TFT_WHITE;
        altAlertActive     = false;
        lastTargetAlt      = targetAltitude;
    }

    int altDiff = abs(altitude - targetAltitude);

    if (!alert1000Triggered && altDiff <= 1000 && altDiff > 200) {
        // Trigger 1000' alert
        alert1000Triggered = true;
        altAlertActive     = true;
        alertStartTime     = millis();
        alertColor         = TFT_WHITE;
    } else if (!alert200Triggered && altDiff <= 200 && altDiff > 50) {
        // Trigger 200' alert (when approaching)
        alert200Triggered = true;
        altAlertActive    = true;
        alertStartTime    = millis();
        alertColor        = TFT_WHITE;
    } else if (altDiff <= 100) {
        // Target reached (within 100')
        altTargetReached = true;
        altAlertActive   = false; // Stop any active alert
    } else if (altTargetReached && altDiff > 200) {
        // Deviated from target - trigger alert once
        altAlertActive   = true;
        alertStartTime   = millis();
        alertColor       = TFT_YELLOW;
        altTargetReached = false; // Clear reached flag
    }

    // Stop alert after 5 seconds
    if (altAlertActive && (millis() - alertStartTime) > 5000) {
        altAlertActive = false;
    }

    // Determine display color
    uint16_t textColor = TFT_CYAN; // Default

    // Yellow if we've deviated and haven't returned/changed target
    if (!altTargetReached && alert200Triggered && altDiff > 200) {
        textColor = TFT_YELLOW;
    }

    // Flash alert if active
    if (altAlertActive) {
        bool isVisible = ((millis() - alertStartTime) % 1000) < 800;
        if (!isVisible) {
            textColor = TFT_BLACK; // Blink off
        } else {
            textColor = alertColor; // Use alert color (WHITE or YELLOW)
        }
    }

    // Return to cyan if back within 200' after deviation
    if (alert200Triggered && !altTargetReached && altDiff <= 200) {
        textColor         = TFT_CYAN;
        alert200Triggered = false; // Reset 200' alert for next approach
    }

    char buf[10];
    if (targetAltitude != 0) {
        sprintf(buf, "%d", targetAltitude);
    } else {
        strcpy(buf, "- - - -");
    }

    // Knock out the last alt, but not the static elements.
    targetAltBox.fillRect(22, 3, 88, 34, TFT_BLACK);
    targetAltBox.setTextColor(textColor);
    targetAltBox.drawString(buf, 110, 21);

    targetAltBox.pushSprite(&attitude, ATTITUDE_WIDTH - ALTITUDE_COL_WIDTH, 0);

    return;

    // headingBox.fillRect(20, 6, 114, 30, TFT_BLACK);
    // headingBox.drawString(buf, headingBox.width() - 25, headingBox.height() - 3);
    // headingBox.pushSprite(lcd.width()-headingBox.width(), 0);
}

void CC_G5_PFD::drawGroundSpeed()
{
    // Magenta in box in lower left.

    static int lastGs = 399;

    if (groundSpeed > 400) return;

    if (lastGs == groundSpeed) return;

    lastGs = groundSpeed;

    char buf[5];
    sprintf(buf, "%d", groundSpeed);

    gsBox.fillSprite(TFT_BLACK);
    gsBox.drawRect(0, 0, SPEED_COL_WIDTH, 40, TFT_DARKGREY);
    gsBox.drawRect(1, 1, SPEED_COL_WIDTH - 2, 38, TFT_DARKGREY);
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

    return;
}

void CC_G5_PFD::drawBall()
{
    // Draw the ball and the turn rate. The ballPos goes from -1.0 to 1.0
    int turnBarCenter = CENTER_COL_CENTER;
    turnBar.fillSprite(GND_COLOR);

    int ballXOffset = (int)(ballPos * BALL_IMG_WIDTH * 1.3f);
    ballSprite.pushSprite(turnBarCenter - ballSprite.width() / 2 + ballXOffset, 0, 0xC2);

    // Draw the ball cage
    turnBar.drawRect(turnBarCenter - 20 - 3, 0, 6, 32, TFT_BLACK);
    turnBar.fillRect(turnBarCenter - 20 - 2, 0, 4, 30, TFT_WHITE);
    turnBar.fillRect(turnBarCenter + 20 - 2, 0, 4, 30, TFT_WHITE);
    turnBar.drawRect(turnBarCenter + 20 - 3, 0, 6, 32, TFT_BLACK);

    // if(! millis() % 100) Serial.printf("ballPos: %f ballXOffset: %d\n", ballPos, ballXOffset);

    // Draw the turn rate bar and markers.
    // Turn rate is in degrees per sec. +3 (right) or -3 (left) is std turn. (full 360 in 120 seconds)
    turnBar.fillRect(turnBarCenter - 1 - 70, 34, 2, 6, TFT_WHITE);
    turnBar.fillRect(turnBarCenter - 1 + 70, 34, 2, 6, TFT_WHITE);
    turnBar.fillRect(turnBarCenter - 1, 34, 3, 6, TFT_DARKGREY);

    // 3 degrees is 69 pixels: 23 pix per degree
    int turnRateWidth = (int)(turnRate * 23);
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
    // if (lastHeading == (int) headingAngle) return;
    // lastHeading = headingAngle;
    int tapeCenter  = CENTER_COL_CENTER;
    int scaleOffset = (int)(fmod(headingAngle, 5.0f) * PX_PER_DEGREE - 8);
    // int xOffset     = (int)headingAngle % 10 * 7 + 17;
    int xOffset = (int)(fmod(headingAngle, 10.0f) * PX_PER_DEGREE) + 17;
    headingTape.fillSprite(DARK_SKY_COLOR);

    // Draw the tape scale
    hScale.pushSprite(0 - scaleOffset, 40 - HSCALE_IMG_HEIGHT, TFT_BLACK);
    int baseHeading = ((int)headingAngle / 10) * 10;

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

    // Serial.printf("h: %f, bH: %d, sO: %d, xO: %d, v1: %d x1: %d v2: %d x2: %d\n",headingAngle, baseHeading, scaleOffset, xOffset, (baseHeading + 350) % 360, 115 - xOffset - 20, (baseHeading + 10) % 360,115 - xOffset + 82 );

    // Draw Nav Course to Steer... but only  if in gps mode. Otherwise draw the CRS.
    // FIX: This should be written to the LCD, not the headingTape.
    if (navSource == NAVSOURCE_GPS)
        headingTape.fillRect(headingToX(navCourseToSteer, headingAngle) - 2, 30, 4, 10, TFT_GREEN);
    else
        headingTape.fillRect(headingToX(navCourse, headingAngle) - 2, 30, 4, 10, TFT_GREEN);

    // Serial.printf("ncs %d, x: %d\n",navCourseToSteer, headingToX(navCourseToSteer, headingAngle));

    // Draw the Ground Course triangle
    headingTape.drawBitmap(headingToX(groundTrack, headingAngle) - 9, 32, POINTER_IMG_DATA, POINTER_IMG_WIDTH, POINTER_IMG_HEIGHT, TFT_MAGENTA);

    // Draw the Heading Bug
    int headingBugOffset = headingToX((float)headingBugAngle, headingAngle) - HEADINGBUG_IMG_WIDTH / 2;
    if (headingBugOffset < 0 - HEADINGBUG_IMG_WIDTH / 2) headingBugOffset = 0 - HEADINGBUG_IMG_WIDTH / 2;
    if (headingBugOffset > headingTape.width() - HEADINGBUG_IMG_WIDTH / 2) headingBugOffset = headingTape.width() - HEADINGBUG_IMG_WIDTH / 2;
    //  Serial.printf("Offset: %d\n", headingBugOffset);
    //    altBug.pushSprite(&headingTape, headingBugOffset, headingTape.height() - HEADINGBUG_IMG_HEIGHT, TFT_WHITE);
    altBug.pushRotateZoom(&headingTape, headingBugOffset + 51, headingTape.height() - HEADINGBUG_IMG_HEIGHT + 10, 0, 0.7, 0.7, TFT_WHITE);

    // Draw the heading box and current heading
    headingTape.drawRect(tapeCenter - 24, 0, 48, 27, TFT_LIGHTGRAY);
    headingTape.fillRect(tapeCenter - 23, 1, 46, 25, TFT_BLACK);
    sprintf(buf, "%03d", (int)roundf(headingAngle));
    headingTape.setTextSize(0.7);
    headingTape.drawString(buf, tapeCenter, 15);
    headingTape.pushSprite(&attitude, SPEED_COL_WIDTH, 0);
}

void CC_G5_PFD::drawGlideSlope()
{

    if (!gsiNeedleValid) return;

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

    int markerCenterPosition = (int)(scaleOffset + ((gsiNeedle + scaleMax) * (190.0 / (scaleMax - scaleMin))) - (deviationDiamond.height() / 2.0));

    if (navSource == NAVSOURCE_GPS) {
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

    if (!cdiNeedleValid) return;

    const float scaleMax    = 127.0;
    const float scaleMin    = -127.0;
    const float scaleOffset = 0.0; // Distance the scale starts from middle of sprite.

    // Refill the sprite to overwrite old diamond.

    // GPS Mode: Magenta triangle always pointing to.
    // ILS Mode: Green diamond.
    // VOR Mode: Green triangle To/From

    horizontalDeviationScale.fillSprite(TFT_BLACK);
    horizontalDeviationScale.pushImage(0, 0, HORIZONTALDEVIATIONSCALE_IMG_WIDTH, HORIZONTALDEVIATIONSCALE_IMG_HEIGHT, HORIZONTALDEVIATIONSCALE_IMG_DATA);
    int markerCenterPosition = (int)(scaleOffset + ((cdiOffset + scaleMax) * (190.0 / (scaleMax - scaleMin))) - (BANKANGLEPOINTER_IMG_WIDTH / 2));

    if (navSource == NAVSOURCE_GPS) {
        // always the magenta triangle
        horizontalDeviationScale.drawBitmap(markerCenterPosition, 2, BANKANGLEPOINTER_IMG_DATA, BANKANGLEPOINTER_IMG_WIDTH, BANKANGLEPOINTER_IMG_HEIGHT, TFT_MAGENTA);
        horizontalDeviationScale.pushSprite(&attitude, CENTER_COL_CENTER, 360);

    } else {
        // ILS or VOR or LOC is a green diamond
        if (gpsApproachType == 4 || gpsApproachType == 2 || gpsApproachType == 5) {

            // diamond
            horizontalDeviationScale.drawBitmap(markerCenterPosition, 2, DIAMONDBITMAP_IMG_DATA, DIAMONDBITMAP_IMG_WIDTH, DIAMONDBITMAP_IMG_HEIGHT, TFT_GREEN);
            horizontalDeviationScale.pushSprite(&attitude, CENTER_COL_CENTER, 360);
        } else {
            // Triangle with to/from
            if (cdiToFrom = 1) {
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
    apBox.fillSprite(TFT_BLACK);
    // Draw Lateral Mode (ROL, HDG, TRK, GPS/VOR/LOC (nav mode), GPS/LOC/BC (Nav movde), TO/GA (Toga))

    apBox.drawFastVLine(152, 0, apBox.height(), TFT_LIGHTGRAY);
    apBox.drawFastVLine(240, 0, apBox.height(), TFT_LIGHTGRAY);

    if (!apActive && !flightDirectorActive) {
        // Nothing on the screen.
        apBox.pushSprite(5, 0);
        return;
    }

    int yBaseline = 32;

    // Draw the Green things.
    char buf[10] = "";
    switch (apLMode) {
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
        sprintf(buf, "%d", apLMode);
        break;
    }
    apBox.setTextDatum(BC_DATUM);
    apBox.setTextColor(TFT_GREEN);
    apBox.drawString(buf, 108, yBaseline);

    switch (apVMode) {
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


    // If alt mode, print the captured altitude (nearst 10')
    strcpy(buf, "");
    char unitsBuf[5] = "";

    if(apVMode == 1) {
        sprintf(buf, "%d", apAltCaptured); 
        strcpy(unitsBuf, "ft");
    }
    // If vs mode, print the VS 
    if (apVMode == 2) {
        sprintf(buf, "%d", apTargetVS);
        strcpy(unitsBuf, "fpm");
    }
    
    // If  IAS mode, print target speed
    if (apVMode == 4) {
        sprintf(buf, "%d", apTargetSpeed);
        strcpy(unitsBuf, "kts");
    }
    
    apBox.setTextDatum(BR_DATUM);
    apBox.drawString(buf, 355, yBaseline);
    apBox.setTextDatum(BL_DATUM);
    apBox.setTextSize(0.4);
    apBox.drawString(unitsBuf, 357, yBaseline-4);
    
    apBox.setTextSize(0.8);

    // Draw Armed modes in white.
    switch (apLArmedMode) {
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
        sprintf(buf, "%d", apLArmedMode);
        break;
    }
    apBox.setTextColor(TFT_WHITE);
    apBox.setTextDatum(BL_DATUM);
    apBox.drawString(buf, 7, yBaseline);

    switch (apVArmedMode) {
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
        sprintf(buf, "%d", apVArmedMode);
        break;
    }
    apBox.setTextDatum(BR_DATUM);
    apBox.drawString(buf, 465, yBaseline);

    apBox.setTextDatum(BC_DATUM);
    apBox.setTextColor(TFT_GREEN);

    if (apActive) {
        // TODO add blink on change.
        apBox.drawString("AP", 178, yBaseline);
    }
    if (apYawDamper) {
        apBox.drawString("YD", 220, yBaseline);
    }

    apBox.pushSprite(5, 0);


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
    if (!flightDirectorActive) return;

    // Use the hollow if AP is off.

    // Set the pitch with the attitude pivot point.
    // Need to set max values for flight director.
    attitude.setPivot(ATTITUDE_WIDTH / 2, 200 + (flightDirectorPitch - pitchAngle) * 8);
    fdTriangle.pushRotated(bankAngle - flightDirectorBank, TFT_WHITE);

    return;
}

void CC_G5_PFD::updateInputValues()
{
    // This gives the cool, smooth value transitions rather than fake looking ones.

    headingAngle = smoothDirection(rawHeadingAngle, headingAngle, 0.15f, 0.02f);
    altitude     = smoothInput(rawAltitude, altitude, 0.1f, 1);
    airspeed     = smoothInput(rawAirspeed, airspeed, 0.02f, 0.01f);
    speedTrend.update(rawAirspeed);

    ballPos       = smoothInput(rawBallPos, ballPos, 0.04f, 0.01f);
    cdiOffset     = smoothInput(rawCdiOffset, cdiOffset, 0.3f, 1.0f);
    bankAngle     = smoothInput(rawBankAngle, bankAngle, 0.3f, 0.05f);
    pitchAngle    = smoothInput(rawPitchAngle, pitchAngle, 0.3f, 0.05f);
    verticalSpeed = smoothInput(rawVerticalSpeed, verticalSpeed, 0.03, 1);
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

    processMenu();
    attitude.pushSprite(0, 40, TFT_MAIN_TRANSPARENT);
    drawGroundSpeed();
    drawKohlsman();
    //    drawMessageIndicator();
    drawBall();
    drawAp();

    unsigned long pushEnd = millis();
    lcd.setTextSize(0.5);
    sprintf(buf, "%4.1f %lu/%lu", 1000.0 / (pushEnd - lastFrameUpdate), drawTime, pushEnd - pushStart);
    // lcd.fillRect(0, 0, SPEED_COL_WIDTH, 40, TFT_BLACK);

//    lcd.drawString(buf, 0, 55);

    //    lcd.drawNumber(data_available, 400, 10);
    // sprintf(buf, "b:%3.0f p:%3.0f ias:%5.1f alt:%d", bankAngle, pitchAngle, airspeed, altitude);
    // lcd.drawString(buf, 0, 10);
    lastFrameUpdate = millis();

    return;
}
