#include "CC_ISIS.h"
#include "Images/isisFont.h"
#include "Sprites/isisBg.h"
#include "Sprites/blackoutArc.h"

#define PITCH_LINE_NARROW  17   // 2.5° and 7.5° tick marks
#define PITCH_LINE_MEDIUM  27   // 5° tick marks
#define PITCH_LINE_WIDE    54   // 10° tick marks (labeled)

LGFX_Sprite attSprite(&lcd);
LGFX_Sprite bgSprite(&attSprite);       // Used for the static elements of the attitude display.n UNNECESSARY!!
LGFX_Sprite ladderValSprite(&attSprite); // Used to hold the scale numbers on the pitch ladder
LGFX_Sprite blackoutArcSprite(&attSprite); // Used to hold the scale numbers on the pitch ladder


CC_ISIS::CC_ISIS()
{
}

void CC_ISIS::setupSprites()
{

    bgSprite.setColorDepth(8);
    bgSprite.createSprite(ISISBG_IMG_WIDTH, ISISBG_IMG_HEIGHT);
    bgSprite.pushImage(0, 0, ISISBG_IMG_WIDTH, ISISBG_IMG_HEIGHT, ISISBG_IMG_DATA);

    attSprite.setColorDepth(8);
    attSprite.createSprite(320, 350);

    ladderValSprite.setColorDepth(8);
    ladderValSprite.createSprite(45, 28); // Hold two digits. Digits are 24 high
    ladderValSprite.setPivot(25, 14);
    ladderValSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    ladderValSprite.setTextDatum(CL_DATUM);
    ladderValSprite.loadFont(A320ISIS24);

    blackoutArcSprite.setColorDepth(8);
    blackoutArcSprite.fillSprite(TFT_BLACK);
    blackoutArcSprite.createSprite(BLACKOUTARC_IMG_WIDTH + 2, BLACKOUTARC_IMG_HEIGHT); // make it a little wider to fix any rotation integer math
    blackoutArcSprite.pushImage(1,0, BLACKOUTARC_IMG_WIDTH, BLACKOUTARC_IMG_HEIGHT, BLACKOUTARC_IMG_DATA);
    blackoutArcSprite.setPivot(BLACKOUTARC_IMG_WIDTH/2, BLACKOUTARC_IMG_HEIGHT/2);
    


}

void CC_ISIS::begin()
{
    loadSettings();

    setupSprites();

    lcd.setColorDepth(8);
    lcd.init();

    lcd.setBrightness(brightnessGamma(g5Settings.lcdBrightness));
    g5State.lcdBrightness = g5Settings.lcdBrightness;

#ifdef USE_GUITION_SCREEN
    lcd.setRotation(3); // Puts the USB jack at the bottom on Guition screen.
#else
    lcd.setRotation(0); // Orients the Waveshare screen with FPCB connector at bottom.
#endif

    lcd.fillScreen(TFT_BLACK);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.loadFont(A320ISIS24);
    lcd.setTextDatum(CC_DATUM);
    lcd.drawString("A320 STARTUP", 240, 240);
}

void CC_ISIS::attach()
{
}

void CC_ISIS::detach()
{
}

void CC_ISIS::set(int16_t messageID, char *setPoint)
{
}

void CC_ISIS::drawAttitude()
{

    const int16_t ATT_WIDTH  = attSprite.width();
    const int16_t ATT_HEIGHT = attSprite.height();
    const int16_t CENTER_X   = attSprite.width() / 2;
    const int16_t CENTER_Y   = attSprite.height() / 2;

    const uint16_t HORIZON_COLOR = 0xFFFF;
    const uint16_t SKY_COLOR     = TFT_BLUE;
    const uint16_t GND_COLOR     = TFT_BROWN;

    float bankRad = g5State.bankAngle * PI / 180.0;
    // Pitch scaling factor (pixels per degree)
    const float PITCH_SCALE = 7.0;

    bool inverted = (g5State.bankAngle > 90.0 || g5State.bankAngle < -90.0);

    // Calculate vertical offset of the horizon due to pitch.
    // When inverted, flip the pitch offset to match what pilot sees from inverted perspective
    // A negative g5State.pitchAngle (nose up) moves the horizon down (positive pixel offset).
    float horizonPixelOffset = inverted ? (g5State.pitchAngle * PITCH_SCALE) : (-g5State.pitchAngle * PITCH_SCALE);

    attSprite.fillSprite(SKY_COLOR);

    // Pre-calculate tan of bank angle for the loop
    float tanBank = tan(bankRad);
    // For each column of pixels, calculate where the horizon intersects
    for (int16_t x = 0; x < attSprite.width(); x++) {
        // Distance from center
        int16_t dx = x - CENTER_X;

        // Calculate horizon Y position for this column
        // The horizon's center is at CENTER_Y + horizonPixelOffset
        float horizonY = (CENTER_Y + horizonPixelOffset) + (dx * tanBank);

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
            ladderValSprite.drawString(pitchText, 2, ladderValSprite.height()/2);
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
        color = TFT_WHITE;
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

void CC_ISIS::drawBackground()
{
    // Draw the yellow background markers.
    bgSprite.pushSprite(0, 168, TFT_MAGENTA);
    
    // Draw the curves top and bottom of the gauge.
    blackoutArcSprite.pushSprite(0,0,TFT_MAGENTA);
    attSprite.setPivot(attSprite.width()/2, attSprite.height() - blackoutArcSprite.height()/2);
    blackoutArcSprite.pushRotated(180.0f, TFT_MAGENTA);
}

void CC_ISIS::drawPressure()
{
    lcd.setTextColor(PRESS_COLOR, TFT_BLACK);
    lcd.setTextDatum(TR_DATUM);
    lcd.drawString("1013", 240, 410);
}

void CC_ISIS::draw()
{
    drawAttitude();
    drawBackground();
    attSprite.pushSprite(75, 56);
    drawPressure();
}

void CC_ISIS::update()
{

    draw();
}
