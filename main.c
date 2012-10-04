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
#include "common.h"
#include "atomicqueue.h"
#include "camera.h"
#include "timer.h"
#include "scripting.h"
#include "preferences.h"
#include "gui.h"
#include "platform.h"
#include "version.h"

struct PNFrameQueue
{
    PNFrame *frame;
    struct PNFrameQueue *next;
};

struct TimerTimestampQueue
{
    TimerTimestamp timestamp;
    struct TimerTimestampQueue *next;
};

pthread_mutex_t frame_queue_mutex, trigger_timestamp_queue_mutex;
PNCamera *camera;
TimerUnit *timer;
ScriptingInterface *scripting;

struct atomicqueue *log_queue;

struct PNFrameQueue *frame_queue = NULL;
struct TimerTimestampQueue *trigger_timestamp_queue = NULL;
char *fatal_error = NULL;

// Take a copy of the framedata and store it for
// processing on the main thread
void queue_framedata(PNFrame *frame)
{
    PNFrame *copy = malloc(sizeof(PNFrame));
    struct PNFrameQueue *tail = malloc(sizeof(struct PNFrameQueue));
    if (!copy || !tail)
    {
        trigger_fatal_error("Allocation error in queue_framedata");
        return;
    }

    // Add to frame queue
    memcpy(copy, frame, sizeof(PNFrame));
    tail->frame = copy;
    tail->next = NULL;

    pthread_mutex_lock(&frame_queue_mutex);
    // Empty queue
    if (frame_queue == NULL)
        frame_queue = tail;
    else
    {
        // Find tail of queue - queue is assumed to be short
        struct PNFrameQueue *item = frame_queue;
        while (item->next != NULL)
            item = item->next;
        item->next = tail;
    }
    pthread_mutex_unlock(&frame_queue_mutex);
}

static PNFrame *pop_framedata()
{
    pthread_mutex_lock(&frame_queue_mutex);
    if (frame_queue == NULL)
    {
        pthread_mutex_unlock(&frame_queue_mutex);
        return NULL;
    }

    // Pop the head frame
    struct PNFrameQueue *head = frame_queue;
    frame_queue = frame_queue->next;
    pthread_mutex_unlock(&frame_queue_mutex);

    PNFrame *frame = head->frame;
    free(head);

    return frame;
}

/*
 * Add a timestamp to the trigger timestamp queue
 */
void queue_trigger_timestamp(TimerTimestamp timestamp)
{
    struct TimerTimestampQueue *tail = malloc(sizeof(struct TimerTimestampQueue));
    if (tail == NULL)
        trigger_fatal_error("Unexpected download - no timestamp available for frame");

    tail->timestamp = timestamp;
    tail->next = NULL;

    pthread_mutex_lock(&trigger_timestamp_queue_mutex);
    size_t count = 0;
    // Empty queue
    if (trigger_timestamp_queue == NULL)
        trigger_timestamp_queue = tail;
    else
    {
        // Find tail of queue - queue is assumed to be short
        struct TimerTimestampQueue *item = trigger_timestamp_queue;
        while (item->next != NULL)
        {
            item = item->next;
            count++;
        }
        item->next = tail;
    }
    count++;
    pthread_mutex_unlock(&trigger_timestamp_queue_mutex);
    pn_log("Pushed timestamp. %d in queue", count);
}

static TimerTimestamp pop_trigger_timestamp()
{
    pthread_mutex_lock(&trigger_timestamp_queue_mutex);
    if (trigger_timestamp_queue == NULL)
    {
        pthread_mutex_unlock(&trigger_timestamp_queue_mutex);
        return (TimerTimestamp) {.valid = false};
    }

    // Pop the head frame
    struct TimerTimestampQueue *head = trigger_timestamp_queue;
    trigger_timestamp_queue = trigger_timestamp_queue->next;
    pthread_mutex_unlock(&trigger_timestamp_queue_mutex);

    TimerTimestamp timestamp = head->timestamp;
    free(head);

    return timestamp;
}

