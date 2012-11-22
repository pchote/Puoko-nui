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
#include <string.h>
#include <fitsio.h>
#include <math.h>
#include "main.h"
#include "atomicqueue.h"
#include "camera.h"
#include "timer.h"
#include "scripting.h"
#include "preferences.h"
#include "gui.h"
#include "platform.h"
#include "version.h"


Camera *camera;
TimerUnit *timer;
ScriptingInterface *scripting;
pthread_mutex_t reset_mutex;

struct atomicqueue *log_queue, *frame_queue, *trigger_queue;
char *fatal_error = NULL;
bool first_frame = true;

void queue_framedata(CameraFrame *frame)
{
    frame->downloaded_time = timer_current_timestamp(timer);
    TimerTimestamp *t = &frame->downloaded_time;

    pthread_mutex_lock(&reset_mutex);
    bool success = atomicqueue_push(frame_queue, frame);
    pthread_mutex_unlock(&reset_mutex);

    if (success)
        pn_log("Frame @ %02d:%02d:%02d. %d queued.",
               t->hours, t->minutes, t->seconds,
               atomicqueue_length(frame_queue));
    else
    {
        pn_log("Failed to push frame. Discarding.");
        free(frame);
    }
}

void queue_trigger(TimerTimestamp *t)
{
    pthread_mutex_lock(&reset_mutex);
    bool success = atomicqueue_push(trigger_queue, t);
    pthread_mutex_unlock(&reset_mutex);

    if (success)
        pn_log("Trigger @ %02d:%02d:%02d%s. %d queued.",
               t->hours, t->minutes, t->seconds,
               (t->locked ? "" : " (UNLOCKED)"),
               atomicqueue_length(trigger_queue));
    else
    {
        pn_log("Failed to push trigger. Discarding.");
        free(t);
    }
}

// Remove all queued frames and timestamps
void clear_queued_data(bool reset_first)
{
    void *item;
    pthread_mutex_lock(&reset_mutex);

    while ((item = atomicqueue_pop(frame_queue)) != NULL)
    {
        pn_log("Discarding queued frame.");
        free(item);
    }

    while ((item = atomicqueue_pop(trigger_queue)) != NULL)
    {
        pn_log("Discarding queued trigger.");
        free(item);
    }

    if (reset_first)
        first_frame = true;

    pthread_mutex_unlock(&reset_mutex);
}

// Write frame data to a fits file
bool save_frame(CameraFrame *frame, TimerTimestamp timestamp, char *filepath)
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

    bool ms_mode = pn_preference_char(TIMER_MILLISECOND_MODE);
    long exposure_time = pn_preference_int(EXPOSURE_TIME);
    if (ms_mode)
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
    if (ms_mode)
        end.milliseconds += exposure_time;
    else
        end.seconds += exposure_time;
    timestamp_normalize(&end);

    char datebuf[15], gpstimebuf[15];
    snprintf(datebuf, 15, "%04d-%02d-%02d", start.year, start.month, start.day);

    if (ms_mode)
        snprintf(gpstimebuf, 15, "%02d:%02d:%02d.%03d", start.hours, start.minutes, start.seconds, start.milliseconds);
    else
        snprintf(gpstimebuf, 15, "%02d:%02d:%02d", start.hours, start.minutes, start.seconds);

    // Used by ImageJ and other programs
    fits_update_key(fptr, TSTRING, "UT_DATE", datebuf, "Exposure start date (GPS)", &status);
    fits_update_key(fptr, TSTRING, "UT_TIME", gpstimebuf, "Exposure start time (GPS)", &status);

    // Used by tsreduce
    fits_update_key(fptr, TSTRING, "UTC-DATE", datebuf, "Exposure start date (GPS)", &status);
    fits_update_key(fptr, TSTRING, "UTC-BEG", gpstimebuf, "Exposure start time (GPS)", &status);

    if (ms_mode)
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
    snprintf(tempbuf, 10, "%0.02f", camera_temperature(camera));
    fits_update_key(fptr, TSTRING, "CCD-TEMP", (void *)tempbuf, "CCD temperature at end of exposure (deg C)", &status);
    fits_update_key(fptr, TSTRING, "CCD-PORT", (void *)camera_port_desc(camera), "CCD readout port description", &status);
    fits_update_key(fptr, TSTRING, "CCD-RATE", (void *)camera_speed_desc(camera), "CCD readout rate description", &status);
    fits_update_key(fptr, TSTRING, "CCD-GAIN", (void *)camera_gain_desc(camera), "CCD readout gain description", &status);
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

