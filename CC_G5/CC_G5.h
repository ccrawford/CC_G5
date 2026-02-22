#pragma once

#include "Arduino.h"
// #include "G5_Menu.h"
#include "G5Common.h"
#include "Sprites\backArrow.h"
#include "Sprites\pfdIcon.h"

// Pin definitions for ESP32
//  #define I2C_SDA_PIN 15      // SDA pin (GPIO15)
//  #define I2C_SCL_PIN 7       // SCL pin (GPIO7)
//  #define INT_PIN 16          // Interrupt pin from RP2040 (GPIO16)
//  #define RP2040_ADDR 0x08    // RP2040 I2C slave address
#define TFT_BACKGROUND_COLOR TFT_BLACK

#define TEXT_BOX_HEIGHT      40
#define COMPASS_OUTER_RADIUS 160
#define COMPASS_INNER_RADIUS 90
#define CUR_HEADING_Y_OFFSET 26
#define NAVSOURCE_GPS        1
#define NAVSOURCE_NAV        0

// Was 188

class CC_G5_HSI : public CC_G5_Base
{

    // NESTED MENU CLASS
    class HSIMenu : public G5MenuBase<CC_G5_HSI>
    {
    private:
        // HSI menu items
        class BackMenuItem : public MenuItemBase
        {
            HSIMenu *menu;

        public:
            BackMenuItem(HSIMenu *m) : menu(m) {}
            String getTitle() override { return "Back"; }
            String getDisplayValue() override { return ""; }
            int    getDisplayValueColor() override { return 0xFFFFFF; }
            void   onEncoderPress() override { menu->setActive(false); }
            void   onEncoderTurn(int delta) override {}

            // Icon support
            const uint16_t* getIcon() override { return BACKARROW_IMG_DATA; }
            int getIconWidth() override { return BACKARROW_IMG_WIDTH; }
            int getIconHeight() override { return BACKARROW_IMG_HEIGHT; }
        };

        class HeadingMenuItem : public MenuItemBase
        {
            HSIMenu *menu;

        public:
            HeadingMenuItem(HSIMenu *m) : menu(m) {}
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

        class CourseMenuItem : public MenuItemBase
        {
            HSIMenu *menu;

        public:
            CourseMenuItem(HSIMenu *m) : menu(m) {}
            String getTitle() override { return "LOC Crse"; }
            String getDisplayValue() override
            {
                char buffer[8];
                sprintf(buffer, "%03d°", (int)g5State.obsAngle);
                return String(buffer);
            }
            int  getDisplayValueColor() override { return TFT_GREEN; } 
            // Only show when we're in VOR or Localizer mode.
            bool isVisible() const override {
                return g5State.navSource != NAVSOURCE_GPS;
            }
            void onEncoderPress() override { menu->enterAdjustmentMode(this); }
            void onEncoderTurn(int delta) override
            {
                menu->sendEncoder("encCourse", abs(delta), delta > 0 ? 0 : 2);
            }
        };

        class OBSCourseMenuItem : public MenuItemBase
        {
            HSIMenu *menu;

        public:
            OBSCourseMenuItem(HSIMenu *m) : menu(m) {}
            String getTitle() override { return "OBS Crse"; }
            String getDisplayValue() override
            {
                char buffer[8];
                sprintf(buffer, "%03d°", g5State.cdiDirection);  // This may vary by aircraft?
                return String(buffer);
            }
            // Only show if we've got OBS active.
            bool isVisible() const override {
                return g5State.obsModeOn && g5State.navSource == NAVSOURCE_GPS;
            }
            int  getDisplayValueColor() override { return TFT_MAGENTA; } 
            void onEncoderPress() override { menu->enterAdjustmentMode(this); }
            void onEncoderTurn(int delta) override
            {
                menu->sendEncoder("encCourse", abs(delta), delta > 0 ? 0 : 2);
            }
        };

        class Bearing1MenuItem : public MenuItemBase
        {
            HSIMenu *menu;

