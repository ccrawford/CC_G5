#include "CC_ISIS.h"
#include "Images/isisFont.h"
#include "Sprites/isisBg.h"
#include "Sprites/attBackground.h"
#include "Sprites/blackoutArc.h"
#include "Sprites/altBg.h"

#include <cmath>

#define PITCH_LINE_NARROW 17 // 2.5° and 7.5° tick marks
#define PITCH_LINE_MEDIUM 27 // 5° tick marks
#define PITCH_LINE_WIDE   54 // 10° tick marks (labeled)

#define ATT_LEFT_EDGE 76
#define ATT_TOP_EDGE  56
#define ATT_HORIZON   194
#define ALT_LEFT_EDGE 398
#define ATT_WIDTH     320
#define ATT_HEIGHT    350

LGFX_Sprite attSprite(&lcd);
// LGFX_Sprite bgSprite(&lcd);       // Used for the static elements of the attitude display. Is this necessary? Probably.
LGFX_Sprite ladderValSprite(&attSprite); // Used to hold the scale numbers on the pitch ladder
LGFX_Sprite blackoutArcSprite(&attSprite);
LGFX_Sprite speedSprite(&lcd); // Holds the speed tape

LGFX_Sprite altSprite(&lcd);         // Holds the alt tape
LGFX_Sprite altBgSprite(&altSprite); // Holds the alt tape
LGFX_Sprite alt100Sprite(&altSprite);

LGFX_Sprite flSprite(&attSprite);

LGFX_Sprite kohlsSprite(&lcd);

CC_ISIS::CC_ISIS()
{
}

void CC_ISIS::setupSprites()
{
    // bgSprite.setColorDepth(8);
    // bgSprite.createSprite(ISISBG_IMG_WIDTH, ISISBG_IMG_HEIGHT);
    // bgSprite.pushImage(0, 0, ISISBG_IMG_WIDTH, ISISBG_IMG_HEIGHT, ISISBG_IMG_DATA);

    attSprite.setColorDepth(8);
    attSprite.createSprite(320, 350);

    ladderValSprite.setColorDepth(8);
    ladderValSprite.createSprite(45, 28); // Hold two digits. Digits are 24 high
    ladderValSprite.setPivot(25, 14);
    ladderValSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    ladderValSprite.setTextDatum(CL_DATUM);
    ladderValSprite.loadFont(A320ISIS24);

    blackoutArcSprite.setColorDepth(8);
    blackoutArcSprite.createSprite(BLACKOUTARC_IMG_WIDTH + 2, BLACKOUTARC_IMG_HEIGHT); // make it a little wider to fix any rotation integer math
    blackoutArcSprite.fillSprite(TFT_BLACK);
    blackoutArcSprite.pushImage(1, 0, BLACKOUTARC_IMG_WIDTH, BLACKOUTARC_IMG_HEIGHT, BLACKOUTARC_IMG_DATA);
    blackoutArcSprite.setPivot(BLACKOUTARC_IMG_WIDTH / 2, BLACKOUTARC_IMG_HEIGHT / 2);

    speedSprite.setColorDepth(8);
    speedSprite.createSprite(ATT_LEFT_EDGE - 1, attSprite.height());
    speedSprite.loadFont(A320ISIS24);
    speedSprite.setColor(TFT_LIGHTGRAY);
    speedSprite.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
    speedSprite.setTextDatum(CR_DATUM);

    altSprite.setColorDepth(8);
    altSprite.createSprite(ALT_LEFT_EDGE, attSprite.height());
    altSprite.loadFont(A320ISIS24);
    altSprite.setColor(TFT_LIGHTGRAY);
    altSprite.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
    altSprite.setTextDatum(CL_DATUM);

    altBgSprite.setColorDepth(8);
    altBgSprite.createSprite(ALTBG_IMG_WIDTH + 2, ALTBG_IMG_HEIGHT); // make it a little wider to fix any rotation integer math
    altBgSprite.fillSprite(TFT_BLACK);
    altBgSprite.pushImage(0, 0, ALTBG_IMG_WIDTH, ALTBG_IMG_HEIGHT, ALTBG_IMG_DATA);
    altBgSprite.setPivot(ALTBG_IMG_WIDTH / 2, ALTBG_IMG_HEIGHT / 2);

    flSprite.setColorDepth(8);
    flSprite.createSprite(60, 33);
    flSprite.loadFont(A320ISIS24);
    flSprite.setTextColor(TFT_GREEN, TFT_BLACK);
    flSprite.setTextDatum(CR_DATUM);

    alt100Sprite.setColorDepth(8);
    alt100Sprite.createSprite(ALTBG_IMG_WIDTH, ALTBG_IMG_HEIGHT);
    alt100Sprite.pushImage(0, 0, ALTBG_IMG_WIDTH, ALTBG_IMG_HEIGHT, ALTBG_IMG_DATA, 8184);
    alt100Sprite.loadFont(A320ISIS24);
    alt100Sprite.setTextColor(TFT_GREEN);
    alt100Sprite.setTextDatum(CR_DATUM);

    kohlsSprite.setColorDepth(8);
    kohlsSprite.createSprite(120, 33);
    kohlsSprite.loadFont(A320ISIS24);
    kohlsSprite.setTextDatum(TL_DATUM);
    kohlsSprite.setTextColor(TFT_BLUE);
}

