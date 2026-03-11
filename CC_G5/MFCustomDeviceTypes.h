#pragma once

// Device type enumeration
enum {
    CUSTOM_PFD_DEVICE        = 0,
    CUSTOM_HSI_DEVICE        = 1,
    CUSTOM_SWITCHABLE_DEVICE = 2  // Runtime-switchable via saved settings
};

// Message ID routing boundaries.
// IDs below MSG_HSI_MIN are common (routed to setCommon on the active device).
// IDs in [MSG_HSI_MIN, MSG_PFD_MIN)  are HSI-specific  (routed to setHSI).
// IDs in [MSG_PFD_MIN, MSG_ISIS_MIN) are PFD-specific  (routed to setPFD).
#define MSG_HSI_MIN  30
#define MSG_PFD_MIN  60