        public:
            Bearing1MenuItem(HSIMenu *m) : menu(m) {}
            String getTitle() override { return "Bearing 1"; }
            String getDisplayValue() override
            {
                return String(getBearingSourceName(g5Settings.bearingPointer1Source));
            }
            int  getDisplayValueColor() override { return TFT_CYAN; }
            void onEncoderPress() override
            {
                std::vector<SelectionOption> options = {
                    {"Off", 0},
                    {"GPS", 1},
                    {"VLOC1", 2},
                    {"VLOC2", 3},
                    {"ADF", 4}
                };
                menu->enterSelectionMode(this, options, (int *)&g5Settings.bearingPointer1Source);
            }
            void onEncoderTurn(int delta) override {}
        };

        class Bearing2MenuItem : public MenuItemBase
        {
            HSIMenu *menu;

        public:
            Bearing2MenuItem(HSIMenu *m) : menu(m) {}
            String getTitle() override { return "Bearing 2"; }
            String getDisplayValue() override
            {
                return String(getBearingSourceName(g5Settings.bearingPointer2Source));
            }
            int  getDisplayValueColor() override { return TFT_CYAN; }
            void onEncoderPress() override
            {
                std::vector<SelectionOption> options = {
                    {"Off", 0},
                    {"GPS", 1},
                    {"VLOC1", 2},
                    {"VLOC2", 3},
                    {"ADF", 4},
                };
                menu->enterSelectionMode(this, options, (int *)&g5Settings.bearingPointer2Source);
            }
            void onEncoderTurn(int delta) override {}
        };

        class SetupMenuItem : public MenuItemBase
        {
            HSIMenu *menu;

        public:
            SetupMenuItem(HSIMenu *m) : menu(m) {}
            String getTitle() override { return "Setup"; }
            String getDisplayValue() override
            {
                return "";
            }
            int  getDisplayValueColor() override { return 0xFFFFFF; }
            void onEncoderPress() override {} // need to create the popup selector
            void onEncoderTurn(int delta) override
            {
            }
        };

        class DeviceMenuItem : public MenuItemBase
        {
            HSIMenu *menu;

        public:
            DeviceMenuItem(HSIMenu *m) : menu(m) {}
            String getTitle() override { return "PFD"; }
            String getDisplayValue() override { return ""; }
            int    getDisplayValueColor() override { return 0xFFFFFF; }
            void   onEncoderPress() override { 
                menu->parent->saveState();
                g5Settings.deviceType = CUSTOM_PFD_DEVICE; 
                saveSettings();
                lcd.fillScreen(TFT_BLACK);
                ESP.restart();
            }
            void   onEncoderTurn(int delta) override {}

            // Icon support
            const uint16_t* getIcon() override { return PFDICON_IMG_DATA; }
            int getIconWidth() override { return PFDICON_IMG_WIDTH; }
            int getIconHeight() override { return PFDICON_IMG_HEIGHT; }
        };

    public:
        HSIMenu(CC_G5_HSI *p) : G5MenuBase<CC_G5_HSI>(p) {}

        void createMenuItems() override
        {
            menuItems.push_back(std::make_unique<BackMenuItem>(this));
            menuItems.push_back(std::make_unique<HeadingMenuItem>(this));
            menuItems.push_back(std::make_unique<CourseMenuItem>(this));
            menuItems.push_back(std::make_unique<OBSCourseMenuItem>(this));
            menuItems.push_back(std::make_unique<Bearing1MenuItem>(this));
            menuItems.push_back(std::make_unique<Bearing2MenuItem>(this));
            menuItems.push_back(std::make_unique<DeviceMenuItem>(this));
        }

        LGFX_Sprite *getTargetSprite() override
        {
            extern LGFX_Sprite compass;
            return &compass;
        }
    };

public:
    CC_G5_HSI();
    void begin();
    void attach();
    void detach();
    void set(int16_t messageID, char *setPoint);
    void saveState() override;   // calls CC_G5_Base::saveState() then saves HSI-specific fields
    bool restoreState();

    void setHSI(int16_t messageId, char* setPoint);

