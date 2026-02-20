#pragma once

#include "Arduino.h"
#include "G5Common.h"

#define PRESS_COLOR TFT_BLUE

class CC_ISIS
{
public:
    CC_ISIS();
    void begin();
    void attach();
    void detach();
    void set(int16_t messageID, char *setPoint);

    void update();

private:
    bool _initialized;

    void setupSprites();
    void drawBackground();
    void drawPressure();
    void drawSpeedTape();
    void drawAltTape();
    void drawAttitude();
    void drawLS();

    void draw();



};