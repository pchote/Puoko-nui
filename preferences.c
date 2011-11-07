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

typedef enum { STRING, CHAR, INT } PNPrefDataType;
typedef struct
{
    PNPreferenceType key;
    PNPrefDataType type;
    union _value
    {
        char *s;
        char c;
        int i;
    } value;
    char *format;
} PNPreferenceStore;

/*
 * Preference definitions
 * Must be defined in the same order as the keys in preference.h
 */

PNPreferenceStore prefs[] =
{
    // Key, Default value, Output format
    {OUTPUT_DIR, STRING, .value.s = "/home/sullivan/", "OutputDir: %s\n"},
    {RUN_PREFIX, STRING, .value.s = "run", "RunPrefix: %s\n"},
    {OBJECT_NAME, STRING, .value.s = "ec20058", "ObjectName: %s\n"},
    {OBSERVERS, STRING, .value.s = "DJS, PC", "Observers: %s\n"},
    {OBSERVATORY, STRING, .value.s = "MJUO", "Observatory: %s\n"},
    {TELESCOPE, STRING, .value.s = "MJUO 1-meter", "Telescope: %s\n"},

    {EXPOSURE_TIME, CHAR, .value.c = 5, "ExposureTime: %hhu\n"},
    {SAVE_FRAMES, CHAR, .value.c = FALSE, "SaveFrames: %hhu\n"},
    {OBJECT_TYPE, CHAR, .value.c = OBJECT_TARGET, "ObjectType: %hhu\n"},
    {USE_TIMER_MONITORING, CHAR, .value.c = TRUE, "UseTimerMonitor: %hhu\n"},
    {TIMER_NOMONITOR_STARTUP_DELAY, CHAR, .value.c = 5, "TimerStartDelay: %hhu\n"},
    {TIMER_NOMONITOR_STOP_DELAY, CHAR, .value.c = 5, "TimerStopDelay: %hhu\n"},
    {SUPERPIXEL_SIZE, CHAR, .value.c = 2, "SuperpixelSize: %hhu\n"},
    {CAMERA_READOUT_MODE, CHAR, .value.c = 0, "CameraReadoutMode: %hhu\n"},

    {RUN_NUMBER, INT, .value.i = 0, "RunNumber: %d\n"},
    {CALIBRATION_DEFAULT_FRAMECOUNT, INT, .value.i = 30, "CalibrationTotalFrames: %d\n"},
    {CALIBRATION_REMAINING_FRAMECOUNT, INT, .value.i = 30, "CalibrationRemainingFrames: %d\n"},
    {CAMERA_TEMPERATURE, INT, .value.i = -5000, "CameraTemperature: %d\n"}

};
int pref_count = sizeof(prefs) / sizeof(prefs[0]);

static void save()
{
    FILE *fp = fopen(filename, "w");
    for (int i = 0; i < pref_count; i++)
        fprintf(fp, prefs[i].format, prefs[i].value);
    fclose(fp);
}

void pn_init_preferences(const char *path)
{
    filename = strdup(path);

    // Copy string values onto the heap
    for (int i = 0; i < pref_count; i++)
        if (prefs[i].type == STRING)
            prefs[i].value.s = strdup(prefs[i].value.s);

    FILE *fp = fopen(filename, "r");
    if (fp)
    {
        char linebuf[1024];
        while (fgets(linebuf, sizeof(linebuf)-1, fp) != NULL)
        {
            size_t compare = strcspn(linebuf, ":");
            if (compare == strlen(linebuf))
                continue; // Line is not in `key: value' format

            for (int i = 0; i < pref_count; i++)
            {
                if (strncmp(linebuf, prefs[i].format, compare))
                    continue;

                if (prefs[i].type == STRING)
                {
                    free(prefs[i].value.s);
                    // Trim whitespace and the newline from the end of the string
                    prefs[i].value.s = strndup(linebuf + compare + 2, strlen(linebuf) - compare - 3);
                }
                else
                    sscanf(linebuf, prefs[i].format, &prefs[i].value);
            }
        }

        fclose(fp);
    }

    // Force saving to false on startup
    prefs[SAVE_FRAMES].value.c = FALSE;

    // Init mutex
    pthread_mutex_init(&access_mutex, NULL);
    save();
}

