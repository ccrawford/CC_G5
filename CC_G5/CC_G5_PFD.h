#pragma once

#include "Arduino.h"
#include "G5Common.h"
#include "SpeedTrend.hpp"
#include "Sprites\backArrow.h"
#include "Sprites\setupIcon.h"
#include "Sprites\hsiIcon.h"

// #include "4inchLCDConfig.h"

#define RGB332(r, g, b) (((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6))
// #define SKY_COLOR   RGB332(100,149,237)  // cornflower blue-ish
#define SKY_COLOR           0x3c71ff  // cornflower blue-ish
#define GND_COLOR           TFT_BROWN // brown
#define DARK_GND_COLOR      0x905900
#define DARK_SKY_COLOR      0x1449f4
#define DARK_BG_TRANS_COLOR RGB332(0x0d, 0x0d, 0x0d)
#define LINE_COLOR          RGB332(255, 255, 255) // white

#define CX       240
#define CY       240
#define SCREEN_W 480
#define SCREEN_H 480

#define AP_BAR_HEIGHT     40
#define TOP_BAR_HEIGHT    40 // This is part of the attitude sprite space.
#define BOTTOM_BAR_HEIGHT 40
#define ATTITUDE_HEIGHT   400

#define ATTITUDE_WIDTH      480
#define ATTITUDE_COLOR_BITS 8

#define SPEED_COL_WIDTH    100
#define ALTITUDE_COL_WIDTH 130
#define CENTER_COL_WIDTH   250 // SCREEN_W - SPEED_COL_WIDTH - ALTITUDE_COL_WIDTH    // 250 screen width - speed col - altitude col
#define CENTER_COL_CENTER  140 // Center of center col is 125, but offset 15 to left.

#define TFT_BACKGROUND_COLOR TFT_BLACK

#define SPEED_ALIVE_SPEED 20

#define TEXT_BOX_HEIGHT 40
#define NAVSOURCE_GPS   1
#define NAVSOURCE_NAV   0


class CC_G5_PFD : public CC_G5_Base
{
    //// NESTED CLASS
    class PFDMenu : public G5MenuBase<CC_G5_PFD>
    {
    private:
        // PFD menu items
        class BackMenuItem : public MenuItemBase
        {
            PFDMenu *menu;

        public:
            BackMenuItem(PFDMenu *m) : menu(m) {}
            String getTitle() override { return "Back"; }
            String getDisplayValue() override { return ""; }
            int    getDisplayValueColor() override { return 0xFFFFFF; }
            void   onEncoderPress() override { menu->setActive(false); }
            void   onEncoderTurn(int delta) override {}

            // Icon support
            const uint16_t *getIcon() override { return BACKARROW_IMG_DATA; }
            int             getIconWidth() override { return BACKARROW_IMG_WIDTH; }
            int             getIconHeight() override { return BACKARROW_IMG_HEIGHT; }
        };

        class HeadingMenuItem : public MenuItemBase
        {
            PFDMenu *menu;

        public:
            HeadingMenuItem(PFDMenu *m) : menu(m) {}
            String getTitle() override { return "Heading"; }
            String getDisplayValue() override
            {
                char buffer[8];
                sprintf(buffer, "%03d°", g5State.headingBugAngle);
                return String(buffer);
            }
            int  getDisplayValueColor() override { return 0x07FF; } // Cyan
            void onEncoderPress() override { menu->enterAdjustmentMode(this); }
            void onEncoderTurn(int delta) override
            {
                menu->sendEncoder("encHeading", abs(delta), delta > 0 ? 0 : 2);
            }
        };

        class TrackMenuItem : public MenuItemBase
        {
            PFDMenu *menu;

        public:
            TrackMenuItem(PFDMenu *m) : menu(m) {}
            String getTitle() override { return "Track"; }
            String getDisplayValue() override
            {
                char buffer[8];
                sprintf(buffer, "%03d°", (int)g5State.groundTrack);
                return String(buffer);
            }
            int getDisplayValueColor() override { return 0xF81F; } // Magenta
            // Only show when we're in VOR or Localizer mode.
            bool isVisible() const override
            {
                return g5State.navSource != NAVSOURCE_GPS;
            }
            void onEncoderPress() override { menu->enterAdjustmentMode(this); }
            void onEncoderTurn(int delta) override
            {
                menu->sendEncoder("encTrack", abs(delta), delta > 0 ? 0 : 2);
            }
        };

        class AltitudeMenuItem : public MenuItemBase
        {
            PFDMenu *menu;

