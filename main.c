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
#include "main.h"
#include "atomicqueue.h"
#include "camera.h"
#include "timer.h"
#include "scripting.h"
#include "preferences.h"
#include "gui.h"
#include "platform.h"
#include "version.h"


PNCamera *camera;
TimerUnit *timer;
ScriptingInterface *scripting;
pthread_mutex_t reset_mutex;

struct atomicqueue *log_queue, *frame_queue, *trigger_queue;
char *fatal_error = NULL;

void queue_framedata(CameraFrame *frame)
{
    frame->downloaded_time = timer_current_timestamp(timer);
    pthread_mutex_lock(&reset_mutex);
    bool success = atomicqueue_push(frame_queue, frame);
    pthread_mutex_unlock(&reset_mutex);

    if (success)
        pn_log("Pushed frame. %d in queue", atomicqueue_length(frame_queue));
    else
    {
        pn_log("Error pushing frame. Frame has been ignored");
        free(frame);
    }
}

void queue_trigger(TimerTimestamp *timestamp)
{
    pthread_mutex_lock(&reset_mutex);
    bool success = atomicqueue_push(trigger_queue, timestamp);
    pthread_mutex_unlock(&reset_mutex);
    if (success)
        pn_log("Pushed trigger. %d in queue", atomicqueue_length(trigger_queue));
    else
    {
        pn_log("Error pushing trigger. Frame has been ignored");
        free(timestamp);
    }
}

// Remove all queued frames and timestamps
void clear_queued_data()
{
    void *item;
    pthread_mutex_lock(&reset_mutex);

    while ((item = atomicqueue_pop(frame_queue)) != NULL)
    {
        pn_log("Discarding queued frame");
        free(item);
    }

    while ((item = atomicqueue_pop(trigger_queue)) != NULL)
    {
        pn_log("Discarding queued trigger");
        free(item);
    }

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
        pn_log("Unable to save file. fitsio error %d", status);
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

    char *instrument = pn_preference_string(INSTRUMENT);
    fits_update_key(fptr, TSTRING, "INSTRUME", (void *)instrument, "Instrument name", &status);
    free(instrument);

    fits_update_key(fptr, TSTRING, "PROG-VER", (void *)program_version() , "Acquisition program version reported by git", &status);

    if (pn_preference_char(CAMERA_OVERSCAN_ENABLED))
    {
        char buf[25];
        unsigned char skip = pn_preference_char(CAMERA_OVERSCAN_SKIP_COLS);
        unsigned char bias = pn_preference_char(CAMERA_OVERSCAN_BIAS_COLS);
        unsigned char superpixel = pn_preference_char(CAMERA_PIXEL_SIZE);
        snprintf(buf, 25, "[%d, %d, %d, %d]", 0, frame->width - (skip + bias)/superpixel, 0, frame->height);
        fits_update_key(fptr, TSTRING, "IMAG-RGN", buf, "Frame image subregion", &status);
        snprintf(buf, 25, "[%d, %d, %d, %d]", frame->width - skip/superpixel, frame->width, 0, frame->height);
        fits_update_key(fptr, TSTRING, "BIAS-RGN", buf, "Frame bias subregion", &status);
    }

    // Trigger timestamp defines the *start* of the frame
    TimerTimestamp start = timestamp;
    TimerTimestamp end = start; end.seconds += exposure_time;
    timestamp_normalize(&end);

    char datebuf[15], gpstimebuf[15];
    snprintf(datebuf, 15, "%04d-%02d-%02d", start.year, start.month, start.day);
    snprintf(gpstimebuf, 15, "%02d:%02d:%02d", start.hours, start.minutes, start.seconds);

    // Used by ImageJ and other programs
    fits_update_key(fptr, TSTRING, "UT_DATE", datebuf, "Exposure start date (GPS)", &status);
    fits_update_key(fptr, TSTRING, "UT_TIME", gpstimebuf, "Exposure start time (GPS)", &status);

    // Used by tsreduce
    fits_update_key(fptr, TSTRING, "UTC-DATE", datebuf, "Exposure start date (GPS)", &status);
    fits_update_key(fptr, TSTRING, "UTC-BEG", gpstimebuf, "Exposure start time (GPS)", &status);

    snprintf(gpstimebuf, 15, "%02d:%02d:%02d", end.hours, end.minutes, end.seconds);
    fits_update_key(fptr, TSTRING, "UTC-END", gpstimebuf, "Exposure end time (GPS)", &status);
    fits_update_key(fptr, TLOGICAL, "GPS-LOCK", &start.locked, "GPS time locked", &status);

    time_t pctime = time(NULL);

    char timebuf[15];
    strftime(timebuf, 15, "%Y-%m-%d", gmtime(&pctime));
    fits_update_key(fptr, TSTRING, "PC-DATE", (void *)timebuf, "PC Date when frame was saved to disk", &status);

    strftime(timebuf, 15, "%H:%M:%S", gmtime(&pctime));
    fits_update_key(fptr, TSTRING, "PC-TIME", (void *)timebuf, "PC Time when frame was saved to disk", &status);

    // Camera temperature
    char tempbuf[10];
    snprintf(tempbuf, 10, "%0.02f", camera->temperature);
    fits_update_key(fptr, TSTRING, "CCD-TEMP", (void *)tempbuf, "CCD temperature at end of exposure in deg C", &status);
    fits_update_key(fptr, TBYTE,   "CCD-PORT", (uint8_t[]){pn_preference_char(CAMERA_READPORT_MODE)},  "CCD Readout port index", &status);
    fits_update_key(fptr, TBYTE,   "CCD-RATE", (uint8_t[]){pn_preference_char(CAMERA_READSPEED_MODE)}, "CCD Readout rate index", &status);
    fits_update_key(fptr, TBYTE,   "CCD-GAIN", (uint8_t[]){pn_preference_char(CAMERA_GAIN_MODE)},      "CCD Readout gain index", &status);

    // Write the frame data to the image and close the file
    fits_write_img(fptr, TUSHORT, 1, frame->width*frame->height, frame->data, &status);
    fits_close_file(fptr, &status);

    // Log any error messages
    while (fits_read_errmsg(fitserr))
        pn_log("cfitsio error: %s", fitserr);

    return true;
}

