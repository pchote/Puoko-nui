/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <string.h>
#include <fitsio.h>

#include "timer.h"
#include "preferences.h"
#include "version.h"
#include "frame.h"

// Transform the frame data in a CameraFrame with the
// flip/transpose operations specified in the preferences.
void frame_process_transforms(CameraFrame *frame)
{
    if (pn_preference_char(FRAME_FLIP_X))
    {
        for (uint16_t j = 0; j < frame->height; j++)
            for (uint16_t i = 0; i < frame->width/2; i++)
            {
                uint16_t temp = frame->data[j*frame->width + i];
                frame->data[j*frame->width + i] = frame->data[j*frame->width + (frame->width - i - 1)];
                frame->data[j*frame->width + (frame->width - i - 1)] = temp;
            }
        
        if (frame->has_image_region)
        {
            uint16_t temp = frame->width - frame->image_region[0];
            frame->image_region[0] = frame->width - frame->image_region[1];
            frame->image_region[1] = temp;
        }
        
        if (frame->has_bias_region)
        {
            uint16_t temp = frame->width - frame->bias_region[0];
            frame->bias_region[0] = frame->width - frame->bias_region[1];
            frame->bias_region[1] = temp;
        }
    }
    
    if (pn_preference_char(FRAME_FLIP_Y))
    {
        for (uint16_t j = 0; j < frame->height/2; j++)
            for (uint16_t i = 0; i < frame->width; i++)
            {
                uint16_t temp = frame->data[j*frame->width + i];
                frame->data[j*frame->width + i] = frame->data[(frame->height - j - 1)*frame->width + i];
                frame->data[(frame->height - j - 1)*frame->width + i] = temp;
            }
        
        if (frame->has_image_region)
        {
            uint16_t temp = frame->width - frame->image_region[2];
            frame->image_region[2] = frame->width - frame->image_region[3];
            frame->image_region[3] = temp;
        }
        
        if (frame->has_bias_region)
        {
            uint16_t temp = frame->width - frame->bias_region[2];
            frame->bias_region[2] = frame->width - frame->bias_region[3];
            frame->bias_region[3] = temp;
        }
    }
    
    if (pn_preference_char(FRAME_TRANSPOSE))
    {
        // Create a copy of the frame to simplify transpose when width != height
        size_t s = frame->width*frame->height*sizeof(uint16_t);
        uint16_t *data = malloc(s);
        if (!data)
        {
            pn_log("Failed to allocate memory. Discarding frame");
            return;
        }
        memcpy(data, frame->data, s);
        
        for (uint16_t j = 0; j < frame->height; j++)
            for (uint16_t i = 0; i < frame->width; i++)
                frame->data[i*frame->height + j] = data[j*frame->width + i];
        free(data);
        
        if (frame->has_image_region)
            for (uint8_t i = 0; i < 2; i++)
            {
                uint16_t temp = frame->image_region[i];
                frame->image_region[i] = frame->image_region[i+2];
                frame->image_region[i+2] = temp;
            }
        
        if (frame->has_bias_region)
            for (uint8_t i = 0; i < 2; i++)
            {
                uint16_t temp = frame->bias_region[i];
                frame->bias_region[i] = frame->bias_region[i+2];
                frame->bias_region[i+2] = temp;
            }
        
        uint16_t temp = frame->height;
        frame->height = frame->width;
        frame->width = temp;
    }
}

