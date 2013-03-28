/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <math.h>

#include "main.h"
#include "atomicqueue.h"
#include "camera.h"
#include "timer.h"
#include "scripting.h"
#include "preferences.h"
#include "gui.h"
#include "platform.h"
#include "frame.h"

Camera *camera;
TimerUnit *timer;
ScriptingInterface *scripting;
pthread_mutex_t reset_mutex;

struct atomicqueue *log_queue, *frame_queue, *trigger_queue;
bool first_frame = true;

// Called by the camera thread to pass ownership of an acquired
// frame to the main thread for processing.
void queue_framedata(CameraFrame *frame)
{
    frame->downloaded_time = timer_current_timestamp(timer);
    TimerTimestamp *t = &frame->downloaded_time;

    pthread_mutex_lock(&reset_mutex);
    bool success = atomicqueue_push(frame_queue, frame);
    pthread_mutex_unlock(&reset_mutex);

    if (!success)
    {
        pn_log("Failed to push frame. Discarding.");
        free(frame);
    }
}

// Called by the timer thread to pass ownership of a received
// trigger timestamp to the main thread for processing.
void queue_trigger(TimerTimestamp *t)
{
    pthread_mutex_lock(&reset_mutex);
    bool success = atomicqueue_push(trigger_queue, t);
    pthread_mutex_unlock(&reset_mutex);

    if (!success)
    {
        pn_log("Failed to push trigger. Discarding.");
        free(t);
    }
}

// Called by main thread to remove all queued frames and
// triggers before starting an acquisition or if a match
// error occurs.
void clear_queued_data(bool reset_first_frame)
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

    if (reset_first_frame)
        first_frame = true;

    pthread_mutex_unlock(&reset_mutex);
}

// Helper function for determining the
// filepath of the next frame
static char *next_filepath()
{
    // Construct the output filepath from the output dir, run prefix, and run number.
    int run_number = pn_preference_int(RUN_NUMBER);
    char *output_dir = pn_preference_string(OUTPUT_DIR);
    char *run_prefix = pn_preference_string(RUN_PREFIX);

    size_t filepath_len = snprintf(NULL, 0, "%s/%s-%04d.fits.gz", output_dir, run_prefix, run_number) + 1;
    char *filepath = malloc(filepath_len*sizeof(char));

    if (filepath)
        snprintf(filepath, filepath_len, "%s/%s-%04d.fits.gz", output_dir, run_prefix, run_number);

    free(run_prefix);
    free(output_dir);

    return filepath;
}

static char *temporary_filepath(const char *dir)
{
    size_t dirlen = strlen(dir);
    char *path = malloc((dirlen + 19)*sizeof(char));

    if (path)
    {
        strcpy(path, dir);

        size_t n = 0;
        do
        {
            // Give up after 1000 failed attempts
            if (n++ > 1000)
            {
                free(path);
                return NULL;
            }

            // Windows will only return numbers in the range 0-0x7FFF
            // but this still gives 32k potential files
            uint32_t test = rand() & 0xFFFF;
            snprintf(path + dirlen, 19, "/temp-%04x.fits.gz", test);
        }
        while (file_exists(path));
    }

    return path;
}