        public:
            AltitudeMenuItem(PFDMenu *m) : menu(m) {}
            String getTitle() override { return "Altitude"; }
            String getDisplayValue() override
            {
                char buffer[8];
                if (g5State.targetAltitude)
                    sprintf(buffer, "%d", g5State.targetAltitude);
                else
                    sprintf(buffer, "%s", "----");
                return String(buffer);
            }
            int  getDisplayValueColor() override { return 0x07FF; } // Cyan
            void onEncoderPress() override { menu->enterAdjustmentMode(this); }
            void onEncoderTurn(int delta) override
            {
                //   menu->parent->targetAltitude += delta * 100;
                //   if (menu->parent->targetAltitude < 0) menu->parent->targetAltitude = 0;
                menu->sendEncoder("encTargetAlt", abs(delta), delta > 0 ? 0 : 2);
            }
        };

        class SettingsMenuItem : public MenuItemBase
        {
            PFDMenu *menu;

        public:
            SettingsMenuItem(PFDMenu *m) : menu(m) {}
            String getTitle() override { return "Setup"; }
            String getDisplayValue() override { return ""; }
            int    getDisplayValueColor() override { return 0xFFFFFF; }
            void   onEncoderPress() override
            {
                menu->currentState = MenuState::SETTINGS_BROWSING;
            }
            void onEncoderTurn(int delta) override {}

            // Icon support
            const uint16_t *getIcon() override { return SETUPICON_IMG_DATA; }
            int             getIconWidth() override { return SETUPICON_IMG_WIDTH; }
            int             getIconHeight() override { return SETUPICON_IMG_HEIGHT; }
        };

    private:
        // Temporary menu item for settings adjustment
        class SettingMenuItem : public MenuItemBase
        {
            SettingDef *setting;

        public:
            SettingMenuItem(SettingDef *s) : setting(s) {}

            String getTitle() override { return setting->name; }

            String getDisplayValue() override
            {
                char buf[8];
                sprintf(buf, "%d", *setting->valuePtr);
                return String(buf);
            }

            int getDisplayValueColor() override { return 0xFFFF00; } // Yellow

            void onEncoderTurn(int delta) override
            {
                int newVal = *setting->valuePtr + delta;
                if (newVal < setting->minVal) newVal = setting->minVal;
                if (newVal > setting->maxVal) newVal = setting->maxVal;
                *setting->valuePtr = newVal;
            }

            void onEncoderPress() override {}
        };

        SettingMenuItem *tempSettingItem = nullptr;

    public:
        PFDMenu(CC_G5_PFD *p) : G5MenuBase<CC_G5_PFD>(p) {}

        void handleEncoder(int delta) override
        {
            if (currentState == MenuState::BROWSING) {
                scrollHighlight(delta);
            } else if (currentState == MenuState::SETTINGS_BROWSING) {
                // Scroll through settings list
                if (delta > 0)
                    parent->settingsMenu.scrollDown();
                else
                    parent->settingsMenu.scrollUp();
            } else if (currentState == MenuState::ADJUSTING && adjustingItem) {
                adjustingItem->onEncoderTurn(delta);
            }
        }

        void handleEncoderButton(bool pressed) override
        {
            if (!pressed) return;

            if (currentState == MenuState::BROWSING) {
                menuItems[currentHighlight]->onEncoderPress();
            } else if (currentState == MenuState::SETTINGS_BROWSING) {
                int idx = parent->settingsMenu.getScrollIndex();
                if (idx == 0) {
                    // Back item selected
                    currentState = MenuState::BROWSING;
                } else {
                    // Setting selected - create temp menu item and enter adjustment mode
                    SettingDef &setting = parent->settingsMenu.getSetting(idx - 1);
                    tempSettingItem     = new SettingMenuItem(&setting);
                    adjustingItem       = tempSettingItem;
                    currentState        = MenuState::ADJUSTING;
                }
            } else if (currentState == MenuState::ADJUSTING) {
                // Save and return to appropriate state
                saveSettings();
                if (tempSettingItem) {
                    delete tempSettingItem;
                    tempSettingItem = nullptr;
                    currentState    = MenuState::SETTINGS_BROWSING; // Return to settings list
                } else {
                    currentState = MenuState::BROWSING; // Return to main menu
                    setActive(false);
                }
                adjustingItem = nullptr;
            }
        }

        class DeviceMenuItem : public MenuItemBase
        {
            PFDMenu *menu;

