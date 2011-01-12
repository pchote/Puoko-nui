/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <limits.h>

#define PREFERENCES_LENGTH 128
typedef struct
{
	char output_directory[PATH_MAX];
	char run_prefix[PREFERENCES_LENGTH];
	int run_number;

	int object_type;
	char object_name[PREFERENCES_LENGTH];

	char observers[PREFERENCES_LENGTH];
	char observatory[PREFERENCES_LENGTH];
	char telescope[PREFERENCES_LENGTH];

	int exposure_time;
	int bin_size;
} RangahauPreferences;


typedef enum
{
	OBJECT_DARK,
	OBJECT_FLAT,
	OBJECT_TARGET
} RangahauObjectType;


void rangahau_load_preferences(RangahauPreferences *prefs, const char *path);
void rangahau_save_preferences(RangahauPreferences *prefs, const char *path);
void rangahau_set_preference_string(char *pref, const char *value);
void rangahau_set_preference_path(char *pref, const char *value);
#endif
