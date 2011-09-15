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
    OUTPUT_DIR,
    RUN_PREFIX,
    OBJECT_NAME,
    OBSERVERS,
    OBSERVATORY,
    TELESCOPE,
} PNPreferenceString;

typedef enum
{
    EXPOSURE_TIME,
    SAVE_FRAMES,
    OBJECT_TYPE,
    USE_TIMER_MONITORING,
    TIMER_NOMONITOR_STARTUP_DELAY,
    TIMER_NOMONITOR_STOP_DELAY,
} PNPreferenceChar;

typedef enum
{
    RUN_NUMBER,
    CALIBRATION_DEFAULT_FRAMECOUNT,
    CALIBRATION_REMAINING_FRAMECOUNT
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
#endif
