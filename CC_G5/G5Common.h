#pragma once
#include <Arduino.h>
#include <vector>
#include <memory>
#include <functional>
#include "MFEEPROM.h"

static const char *TAG_CC_G5   = "CC_G5_PFD";
static const char *TAG_I2C     = "CC_G5_I2C";
static const char *TAG_SPRITES = "CC_G5_SPRITES";

#define USE_GUITION_SCREEN

// Screen configuration selection
#ifdef USE_GUITION_SCREEN
#include "4inchLCDConfig_Guition.h"
#else
#include "4inchLCDConfig.h"
#endif
// #include "G5_Menu.h"
#include "commandmessenger.h"
#include <Wire.h>

#define CC_G5_SETTINGS_OFFSET 2048     // Well past MF config end (59 + 1496 = 1555)
#define TFT_MAIN_TRANSPARENT  TFT_PINK // Just pick a color not used in either display

struct CC_G5_Settings {
    int     version               = 1;
    uint8_t bearingPointer1Source = 0; // 0: Off, 1: GPS, 2: Nav1, 3: Nav2, 4: ADF
    uint8_t bearingPointer2Source = 0; // 0: Off, 1: GPS, 2: Nav1, 3: Nav2, 4: ADF
    uint8_t Vr                    = 60;
    uint8_t Vx                    = 75;
    uint8_t Vy                    = 91;
    uint8_t Va                    = 112;
    uint8_t Vfe                   = 110;
    uint8_t Vs0                   = 54;
    uint8_t Vs1                   = 61;
    uint8_t Vg                    = 80;
    uint8_t Vno                   = 145;
    uint8_t Vne                   = 182;
    uint8_t Vflaplo               = 54;
    uint8_t Vflaphi               = 100;
    uint8_t Vgrnlo                = 61;
    uint8_t Vgrnhi                = 145;
    uint8_t Vyello                = 145;
    uint8_t Vyelhi                = 182;
    uint8_t Vredlo                = 182;
    uint8_t pitchScale            = 15;
    uint8_t speedScale            = 10;
    uint8_t baroUnit              = 0; // 0: inHg, 1: kPa, 3: mmHg
    uint8_t speedUnits            = 0; // 0:knot 1:mph 2:kph
    uint8_t distanceUnits         = 0; // 0: nm, 1: miles, 2:km
    uint8_t tempUnits             = 0; // 0: F, 1: C
};

extern CC_G5_Settings g5Settings;

// Settings definition struct
struct SettingDef {
    const char *name;
    uint8_t    *valuePtr;
    uint8_t     minVal;
    uint8_t     maxVal;
};

// Settings menu class
class PFDSettingsMenu
{
private:
    std::vector<SettingDef> settings;
    int                     scrollIndex = 0;

public:
    PFDSettingsMenu()
    {
        // Define all V-speed settings
        settings = {
            {"Vs0", &g5Settings.Vs0, 0, 255}, // Bottom of white arc. Top of slow red arc.
            {"Vs1", &g5Settings.Vs1, 0, 255}, // Bottom of green arc
            {"Vr", &g5Settings.Vr, 0, 255},   // Rotation speed. Tape marker.
            {"Vx", &g5Settings.Vx, 0, 255},   // Max Climb. Tape marker.
            {"Vy", &g5Settings.Vy, 0, 255},   // Best rate of climb. Tape marker.
            {"Vg", &g5Settings.Vg, 0, 255},   // Glide speed. Tape marker.
            {"Va", &g5Settings.Va, 0, 255},   // Maneuver speed. not shown.
            {"Vfe", &g5Settings.Vfe, 0, 255}, // Max Flaps extended. top of white arc
            {"Vno", &g5Settings.Vno, 0, 255}, // Max normal operating speed. Top of green, start of Yellow
            {"Vne", &g5Settings.Vne, 0, 255}, // Never exceed. Top of yellow, start of barber pole. (We just use red)
        };
    }