void pn_free_preferences()
{
    pthread_mutex_destroy(&access_mutex);
    free(filename);
}

char *pn_preference_string(PNPreferenceType key)
{
    if (prefs[key].type == STRING)
    {
        pthread_mutex_lock(&access_mutex);
        char *ret = strdup(prefs[key].value.s);
        pthread_mutex_unlock(&access_mutex);
        return ret;
    }

    pn_log("ERROR: Attempting to access preference %d as a string", key);
    return strdup("Invalid key");
}

unsigned char pn_preference_char(PNPreferenceType key)
{
    if (prefs[key].type == CHAR)
    {
        pthread_mutex_lock(&access_mutex);
        unsigned char ret = prefs[key].value.c;
        pthread_mutex_unlock(&access_mutex);
        return ret;
    }

    pn_log("ERROR: Attempting to access preference %d as a char", key);
    return 0;
}

int pn_preference_int(PNPreferenceType key)
{
    if (prefs[key].type == INT)
    {
        pthread_mutex_lock(&access_mutex);
        int ret = prefs[key].value.i;
        pthread_mutex_unlock(&access_mutex);
        return ret;
    }

    pn_log("ERROR: Attempting to access preference %d as an int", key);
    return 0;
}

void pn_preference_increment_framecount()
{
    pthread_mutex_lock(&access_mutex);
    // Increment the run number
    prefs[RUN_NUMBER].value.i++;

    // Decrement the calibration frame count if applicable
    if (prefs[OBJECT_TYPE].value.c != OBJECT_TARGET &&
        prefs[CALIBRATION_REMAINING_FRAMECOUNT].value.i > 0)
        prefs[CALIBRATION_REMAINING_FRAMECOUNT].value.i--;

    save();
    pthread_mutex_unlock(&access_mutex);
}

unsigned char pn_preference_toggle_save()
{
    pthread_mutex_lock(&access_mutex);
    unsigned char ret = prefs[SAVE_FRAMES].value.c ^= TRUE;
    pthread_mutex_unlock(&access_mutex);
    return ret;
}

unsigned char pn_preference_allow_save()
{
    pthread_mutex_lock(&access_mutex);
    unsigned char ret = (prefs[OBJECT_TYPE].value.c == OBJECT_TARGET ||
                         prefs[CALIBRATION_REMAINING_FRAMECOUNT].value.i > 0);
    pthread_mutex_unlock(&access_mutex);
    return ret;
}

void pn_preference_set(PNPreferenceType key, void *val)
{
    pthread_mutex_lock(&access_mutex);
    switch (prefs[key].type)
    {
        case STRING: free(prefs[key].value.s); prefs[key].value.s = strdup(*((char **)val));
        case CHAR: prefs[key].value.c = *((char *)val);
        case INT: prefs[key].value.i = *((int *)val);
    }
    save();
    pthread_mutex_unlock(&access_mutex);
}

void pn_preference_set_char(PNPreferenceType key, unsigned char val)
{
    if (prefs[key].type != CHAR)
    {
        pn_log("ERROR: Attempting to set invalid char pref %d", key);
        return;
    }

    pn_preference_set(key, &val);
}

void pn_preference_set_string(PNPreferenceType key, const char *val)
{
    if (prefs[key].type != STRING)
    {
        pn_log("ERROR: Attempting to set invalid string pref %d", key);
        return;
    }

    pn_preference_set(key, &val);
}

void pn_preference_set_int(PNPreferenceType key, int val)
{
    if (prefs[key].type != INT)
    {
        pn_log("ERROR: Attempting to set invalid int pref %d", key);
        return;
    }

    pn_preference_set(key, &val);
}