void CC_ISIS::begin()
{
    loadSettings();

    lcd.setColorDepth(8);
    lcd.init();

    setupSprites();

    lcd.setBrightness(brightnessGamma(g5Settings.lcdBrightness));
    g5State.lcdBrightness = g5Settings.lcdBrightness;

#ifdef USE_GUITION_SCREEN
    lcd.setRotation(3); // Puts the USB jack at the bottom on Guition screen.
#else
    lcd.setRotation(0); // Orients the Waveshare screen with FPCB connector at bottom.
#endif

    lcd.fillScreen(TFT_BLACK);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    // lcd.loadFont(A320ISIS24);
    // lcd.setTextDatum(CC_DATUM);
    // lcd.drawString("A320 STARTUP", 240, 240);
}

void CC_ISIS::attach()
{
}

void CC_ISIS::detach()
{
}

void CC_ISIS::set(int16_t messageID, char *setPoint)
{
    // Common messages are handled by the base class.
    if (messageID < MSG_HSI_MIN)
        setCommon(messageID, setPoint);
    // ISIS-specific messages (IDs >= MSG_ISIS_MIN) go here when implemented.
    switch (messageID) {
    case 80: // Pitch
        g5State.pitchAngle = atof(setPoint);
        break;
    case 72: // Bank
        g5State.bankAngle = atof(setPoint);
        break;
    case 60: // Airspeed
        g5State.airspeed = atof(setPoint);
        break;
    case 77: // Indicated Altitude
        g5State.altitude = atof(setPoint);
        break;
    case 100: // Pressure in mb
        g5State.mbPressure = atoi(setPoint);
    }
}