// Remove all queued frames or timestamps atomically
void clear_queued_data()
{
    pthread_mutex_lock(&frame_queue_mutex);
    pthread_mutex_lock(&trigger_timestamp_queue_mutex);

    // Other threads have closed, so don't worry about locking
    while (frame_queue != NULL)
    {
        struct PNFrameQueue *next = frame_queue->next;
        free(frame_queue->frame);
        free(frame_queue);
        frame_queue = next;
        pn_log("Discarding queued framedata");
    }

    while (trigger_timestamp_queue != NULL)
    {
        struct TimerTimestampQueue *next = trigger_timestamp_queue->next;
        free(trigger_timestamp_queue);
        trigger_timestamp_queue = next;
        pn_log("Discarding queued timestamp");
    }

    pthread_mutex_unlock(&trigger_timestamp_queue_mutex);
    pthread_mutex_unlock(&frame_queue_mutex);
}

// Write frame data to a fits file
bool save_frame(PNFrame *frame, TimerTimestamp timestamp, float camera_temperature, char *filepath)
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
    char tempbuf[15];
    sprintf(tempbuf, "%0.02f", camera_temperature);
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

void process_framedata(PNFrame *frame)
{
    TimerTimestamp timestamp = pop_trigger_timestamp();
    pn_log("Frame downloaded");
    float camera_temperature = pn_camera_temperature();

    if (pn_preference_char(SAVE_FRAMES))
    {
        // Construct the output filepath from the output dir, run prefix, and run number.
        // Saving will fail if a file with the same name already exists
        char *filepath;
        int run_number = pn_preference_int(RUN_NUMBER);
        char *output_dir = pn_preference_string(OUTPUT_DIR);
        char *run_prefix = pn_preference_string(RUN_PREFIX);
        asprintf(&filepath, "%s/%s-%04d.fits.gz", output_dir, run_prefix, run_number);
        free(run_prefix);
        free(output_dir);

        pn_log("Saving frame %s", filepath);
        if (!save_frame(frame, timestamp, camera_temperature, filepath))
        {
            pn_log("Save failed. Discarding frame");
            return;
        }

        pn_preference_increment_framecount();
        scripting_notify_frame(scripting, filepath);
        free(filepath);
    }

    // Update frame preview
    save_frame(frame, timestamp, camera_temperature, "!preview.fits.gz");
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
            simulate_timer = true;
    }

    // Initialization
    pthread_mutex_init(&frame_queue_mutex, NULL);
    pthread_mutex_init(&trigger_timestamp_queue_mutex, NULL);

    log_queue = atomicqueue_create();

    // Open the log file for writing
    time_t start = time(NULL);
    char namebuf[32];
    strftime(namebuf, 32, "logs/%Y%m%d-%H%M%S.log", gmtime(&start));
    FILE *logFile = fopen(namebuf, "w");
    if (logFile == NULL)
    {
        fprintf(stderr, "Unable to create logfile %s\n", namebuf);
        exit(1);
    }

    pn_init_preferences("preferences.dat");
    timer = timer_new(simulate_timer);
    camera = pn_camera_new(simulate_camera);
    scripting = scripting_new();

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

        // Process stored frames
        PNFrame *frame = pop_framedata();
        while (frame != NULL)
        {
            process_framedata(frame);
            free(frame);
            frame = pop_framedata();
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

    pthread_mutex_destroy(&trigger_timestamp_queue_mutex);
    pthread_mutex_destroy(&frame_queue_mutex);
    atomicqueue_destroy(log_queue);

    return 0;
}

// Add a message to the gui and saved log files
void pn_log(const char *format, ...)
{
    // Add timestamp to beginning of format string
    size_t new_format_len = strlen(format) + 18;
    char *new_format = malloc(new_format_len*sizeof(char));
    if (!new_format)
        return;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t seconds = tv.tv_sec;
    struct tm *ptm = gmtime(&seconds);
    char timebuf[9];
    strftime(timebuf, 9, "%H:%M:%S", ptm);
    snprintf(new_format, new_format_len, "[%s.%03d] %s", timebuf, (int)(tv.tv_usec / 1000), format);

    // Construct log line
    char *message;
    va_list args;
    va_start(args, format);
    vasprintf(&message, new_format, args);
    va_end(args);
    free(new_format);

    // Store messages to update log from main thread
    if (!atomicqueue_push(log_queue, message))
        free(message);
}