// Save a frame and trigger to disk
// Returns true on success or false on failure
bool frame_save(CameraFrame *frame, TimerTimestamp timestamp, char *filepath)
{
    fitsfile *fptr;
    int status = 0;
    char fitserr[128];
    
    // Create a new fits file
    if (fits_create_file(&fptr, filepath, &status))
    {
        pn_log("Failed to save file. fitsio error %d.", status);
        while (fits_read_errmsg(fitserr))
            pn_log(fitserr);
        
        return false;
    }
    
    // Create the primary array image (16-bit short integer pixels
    long size[2] = { frame->width, frame->height };
    fits_create_img(fptr, USHORT_IMG, 2, size, &status);
    
    // Write header keys
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
    
    bool highres = pn_preference_char(TIMER_HIGHRES_TIMING);
    long exposure_time = pn_preference_int(EXPOSURE_TIME);
    if (highres)
    {
        double exptime = exposure_time / 1000.0;
        fits_update_key(fptr, TDOUBLE, "EXPTIME", &exptime, "Actual integration time (sec)", &status);
    }
    else
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
    
    char *instrument = pn_preference_string(INSTRUMENT);
    fits_update_key(fptr, TSTRING, "INSTRUME", (void *)instrument, "Instrument name", &status);
    free(instrument);
    
    char *filter = pn_preference_string(FILTER);
    fits_update_key(fptr, TSTRING, "FILTER", (void *)filter, "Filter type", &status);
    free(filter);
    
    fits_update_key(fptr, TSTRING, "PROG-VER", (void *)program_version() , "Acquisition program version reported by git", &status);
    
    // Trigger timestamp defines the *start* of the frame
    TimerTimestamp start = timestamp;
    TimerTimestamp end = start;
    if (highres)
        end.milliseconds += exposure_time;
    else
        end.seconds += exposure_time;
    timestamp_normalize(&end);
    
    char datebuf[15], gpstimebuf[15];
    snprintf(datebuf, 15, "%04d-%02d-%02d", start.year, start.month, start.day);
    
    if (highres)
        snprintf(gpstimebuf, 15, "%02d:%02d:%02d.%03d", start.hours, start.minutes, start.seconds, start.milliseconds);
    else
        snprintf(gpstimebuf, 15, "%02d:%02d:%02d", start.hours, start.minutes, start.seconds);
    
    // Used by ImageJ and other programs
    fits_update_key(fptr, TSTRING, "UT_DATE", datebuf, "Exposure start date (GPS)", &status);
    fits_update_key(fptr, TSTRING, "UT_TIME", gpstimebuf, "Exposure start time (GPS)", &status);
    
    // Used by tsreduce
    fits_update_key(fptr, TSTRING, "UTC-DATE", datebuf, "Exposure start date (GPS)", &status);
    fits_update_key(fptr, TSTRING, "UTC-BEG", gpstimebuf, "Exposure start time (GPS)", &status);
    
    if (highres)
        snprintf(gpstimebuf, 15, "%02d:%02d:%02d.%03d", end.hours, end.minutes, end.seconds, end.milliseconds);
    else
        snprintf(gpstimebuf, 15, "%02d:%02d:%02d", end.hours, end.minutes, end.seconds);
    
    fits_update_key(fptr, TSTRING, "UTC-END", gpstimebuf, "Exposure end time (GPS)", &status);
    fits_update_key(fptr, TLOGICAL, "UTC-LOCK", &start.locked, "UTC time has GPS lock", &status);
    
    time_t pctime = time(NULL);
    
    char timebuf[15];
    strftime(timebuf, 15, "%Y-%m-%d", gmtime(&pctime));
    fits_update_key(fptr, TSTRING, "PC-DATE", (void *)timebuf, "PC Date when frame was saved to disk", &status);
    
    strftime(timebuf, 15, "%H:%M:%S", gmtime(&pctime));
    fits_update_key(fptr, TSTRING, "PC-TIME", (void *)timebuf, "PC Time when frame was saved to disk", &status);
    
    // Camera temperature
    char tempbuf[10];
    snprintf(tempbuf, 10, "%0.02f", frame->temperature);
    fits_update_key(fptr, TSTRING, "CCD-TEMP", (void *)tempbuf, "CCD temperature at end of exposure (deg C)", &status);
    fits_update_key(fptr, TSTRING, "CCD-PORT", (void *)frame->port_desc, "CCD readout port description", &status);
    fits_update_key(fptr, TSTRING, "CCD-RATE", (void *)frame->speed_desc, "CCD readout rate description", &status);
    fits_update_key(fptr, TSTRING, "CCD-GAIN", (void *)frame->gain_desc, "CCD readout gain description", &status);
    fits_update_key(fptr, TLONG,   "CCD-BIN",  &(long){pn_preference_char(CAMERA_BINNING)},  "CCD pixel binning", &status);
    
    if (frame->has_timestamp)
        fits_update_key(fptr, TDOUBLE, "CCD-TIME", &frame->timestamp, "CCD time relative to first exposure in seconds", &status);
    
    char *pscale = pn_preference_string(CAMERA_PLATESCALE);
    fits_update_key(fptr, TDOUBLE, "IM-SCALE",  &(double){pn_preference_char(CAMERA_BINNING)*atof(pscale)},  "Image scale (arcsec/px)", &status);
    free(pscale);
    
    if (frame->has_image_region)
    {
        char buf[25];
        snprintf(buf, 25, "[%d, %d, %d, %d]",
                 frame->image_region[0], frame->image_region[1],
                 frame->image_region[2], frame->image_region[3]);
        fits_update_key(fptr, TSTRING, "IMAG-RGN", buf, "Frame image subregion", &status);
    }
    
    if (frame->has_bias_region)
    {
        char buf[25];
        snprintf(buf, 25, "[%d, %d, %d, %d]",
                 frame->bias_region[0], frame->bias_region[1],
                 frame->bias_region[2], frame->bias_region[3]);
        fits_update_key(fptr, TSTRING, "BIAS-RGN", buf, "Frame bias subregion", &status);
    }
    
    // Write the frame data to the image and close the file
    fits_write_img(fptr, TUSHORT, 1, frame->width*frame->height, frame->data, &status);
    fits_close_file(fptr, &status);
    
    // Log any error messages
    while (fits_read_errmsg(fitserr))
        pn_log("cfitsio error: %s.", fitserr);
    
    return true;
}
