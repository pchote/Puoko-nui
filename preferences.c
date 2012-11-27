/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "preferences.h"
#include "platform.h"
#include "main.h"

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
    char *name;
    char *format;
} PNPreferenceStore;

/*
 * Preference definitions
 * Must be defined in the same order as the keys in preference.h
 */

PNPreferenceStore prefs[] =
{
    // Key, Default value, Output format
    {OUTPUT_DIR,  STRING, .value.s = "",                "OutputDir"},
    {RUN_PREFIX,  STRING, .value.s = "run",             "RunPrefix"},
    {OBJECT_NAME, STRING, .value.s = "",                "ObjectName"},
    {OBSERVERS,   STRING, .value.s = "",                "Observers"},
    {OBSERVATORY, STRING, .value.s = "",                "Observatory"},
    {TELESCOPE,   STRING, .value.s = "",                "Telescope"},
    {INSTRUMENT,  STRING, .value.s = "Puoko-nui",       "Instrument"},
    {FILTER,      STRING, .value.s = "",                "Filter"},

    {EXPOSURE_TIME,             INT,  .value.c = 5,     "ExposureTime"},
    {SAVE_FRAMES,               CHAR, .value.c = false, "SaveFrames"},
    {REDUCE_FRAMES,             CHAR, .value.c = false, "ReduceFrames"},
    {OBJECT_TYPE,               CHAR, .value.c = OBJECT_TARGET, "ObjectType"},
    {RUN_NUMBER,                INT,  .value.i = 0,     "RunNumber"},
    {CALIBRATION_COUNTDOWN,     INT, .value.i = 30,     "CalibrationRemainingFrames"},

    {TIMER_MONITOR_LOGIC_OUT,   CHAR, .value.c = true,  "TimerMonitorLogicOut"},
    {TIMER_HIGHRES_TIMING,      CHAR, .value.c = 0,     "TimerHighResolutionTiming"},
    {TIMER_BAUD_RATE,           INT,  .value.i = 9600,  "TimerBaudRate"},

    {CAMERA_BINNING,            CHAR, .value.c = 1,     "CameraBinning"},
    {CAMERA_READPORT_MODE,      CHAR, .value.c = 0,     "CameraReadoutPortMode"},
    {CAMERA_READSPEED_MODE,     CHAR, .value.c = 0,     "CameraReadoutSpeedMode"},
    {CAMERA_GAIN_MODE,          CHAR, .value.c = 0,     "CameraGainMode"},
    {CAMERA_TEMPERATURE,        INT,  .value.i = -5000, "CameraTemperature"},

    {CAMERA_OVERSCAN_COLS,      CHAR, .value.c = 0,     "CameraOverscanColumns"},
    {CAMERA_PLATESCALE,       STRING, .value.s = "0.33","CameraPlatescale"},

    {CAMERA_WINDOW_X,           INT,  .value.i = 0,     "CameraWindowX"},
    {CAMERA_WINDOW_Y,           INT,  .value.i = 0,     "CameraWindowY"},
    {CAMERA_WINDOW_WIDTH,       INT,  .value.i = 1024,  "CameraWindowWidth"},
    {CAMERA_WINDOW_HEIGHT,      INT,  .value.i = 1024,  "CameraWindowHeight"},
    {CAMERA_FRAME_BUFFER_SIZE,  INT,  .value.i = 5,     "CameraFrameBufferSize"},

    {PROEM_EXPOSURE_SHORTCUT,   INT,  .value.i = 5,    "ProEMExposureShortcut"},
    {VALIDATE_TIMESTAMPS,       CHAR, .value.c = 1,     "ValidateTimestamps"},

    {FRAME_FLIP_X,              CHAR, .value.c = 0,     "FrameFlipX"},
    {FRAME_FLIP_Y,              CHAR, .value.c = 0,     "FrameFlipY"},
    {FRAME_TRANSPOSE,           CHAR, .value.c = 0,     "FrameTranspose"},

#if (defined _WIN32)
    {MSYS_BASH_PATH, STRING, .value.s = "C:/MinGW/MSYS/bin/bash.exe",    "MsysBashPath"}
#endif
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

    for (size_t i = 0; i < pref_count; i++)
    {
        char *fmt;
        switch (prefs[i].type)
        {
            case STRING:
                fmt = "%s: %%s\n";
                prefs[i].value.s = strdup(prefs[i].value.s);
            break;
            case CHAR: fmt = "%s: %%hhu\n"; break;
            case INT: fmt = "%s: %%d\n"; break;
        }

        // Build format string
        size_t len = snprintf(NULL, 0, fmt, prefs[i].name) + 1;
        prefs[i].format = malloc(len*sizeof(char));
        snprintf(prefs[i].format, len, fmt, prefs[i].name);
    }

    FILE *fp = fopen(filename, "r");
    if (fp)
    {
        char linebuf[1024];
        while (fgets(linebuf, sizeof(linebuf)-1, fp) != NULL)
        {
            size_t compare = strcspn(linebuf, ":");
            if (compare == strlen(linebuf))
                continue; // Line is not in `key: value' format

            for (size_t i = 0; i < pref_count; i++)
            {
                if (strncmp(linebuf, prefs[i].name, compare))
                    continue;

                if (prefs[i].type == STRING)
                {
                    char stringbuf[1024];
                    sscanf(linebuf + compare, ": %[^\n]", stringbuf);
                    prefs[i].value.s = strdup(stringbuf);
                }
                else
                    sscanf(linebuf, prefs[i].format, &prefs[i].value);
            }
        }

        fclose(fp);
    }

    // Force saving to false on startup
    prefs[SAVE_FRAMES].value.c = false;

    // Init mutex
    pthread_mutex_init(&access_mutex, NULL);
    save();
}

void pn_free_preferences()
{
    for (size_t i = 0; i < pref_count; i++)
    {
        free(prefs[i].format);
        if (prefs[i].type == STRING)
            free(prefs[i].value.s);
    }

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
        prefs[CALIBRATION_COUNTDOWN].value.i > 0)
    {
        prefs[CALIBRATION_COUNTDOWN].value.i--;

        // Disable saving
        if (prefs[CALIBRATION_COUNTDOWN].value.i == 0)
            prefs[SAVE_FRAMES].value.c = false;
    }
    save();
    pthread_mutex_unlock(&access_mutex);
}

unsigned char pn_preference_toggle_save()
{
    if (!pn_preference_allow_save())
        return false;

    pthread_mutex_lock(&access_mutex);
    unsigned char ret = prefs[SAVE_FRAMES].value.c ^= true;
    pthread_mutex_unlock(&access_mutex);
    return ret;
}

unsigned char pn_preference_allow_save()
{
    pthread_mutex_lock(&access_mutex);
    unsigned char ret = (prefs[OBJECT_TYPE].value.c == OBJECT_TARGET ||
                         prefs[CALIBRATION_COUNTDOWN].value.i > 0);
    pthread_mutex_unlock(&access_mutex);
    return ret;
}

void pn_preference_set(PNPreferenceType key, void *val)
{
    pthread_mutex_lock(&access_mutex);
    switch (prefs[key].type)
    {
        case STRING: free(prefs[key].value.s); prefs[key].value.s = strdup(*((char **)val)); break;
        case CHAR: prefs[key].value.c = *((char *)val); break;
        case INT: prefs[key].value.i = *((int *)val); break;
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