    int         getSettingsCount() { return settings.size(); }
    SettingDef &getSetting(int index) { return settings[index]; }
    int         getScrollIndex() { return scrollIndex; }
    void        setScrollIndex(int index) { scrollIndex = index; }
    void        scrollUp()
    {
        if (scrollIndex > 0) scrollIndex--;
    }
    void scrollDown()
    {
        if (scrollIndex < settings.size()) scrollIndex++; // There's a back menu at top.
    }
};

extern LGFX        lcd;
extern LGFX_Sprite deviationScale; // The four circles
extern LGFX_Sprite glideDeviationScale;
extern LGFX_Sprite deviationDiamond;
extern LGFX_Sprite headingBox;
extern LGFX_Sprite gsBox;

// TwoWire myI2C(1);
// Pin definitions for ESP32 - different pins based on screen type
#ifdef USE_GUITION_SCREEN
#define I2C_SDA_PIN 1  // SDA pin for Guition screen (GPIO1)
#define I2C_SCL_PIN 2  // SCL pin for Guition screen (GPIO2)
#define INT_PIN     40 // Interrupt pin from RP2040 (GPI40)
#else
#define I2C_SDA_PIN 15 // SDA pin (GPIO15)
#define I2C_SCL_PIN 7  // SCL pin (GPIO7)
#define INT_PIN     16 // Interrupt pin from RP2040 (GPIO16)
#endif
#define RP2040_ADDR 0x08 // RP2040 I2C slave address

// Button codes from the rp2040
enum ButtonEventType : uint8_t {
    BUTTON_IDLE         = 0,
    BUTTON_CLICKED      = 1,
    BUTTON_PRESSED      = 2,
    BUTTON_LONG_PRESSED = 3,
    BUTTON_RELEASED     = 4
};

// G5_Hardware class - manages I2C encoder interface and other hardware
class G5_Hardware
{
public:
    G5_Hardware() = default;

    // Called from interrupt handler lambda when RP2040 signals data available
    void setDataAvailable()
    {
        dataAvailable = true;
    }

    // Read data from RP2040 and return true if new data was available
    bool readEncoderData(int8_t &outDelta, int8_t &outEncButton, int8_t &outExtraButton);

    // Check if data is ready to be read
    bool hasData() const { return dataAvailable; }

    // Get accumulated encoder value (total rotation since power-on)
    int getEncoderValue() const { return encoderValue; }

    // Get current button states (returns ButtonEventType values)
    int getEncoderButton() const { return encoderButton; }
    int getExtraButton() const { return extraButton; }

    // Control LED on RP2040
    void setLedState(bool state);

private:
    volatile bool dataAvailable = false;
    int           encoderValue  = 0;
    int           encoderButton = 0;
    int           extraButton   = 0;
};

// Global hardware interface instance
extern G5_Hardware g5Hardware;

// I2C utility function
void sendEncoder(String name, int count, bool increase);

float smoothDirection(float inputDir, float currentDir, float alpha, float threashold);
float smoothAngle(float input, float current, float alpha, float threshold);
int   smoothInput(int input, int current, float alpha, int threashold);
float smoothInput(float input, float current, float alpha, float snapThreashold);

extern MFEEPROM MFeeprom;
bool            loadSettings();
bool            saveSettings();

// Selection option for popup menus
struct SelectionOption {
    const char *label;
    int         value;
};

// Helper to get bearing source name from value
inline const char *getBearingSourceName(int source)
{
    static const char *names[] = {"Off", "GPS", "VLOC1", "VLOC2", "ADF"};
    if (source >= 0 && source < 5) return names[source];
    return "???";
}

// MENU SYSTEM

template <typename ParentDevice>
class G5MenuBase
{
public:
    enum class MenuState {
        BROWSING,
        ADJUSTING,
        SELECTING,
        SETTINGS_BROWSING
    };