void process_framedata(CameraFrame *frame, TimerTimestamp timestamp)
{
    pn_log("Frame downloaded");

    if (pn_preference_char(SAVE_FRAMES))
    {
        // Construct the output filepath from the output dir, run prefix, and run number.
        // Saving will fail if a file with the same name already exists
        int run_number = pn_preference_int(RUN_NUMBER);
        char *output_dir = pn_preference_string(OUTPUT_DIR);
        char *run_prefix = pn_preference_string(RUN_PREFIX);
        size_t filepath_len = strlen(output_dir) + strlen(run_prefix) + 15;
        char *filepath = malloc(filepath_len*sizeof(char));
        if (!filepath)
        {
            pn_log("filepath alloc failed. Discarding frame");
            free(run_prefix);
            free(output_dir);
            return;
        }

        snprintf(filepath, filepath_len, "%s/%s-%04d.fits.gz", output_dir, run_prefix, run_number);
        free(run_prefix);
        free(output_dir);

        pn_log("Saving frame %s", filepath);
        if (!save_frame(frame, timestamp, filepath))
        {
            pn_log("Save failed. Discarding frame");
            return;
        }

        pn_preference_increment_framecount();
        scripting_notify_frame(scripting, filepath);
        free(filepath);
    }

    // Update frame preview
    save_frame(frame, timestamp, "!preview.fits.gz");
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
    camera = pn_camera_new(simulate_camera);
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
    pn_camera_spawn_thread(camera, &args);

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

            // Ensure that the trigger and frame download times are consistent
            // TODO: This should take the camera readout time into account
            if (timestamp_to_time_t(&frame->downloaded_time) >= timestamp_to_time_t(trigger))
                process_framedata(frame, *trigger);
            else
            {
                pn_log("WARNING: Frame downloaded before trigger was received");
                pn_log("Download timestamp: %02d:%02d:%02d", frame->downloaded_time.hours, frame->downloaded_time.minutes, frame->downloaded_time.seconds);
                pn_log("Trigger timestamp: %02d:%02d:%02d", trigger->hours, trigger->minutes, trigger->seconds);
                pn_log("Discarding all stored frames and triggers");
                clear_queued_data();
            }
            free(trigger);
            free(frame->data);
            free(frame);
        }

        // Update UI with queued log messages
        char *log_message;
        while ((log_message = atomicqueue_pop(log_queue)) != NULL)
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
    pn_camera_shutdown(camera);
    timer_shutdown(timer);
    scripting_shutdown(scripting);

    // Final cleanup
    if (fatal_error)
        free(fatal_error);

    clear_queued_data();

    timer_free(timer);
    pn_camera_free(camera);
    scripting_free(scripting);
    pn_free_preferences();
    pn_ui_free();
    fclose(logFile);

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
