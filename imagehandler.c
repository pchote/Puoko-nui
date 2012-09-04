/*
 * Copyright 2010, 2011, 2012 Paul Chote
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

extern PNCamera *camera;
extern PNGPS *gps;

// A quick and dirty method for opening ds9
// Beware of race conditions: it will take some time
// between calling this, and ds9 actually being available
// Runs at program startup in the main thread, or on frame acquisition in the camera thread
void launch_ds9()
{
#ifdef USE_SCRIPT_PREVIEW
    char *preview_command = pn_preference_string(FRAME_PREVIEW_COMMAND);
    char *startup_command;
    asprintf(&startup_command, "%s startup", preview_command);
    system(startup_command);
    free(startup_command);
    free(preview_command);
#endif
}

#pragma mark Frame Saving/Preview. Runs in camera thread

// Display a frame in DS9
void pn_preview_frame(PNFrame *frame, PNGPSTimestamp timestamp)
{
#ifdef USE_SCRIPT_PREVIEW
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
#ifdef USE_PICAM
    char *title = "Exposure starting %04d-%02d-%02d %02d:%02d:%02d";
#else
    char *title = "Exposure ending %04d-%02d-%02d %02d:%02d:%02d";
#endif

    char buf[128];
    sprintf(buf, title,
            timestamp.year, timestamp.month, timestamp.day,
            timestamp.hours, timestamp.minutes, timestamp.seconds);
    fits_update_key(fptr, TSTRING, "OBJECT", &buf, NULL, &status);

    // Write the frame data to the image and close the file
    fits_write_img(fptr, TUSHORT, 1, frame->width*frame->height, frame->data, &status);
    fits_close_file(fptr, &status);

    // Log any error messages
    while (fits_read_errmsg(fitserr))
        pn_log("cfitsio error: %s", fitserr);

    char *preview_command = pn_preference_string(FRAME_PREVIEW_COMMAND);
    system(preview_command);
    free(preview_command);
#endif
}


// Write frame data to a fits file
void pn_save_frame(PNFrame *frame, PNGPSTimestamp timestamp)
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
        return;
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
        // TODO: Temporary hack - unhardcode me
        char buf[25];
        unsigned char skip = pn_preference_char(CAMERA_OVERSCAN_SKIP_COLS);
        unsigned char bias = pn_preference_char(CAMERA_OVERSCAN_BIAS_COLS);
        unsigned char superpixel = pn_preference_char(CAMERA_PIXEL_SIZE);
        sprintf(buf, "[%d, %d, %d, %d]", 0, frame->width - (skip + bias)/superpixel, 0, frame->height);
        fits_update_key(fptr, TSTRING, "IMAG-RGN", buf, "Frame image subregion", &status);
        
        sprintf(buf, "[%d, %d, %d, %d]", frame->width - skip/superpixel, frame->width, 0, frame->height);
        fits_update_key(fptr, TSTRING, "BIAS-RGN", buf, "Frame bias subregion", &status);
    }
    
#ifdef USE_PICAM
    // Trigger timestamp defines the *start* of the frame
    PNGPSTimestamp start = timestamp;
    PNGPSTimestamp end = pn_timestamp_subtract_seconds(timestamp, -exposure_time);
#else
    // Trigger timestamp defines the *end* of the frame
    PNGPSTimestamp start = pn_timestamp_subtract_seconds(timestamp, exposure_time);
    PNGPSTimestamp end = timestamp;
#endif
    
    if (timestamp.valid)
    {
        char datebuf[15];
        sprintf(datebuf, "%04d-%02d-%02d", start.year, start.month, start.day);
        fits_update_key(fptr, TSTRING, "UTC-DATE", datebuf, "Exposure start date (GPS)", &status);
        
        char gpstimebuf[15];
        sprintf(gpstimebuf, "%02d:%02d:%02d", start.hours, start.minutes, start.seconds);
        fits_update_key(fptr, TSTRING, "UTC-BEG", gpstimebuf, "Exposure start time (GPS)", &status);
        
        sprintf(gpstimebuf, "%02d:%02d:%02d", end.hours, end.minutes, end.seconds);
        fits_update_key(fptr, TSTRING, "UTC-END", gpstimebuf, "Exposure end time (GPS)", &status);
        fits_update_key(fptr, TLOGICAL, "GPS-LOCK", &start.locked, "GPS time locked", &status);
    }
    else
        fits_update_key(fptr, TLOGICAL, "GPS-VALID", &timestamp.valid, "GPS timestamp unavailable", &status);
    
    time_t pcend = time(NULL);
    time_t pcstart = pcend - exposure_time;
    
    char timebuf[15];
    strftime(timebuf, 15, "%Y-%m-%d", gmtime(&pcstart));
    fits_update_key(fptr, TSTRING, "PC-DATE", (void *)timebuf, "Exposure start date (PC)", &status);
    
    strftime(timebuf, 15, "%H:%M:%S", gmtime(&pcstart));
    fits_update_key(fptr, TSTRING, "PC-BEG", (void *)timebuf, "Exposure start time (PC)", &status);
    
    strftime(timebuf, 15, "%H:%M:%S", gmtime(&pcend));
    fits_update_key(fptr, TSTRING, "PC-END", (void *)timebuf, "Exposure end time (PC)", &status);
    
    // Camera temperature
    pthread_mutex_lock(&camera->read_mutex);
    sprintf(timebuf, "%0.02f", camera->temperature);
    pthread_mutex_unlock(&camera->read_mutex);
    
    fits_update_key(fptr, TSTRING, "CCD-TEMP", (void *)timebuf, "CCD temperature at end of exposure in deg C", &status);
    
    // Write the frame data to the image and close the file
    fits_write_img(fptr, TUSHORT, 1, frame->width*frame->height, frame->data, &status);
    fits_close_file(fptr, &status);
    
    // Log any error messages
    while (fits_read_errmsg(fitserr))
        pn_log("cfitsio error: %s", fitserr);
    
    // Call frame_available.sh to run the online reduction code
    char cmd[PATH_MAX];
    sprintf(cmd,"./frame_available.sh %s&", filepath);
    system(cmd);
    
    free(filepath);
}