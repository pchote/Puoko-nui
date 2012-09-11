/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <fitsio.h>
#include <string.h>
#include "imagehandler.h"
#include "preferences.h"
#include "common.h"
#include "platform.h"

// A quick and dirty method for opening ds9
// Beware of race conditions: it will take some time
void pn_run_startup_script()
{
    #if (defined _WIN32 || defined _WIN64)
    run_command_async("powershell -executionpolicy bypass -command .\\startup.ps1");
    #else
    run_command_async("./startup.sh &");
    #endif
}

void pn_run_preview_script(const char *filepath)
{
#if (defined _WIN32 || defined _WIN64)
    run_command_async("powershell -executionpolicy bypass -command .\\preview.ps1");
#else
    run_command_async("./preview.sh &");
#endif
}

void pn_run_saved_script(const char *filepath)
{
    char *cmd;
#if (defined _WIN32 || defined _WIN64)
    asprintf(&cmd, "powershell  -executionpolicy bypass -command \"./frame_available.ps1 %s \"", filepath);
#else
    asprintf(&cmd, "./frame_available.sh %s&", filepath);
#endif
    run_command_async(cmd);
    free(cmd);
}

#pragma mark Frame Saving/Preview. Runs in camera thread

// Display a frame in DS9
void pn_save_preview(PNFrame *frame, TimerTimestamp timestamp)
{
    fitsfile *fptr;
    int status = 0;
    char fitserr[128];

    // Create a new fits file
    if (fits_create_file(&fptr, "!preview.fits.gz", &status))
    {
        pn_log("Unable to save temporary file. fitsio error %d", status);
        while (fits_read_errmsg(fitserr))
            pn_log(fitserr);
        return;
    }

    // Create the primary array image (16-bit short integer pixels
    long size[2] = { frame->width, frame->height };
    fits_create_img(fptr, USHORT_IMG, 2, size, &status);

    // Write a message into the OBJECT header for ds9 to display
    char buf[128];
    sprintf(buf, "Exposure starting %04d-%02d-%02d %02d:%02d:%02d",
            timestamp.year, timestamp.month, timestamp.day,
            timestamp.hours, timestamp.minutes, timestamp.seconds);
    fits_update_key(fptr, TSTRING, "OBJECT", &buf, NULL, &status);

    // Write the frame data to the image and close the file
    fits_write_img(fptr, TUSHORT, 1, frame->width*frame->height, frame->data, &status);
    fits_close_file(fptr, &status);

    // Log any error messages
    while (fits_read_errmsg(fitserr))
        pn_log("cfitsio error: %s", fitserr);
}

