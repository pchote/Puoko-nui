/*
* Copyright 2010, 2011 Paul Chote
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
#include "common.h"
#include "camera.h"
#include "gps.h"
#include "preferences.h"
#include "ui.h"
#include "imagehandler.h"

#include <assert.h>

PNCamera *camera;
PNGPS *gps;


#pragma mark Main program logic

static pthread_t timer_thread, camera_thread;
static bool timer_thread_initialized = false;
static bool camera_thread_initialized = false;
static bool shutdown = false;

pthread_mutex_t log_mutex, frame_queue_mutex;
FILE *logFile;

struct PNFrameQueue
{
    PNFrame *frame;
    struct PNFrameQueue *next;
};

struct PNFrameQueue *frame_queue = NULL;

// Take a copy of the framedata and store it for
// processing on the main thread
void queue_framedata(PNFrame *frame)
{
    // TODO: Replace asserts with something more hardware cleanup friendly
    PNFrame *copy = malloc(sizeof(PNFrame));
    assert(copy != NULL);
    memcpy(copy, frame, sizeof(PNFrame));

    // Add to frame queue
    struct PNFrameQueue *tail = malloc(sizeof(struct PNFrameQueue));
    assert(tail);

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

void process_framedata(PNFrame *frame)
{
    // Pop the head frame from the queue
    PNGPSTimestamp timestamp = pn_gps_pop_trigger();

    pn_log("Frame downloaded");
    if (pn_preference_char(SAVE_FRAMES))
    {
        pn_save_frame(frame, timestamp);
        pn_preference_increment_framecount();
    }

    // Display the frame in ds9
    pn_preview_frame(frame, timestamp);

    free(frame);
}

int main(int argc, char *argv[])
{
    //
    // Initialization
    //
    pthread_mutex_init(&log_mutex, NULL);
    pthread_mutex_init(&frame_queue_mutex, NULL);

    PNGPS _gps = pn_gps_new();
    gps = &_gps;
    
    PNCamera _camera = pn_camera_new();
    camera = &_camera;

    // Open the log file for writing
    time_t start = time(NULL);
    char namebuf[32];
    strftime(namebuf, 32, "logs/%Y%m%d-%H%M%S.log", gmtime(&start));
    logFile = fopen(namebuf, "w");
    if (logFile == NULL)
    {
        fprintf(stderr, "Unable to create logfile %s\n", namebuf);
        exit(1);
    }
    init_log_gui();

    launch_ds9();
    pn_init_preferences("preferences.dat");

    bool simulate_camera = false;
    bool simulate_timer = false;

    // Parse the commandline args
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--simulate-camera") == 0)
            simulate_camera = true;

        if (strcmp(argv[i], "--simulate-timer") == 0)
            simulate_timer = true;
    }

    // Timer unit
    if (simulate_timer)
        pthread_create(&timer_thread, NULL, pn_simulated_timer_thread, (void *)&gps);
    else
        pthread_create(&timer_thread, NULL, pn_timer_thread, (void *)&gps);

    timer_thread_initialized = true;

    if (simulate_camera)
        pthread_create(&camera_thread, NULL, pn_simulated_camera_thread, (void *)&camera);
    else
    {
        #ifdef USE_PVCAM
        pthread_create(&camera_thread, NULL, pn_pvcam_camera_thread, (void *)&camera);
		#elif defined USE_PICAM
		pthread_create(&camera_thread, NULL, pn_picam_camera_thread, (void *)&camera);
        #else
        pthread_create(&camera_thread, NULL, pn_simulated_camera_thread, (void *)&camera);
        #endif
    }
    camera_thread_initialized = true;

    //
    // Main program loop
    //
    pn_ui_new();

    for (;;)
    {
        // Process stored frames
        struct PNFrameQueue *head = NULL;
        do
        {
            pthread_mutex_lock(&frame_queue_mutex);
            head = frame_queue;

            // No frames to process
            if (head == NULL)
            {
                pthread_mutex_unlock(&frame_queue_mutex);
                break;
            }

            // Pop the head frame
            frame_queue = frame_queue->next;
            pthread_mutex_unlock(&frame_queue_mutex);

            PNFrame *frame = head->frame;
            free(head);
            process_framedata(frame);
        } while (head != NULL);

        bool request_shutdown = pn_ui_update();
        if (request_shutdown)
            break;
    }

    pn_ui_free();

    //
    // Shutdown hardware and cleanup
    //
    shutdown = true;

    // Tell the GPS and Camera threads to terminate themselves
    pthread_mutex_lock(&camera->read_mutex);
    camera->desired_mode = SHUTDOWN;
    pthread_mutex_unlock(&camera->read_mutex);
    gps->shutdown = true;

    // Wait for the GPS and Camera threads to terminate
    void **retval = NULL;
    if (camera_thread_initialized)
        pthread_join(camera_thread, retval);
    if (timer_thread_initialized)
        pthread_join(timer_thread, retval);

    // Final cleanup
    pn_gps_free(gps);
    pn_camera_free(camera);
    pn_free_preferences();
    fclose(logFile);
    pthread_mutex_destroy(&frame_queue_mutex);
    pthread_mutex_destroy(&log_mutex);

    return 0;
}

// Add a message to the gui and saved log files
void pn_log(const char * format, ...)
{
    va_list args;
    va_start(args, format);

    // Log time
    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm* ptm = gmtime(&tv.tv_sec);
    char timebuf[9];
    strftime(timebuf, 9, "%H:%M:%S", ptm);

    pthread_mutex_lock(&log_mutex);

    // Construct log line
    char *msgbuf, *linebuf;
    vasprintf(&msgbuf, format, args);
    asprintf(&linebuf, "[%s.%03d] %s", timebuf, tv.tv_usec / 1000, msgbuf);
    free(msgbuf);

    // Log to file
    fprintf(logFile, "%s", linebuf);
    fprintf(logFile, "\n");

    // Add to gui
    if (!shutdown)
        add_log_line(linebuf);

    pthread_mutex_unlock(&log_mutex);

    free(linebuf);
    va_end(args);
}
