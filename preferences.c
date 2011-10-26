/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "preferences.h"
#include "common.h"

static char *filename;
static pthread_mutex_t access_mutex;

typedef struct
{
	char *output_directory;
	char *run_prefix;
	char *object_name;

	char *observers;
	char *observatory;
	char *telescope;

	unsigned char exposure_time;
    unsigned char save_frames; // set to FALSE on every startup
    unsigned char use_timer_monitoring;
    unsigned char timer_nomonitor_startup_delay;
    unsigned char timer_nomonitor_stop_delay;
	PNFrameType object_type;

    int calibration_default_framecount;
    int calibration_remaining_framecount;
	int run_number;
} PNPreferences;

static PNPreferences prefs;

static void save()
{
    FILE *fp = fopen(filename, "w");
    fprintf(fp, "OutputDir: %s\n", prefs.output_directory);
    fprintf(fp, "RunPrefix: %s\n", prefs.run_prefix);
    fprintf(fp, "ObjectName: %s\n", prefs.object_name);
    fprintf(fp, "Observers: %s\n", prefs.observers);
    fprintf(fp, "Observatory: %s\n", prefs.observatory);
    fprintf(fp, "Telescope: %s\n", prefs.telescope);
    fprintf(fp, "ExposureTime: %d\n", prefs.exposure_time);
    fprintf(fp, "UseTimerMonitor: %d\n", prefs.use_timer_monitoring);
    fprintf(fp, "TimerStartDelay: %d\n", prefs.timer_nomonitor_startup_delay);
    fprintf(fp, "TimerStopDelay: %d\n", prefs.timer_nomonitor_stop_delay);
    fprintf(fp, "ObjectType: %d\n", prefs.object_type);
    fprintf(fp, "CalibrationTotalFrames: %d\n", prefs.calibration_default_framecount);
    fprintf(fp, "CalibrationRemainingFrames: %d\n", prefs.calibration_remaining_framecount);
    fprintf(fp, "RunNumber: %d\n", prefs.run_number);
    fclose(fp);
}

void pn_init_preferences(const char *path)
{
    filename = strdup(path);

    // Set defaults
    prefs.observatory = strdup("MJUO");
    prefs.telescope = strdup("MJUO 1-meter");
    prefs.observers = strdup("DJS, PC");
    prefs.object_type = OBJECT_TARGET;
    prefs.object_name = strdup("ec20058");

    prefs.output_directory = strdup("/home/sullivan/");
    prefs.run_prefix = strdup("run");
    prefs.run_number = 0;

    prefs.exposure_time = 5;

    prefs.calibration_default_framecount = 30;
    prefs.calibration_remaining_framecount = 30;
    prefs.save_frames = FALSE;

    prefs.use_timer_monitoring = TRUE;
    prefs.timer_nomonitor_startup_delay = 5;
    prefs.timer_nomonitor_stop_delay = 5;

    FILE *fp = fopen(filename, "r");

	if (fp)
    {
        char linebuf[1024];
        while (fgets(linebuf, sizeof(linebuf)-1, fp) != NULL)
        {
            if (!strncmp(linebuf,"OutputDir:", 10))
                prefs.output_directory = strndup(linebuf + 11, strlen(linebuf) - 12);
            else if (!strncmp(linebuf, "RunPrefix:", 10))
                prefs.run_prefix = strndup(linebuf + 11, strlen(linebuf) - 12);
            else if (!strncmp(linebuf, "ObjectName:", 11))
                prefs.object_name = strndup(linebuf + 12, strlen(linebuf) - 13);
            else if (!strncmp(linebuf, "Observers:", 10))
                prefs.observers = strndup(linebuf + 11, strlen(linebuf) - 12);
            else if (!strncmp(linebuf, "Observatory:", 12))
                prefs.observatory = strndup(linebuf + 13, strlen(linebuf) - 14);
            else if (!strncmp(linebuf, "Telescope:", 10))
                prefs.telescope = strndup(linebuf + 11, strlen(linebuf) - 12);
            else if (!strncmp(linebuf, "ExposureTime:", 13))
                sscanf(linebuf, "ExposureTime: %hhu\n", &prefs.exposure_time);
            else if (!strncmp(linebuf, "UseTimerMonitor:", 16))
                sscanf(linebuf, "UseTimerMonitor: %hhu\n", &prefs.use_timer_monitoring);
            else if (!strncmp(linebuf, "TimerStartDelay:", 16))
                sscanf(linebuf, "TimerStartDelay: %hhu\n", &prefs.timer_nomonitor_startup_delay);
            else if (!strncmp(linebuf, "TimerStopDelay:", 15))
                sscanf(linebuf, "TimerStopDelay: %hhu\n", &prefs.timer_nomonitor_stop_delay);
            else if (!strncmp(linebuf, "ObjectType:", 11))
                sscanf(linebuf, "ObjectType: %hhu\n", (unsigned char *)&prefs.object_type);
            else if (!strncmp(linebuf, "CalibrationTotalFrames:", 23))
                sscanf(linebuf, "CalibrationTotalFrames: %d\n", &prefs.calibration_default_framecount);
            else if (!strncmp(linebuf, "CalibrationRemainingFrames:", 27))
                sscanf(linebuf, "CalibrationRemainingFrames: %d\n", &prefs.calibration_remaining_framecount);
            else if (!strncmp(linebuf, "RunNumber:", 10))
                sscanf(linebuf, "RunNumber: %d\n", &prefs.run_number);
        }

        fclose(fp);
    }

    // Init mutex
    pthread_mutex_init(&access_mutex, NULL);
    save();
}