    class MenuItemBase
    {
    public:
        virtual ~MenuItemBase()                 = default;
        virtual String getTitle()               = 0;
        virtual String getDisplayValue()        = 0;
        virtual int    getDisplayValueColor()   = 0;
        virtual void   onEncoderTurn(int delta) = 0;
        virtual void   onEncoderPress()         = 0;

        virtual bool isVisible() const { return true; }

        // Icon support - return nullptr if no icon
        virtual const uint16_t *getIcon() { return nullptr; }
        virtual int             getIconWidth() { return 0; }
        virtual int             getIconHeight() { return 0; }
    };

protected:
    ParentDevice                              *parent;
    std::vector<std::unique_ptr<MenuItemBase>> menuItems;
    int                                        currentHighlight = 0;
    MenuItemBase                              *adjustingItem    = nullptr;

    // Selection popup state
    std::vector<SelectionOption> selectionOptions;
    int                         *selectionValuePtr  = nullptr;
    int                          selectionHighlight = 0; // Index in options array

public:
    bool      menuActive   = false;
    MenuState currentState = MenuState::BROWSING;
    int       menuXpos     = 0;
    int       menuYpos     = 0;
    int       menuHeight   = 0;
    int       menuWidth    = 0;

    G5MenuBase(ParentDevice *p) : parent(p) {}

    // Pure virtual - each device defines its menu items
    virtual void createMenuItems() = 0;

    // Pure virtual - each device provides its target sprite
    virtual LGFX_Sprite *getTargetSprite() = 0;

    void initializeMenu()
    {
        menuItems.clear();
        createMenuItems();
        currentState     = MenuState::BROWSING;
        currentHighlight = 0;
        while (currentHighlight < menuItems.size() && 
            !menuItems[currentHighlight]->isVisible()) {
                currentHighlight++;
            }
            if (currentHighlight >= menuItems.size()) currentHighlight = 0;
    }

    bool setActive(bool isActive)
    {
        menuActive = isActive;
        if (!isActive) currentState = MenuState::BROWSING;
        return menuActive;
    }

    virtual void handleEncoder(int delta)
    { // Make virtual to support the Settings in the PFD.
        if (currentState == MenuState::BROWSING) {
            scrollHighlight(delta);
        } else if (currentState == MenuState::SELECTING) {
            // Scroll through selection options
            selectionHighlight += (delta > 0) ? 1 : -1;
            if (selectionHighlight < 0) selectionHighlight = 0;
            if (selectionHighlight >= selectionOptions.size()) selectionHighlight = selectionOptions.size() - 1;
        } else if (currentState == MenuState::ADJUSTING && adjustingItem) {
            adjustingItem->onEncoderTurn(delta);
        }
    }

    virtual void handleEncoderButton(bool pressed)
    {
        if (!pressed) return;

        if (currentState == MenuState::BROWSING) {
            // Make sure current item is visible
            if (currentHighlight >= menuItems.size() || !menuItems[currentHighlight]->isVisible()) {
                return;   // Shouldn't happen...
            }
            menuItems[currentHighlight]->onEncoderPress();

        } else if (currentState == MenuState::SELECTING) {
            // Save selected value and return to menu
            *selectionValuePtr = selectionOptions[selectionHighlight].value;
            saveSettings();
            adjustingItem = nullptr;
            currentState  = MenuState::BROWSING;
            setActive(false); // Not sure if menu should stay open or close. This will close it.
        } else if (currentState == MenuState::ADJUSTING) {
            adjustingItem = nullptr;
            currentState  = MenuState::BROWSING;
            setActive(false);
        }
    }

    void enterSelectionMode(MenuItemBase *item, std::vector<SelectionOption> options, int *valuePtr)
    {
        adjustingItem     = item;
        selectionOptions  = options;
        selectionValuePtr = valuePtr;

        // Find current value in options and set highlight
        selectionHighlight = 0;
        for (int i = 0; i < options.size(); i++) {
            if (options[i].value == *valuePtr) {
                selectionHighlight = i;
                break;
            }
        }

        currentState = MenuState::SELECTING;
    }

