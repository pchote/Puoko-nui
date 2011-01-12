/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include "preferences.h"
#include <stdio.h>
#include <string.h>
#include "view.h"

void rangahau_load_preferences(RangahauPreferences *prefs, const char *path)
{
	FILE *fp = fopen(path, "r");
	if (!fp)
	{
		fprintf(stderr, "Could not open `%s`. Initialising with default settings\n", path);
		rangahau_set_preference_string(prefs->observatory, "MJUO");
		rangahau_set_preference_string(prefs->telescope, "MJUO 1-meter");
		rangahau_set_preference_string(prefs->observers, "DJS");
		prefs->object_type = OBJECT_TARGET;
		rangahau_set_preference_string(prefs->object_name, "ec20058");

		rangahau_set_preference_path(prefs->output_directory, "/home/sullivan/Desktop");
		rangahau_set_preference_string(prefs->run_prefix, "run");
		prefs->run_number = 0;

		prefs->exposure_time = 5;
		prefs->bin_size = 1;
	}
	else
	{	
		fread(prefs, sizeof(*prefs), 1, fp);
	}
}

void rangahau_save_preferences(RangahauPreferences *prefs, const char *path)
{
	FILE *fp = fopen(path, "w");
	fwrite(prefs, sizeof(*prefs), 1, fp);
	fclose(fp);
}

/* Set a preference string safely (avoid buffer overruns) */
void rangahau_set_preference_string(char *pref, const char *value)
{
	strncpy(pref, value, PREFERENCES_LENGTH);
	pref[PREFERENCES_LENGTH-1] = '\0';
}

void rangahau_set_preference_path(char *pref, const char *value)
{
	strncpy(pref, value, PATH_MAX);
	pref[PATH_MAX-1] = '\0';
}