void process_framedata(CameraFrame *frame, TimerTimestamp timestamp)
{
    if (first_frame)
    {
        // The first frame from the MicroMax corresponds to the startup
        // and alignment period, so is meaningless
        // The first frame from the ProEM has incorrect cleaning so the
        // bias is inconsistent with the other frames
        pn_log("Discarding first frame.");
        first_frame = false;
        return;
    }

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
        for (uint16_t j = 0; j < frame->height; j++)
            for (uint16_t i = 0; i < j; i++)
            {
                uint16_t temp = frame->data[j*frame->width + i];
                frame->data[j*frame->width + i] = frame->data[i*frame->width + j];
                frame->data[i*frame->width + j] = temp;
            }

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
    }

    if (pn_preference_char(SAVE_FRAMES))
    {
        // Construct the output filepath from the output dir, run prefix, and run number.
        int run_number = pn_preference_int(RUN_NUMBER);
        char *output_dir = pn_preference_string(OUTPUT_DIR);
        char *run_prefix = pn_preference_string(RUN_PREFIX);

        // Save to a temporary file and then rename to the final name
        // to ensure the save operation appears atomic to tsreduce,
        // which uses the frame prefix to determine frames
        size_t temppath_len = snprintf(NULL, 0, "%s/XXXXXX", output_dir) + 1;
        char *temppath = malloc(temppath_len*sizeof(char));
        if (!temppath)
        {
            pn_log("Failed to allocate memory. Discarding frame");
            free(run_prefix);
            free(output_dir);
            return;
        }

        snprintf(temppath, temppath_len, "%s/XXXXXX", output_dir);
        mktemp(temppath);
        if (strlen(temppath) == 0)
        {
            pn_log("Failed to create unique temporary filename. Discarding frame");
            free(run_prefix);
            free(output_dir);
            return;
        }

        if (!save_frame(frame, timestamp, temppath))
        {
            free(output_dir);
            free(run_prefix);
            free(temppath);
            pn_log("Saving to save temporary file. Discarding frame.");
            return;
        }

        size_t filepath_len = snprintf(NULL, 0, "%s/%s-%04d.fits.gz", output_dir, run_prefix, run_number) + 1;
        char *filepath = malloc(filepath_len*sizeof(char));
        if (!filepath)
        {
            pn_log("Failed to allocate memory. Discarding frame");
            free(run_prefix);
            free(output_dir);
            free(temppath);
            return;
        }
        snprintf(filepath, filepath_len, "%s/%s-%04d.fits.gz", output_dir, run_prefix, run_number);

        // Don't overwrite existing files
        if (!rename_atomically(temppath, filepath, false))
            pn_log("Failed to save `%s' (already exists?). Saved instead as `%s' ", last_path_component(filepath), last_path_component(temppath));
        else
        {
            scripting_notify_frame(scripting, filepath);
            pn_log("Saved `%s'.", last_path_component(filepath));
        }

        pn_preference_increment_framecount();
        free(filepath);
        free(temppath);

        free(run_prefix);
        free(output_dir);
    }

    // Update frame preview atomically
    char preview[16];
    strcpy(preview, "preview.XXXXXX");
    mktemp(preview);
    save_frame(frame, timestamp, preview);

    if (!rename_atomically(preview, "preview.fits.gz", true))
    {
        pn_log("Failed to overwrite preview frame.");
        delete_file(preview);
    }
    else
        scripting_update_preview(scripting);
}

void trigger_fatal_error(char *message)
{
    pn_log("Fatal Error: %s", message);
    fatal_error = message;
}