    void scrollHighlight(int delta)
    {
        int newHighlight = getNextVisibleItem(currentHighlight, delta);

        // Boundary check
        if (newHighlight < 0) newHighlight = 0;
        if (newHighlight >= menuItems.size()) newHighlight = menuItems.size() - 1;

        // Make sure we landed on a visible item
        if (newHighlight < menuItems.size() && menuItems[newHighlight]->isVisible()) {
            currentHighlight = newHighlight;
        }
    }

    int getVisibleItemCount() const
    {
        int count = 0;
        for (const auto &item : menuItems) {
            if (item->isVisible()) count++;
        }
        return count;
    }

    int getNextVisibleItem(int currentIndex, int delta) const
    {
        int step = (delta > 0) ? 1 : -1;
        int idx  = currentIndex + step;

        while (idx >= 0 && idx < menuItems.size()) {
            if (menuItems[idx]->isVisible()) {
                return idx;
            }
            idx += step;
        }

        // No more visible items.
        return currentIndex;
    }

    // Get the visible index of an absolute index (for display purposes)
    // Returns which "nth visible item" this is
    int getVisibleIndex(int absoluteIndex) const
    {
        int visibleIdx = 0;
        for (int i = 0; i < absoluteIndex && i < menuItems.size(); i++) {
            if (menuItems[i]->isVisible()) visibleIdx++;
        }
        return visibleIdx;
    }

    void enterAdjustmentMode(MenuItemBase *item)
    {
        adjustingItem = item;
        currentState  = MenuState::ADJUSTING;
    }