        public:
            DeviceMenuItem(PFDMenu *m) : menu(m) {}
            String getTitle() override { return "HSI"; }
            String getDisplayValue() override { return ""; }
            int    getDisplayValueColor() override { return 0xFFFFFF; }
            void   onEncoderPress() override
            {
                menu->parent->saveState();
                g5Settings.deviceType = CUSTOM_HSI_DEVICE;
                saveSettings();
                lcd.fillScreen(TFT_BLACK);
                ESP.restart();
            }
            void onEncoderTurn(int delta) override {}

            // Icon support
            const uint16_t *getIcon() override { return HSIICON_IMG_DATA; }
            int             getIconWidth() override { return HSIICON_IMG_WIDTH; }
            int             getIconHeight() override { return HSIICON_IMG_HEIGHT; }
        };

        void createMenuItems() override
        {
            menuItems.push_back(std::make_unique<BackMenuItem>(this));
            menuItems.push_back(std::make_unique<HeadingMenuItem>(this));
            menuItems.push_back(std::make_unique<TrackMenuItem>(this));
            menuItems.push_back(std::make_unique<AltitudeMenuItem>(this));
            menuItems.push_back(std::make_unique<DeviceMenuItem>(this));
            menuItems.push_back(std::make_unique<SettingsMenuItem>(this));
        }

        LGFX_Sprite *getTargetSprite() override
        {
            extern LGFX_Sprite attitude;
            return &attitude;
        }

        void drawSettingsList()
        {
            auto targetSprite = getTargetSprite();
            if (!targetSprite) return;

            int listX = 50, listY = 50;
            int itemHeight   = 35;
            int listWidth    = targetSprite->width() - 100;
            int visibleItems = 8; // How many items to show at once

            // Draw background
            targetSprite->fillRoundRect(listX - 5, listY - 5, listWidth + 10, visibleItems * itemHeight + 10, 4,
                                        0x7BEF);
            targetSprite->fillRoundRect(listX, listY, listWidth, visibleItems * itemHeight, 3, 0x0000);

            int scrollIdx = parent->settingsMenu.getScrollIndex();
            int startIdx  = max(0, scrollIdx - 3); // Keep selected item roughly centered

            //   // Draw "Back" item first if at top
            //   if (startIdx == 0) {
            //       bool selected = (scrollIdx == 0);
            //       targetSprite->setTextColor(selected ? TFT_CYAN : TFT_WHITE, TFT_BLACK);
            //       targetSprite->setTextDatum(lgfx::v1::textdatum::textdatum_t::middle_left);
            //       targetSprite->setTextSize(0.8);
            //       targetSprite->drawString("< Back", listX + 10, listY + itemHeight / 2);
            //       startIdx = 1;
            //   }

            // Draw settings items
            int settingsCount = parent->settingsMenu.getSettingsCount();
            for (int i = startIdx; i < min(startIdx + visibleItems, settingsCount + 1); i++) {
                int  yPos     = listY + ((i - startIdx) * itemHeight);
                bool selected = (i == scrollIdx);

                if (i == 0) {
                    // Back item
                    targetSprite->setTextColor(selected ? TFT_CYAN : TFT_WHITE, TFT_BLACK);
                    targetSprite->setTextDatum(lgfx::v1::textdatum::textdatum_t::middle_left);
                    targetSprite->setTextSize(0.8);
                    targetSprite->drawString("< Back", listX + 10, yPos + itemHeight / 2);
                } else {
                    // Setting item
                    SettingDef &setting = parent->settingsMenu.getSetting(i - 1);

                    // Draw name
                    targetSprite->setTextColor(selected ? TFT_CYAN : TFT_WHITE, TFT_BLACK);
                    targetSprite->setTextDatum(lgfx::v1::textdatum::textdatum_t::middle_left);
                    targetSprite->setTextSize(0.8);
                    targetSprite->drawString(setting.name, listX + 10, yPos + itemHeight / 2);

                    // Draw value
                    char valStr[16];
                    sprintf(valStr, "%d", *setting.valuePtr);
                    targetSprite->setTextColor(selected ? TFT_YELLOW : 0x7BEF, TFT_BLACK);
                    targetSprite->setTextDatum(lgfx::v1::textdatum::textdatum_t::middle_right);
                    targetSprite->drawString(valStr, listX + listWidth - 10, yPos + itemHeight / 2);
                }
            }
        }
    };

public:
    CC_G5_PFD();
    void begin();
    void attach();
    void detach();
    void set(int16_t messageID, char *setPoint);
    void setPFD(int16_t messageID, char *setPoint);
    void saveState() override;   // calls CC_G5_Base::saveState() then saves PFD-specific fields
    bool restoreState();
    void update();
    // G5_Menu menu;
    PFDMenu              pfdMenu{this};
    PFDSettingsMenu      settingsMenu;
    SpeedTrendCalculator speedTrend;

private:
    bool    _initialised;
    uint8_t _pin1, _pin2, _pin3;

