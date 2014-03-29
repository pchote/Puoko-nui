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
    char *format;
} PNPreferenceStore;

/*
 * Preference definitions
 * Must be defined in the same order as the keys in preference.h
 */

PNPreferenceStore prefs[] =
{
    // Key, Default value, Output format
    {OUTPUT_DIR,  STRING, .value.s = "",                "OutputDir: %s\n"},
    {RUN_PREFIX,  STRING, .value.s = "run",             "RunPrefix: %s\n"},
    {OBJECT_NAME, STRING, .value.s = "",                "ObjectName: %s\n"},
    {OBSERVERS,   STRING, .value.s = "",                "Observers: %s\n"},
    {OBSERVATORY, STRING, .value.s = "",                "Observatory: %s\n"},
    {TELESCOPE,   STRING, .value.s = "",                "Telescope: %s\n"},
    {INSTRUMENT,  STRING, .value.s = "Puoko-nui",       "Instrument: %s\n"},
    {FILTER,      STRING, .value.s = "",                "Filter: %s\n"},

    {EXPOSURE_TIME,             INT,  .value.c = 5,     "ExposureTime: %d\n"},
    {SAVE_FRAMES,               CHAR, .value.c = false, "SaveFrames: %hhu\n"},
    {REDUCE_FRAMES,             CHAR, .value.c = false, "ReduceFrames: %hhu\n"},
    {OBJECT_TYPE,               CHAR, .value.c = OBJECT_TARGET, "ObjectType: %hhu\n"},
    {RUN_NUMBER,                INT,  .value.i = 0,     "RunNumber: %d\n"},

    {BURST_ENABLED,             CHAR, .value.c = 0,     "BurstMode: %hhu\n"},
    {BURST_COUNTDOWN,           INT,  .value.i = 30,    "BurstCountdown: %d\n"},

    {TIMER_MONITOR_LOGIC_OUT,   CHAR, .value.c = true,  "TimerMonitorLogicOut: %hhu\n"},
    {TIMER_TRIGGER_MODE,        CHAR, .value.c = 0,     "TimerTriggerMode: %hhu\n"},
    {TIMER_ALIGN_FIRST_EXPOSURE,CHAR, .value.c = 1,     "TimerAlignFirstExposure: %hhu\n"},
    {TIMER_SERIAL_PORT,       STRING, .value.s = "/dev/ttyUSB0", "TimerSerialPort: %s\n"},
    {TIMER_BAUD_RATE,           INT,  .value.i = 9600,  "TimerBaudRate: %d\n"},

    {CAMERA_BINNING,            CHAR, .value.c = 1,     "CameraBinning: %hhu\n"},
    {CAMERA_READPORT_MODE,      CHAR, .value.c = 0,     "CameraReadoutPortMode: %hhu\n"},
    {CAMERA_READSPEED_MODE,     CHAR, .value.c = 0,     "CameraReadoutSpeedMode: %hhu\n"},
    {CAMERA_GAIN_MODE,          CHAR, .value.c = 0,     "CameraGainMode: %hhu\n"},
    {CAMERA_TEMPERATURE,        INT,  .value.i = -5000, "CameraTemperature: %d\n"},

    {CAMERA_OVERSCAN_COLS,      CHAR, .value.c = 0,     "CameraOverscanColumns: %d\n"},
    {CAMERA_PLATESCALE,       STRING, .value.s = "0.33","CameraPlatescale: %s\n"},

    {CAMERA_WINDOW_X,           INT,  .value.i = 0,     "CameraWindowX: %d\n"},
    {CAMERA_WINDOW_Y,           INT,  .value.i = 0,     "CameraWindowY: %d\n"},
    {CAMERA_WINDOW_WIDTH,       INT,  .value.i = 1024,  "CameraWindowWidth: %d\n"},
    {CAMERA_WINDOW_HEIGHT,      INT,  .value.i = 1024,  "CameraWindowHeight: %d\n"},
    {CAMERA_DISABLE_SHUTTER,    CHAR, .value.c = 0,     "CameraDisableShutter: %hhu\n"},
    {CAMERA_FRAME_BUFFER_SIZE,  INT,  .value.i = 5,     "CameraFrameBufferSize: %d\n"},

    {PROEM_EXPOSURE_SHORTCUT,   INT,  .value.i = 5,     "ProEMExposureShortcut: %d\n"},
    {PROEM_EM_GAIN,             INT,  .value.i = 1,     "ProEMEMGain: %d\n"},
    {PROEM_SHIFT_MODE,          CHAR, .value.c = 1,     "ProEMShiftMode: %hhu\n"},
    {VALIDATE_TIMESTAMPS,       CHAR, .value.c = 1,     "ValidateTimestamps: %hhu\n"},

    {FRAME_FLIP_X,              CHAR, .value.c = 0,     "FrameFlipX: %hhu\n"},
    {FRAME_FLIP_Y,              CHAR, .value.c = 0,     "FrameFlipY: %hhu\n"},
    {FRAME_TRANSPOSE,           CHAR, .value.c = 0,     "FrameTranspose: %hhu\n"},
    {PREVIEW_RATE_LIMIT,        INT,  .value.i = 500,   "PreviewRateLimit: %d\n"},

#if (defined _WIN32)
    {MSYS_BASH_PATH, STRING, .value.s = "C:/MinGW/msys/1.0/bin/bash.exe",    "MsysBashPath: %s\n"}
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

                    // Value starts after the ': ' characters
                    char *value = linebuf + compare + 2;
                    size_t value_len = strlen(value);

                    // Remove newline from input
                    if (value_len > 0)
                    {
                        prefs[i].value.s = strdup(value);
                        prefs[i].value.s[value_len - 1] = '\0';
                    }
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
    if (prefs[BURST_ENABLED].value.c &&
        prefs[BURST_COUNTDOWN].value.i > 0)
    {
        prefs[BURST_COUNTDOWN].value.i--;

        // Disable saving
        if (prefs[BURST_COUNTDOWN].value.i == 0)
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
    unsigned char ret = (!prefs[BURST_ENABLED].value.c ||
                         prefs[BURST_COUNTDOWN].value.i > 0);
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