int main(int argc, char *argv[])
{
    // Parse the commandline args
    bool simulate_camera = false;
    bool simulate_timer = false;
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--simulate-camera") == 0)
            simulate_camera = true;

        if (strcmp(argv[i], "--simulate-timer") == 0)
        {
            simulate_timer = true;
            simulate_camera = true;
        }
    }

    // Initialization
    pthread_mutex_init(&reset_mutex, NULL);

    log_queue = atomicqueue_create();
    frame_queue = atomicqueue_create();
    trigger_queue = atomicqueue_create();

    if (!log_queue || !frame_queue || !trigger_queue)
    {
        fprintf(stderr, "Failed to allocate queues\n");
        return 1;
    }

    // Open the log file for writing
    time_t start = time(NULL);
    char namebuf[32];
    strftime(namebuf, 32, "logs/%Y%m%d-%H%M%S.log", gmtime(&start));
    FILE *logFile = fopen(namebuf, "w");
    if (logFile == NULL)
    {
        fprintf(stderr, "Unable to create logfile %s\n", namebuf);
        return 1;
    }

    pn_init_preferences("preferences.dat");
    timer = timer_new(simulate_timer);
    camera = camera_new(simulate_camera);
    scripting = scripting_new();

    if (!timer || !camera || !scripting)
    {
        fprintf(stderr, "Failed to allocate thread components\n");
        return 1;
    }

    // Start ui early so it can catch log events
    pn_ui_new(camera, timer);

    ThreadCreationArgs args;
    args.camera = camera;
    args.timer = timer;

    scripting_spawn_thread(scripting, &args);
    timer_spawn_thread(timer, &args);
    camera_spawn_thread(camera, &args);

    // Main program loop
    for (;;)
    {
        if (fatal_error)
        {
            pn_ui_show_fatal_error(fatal_error);
            break;
        }

        // Match frame with trigger and save to disk
        while (atomicqueue_length(frame_queue) && atomicqueue_length(trigger_queue))
        {
            CameraFrame *frame = atomicqueue_pop(frame_queue);
            TimerTimestamp *trigger = atomicqueue_pop(trigger_queue);

            // PVCAM/simulated triggers indicate the end of the frame
            // TODO: Move this into camera implementation
#ifndef USE_PICAM
            uint16_t exposure = pn_preference_int(EXPOSURE_TIME);
            if (pn_preference_char(TIMER_MILLISECOND_MODE))
            {
                trigger->seconds -= exposure / 1000;
                trigger->milliseconds -= exposure % 1000;
            }
            else
                trigger->seconds -= exposure;

            timestamp_normalize(trigger);
#endif

            double exptime = pn_preference_int(EXPOSURE_TIME);
            if (pn_preference_char(TIMER_MILLISECOND_MODE))
                exptime /= 1000;

            // Ensure that the trigger and frame download times are consistent
            double estimated_start_time = timestamp_to_unixtime(&frame->downloaded_time) - camera_readout_time(camera) - exptime;
            double mismatch = estimated_start_time - timestamp_to_unixtime(trigger);
            bool process = true;

            // Allow at least 1 second of leeway to account for the
            // delay in recieving the GPS time and any other factors
            if (fabs(mismatch) > 1.5)
            {
                if (pn_preference_char(VALIDATE_TIMESTAMPS))
                {
                    TimerTimestamp estimate_start = frame->downloaded_time;
                    estimate_start.seconds -= camera_readout_time(camera) + exptime;
                    timestamp_normalize(&estimate_start);

                    pn_log("ERROR: Estimated frame start doesn't match trigger start. Mismatch: %g", mismatch);
                    pn_log("Frame recieved: %02d:%02d:%02d", frame->downloaded_time.hours, frame->downloaded_time.minutes, frame->downloaded_time.seconds);
                    pn_log("Estimated frame start: %02d:%02d:%02d", estimate_start.hours, estimate_start.minutes, estimate_start.seconds);
                    pn_log("Trigger start: %02d:%02d:%02d", trigger->hours, trigger->minutes, trigger->seconds);

                    pn_log("Discarding all stored frames and triggers.");
                    clear_queued_data(false);
                    process = false;
                }
                else
                    pn_log("WARNING: Estimated frame start doesn't match trigger start. Mismatch: %g", mismatch);
            }

            if (process)
                process_framedata(frame, *trigger);

            free(trigger);
            free(frame->data);
            free(frame);
        }

        // Update UI with queued log messages
        char *log_message;
        while ((log_message = atomicqueue_pop(log_queue)))
        {
            fprintf(logFile, "%s\n", log_message);
            fflush(logFile);

            pn_ui_log_line(log_message);
            free(log_message);
        }

        bool request_shutdown = pn_ui_update();
        if (request_shutdown)
            break;
        
        millisleep(100);
    }

    // Wait for camera and timer threads to terminate
    camera_shutdown(camera);
    timer_shutdown(timer);
    scripting_shutdown(scripting);
    clear_queued_data(true);

    timer_free(timer);
    camera_free(camera);
    scripting_free(scripting);
    pn_free_preferences();
    pn_ui_free();

    // Save any final log messages
    char *log_message;
    while ((log_message = atomicqueue_pop(log_queue)))
    {
        fprintf(logFile, "%s\n", log_message);
        fflush(logFile);
        free(log_message);
    }
    fclose(logFile);

    // Final cleanup
    if (fatal_error)
        free(fatal_error);

    atomicqueue_destroy(trigger_queue);
    atomicqueue_destroy(frame_queue);
    atomicqueue_destroy(log_queue);
    pthread_mutex_destroy(&reset_mutex);

    return 0;
}

// Add a message to the gui and saved log files
void pn_log(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    // Allocate message string with additional space for a timestamp
    char *message = calloc(len+16,sizeof(char));
    if (!message)
    {
        fprintf(stderr, "Failed to allocate format for log message\n");
        return;
    }

    // Add timestamp to beginning of format string
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t seconds = tv.tv_sec;
    struct tm *ptm = gmtime(&seconds);

    // Construct log line
    strcat(message, "[");
    strftime(&message[1], 9, "%H:%M:%S", ptm);
    snprintf(&message[9], 5, ".%03d", (int)(tv.tv_usec / 1000));
    strcat(message, "] ");

    va_start(args, format);
    vsnprintf(&message[15], len + 1, format, args);
    va_end(args);

    // Store messages to update log from main thread
    if (!atomicqueue_push(log_queue, message))
    {
        fprintf(stderr, "Failed to push log message. Message has been ignored\n");
        free(message);
    }
}