// Called by the main thread to save a matched frame
// and trigger timestamp to disk.
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

    frame_process_transforms(frame);
    if (pn_preference_char(SAVE_FRAMES))
    {
        char *filepath = next_filepath();
        if (!filepath)
        {
            pn_log("Failed to determine next file path. Discarding frame");
            return;
        }

        char *dir = pn_preference_string(OUTPUT_DIR);
        char *temppath = temporary_filepath(dir);
        free(dir);
        if (!filepath)
        {
            pn_log("Failed to create unique temporary filename. Discarding frame");
            return;
        }

        if (!frame_save(frame, timestamp, temppath))
        {
            free(filepath);
            free(temppath);
            pn_log("Saving to save temporary file. Discarding frame.");
            return;
        }

        // Don't overwrite existing files
        if (!rename_atomically(temppath, filepath, false))
            pn_log("Failed to save `%s' (already exists?). Saved instead as `%s' ",
                   last_path_component(filepath), last_path_component(temppath));
        else
        {
            scripting_notify_frame(scripting, filepath);
            pn_log("Saved `%s'.", last_path_component(filepath));
        }

        pn_preference_increment_framecount();
        free(filepath);
        free(temppath);
    }

    // Update frame preview atomically
    char *temp_preview = temporary_filepath(".");
    if (!temp_preview)
    {
        pn_log("Error creating temporary filepath. Skipping preview");
        return;
    }

    frame_save(frame, timestamp, temp_preview);
    if (!rename_atomically(temp_preview, "preview.fits.gz", true))
    {
        pn_log("Failed to overwrite preview frame.");
        delete_file(temp_preview);
    }
    else
        scripting_update_preview(scripting);

    free(temp_preview);
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

    // Seed random number generator
    // Only used for generating random unused filenames and generating
    // simulated frame data, so the default rand() is acceptable
    srand(time(NULL));

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

    scripting_spawn_threads(scripting, &args);
    timer_spawn_thread(timer, &args);
    camera_spawn_thread(camera, &args);

    // Main program loop
    enum main_status {NORMAL, ERROR, SHUTDOWN};
    enum main_status status = NORMAL;
    time_t last_shutdown_update = 0;
    for (;;)
    {
        if (status == NORMAL && (!camera_thread_alive(camera) || !timer_thread_alive(timer) ||
            !scripting_reduction_thread_alive(scripting) || !scripting_preview_thread_alive(scripting)))
        {
            pn_ui_show_fatal_error();
            camera_notify_shutdown(camera);
            timer_notify_shutdown(timer);
            scripting_notify_shutdown(scripting);
            status = ERROR;
            pn_log("A fatal error has occurred.");
            pn_log("Uninitializing hardware...");
        }

        // Match frame with trigger and save to disk
        while (status == NORMAL && atomicqueue_length(frame_queue) && atomicqueue_length(trigger_queue))
        {
            CameraFrame *frame = atomicqueue_pop(frame_queue);
            TimerTimestamp *trigger = atomicqueue_pop(trigger_queue);

            // Conver trigger to start of the exposure
            camera_normalize_trigger(camera, trigger);

            double exptime = pn_preference_int(EXPOSURE_TIME);
            if (pn_preference_char(TIMER_HIGHRES_TIMING))
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
        if ((status == NORMAL || status == ERROR) && request_shutdown)
        {
            camera_notify_shutdown(camera);
            timer_notify_shutdown(timer);
            scripting_notify_shutdown(scripting);
            status = SHUTDOWN;
        }

        // Normal shutdown complete
        if (status == SHUTDOWN)
        {
            bool ca = camera_thread_alive(camera);
            bool ta = timer_thread_alive(timer);
            bool ra = scripting_reduction_thread_alive(scripting);
            bool pa = scripting_preview_thread_alive(scripting);

            time_t current = time(NULL);
            // Threads have terminated - continue shutdown
            if (!ca && !ta && !ra && !pa)
                break;
            else if (current > last_shutdown_update + 5)
            {
                // Update log with shutdown status every 5 seconds
                last_shutdown_update = current;
                if (ca)
                    pn_log("Waiting for camera thread to terminate...");
                if (ta)
                    pn_log("Waiting for timer thread to terminate...");
                if (ra)
                    pn_log("Waiting for reduction thread to terminate...");
                if (pa)
                    pn_log("Waiting for preview thread to terminate...");
            }
        }

        millisleep(100);
    }

    // Wait for camera and timer threads to terminate
    timer_join_thread(timer);
    camera_join_thread(camera);
    scripting_join_threads(scripting);

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
    clear_queued_data(true);

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
    char *message = calloc(len + 16,sizeof(char));
    if (!message)
    {
        fprintf(stderr, "Failed to allocate format for log message\n");
        return;
    }

    // Add timestamp to beginning of format string
    TimerTimestamp t = system_time();
    snprintf(message, 16, "[%02d:%02d:%02d.%03d] ", t.hours, t.minutes, t.seconds, t.milliseconds);

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