    void drawMenu()
    {

        static int currentStartPos = 0;
        if (!menuActive || currentState == MenuState::ADJUSTING || currentState == MenuState::SELECTING) return;

        auto targetSprite = getTargetSprite();
        if (!targetSprite) return;

        int itemWidth = 105, itemHeight = 80, itemSpacing = 9, outlineWidth = 4;
        int curX   = itemSpacing;
        int yPos   = targetSprite->height() - 120;
        menuYpos   = yPos;
        menuXpos   = 0;
        menuWidth  = targetSprite->width();
        menuHeight = 108;

        int menuSize = (targetSprite->width() - outlineWidth * 2 - itemSpacing * 3) / itemWidth;

        // Visible items
        int visibleItemCount = getVisibleItemCount();
        int currentVisibleIndex = getVisibleIndex(currentHighlight);

        // Draw menu background directly on target sprite
        targetSprite->fillRoundRect(0, yPos, targetSprite->width(), 108, outlineWidth, 0x7BEF);                                                                     // Light gray. Include space for the scroll bar at bottom.
        targetSprite->fillRoundRect(outlineWidth, yPos + outlineWidth, targetSprite->width() - outlineWidth * 2, 108 - outlineWidth * 2, outlineWidth / 2, 0x0000); // Black

        yPos += itemSpacing;

        // Figure scroll position based on visible items.
        if (currentVisibleIndex < currentStartPos) currentStartPos = currentVisibleIndex;
        if (currentVisibleIndex > currentStartPos + (menuSize - 1)) currentStartPos = currentVisibleIndex - (menuSize - 1);
        //      Serial.printf("cHi: %d, cSPo: %d\n", currentHighlight, currentStartPos);

        // Draw visible menu items (up to menuSize + 1)
        int visibleDrawn = 0;
        int visibleSkipped = 0;

        for (int i = 0; i <menuItems.size() && visibleDrawn < menuSize; i++) {
            if (!menuItems[i]->isVisible()) continue;

            // Skip items before scroll
            if (visibleSkipped < currentStartPos) {
                visibleSkipped++;
                continue;
            }

            bool isHighlighted = (i == currentHighlight);

            // Draw item rectangle
            targetSprite->fillRoundRect(curX, yPos, itemWidth, itemHeight, outlineWidth, isHighlighted ? 0xFFFF : 0x7BEF); // White if selected, gray otherwise
            targetSprite->fillRoundRect(curX + outlineWidth, yPos + outlineWidth, itemWidth - outlineWidth * 2, itemHeight - outlineWidth * 2, outlineWidth / 2, 0x0000);

            // Draw item title
            targetSprite->setTextColor(0xFFFF, 0x0000); // White on black
            targetSprite->setTextDatum(lgfx::v1::textdatum::textdatum_t::top_center);
            targetSprite->setTextSize(0.6);
            targetSprite->drawString(menuItems[i]->getTitle().c_str(), curX + itemWidth / 2, yPos + 15);

            // Draw item icon if it has one
            const uint16_t *iconData = menuItems[i]->getIcon();
            if (iconData != nullptr) {
                int iconWidth  = menuItems[i]->getIconWidth();
                int iconHeight = menuItems[i]->getIconHeight();
                int iconX      = curX + ((itemWidth - iconWidth) / 2);
                int iconY      = yPos + ((itemHeight - iconHeight) / 2) + 10; // Offset down from title
                targetSprite->pushImage(iconX, iconY, iconWidth, iconHeight, iconData);
            }

            // Draw item value if it has one
            String displayValue = menuItems[i]->getDisplayValue();
            if (displayValue.length() > 0) {
                targetSprite->setTextColor(menuItems[i]->getDisplayValueColor(), 0x0000);
                targetSprite->setTextDatum(lgfx::v1::textdatum::textdatum_t::bottom_center);
                targetSprite->setTextSize(0.8);
                targetSprite->drawString(displayValue.c_str(), curX + itemWidth / 2, yPos + itemHeight - 3);
            }

            visibleDrawn++;
            curX += (itemWidth + itemSpacing);
        }
        // Draw the scroll bar based on visible count

        yPos               = targetSprite->height() - 120 + 100 - outlineWidth;
        int totalLineWidth = (targetSprite->width() - (outlineWidth * 4) - (itemSpacing * 2));
        int lineIncrement  = 0;
        int xPos           = outlineWidth * 2 + itemSpacing;
        int lineWidth      = totalLineWidth;

        if (visibleItemCount > menuSize) {
            lineIncrement = totalLineWidth / visibleItemCount;
            // draw margin line
            targetSprite->drawFastHLine(xPos, yPos + 1, totalLineWidth, TFT_DARKGRAY);

            // adjust scroll bar width to fractional size
            lineWidth = (menuSize * lineIncrement);
            xPos += currentStartPos * lineIncrement;
        }

        // draw scroll bar
        targetSprite->drawFastHLine(xPos, yPos, lineWidth, TFT_WHITE);
        targetSprite->drawFastHLine(xPos, yPos + 1, lineWidth, TFT_WHITE);
        targetSprite->drawFastHLine(xPos, yPos + 2, lineWidth, TFT_WHITE);

        // The target sprite will get pushed to screen in the main loop.
    }

    void drawAdjustmentPopup()
    {
        if (!adjustingItem) return;

        //  LGFX_Sprite *sprite = getTargetSprite();
        //      sprite->fillSprite(TFT_MAIN_TRANSPARENT);  // clear the sprite
        // clear the previous menu

        auto targetSprite = getTargetSprite();
        if (!targetSprite) return;

        // Draw popup in center of target sprite
        int popupWidth = 160, popupHeight = 100;
        int centerX = (targetSprite->width() - popupWidth) / 2;
        int centerY = (targetSprite->height() - popupHeight - 15); // 15 off the bottom

        // Draw popup background
        targetSprite->fillRoundRect(centerX, centerY, popupWidth, popupHeight, 3, TFT_BLACK);
        targetSprite->fillGradientRect(centerX + 2, centerY + 2, popupWidth - 4, 40, 0x7BEF, 0x0000, lgfx::v1::gradient_fill_styles::vertical_linear);
        targetSprite->drawRoundRect(centerX, centerY, popupWidth, popupHeight, 3, 0xFFFF);
        targetSprite->drawRoundRect(centerX + 1, centerY + 1, popupWidth - 2, popupHeight - 2, 2, 0xFFFF);

        // Draw title
        targetSprite->setTextColor(0xFFFF);
        targetSprite->setTextDatum(lgfx::v1::textdatum::textdatum_t::top_center);
        targetSprite->setTextSize(0.7);
        targetSprite->drawString(adjustingItem->getTitle(), centerX + popupWidth / 2, centerY + 10);

        // Draw current value with color
        targetSprite->setTextColor(adjustingItem->getDisplayValueColor());
        targetSprite->setTextSize(1.0);
        targetSprite->setTextDatum(lgfx::v1::textdatum::textdatum_t::middle_center);
        String valueStr = adjustingItem->getDisplayValue();
        targetSprite->drawString(valueStr, centerX + popupWidth / 2, centerY + popupHeight / 2 + 15);
    }