    int speedToY(float, float);
    int altToY(int, int);
    int headingToX(float, float);

    void setupSprites();

    void updateInputValues();

    void setVSpeeds(char *);

    void          drawAttitude();
    void          drawSpeedTape();
    void          drawSpeedPointers();
    void          drawSpeedTrend();
    void          drawAltTape();
    void          drawDensityAlt();
    void          drawHorizonMarker();
    void          drawGroundSpeed();
    void          drawKohlsman();
    void          drawAltTarget();
    void          drawBall();
    void          drawHeadingTape();
    void          blinkAP();
    unsigned long apBlinkEnd = 0;

    void drawCDIBar();
    void drawGlideSlope();

    void drawFlightDirector();

    void drawAp();
    void setFDBitmap();

    void drawMessageIndicator();

    void processMenu();
    void drawMenu();
    void drawAdjustmentPopup(); // Draw popup during normal updates
    void read_rp2040_data();

    unsigned long lastMFUpdate = 0;

public:
    // Sprite management for menu system

    // Altitude Alert State Machine
    enum AltAlertState {
        ALT_IDLE,        // Far from target (>1000')
        ALT_WITHIN_1000, // Within 1000' but not within 200'
        ALT_WITHIN_200,  // Within 200' but not captured
        ALT_CAPTURED,    // Within capture threshold (±100')
        ALT_DEVIATED     // Was captured, now outside ±200'
    };

    // Altitude alert constants
    static constexpr int ALT_ALERT_1000_THRESHOLD = 1000;
    static constexpr int ALT_ALERT_200_THRESHOLD  = 200;
    static constexpr int ALT_DEVIATION_THRESHOLD  = 200; // ±200' deviation band
    static constexpr int ALT_CAPTURE_THRESHOLD    = 100; // Must be within ±100' to initially capture

    // Alert state tracking
    AltAlertState altAlertState  = ALT_IDLE;
    unsigned long alertStartTime = 0;
    bool          altAlertActive = false;
    uint16_t      alertColor     = TFT_CYAN;

    // OLD LOCAL VARIABLES - now in g5State (keeping for reference)
    // float headingAngle    = 0.0f;
    // float rawHeadingAngle = 90.0f;
    // float bankAngle    = -178.0f;
    // float rawBankAngle = 10.0f;
    // float pitchAngle    = 4.0f;
    // float rawPitchAngle = 10.0f;
    // float airspeed    = 5;
    // float rawAirspeed = 60;
    // int altitude    = 300;
    // int rawAltitude = -100;
    // int   groundSpeed = 55;
    // float kohlsman    = 29.94;
    // int verticalSpeed    = -1000;
    // int rawVerticalSpeed = 500;
    // int           targetAltitude     = 500;
    // float groundTrack      = headingAngle + 5;
    // int   navCourseToSteer = 21;
    // float turnRate         = -3.0;
    // int   headingBugAngle  = 0;
    // float ballPos    = 0.0;
    // float rawBallPos = 0.8;
    // int   navSource = 1;
    // float navCourse = 256.0f;
    // int gsiNeedleValid = 1;
    // float rawGsiNeedle   = 0.0;
    // float gsiNeedle      = 10.0; // Vertical error. (A:NAV GSI:1,Number) +/-119
    // int cdiNeedleValid = 1;
    // int cdiToFrom      = 0;
    // float cdiOffset    = 10.0; // The deviation scale offset.
    // float rawCdiOffset = 10.0; // The deviation scale offset.
    // int   flightDirectorActive = 1;
    // float flightDirectorPitch  = 5.0;
    // float flightDirectorBank   = -10.0;
    // bool terminalModeActive = true;
    // int  navCDILabelIndex   = 0; // NavCDILabel. GPS:0, LOC1:1, VOR1:2, DME1:3, LOC2:4, VOR2:5, DME2:6, Blank:7
    // int  gpsApproachType    = 0; // gps approach approach type values. 99: none active.
    // int oat = 30;
    // int cdiScaleLabel = 1;
    // // Autopilot
    // int apActive      = 0;
    // int apVMode       = 0;
    // int apLMode       = 0;
    // int apVArmedMode  = 0;
    // int apLArmedMode  = 0;
    // int apYawDamper   = 0;
    // int apTargetSpeed = 0;
    // int apAltCaptured = 0; // THIS IS NOT THE TARGET ALT. it's the displayed captured altitude (nearest 10')
    // int apTargetVS    = 0;
};
