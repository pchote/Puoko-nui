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
    OBJECT_FOCUS,
    OBJECT_TARGET,
    OBJECT_BIAS
} PNFrameType;

typedef enum
{
    TRIGGER_SECONDS,
    TRIGGER_MILLISECONDS,
    TRIGGER_BIAS
} PNTriggerMode;

typedef enum
{
    OUTPUT_DIR,
    RUN_PREFIX,
    OBJECT_NAME,
    OBSERVERS,
    OBSERVATORY,
    TELESCOPE,
    INSTRUMENT,
    FILTER,
    EXPOSURE_TIME,
    SAVE_FRAMES,
    REDUCE_FRAMES,
    OBJECT_TYPE,
    RUN_NUMBER,

    BURST_ENABLED,
    BURST_COUNTDOWN,

    TIMER_MONITOR_LOGIC_OUT,
    TIMER_TRIGGER_MODE,
    TIMER_ALIGN_FIRST_EXPOSURE,
    TIMER_SERIAL_PORT,
    TIMER_BAUD_RATE,

    CAMERA_BINNING,
    CAMERA_READPORT_MODE,
    CAMERA_READSPEED_MODE,
    CAMERA_GAIN_MODE,
    CAMERA_TEMPERATURE,

    CAMERA_OVERSCAN_COLS,
    CAMERA_PLATESCALE,

    CAMERA_WINDOW_X,
    CAMERA_WINDOW_Y,
    CAMERA_WINDOW_WIDTH,
    CAMERA_WINDOW_HEIGHT,
    CAMERA_DISABLE_SHUTTER,
    CAMERA_FRAME_BUFFER_SIZE,

    PROEM_EXPOSURE_SHORTCUT,
    PROEM_EM_GAIN,
    PROEM_SHIFT_MODE,
    VALIDATE_TIMESTAMPS,

    FRAME_FLIP_X,
    FRAME_FLIP_Y,
    FRAME_TRANSPOSE,
    PREVIEW_RATE_LIMIT,

#if (defined _WIN32)
    MSYS_BASH_PATH,
#endif
    LIST_END // Dummy entry to workaround c++ not allowing list end with ','
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