    // Draw Selection, used to select bearing pointer source
    void drawSelectionPopup()
    {
        if (!adjustingItem || selectionOptions.empty()) return;

        auto targetSprite = getTargetSprite();
        if (!targetSprite) return;

        // Popup dimensions
        int popupWidth   = 200;
        int itemHeight   = 50;
        int spacing      = 8;
        int maxVisible   = 4;
        int visibleItems = min(maxVisible, (int)selectionOptions.size());
        int popupHeight  = (visibleItems * itemHeight) + ((visibleItems + 1) * spacing) + 40; // +40 for title

        int centerX = (targetSprite->width() - popupWidth) / 2;
        int centerY = (targetSprite->height() - popupHeight) / 2;

        // Draw popup background
        targetSprite->fillRoundRect(centerX, centerY, popupWidth, popupHeight, 4, 0x7BEF); // Light gray
        targetSprite->fillRoundRect(centerX + 3, centerY + 3, popupWidth - 6, popupHeight - 6, 3, TFT_BLACK);

        // Draw title
        targetSprite->setTextColor(TFT_WHITE);
        targetSprite->setTextDatum(lgfx::v1::textdatum::textdatum_t::top_center);
        targetSprite->setTextSize(0.7);
        targetSprite->drawString(adjustingItem->getTitle(), centerX + popupWidth / 2, centerY + 10);

        // Calculate scroll offset to keep selection visible
        int startIdx = 0;
        if (selectionHighlight >= maxVisible) {
            startIdx = selectionHighlight - maxVisible + 1;
        }

        // Draw option buttons
        int yPos = centerY + 40;
        for (int i = startIdx; i < min(startIdx + maxVisible, (int)selectionOptions.size()); i++) {
            bool isSelected = (i == selectionHighlight);
            bool isCurrent  = (selectionOptions[i].value == *selectionValuePtr);

            // Button colors
            uint16_t borderColor = isSelected ? TFT_WHITE : 0x7BEF;
            uint16_t bgColor     = TFT_BLACK;
            uint16_t textColor   = isCurrent ? TFT_CYAN : TFT_WHITE;

            int btnX     = centerX + spacing;
            int btnWidth = popupWidth - (spacing * 2);

            // Draw button
            targetSprite->fillRoundRect(btnX, yPos, btnWidth, itemHeight, 3, borderColor);
            targetSprite->fillRoundRect(btnX + 2, yPos + 2, btnWidth - 4, itemHeight - 4, 2, bgColor);

            // Draw label
            targetSprite->setTextColor(textColor, bgColor);
            targetSprite->setTextDatum(lgfx::v1::textdatum::textdatum_t::middle_center);
            targetSprite->setTextSize(0.9);
            targetSprite->drawString(selectionOptions[i].label, btnX + btnWidth / 2, yPos + itemHeight / 2);

            yPos += itemHeight + spacing;
        }
    }

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

    void sendButton(String name, int pushType)
    {
        cmdMessenger.sendCmdStart(kButtonChange);
        cmdMessenger.sendCmdArg(name);
        cmdMessenger.sendCmdArg(pushType);
        cmdMessenger.sendCmdEnd();
    }
};
