/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <limits.h>

#ifndef PREFERENCES_H
#define PREFERENCES_H

#define PREFERENCES_LENGTH 128

typedef enum
{
    OBJECT_DARK,
    OBJECT_FLAT,
    OBJECT_TARGET
} PNFrameType;

typedef enum
{
    OUTPUT_DIR = 0,
    RUN_PREFIX = 1,
    OBJECT_NAME = 2,
    OBSERVERS = 3,
    OBSERVATORY = 4,
    TELESCOPE = 5,
} PNPreferenceString;

typedef enum
{
    EXPOSURE_TIME = 6,
    SAVE_FRAMES = 7,
    OBJECT_TYPE = 8,
    USE_TIMER_MONITORING = 9,
    TIMER_NOMONITOR_STARTUP_DELAY = 10,
    TIMER_NOMONITOR_STOP_DELAY = 11,
    SUPERPIXEL_SIZE = 15
} PNPreferenceChar;

typedef enum
{
    RUN_NUMBER = 12,
    CALIBRATION_DEFAULT_FRAMECOUNT = 13,
    CALIBRATION_REMAINING_FRAMECOUNT = 14
} PNPreferenceInt;

void pn_init_preferences(const char *path);
void pn_free_preferences();
void pn_save_preferences();

char *pn_preference_string(PNPreferenceString key);
unsigned char pn_preference_char(PNPreferenceChar key);
int pn_preference_int(PNPreferenceInt key);

void pn_preference_increment_framecount();
unsigned char pn_preference_toggle_save();
unsigned char pn_preference_allow_save();

void pn_preference_set_char(PNPreferenceChar key, unsigned char val);
void pn_preference_set_string(PNPreferenceString key, const char *val);
void pn_preference_set_int(PNPreferenceInt key, int val);
#endif