void CC_ISIS::drawAttitude()
{

    const int16_t CENTER_X = attSprite.width() / 2;
    const int16_t CENTER_Y = ATT_HORIZON;

    const uint16_t HORIZON_COLOR = 0xFFFF;
    const uint16_t SKY_COLOR     = TFT_BLUE;
    const uint16_t GND_COLOR     = 37316; // = TFT_BROWN;

    float bankRad = g5State.bankAngle * PIf / 180.0f;
    // Pitch scaling factor (pixels per degree)
    const float PITCH_SCALE = 8.0;

    bool inverted = (g5State.bankAngle > 90.0 || g5State.bankAngle < -90.0);

    // Calculate vertical offset of the horizon due to pitch.
    // When inverted, flip the pitch offset to match what pilot sees from inverted perspective
    // A negative g5State.pitchAngle (nose up) moves the horizon down (positive pixel offset).
    float horizonPixelOffset = inverted ? (g5State.pitchAngle * PITCH_SCALE) : (-g5State.pitchAngle * PITCH_SCALE);

    attSprite.fillSprite(SKY_COLOR);

    // Pre-calculate trig for the loop.
    // tanBank drives the slope of the horizon line across columns.
    // When pitch is non-zero, rotating that offset line by bankAngle shifts the Y-intercept
    // at CENTER_X by horizonPixelOffset/cosBank (not just horizonPixelOffset), so we need cosBank too.
    float tanBank = tan(bankRad);
    float cosBank = cos(bankRad);
    // Guard: at ~90° bank cosBank→0 and the formula would blow up. Clamp to ±0.01 (≈89.4°).
    float safeCosBank    = (fabsf(cosBank) < 0.01f) ? (cosBank < 0.0f ? -0.01f : 0.01f) : cosBank;
    float horizonCenterY = CENTER_Y + horizonPixelOffset / safeCosBank;

    for (int16_t x = 0; x < attSprite.width(); x++) {
        // Distance from center
        int16_t dx = x - CENTER_X;

        // Correct horizon Y for this column: the rotated-line equation gives
        //   y = (CENTER_Y + horizonPixelOffset/cosBank) + dx*tanBank
        // which matches the rotation-matrix method used for the pitch ladder and horizon line.
        float horizonY = horizonCenterY + (dx * tanBank);

        int16_t horizonPixel = round(horizonY);

        // if (inverted) {
        //     // When inverted, ground is ABOVE the horizon line
        //     if (horizonPixel > 0) {
        //         attSprite.drawFastVLine(x, 0, min((int16_t)ATT_HEIGHT, horizonPixel),  GND_COLOR);
        //         if (x < SPEED_COL_WIDTH || x > (ATTITUDE_WIDTH - ALTITUDE_COL_WIDTH)) attitude.drawFastVLine(x, max((int16_t)0, horizonPixel), ATTITUDE_HEIGHT - max((int16_t)0, horizonPixel), (x < SPEED_COL_WIDTH || x > ATTITUDE_WIDTH - ALTITUDE_COL_WIDTH) ? DARK_SKY_COLOR : SKY_COLOR);
        //         // attitude.drawFastVLine(x, 0, min((int16_t)attitude.height(), horizonPixel), GND_COLOR);
        //     }
        // } else {
        // When upright, ground is BELOW the horizon line
        if (horizonPixel < attSprite.height()) {
            attSprite.drawFastVLine(x, max((int16_t)0, horizonPixel), ATT_HEIGHT - max((int16_t)0, horizonPixel), GND_COLOR);
            // attSprite.drawLine(x, max((int16_t)0, horizonPixel), x, ATT_HEIGHT, GND_COLOR);
        }
    }

    // --- 2. Draw Pitch Ladder (with correct math) ---
    // cosBank was already computed above for the fill loop; compute sinBank here.
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

        attSprite.drawLine(x1, y1, x2, y2, color);
        //        attSprite.drawWideLine(x1, y1, x2, y2, 2, color);

        if (showNumber && abs(pitchDegrees) >= 10) {
            char pitchText[4];
            sprintf(pitchText, "%d", (int)abs(pitchDegrees));

            float textOffset   = halfWidth + 15;
            float text1x_unrot = -textOffset;
            float texty_unrot  = verticalOffset;

            int16_t textX1 = CENTER_X + text1x_unrot * cosBank - texty_unrot * sinBank;
            int16_t textY1 = CENTER_Y + text1x_unrot * sinBank + texty_unrot * cosBank;

            ladderValSprite.fillSprite(TFT_BLACK);
            ladderValSprite.drawString(pitchText, 2, ladderValSprite.height() / 2);
            attSprite.setPivot(textX1, textY1);
            ladderValSprite.pushRotated(inverted ? g5State.bankAngle + 180.0 : g5State.bankAngle, TFT_BLACK);
            //            ladderValSprite.pushRotated(inverted ? g5State.bankAngle + 180.0 : g5State.bankAngle);
        }
    };

    // Define and draw all the pitch lines.
    // 2.5° increments, 90 to -90 (0° is the horizon, drawn separately).
    // Wide (80) + number at every 10°; medium (60) at every 5°; small (40) at 2.5° / 7.5° offsets.
    const struct {
        float deg;
        int   width;
        bool  num;
    } pitch_lines[] = {
        // clang-format off
        { 90.0, PITCH_LINE_WIDE, true },  { 87.5, PITCH_LINE_NARROW, false}, { 85.0, PITCH_LINE_MEDIUM, false}, { 82.5, PITCH_LINE_NARROW, false},
        { 80.0, PITCH_LINE_WIDE, true },  { 77.5, PITCH_LINE_NARROW, false}, { 75.0, PITCH_LINE_MEDIUM, false}, { 72.5, PITCH_LINE_NARROW, false},
        { 70.0, PITCH_LINE_WIDE, true },  { 67.5, PITCH_LINE_NARROW, false}, { 65.0, PITCH_LINE_MEDIUM, false}, { 62.5, PITCH_LINE_NARROW, false},
        { 60.0, PITCH_LINE_WIDE, true },  { 57.5, PITCH_LINE_NARROW, false}, { 55.0, PITCH_LINE_MEDIUM, false}, { 52.5, PITCH_LINE_NARROW, false},
        { 50.0, PITCH_LINE_WIDE, true },  { 47.5, PITCH_LINE_NARROW, false}, { 45.0, PITCH_LINE_MEDIUM, false}, { 42.5, PITCH_LINE_NARROW, false},
        { 40.0, PITCH_LINE_WIDE, true },  { 37.5, PITCH_LINE_NARROW, false}, { 35.0, PITCH_LINE_MEDIUM, false}, { 32.5, PITCH_LINE_NARROW, false},
        { 30.0, PITCH_LINE_WIDE, true },  { 27.5, PITCH_LINE_NARROW, false}, { 25.0, PITCH_LINE_MEDIUM, false}, { 22.5, PITCH_LINE_NARROW, false},
        { 20.0, PITCH_LINE_WIDE, true },  { 17.5, PITCH_LINE_NARROW, false}, { 15.0, PITCH_LINE_MEDIUM, false}, { 12.5, PITCH_LINE_NARROW, false},
        { 10.0, PITCH_LINE_WIDE, true },  {  7.5, PITCH_LINE_NARROW, false}, {  5.0, PITCH_LINE_MEDIUM, false}, {  2.5, PITCH_LINE_NARROW, false},
        { -2.5, PITCH_LINE_NARROW, false},  { -5.0, PITCH_LINE_MEDIUM, false}, { -7.5, PITCH_LINE_NARROW, false}, {-10.0, PITCH_LINE_WIDE, true },
        {-12.5, PITCH_LINE_NARROW, false},  {-15.0, PITCH_LINE_MEDIUM, false}, {-17.5, PITCH_LINE_NARROW, false}, {-20.0, PITCH_LINE_WIDE, true },
        {-22.5, PITCH_LINE_NARROW, false},  {-25.0, PITCH_LINE_MEDIUM, false}, {-27.5, PITCH_LINE_NARROW, false}, {-30.0, PITCH_LINE_WIDE, true },
        {-32.5, PITCH_LINE_NARROW, false},  {-35.0, PITCH_LINE_MEDIUM, false}, {-37.5, PITCH_LINE_NARROW, false}, {-40.0, PITCH_LINE_WIDE, true },
        {-42.5, PITCH_LINE_NARROW, false},  {-45.0, PITCH_LINE_MEDIUM, false}, {-47.5, PITCH_LINE_NARROW, false}, {-50.0, PITCH_LINE_WIDE, true },
        {-52.5, PITCH_LINE_NARROW, false},  {-55.0, PITCH_LINE_MEDIUM, false}, {-57.5, PITCH_LINE_NARROW, false}, {-60.0, PITCH_LINE_WIDE, true },
        {-62.5, PITCH_LINE_NARROW, false},  {-65.0, PITCH_LINE_MEDIUM, false}, {-67.5, PITCH_LINE_NARROW, false}, {-70.0, PITCH_LINE_WIDE, true },
        {-72.5, PITCH_LINE_NARROW, false},  {-75.0, PITCH_LINE_MEDIUM, false}, {-77.5, PITCH_LINE_NARROW, false}, {-80.0, PITCH_LINE_WIDE, true },
        {-82.5, PITCH_LINE_NARROW, false},  {-85.0, PITCH_LINE_MEDIUM, false}, {-87.5, PITCH_LINE_NARROW, false}, {-90.0, PITCH_LINE_WIDE, true },
        // clang-format on
    };

    uint16_t color = TFT_RED;
    for (const auto &line : pitch_lines) {
        // Simple culling: only draw lines that are somewhat close to the screen
        float verticalPos = abs(line.deg - g5State.pitchAngle) * PITCH_SCALE;
        color             = TFT_WHITE;
        if (verticalPos > 0 && verticalPos < attSprite.height()) {
            //        Serial.printf("Line: d: %f, vPos: %d, col: %d\n", line.deg, verticalPos, color);
            drawPitchLine(line.deg, line.width, line.num, color);
        }
    }

    // --- 3. Draw Horizon Line ---
    // The horizon is just a pitch line at 0 degrees.
    // We draw it extra long to ensure it always crosses the screen.
    float horiz_unrot_y = (0 - g5State.pitchAngle) * PITCH_SCALE;
    float lineLength    = attSprite.width() * 1.5;

    int16_t hx1 = CENTER_X + (-lineLength / 2.0) * cosBank - horiz_unrot_y * sinBank;
    int16_t hy1 = CENTER_Y + (-lineLength / 2.0) * sinBank + horiz_unrot_y * cosBank;
    int16_t hx2 = CENTER_X + (lineLength / 2.0) * cosBank - horiz_unrot_y * sinBank;
    int16_t hy2 = CENTER_Y + (lineLength / 2.0) * sinBank + horiz_unrot_y * cosBank;

    attSprite.drawLine(hx1, hy1, hx2, hy2, HORIZON_COLOR);
    attSprite.drawLine(hx1, hy1 + 1, hx2, hy2 + 1, HORIZON_COLOR); // Thicker line
}