// Write frame data to a fits file
// Returns the filepath of the saved frame, to be freed by the caller
const char *pn_save_frame(PNFrame *frame, TimerTimestamp timestamp, PNCamera *camera)
{
    fitsfile *fptr;
    int status = 0;
    char fitserr[128];

    // Construct the output filepath from the output dir, run prefix, and run number.
    // Saving will fail if a file with the same name already exists
    char *output_dir = pn_preference_string(OUTPUT_DIR);
    char *run_prefix = pn_preference_string(RUN_PREFIX);
    int run_number = pn_preference_int(RUN_NUMBER);

    char *filepath;
    asprintf(&filepath, "%s/%s-%04d.fits.gz", output_dir, run_prefix, run_number);
    free(output_dir);

    pn_log("Saving frame %s", filepath);

    // Create a new fits file
    if (fits_create_file(&fptr, filepath, &status))
    {
        pn_log("Unable to save file. fitsio error %d", status);
        while (fits_read_errmsg(fitserr))
            pn_log(fitserr);
        return NULL;
    }

    // Create the primary array image (16-bit short integer pixels
    long size[2] = { frame->width, frame->height };
    fits_create_img(fptr, USHORT_IMG, 2, size, &status);

    // Write header keys
    fits_update_key(fptr, TSTRING, "RUN", (void *)run_prefix, "name of this run", &status);
    free(run_prefix);

    switch (pn_preference_char(OBJECT_TYPE))
    {
        case OBJECT_DARK:
            fits_update_key(fptr, TSTRING, "OBJECT", "Dark", "Object name", &status);
            break;
        case OBJECT_FLAT:
            fits_update_key(fptr, TSTRING, "OBJECT", "Flat Field", "Object name", &status);
            break;
        default:
        {
            char *object_name = pn_preference_string(OBJECT_NAME);
            fits_update_key(fptr, TSTRING, "OBJECT", (void *)object_name, "Object name", &status);
            free(object_name);
            break;
        }
    }

    long exposure_time = pn_preference_char(EXPOSURE_TIME);
    fits_update_key(fptr, TLONG, "EXPTIME", &exposure_time, "Actual integration time (sec)", &status);

    char *observers = pn_preference_string(OBSERVERS);
    fits_update_key(fptr, TSTRING, "OBSERVER", (void *)observers, "Observers", &status);
    free(observers);

    char *observatory = pn_preference_string(OBSERVATORY);
    fits_update_key(fptr, TSTRING, "OBSERVAT", (void *)observatory, "Observatory", &status);
    free(observatory);

    char *telescope = pn_preference_string(TELESCOPE);
    fits_update_key(fptr, TSTRING, "TELESCOP", (void *)telescope, "Telescope name", &status);
    free(telescope);

    fits_update_key(fptr, TSTRING, "PROGRAM", "puoko-nui", "Data acquistion program", &status);
    fits_update_key(fptr, TSTRING, "INSTRUME", "puoko-nui", "Instrument", &status);

    if (pn_preference_char(CAMERA_OVERSCAN_ENABLED))
    {
        char buf[25];
        unsigned char skip = pn_preference_char(CAMERA_OVERSCAN_SKIP_COLS);
        unsigned char bias = pn_preference_char(CAMERA_OVERSCAN_BIAS_COLS);
        unsigned char superpixel = pn_preference_char(CAMERA_PIXEL_SIZE);
        sprintf(buf, "[%d, %d, %d, %d]", 0, frame->width - (skip + bias)/superpixel, 0, frame->height);
        fits_update_key(fptr, TSTRING, "IMAG-RGN", buf, "Frame image subregion", &status);

        sprintf(buf, "[%d, %d, %d, %d]", frame->width - skip/superpixel, frame->width, 0, frame->height);
        fits_update_key(fptr, TSTRING, "BIAS-RGN", buf, "Frame bias subregion", &status);
    }

    // Trigger timestamp defines the *start* of the frame
    TimerTimestamp start = timestamp;
    TimerTimestamp end = start; end.seconds += exposure_time;
    end = pn_timestamp_normalize(end);

    if (timestamp.valid)
    {
        char datebuf[15], gpstimebuf[15];
        sprintf(datebuf, "%04d-%02d-%02d", start.year, start.month, start.day);
        sprintf(gpstimebuf, "%02d:%02d:%02d", start.hours, start.minutes, start.seconds);

        // Used by ImageJ and other programs
        fits_update_key(fptr, TSTRING, "UT_DATE", datebuf, "Exposure start date (GPS)", &status);
        fits_update_key(fptr, TSTRING, "UT_TIME", gpstimebuf, "Exposure start time (GPS)", &status);

        // Used by tsreduce
        fits_update_key(fptr, TSTRING, "UTC-DATE", datebuf, "Exposure start date (GPS)", &status);
        fits_update_key(fptr, TSTRING, "UTC-BEG", gpstimebuf, "Exposure start time (GPS)", &status);

        sprintf(gpstimebuf, "%02d:%02d:%02d", end.hours, end.minutes, end.seconds);
        fits_update_key(fptr, TSTRING, "UTC-END", gpstimebuf, "Exposure end time (GPS)", &status);
        fits_update_key(fptr, TLOGICAL, "GPS-LOCK", &start.locked, "GPS time locked", &status);
    }
    else
        fits_update_key(fptr, TLOGICAL, "GPS-VALID", &timestamp.valid, "GPS timestamp unavailable", &status);

    time_t pctime = time(NULL);

    char timebuf[15];
    strftime(timebuf, 15, "%Y-%m-%d", gmtime(&pctime));
    fits_update_key(fptr, TSTRING, "PC-DATE", (void *)timebuf, "PC Date when frame was saved to disk", &status);

    strftime(timebuf, 15, "%H:%M:%S", gmtime(&pctime));
    fits_update_key(fptr, TSTRING, "PC-TIME", (void *)timebuf, "PC Time when frame was saved to disk", &status);

    // Camera temperature
    sprintf(timebuf, "%0.02f", pn_camera_temperature());

    fits_update_key(fptr, TSTRING, "CCD-TEMP", (void *)timebuf, "CCD temperature at end of exposure in deg C", &status);

    // Write the frame data to the image and close the file
    fits_write_img(fptr, TUSHORT, 1, frame->width*frame->height, frame->data, &status);
    fits_close_file(fptr, &status);

    // Log any error messages
    while (fits_read_errmsg(fitserr))
        pn_log("cfitsio error: %s", fitserr);

    return filepath;
}