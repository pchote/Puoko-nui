/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <stdio.h>
#include <string.h>
#include "preferences.h"
#include "view.h"

void pn_load_preferences(PNPreferences *prefs, const char *path)
{
	FILE *fp = fopen(path, "r");
	if (!fp)
	{
		fprintf(stderr, "Could not open `%s`. Initialising with default settings\n", path);
		pn_set_preference_string(prefs->observatory, "MJUO");
		pn_set_preference_string(prefs->telescope, "MJUO 1-meter");
		pn_set_preference_string(prefs->observers, "DJS, PC");
		prefs->object_type = OBJECT_TARGET;
		pn_set_preference_string(prefs->object_name, "ec20058");

		pn_set_preference_path(prefs->output_directory, "/home/sullivan/Desktop");
		pn_set_preference_string(prefs->run_prefix, "run");
		prefs->run_number = 0;

		prefs->exposure_time = 5;

        prefs->calibration_default_framecount = 30;
        prefs->calibration_remaining_framecount = 30;
	}
	else
	{	
		fread(prefs, sizeof(*prefs), 1, fp);
	}
}

void pn_save_preferences(PNPreferences *prefs, const char *path)
{
	FILE *fp = fopen(path, "w");
	fwrite(prefs, sizeof(*prefs), 1, fp);
	fclose(fp);
}

/* Set a preference string safely (avoid buffer overruns) */
void pn_set_preference_string(char *pref, const char *value)
{
	strncpy(pref, value, PREFERENCES_LENGTH);
	pref[PREFERENCES_LENGTH-1] = '\0';
}

void pn_set_preference_path(char *pref, const char *value)
{
	strncpy(pref, value, PATH_MAX);
	pref[PATH_MAX-1] = '\0';
}