void CC_ISIS::drawSpeedTape()
{
    // Speed tape is drastically simpler than the core G5. Only a scrolling speed tape with no digit readout or rolling numbers
    // Speed shown in 10's with tiny hash marks at 5knots below 250, small 10kt hash marks and tiny marks with number at the 20s

    // I measure 153px for 40 kt = 3.8px/kt Let's call it 4.
    // The screen can show about 85kt at one time.

    const float pixPerKt = 3.8f;

    const float curSpeed = max(g5State.airspeed, 30.0f);
    const int   yOffset  = (int)(pixPerKt * fmodf(curSpeed, 20.0f)) - 35; // offset factor for non-centered arrow.
    const int   first20  = (int)(curSpeed / 20.0f) * 20;

    const int speedMarkWidth = 6;

    speedSprite.fillSprite(TFT_BLACK);
    speedSprite.setColor(TFT_WHITE);

    int curY = +yOffset;

    //    Serial.printf("CurY: %d  first20: %d yOffset: %d\n", curY, first20, yOffset);

    for (int i = first20 + 60; i > first20 - 60; i -= 5) {

        if (i < 30) continue;

        if (i % 20 == 0) {
            speedSprite.drawNumber(i, speedSprite.width() - 6, curY);
            speedSprite.drawWideLine(70, curY, 75, curY, 1, TFT_WHITE);
            // speedSprite.drawFastHLine(speedSprite.width() - 12, curY, 5);
            // speedSprite.drawFastHLine(speedSprite.width() - 12, curY+1, 5);
        } else if (i % 10 == 0) {
            speedSprite.drawWideLine(65, curY, 75, curY, 1, TFT_WHITE);
            // speedSprite.drawFastHLine(speedSprite.width() - 16, curY, 10);
            // speedSprite.drawFastHLine(speedSprite.width() - 16, curY+1, 10);
        } else if (i <= 245) {
            speedSprite.drawWideLine(70, curY, 75, curY, 1, TFT_WHITE);
            // speedSprite.drawFastHLine(speedSprite.width() - 12, curY, 5);
            // speedSprite.drawFastHLine(speedSprite.width() - 12, curY+1, 5);
        }

        curY += (int)(pixPerKt * 5); // pixPerKt * 5
    }

    speedSprite.pushSprite(0, ATT_TOP_EDGE);
}

