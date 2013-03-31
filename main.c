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
#include "preview_script.h"
#include "reduction_script.h"
#include "preferences.h"
#include "gui.h"
#include "platform.h"
#include "frame_manager.h"

Modules *modules;
struct atomicqueue *log_queue;

// Passes frame data from Camera implementation -> FrameManager thread
void queue_framedata(CameraFrame *f)
{
    f->downloaded_time = timer_current_timestamp(modules->timer);
    frame_manager_queue_frame(modules->frame, f);
}

// Passes trigger data from Timer -> FrameManager thread
void queue_trigger(TimerTimestamp *t)
{
    if (camera_is_simulated(modules->camera))
        camera_simulate_frame(modules->camera);

    frame_manager_queue_trigger(modules->frame, t);
}

void clear_queued_data(bool reset_first_frame)
{
    frame_manager_purge_queues(modules->frame, reset_first_frame);
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
    log_queue = atomicqueue_create();
    if (!log_queue)
    {
        fprintf(stderr, "Failed to allocate log queue\n");
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

    Modules temp = (Modules)
    {
        .camera = camera_new(simulate_camera),
        .timer = timer_new(simulate_timer),
        .frame = frame_manager_new(),
        .preview = preview_script_new(),
        .reduction = reduction_script_new()
    };

    modules = calloc(1, sizeof(Modules));
    memcpy(modules, &temp, sizeof(Modules));

    if (!modules->timer || !modules->camera || !modules->reduction || !modules->preview || !modules->frame)
    {
        fprintf(stderr, "Failed to allocate thread components\n");
        return 1;
    }

    // Start ui early so it can catch log events
    pn_ui_new(modules->camera, modules->timer);

    reduction_script_spawn_thread(modules->reduction, modules);
    preview_script_spawn_thread(modules->preview, modules);
    frame_manager_spawn_thread(modules->frame, modules);
    timer_spawn_thread(modules->timer, modules);
    camera_spawn_thread(modules->camera, modules);

    // Main program loop
    enum main_status {NORMAL, ERROR, SHUTDOWN};
    enum main_status status = NORMAL;
    time_t last_shutdown_update = 0;
    for (;;)
    {
        if (status == NORMAL && (!camera_thread_alive(modules->camera) || !timer_thread_alive(modules->timer) ||
            !frame_manager_thread_alive(modules->frame) ||
            !reduction_script_thread_alive(modules->reduction) || !preview_script_thread_alive(modules->preview)))
        {
            pn_ui_show_fatal_error();
            camera_notify_shutdown(modules->camera);
            timer_notify_shutdown(modules->timer);
            frame_manager_notify_shutdown(modules->frame);
            reduction_script_notify_shutdown(modules->reduction);
            preview_script_notify_shutdown(modules->preview);
            status = ERROR;
            pn_log("A fatal error has occurred.");
            pn_log("Uninitializing hardware...");
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
            camera_notify_shutdown(modules->camera);
            timer_notify_shutdown(modules->timer);
            frame_manager_notify_shutdown(modules->frame);
            reduction_script_notify_shutdown(modules->reduction);
            preview_script_notify_shutdown(modules->preview);
            status = SHUTDOWN;
        }

        // Normal shutdown complete
        if (status == SHUTDOWN)
        {
            bool ca = camera_thread_alive(modules->camera);
            bool ta = timer_thread_alive(modules->timer);
            bool fa = frame_manager_thread_alive(modules->frame);
            bool ra = reduction_script_thread_alive(modules->reduction);
            bool pa = preview_script_thread_alive(modules->preview);

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
                if (fa)
                    pn_log("Waiting for frame manager thread to terminate...");
                if (ra)
                    pn_log("Waiting for reduction thread to terminate...");
                if (pa)
                    pn_log("Waiting for preview thread to terminate...");
            }
        }

        millisleep(100);
    }

    // Wait for camera and timer threads to terminate
    timer_join_thread(modules->timer);
    camera_join_thread(modules->camera);
    frame_manager_join_thread(modules->frame);
    reduction_script_join_thread(modules->reduction);
    preview_script_join_thread(modules->preview);

    timer_free(modules->timer);
    camera_free(modules->camera);
    frame_manager_free(modules->frame);
    reduction_script_free(modules->reduction);
    preview_script_free(modules->preview);

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

    atomicqueue_destroy(log_queue);

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