    void update();
    // G5_Menu menu;
    HSIMenu hsiMenu{this};



private:
    bool    _initialised;

    void updateCommon();
    void updateGps();
    void updateNav();

    void updateInputValues();

    void setupSprites();
    void drawCompass();
    void drawPlaneIcon();
    void drawCDIPointer();
    void drawCurrentTrack();
    void drawCurrentHeading();
    void drawCDIBar();
    void drawWPTAlert();
    void drawGlideSlope();
    void drawHeadingBug();
    void drawHeadingBugValue();
    void drawGroundSpeed();
    void drawDesiredTrack();
    void drawVORCourseBox();

    void drawBearingPointer1();
    void drawBearingPointer2();

    void drawDistNextWaypoint();
    void drawDeviationScale();
    void drawCompassOuterMarkers();
    void drawCDISource();
    void drawNavCDILabel();
    void drawCDIScaleLabel(); // ENR, TERM, etc on right side of arrow.
    void drawRadioNavApproachType();

    void drawWind();

    void processMenu();

    void read_rp2040_data();

    void setNavSource();

public:
    // Sprite management for menu system
    void setupCompassSprites();

    int compassCenterX;
    int compassCenterY;

    char bearingPointer1Source[10];

    // Local calculated bearing pointer angles (computed from g5State sources)
    float getBearingPointerAngle(uint8_t source);
    float bearingPointer1Angle    = 10.0;
    float rawBearingPointer1Angle = 10.0;
    float bearingPointer2Angle    = 200.0;
    float rawBearingPointer2Angle = 350.0;

    // OLD LOCAL VARIABLES - now in g5State (keeping for reference)
    // float headingAngle    = 0.0f;
    // float rawHeadingAngle = 90.0f;
    // int cdiDirection   = 0;
    // int navSource      = 1; // 1-GPS, 0-VOR
    // int cdiNeedleValid = 1;
    // float cdiOffset    = 100.0; // The deviation scale offset.
    // float rawCdiOffset = 0.0;   // The deviation scale offset.
    // int   cdiToFrom          = 2; // 0 off, 1 To, 2 From
    // int   headingBugAngle    = 0;
    // int   groundSpeed        = 0;
    // float distNextWaypoint   = 0.1;
    // int   groundTrack        = 330;
    // int   desiredTrackValid  = 1;
    // float desiredTrack       = 200.0f;
    // int   dtkIsValid         = 1;
    // bool  terminalModeActive = true;
    // int   navCDILabelIndex   = 0; // NavCDILabel. GPS:0, LOC1:1, VOR1:2, DME1:3, LOC2:4, VOR2:5, DME2:6, Blank:7
    // int   gpsApproachType    = 0; // gps approach approach type values. 19: none active.
    // 0 = None 1 = GPS 2 = VOR 3 = NDB 4 = ILS 5 = Localizer 6 = SDF 7 = LDA 8 = VOR/ DME 9 = NDB/ DME 10 = RNAV 11 = Backcourse
    // int   obsModeOn = 0; // 1 on, 0 off
    // float obsAngle  = 0; // Obs direction.
    // int cdiScaleLabel = 1;
    // float bearingAngleGPS         = 10.0f;
    // float bearingAngleVLOC1       = 20.0f;
    // int   vloc1Type               = 1; // Bearing label. LOC:1, VOR:2, DME:3, ADF:4, Detuned:7
    // float bearingAngleVLOC2       = 30.0f;
    // float bearingAngleADF         = 15.0f;
    // int   vloc2Type               = 7; // Bearing label. LOC:1, VOR:2, DME:3, Detuned:7
    // bool adfValid = true;
    // int   gsiNeedleValid = 1;
    // float gsiNeedle      = -100.0; // Vertical error. (A:NAV GSI:1,Number) +/-119
    // float rawGsiNeedle   = 0.0;    // Vertical error. (A:NAV GSI:1,Number) +/-119
    // float windSpeed    = 5.0f;
    // float rawWindSpeed = 1.5;
    // float windDir    = 20.0;
    // float rawWindDir = 200;
};