void CC_ISIS::drawAltTape()
{

    // Value display 20' increments but scrolls to single digit. Shows 5 digigs of alt (i.e. up to 99,980').
    // Vertical Tape shows FL (100's of feet) at 500' increments (200, 205, 210 etc)
    //  Which is also in the left (wide) part of the digit box
    // The tall (right) part of the digit box shows 0-99 feet
    //
    // Hash marks every 100' on the tape. Labels every 500' Values as three digits with leading 0s

    float curAlt = g5State.altitude;
    int   fl     = (int)(curAlt / 100);   // hundreds and above (e.g. 1234ft → fl=12)

    char buf[8];

    // Clear Sprites
    altSprite.fillSprite(TFT_BLACK);
    alt100Sprite.pushImage(0, 0, ALTBG_IMG_WIDTH, ALTBG_IMG_HEIGHT, ALTBG_IMG_DATA, 8184); // MagentaRGB
    flSprite.fillSprite(TFT_BLACK);

    // Compute the scroll state once; everything below uses these values.
    // dispUnit: the current 20-ft band label (changes only at band boundaries).
    // sub20:    position within that band (0..20, continuous — never jumps).
    int   dispUnit      = (int)(curAlt / 20) * 20;
    float sub20         = fmodf(curAlt, 20.0f);

    // nearRoll: true during the last 20-ft band of each 100-ft block (the 80-99 range).
    // rollFraction: 0.0→1.0 progress through that roll-over window.
    bool  nearRoll      = (dispUnit % 100 >= 80);
    float rollFraction  = nearRoll ? (sub20 / 20.0f) : 0.0f;

    // --- Thousands-and-above digits (flSprite) ---
    // Rolls only when the hundreds digit is also about to turn over (fl%10 == 9).
    {
        bool      nearRollK = nearRoll && ((fl % 10) == 9);
        const int centY     = flSprite.height() / 2;          // 16
        int       offset    = nearRollK ? (int)(rollFraction * centY) : 0;
        flSprite.setTextSize(1.2f);
        if (nearRollK) {
            // Next thousands value rises from below
            flSprite.drawNumber(fl / 10 + 1, flSprite.width() - 2, centY - offset + centY);
        }
        flSprite.drawNumber(fl / 10, flSprite.width() - 2, centY - offset);
    }

    // --- Hundreds digit (alt100Sprite, left column at x=24) ---
    // Rolls during the last 20-ft band of each 100-ft block.
    {
        const int centY   = alt100Sprite.height() / 2 + 2;    // 34
        int       offset  = (int)(rollFraction * centY);
        alt100Sprite.setTextSize(1.2f);
        if (nearRoll) {
            // Next hundreds digit rises from below
            alt100Sprite.drawNumber((fl % 10 + 1) % 10, 24, centY - offset + centY);
        }
        alt100Sprite.drawNumber(fl % 10, 24, centY - offset);
    }

    // --- 20-ft scroll (alt100Sprite, right column at x=70) ---
    // curY: Y centre of the current label; starts at sprite centre (32) and ascends
    // (decreases) as altitude increases, so higher values enter from below.
    const float pxPerFt = 60.0f / 20.0f;                      // 3 px per foot
    int curY = (int)(32 - sub20 * pxPerFt);                   // 32..-25 as sub20 goes 0→19

    alt100Sprite.setTextSize(1.0f);
    sprintf(buf, "%02d", dispUnit % 100);                      // current label, ascends off top
    alt100Sprite.drawString(buf, 70, curY);
    sprintf(buf, "%02d", (dispUnit + 20) % 100);               // next-higher, enters from below
    alt100Sprite.drawString(buf, 70, curY + 60);

    flSprite.pushSprite(attSprite.width() - flSprite.width(), ATT_HORIZON - flSprite.height() / 2);

    // Draw the tape

    const float pixPerFt = 0.245f;
    const int   yOffset  = (int)(pixPerFt * fmodf(curAlt, 20.0f)) - 35;
    const int   first500 = (int)(curAlt / 500.0f) * 500;

    const int markerWidth = 15;
    altSprite.setTextDatum(CL_DATUM);

    curY = yOffset;

    //  Serial.printf("curY:%d yOffset: %d first500:%d\n", curY, yOffset, first500);

    for (int i = first500 + 1000; i > first500 - 1000; i -= 100) {

        altSprite.drawWideLine(2, curY, 2 + markerWidth, curY, 1, TFT_WHITE);
        if (i % 500 == 0) {
            sprintf(buf, "%03d", i / 100);
            altSprite.drawString(buf, 10, curY);
        }

        curY += (int)(pixPerFt * 100);
    }

    //    Serial.printf("curY:%d yOffset: %d first500:%d\n", curY, yOffset, first500);

    alt100Sprite.pushSprite(0, ATT_HORIZON - alt100Sprite.height() / 2 - 3);
    altSprite.pushSprite(ALT_LEFT_EDGE - 2, ATT_TOP_EDGE);

    return;
}

