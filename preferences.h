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
typedef struct
{
	char output_directory[PATH_MAX];
	char run_prefix[PREFERENCES_LENGTH];
	int run_number;

	char object_type;
	char object_name[PREFERENCES_LENGTH];

	char observers[PREFERENCES_LENGTH];
	char observatory[PREFERENCES_LENGTH];
	char telescope[PREFERENCES_LENGTH];

	unsigned char exposure_time;
    int calibration_default_framecount;
    int calibration_remaining_framecount;

    char save_frames;
} PNPreferences;


typedef enum
{
	OBJECT_DARK,
	OBJECT_FLAT,
	OBJECT_TARGET
} PNFrameType;


void pn_load_preferences(PNPreferences *prefs, const char *path);
void pn_save_preferences(PNPreferences *prefs, const char *path);
void pn_set_preference_string(char *pref, const char *value);
void pn_set_preference_path(char *pref, const char *value);
#endif
