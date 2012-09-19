/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <limits.h>

#define PREFERENCES_LENGTH 128

typedef enum
{
    OBJECT_DARK,
    OBJECT_FLAT,
    OBJECT_TARGET
} PNFrameType;

typedef enum
{
    OUTPUT_DIR,
    RUN_PREFIX,
    OBJECT_NAME,
    OBSERVERS,
    OBSERVATORY,
    TELESCOPE,
    INSTRUMENT,
    EXPOSURE_TIME,
    SAVE_FRAMES,
    OBJECT_TYPE,
    RUN_NUMBER,
    CALIBRATION_COUNTDOWN,

    TIMER_MONITOR_LOGIC_OUT,

    CAMERA_PIXEL_SIZE,
    CAMERA_READPORT_MODE,
    CAMERA_READSPEED_MODE,
    CAMERA_GAIN_MODE,
    CAMERA_TEMPERATURE,

    CAMERA_OVERSCAN_ENABLED,
    CAMERA_OVERSCAN_SKIP_COLS,
    CAMERA_OVERSCAN_BIAS_COLS,
#if (defined _WIN32)
    MSYS_BASH_PATH
#endif
} PNPreferenceType;

void pn_init_preferences(const char *path);
void pn_free_preferences();
void pn_save_preferences();

char *pn_preference_string(PNPreferenceType key);
unsigned char pn_preference_char(PNPreferenceType key);
int pn_preference_int(PNPreferenceType key);

void pn_preference_increment_framecount();
unsigned char pn_preference_toggle_save();
unsigned char pn_preference_allow_save();

void pn_preference_set_char(PNPreferenceType key, unsigned char val);
void pn_preference_set_string(PNPreferenceType key, const char *val);
void pn_preference_set_int(PNPreferenceType key, int val);
#endif