void CC_ISIS::drawBackground()
{
    // Draw the yellow background markers.
    // bgSprite.pushSprite(0, ATT_TOP_EDGE + ATT_HORIZON + 33, TFT_MAGENTA);  // 33 is tip of pointer from top of bgSprite

    attSprite.pushImage(0, ATT_HORIZON - 25, ATTBACKGROUND_IMG_WIDTH, ATTBACKGROUND_IMG_HEIGHT, ATTBACKGROUND_IMG_DATA, 8184);

    // Draw the curves top and bottom of the gauge.
    blackoutArcSprite.pushSprite(0, 0, TFT_MAGENTA);
    attSprite.setPivot(attSprite.width() / 2, attSprite.height() - blackoutArcSprite.height() / 2);
    blackoutArcSprite.pushRotated(180.0f, TFT_MAGENTA);
}

void CC_ISIS::drawPressure()
{
    kohlsSprite.fillSprite(TFT_BLACK);
    kohlsSprite.drawNumber(g5State.mbPressure, 1, 1);
    kohlsSprite.pushSprite(140, 420);
}

void CC_ISIS::draw()
{
    drawAttitude();
    drawBackground();
    drawSpeedTape();
    drawAltTape();
    attSprite.pushSprite(ATT_LEFT_EDGE, ATT_TOP_EDGE);
    drawPressure();
}

void CC_ISIS::update()
{
    static unsigned long lastDelta = 0;
    draw();
    char buf[80];
    sprintf(buf, "p:%.1f b:%.1f a:%.1f a:%d", g5State.pitchAngle, g5State.bankAngle, g5State.airspeed, g5State.altitude);
    lcd.setTextSize(2.0f);
    lcd.fillRect(0, 0, 480, 25);
    lcd.drawString(buf, 20, 20);
    if (millis() > lastDelta + 500) {
        g5State.bankAngle += 0.5;
        if (g5State.bankAngle > 30) g5State.bankAngle = -30.0f;
        lastDelta = millis();

        g5State.airspeed += 1.0f;
        if (g5State.airspeed > 500.0f) g5State.airspeed = 0.0f;

        g5State.altitude += 10;
        if (g5State.altitude > 15000) g5State.altitude = 0;
    }
}