void pn_free_preferences()
{
    pthread_mutex_destroy(&access_mutex);
    free(filename);
}

char *pn_preference_string(PNPreferenceString key)
{
    char *val, *ret;
    pthread_mutex_lock(&access_mutex);
    switch (key)
    {
        case OUTPUT_DIR: val = prefs.output_directory; break;
        case RUN_PREFIX: val = prefs.run_prefix; break;
        case OBJECT_NAME:
            switch (prefs.object_type)
            {
                case OBJECT_DARK:
                    val = "DARK";
                    break;
                case OBJECT_FLAT:
                    val = "FLAT";
                    break;
                default:
                case OBJECT_TARGET:
                    val = prefs.object_name;
                    break;
            }
        break;
        case OBSERVERS: val = prefs.observers; break;
        case OBSERVATORY: val = prefs.observatory; break;
        case TELESCOPE: val = prefs.telescope; break;
        default: val = "Invalid key"; break;
    }
    ret = strdup(val);
    pthread_mutex_unlock(&access_mutex);

    return ret;
}

unsigned char pn_preference_char(PNPreferenceChar key)
{
    char val;
    pthread_mutex_lock(&access_mutex);
    switch (key)
    {
        case EXPOSURE_TIME: val = prefs.exposure_time; break;
        case SAVE_FRAMES: val = prefs.save_frames; break;
        case OBJECT_TYPE: val = prefs.object_type; break;
        case USE_TIMER_MONITORING: val = prefs.use_timer_monitoring; break;
        case TIMER_NOMONITOR_STARTUP_DELAY: val = prefs.timer_nomonitor_startup_delay; break;
        case TIMER_NOMONITOR_STOP_DELAY: val = prefs.timer_nomonitor_stop_delay; break;
        default: val = 0;
    }
    pthread_mutex_unlock(&access_mutex);

    return val;
}

int pn_preference_int(PNPreferenceInt key)
{
    int val;
    pthread_mutex_lock(&access_mutex);
    switch (key)
    {
        case RUN_NUMBER: val = prefs.run_number; break;
        case CALIBRATION_DEFAULT_FRAMECOUNT: val = prefs.calibration_default_framecount; break;
        case CALIBRATION_REMAINING_FRAMECOUNT: val = prefs.calibration_remaining_framecount; break;
        default: val = 0;
    }
    pthread_mutex_unlock(&access_mutex);

    return val;
}

void pn_preference_increment_framecount()
{
    pthread_mutex_lock(&access_mutex);

    // Incrememnt the run number
    prefs.run_number++;

    // Decrement the calibration frame count if applicable
    if (prefs.object_type != OBJECT_TARGET && prefs.calibration_remaining_framecount > 0)
        --prefs.calibration_remaining_framecount;

    save();
    pthread_mutex_unlock(&access_mutex);
}

unsigned char pn_preference_toggle_save()
{
    pthread_mutex_lock(&access_mutex);
    unsigned char ret = prefs.save_frames ^= 1;
    save();
    pthread_mutex_unlock(&access_mutex);
    return ret;
}

unsigned char pn_preference_allow_save()
{
    pthread_mutex_lock(&access_mutex);
    unsigned char ret = prefs.object_type == OBJECT_TARGET || prefs.calibration_remaining_framecount > 0;
    pthread_mutex_unlock(&access_mutex);

    return ret;
}

void pn_preference_set_char(PNPreferenceChar key, unsigned char val)
{
    pthread_mutex_lock(&access_mutex);
    switch (key)
    {
        case EXPOSURE_TIME: prefs.exposure_time = val; break;
        case SAVE_FRAMES: prefs.save_frames = val; break;
        case OBJECT_TYPE: prefs.object_type = val; break;
        case USE_TIMER_MONITORING: prefs.use_timer_monitoring = val; break;
        case TIMER_NOMONITOR_STARTUP_DELAY: prefs.timer_nomonitor_startup_delay = val; break;
        case TIMER_NOMONITOR_STOP_DELAY: prefs.timer_nomonitor_stop_delay = val; break;
    }
    save();
    pthread_mutex_unlock(&access_mutex);
}

void pn_preference_set_string(PNPreferenceString key, const char *val)
{
    pthread_mutex_lock(&access_mutex);
    switch (key)
    {
        case OUTPUT_DIR: free(prefs.output_directory); prefs.output_directory = strdup(val); break;
        case RUN_PREFIX: free(prefs.run_prefix); prefs.run_prefix = strdup(val); break;
        case OBJECT_NAME: free(prefs.object_name); prefs.object_name = strdup(val); break;
        case OBSERVERS: free(prefs.observers); prefs.observers = strdup(val); break;
        case OBSERVATORY: free(prefs.observatory); prefs.observatory = strdup(val); break;
        case TELESCOPE: free(prefs.telescope); prefs.telescope = strdup(val); break;
    }
    save();
    pthread_mutex_unlock(&access_mutex);
}

void pn_preference_set_int(PNPreferenceInt key, int val)
{
    pthread_mutex_lock(&access_mutex);
    switch (key)
    {
        case RUN_NUMBER: prefs.run_number = val; break;
        case CALIBRATION_DEFAULT_FRAMECOUNT: prefs.calibration_default_framecount = val; break;
        case CALIBRATION_REMAINING_FRAMECOUNT: prefs.calibration_remaining_framecount = val; break;
    }
    save();
    pthread_mutex_unlock(&access_mutex);
}
