/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#include "common.h"
#include "camera.h"
#include "timer.h"
#include "preferences.h"
#include "platform.h"

#pragma mark Creation and Destruction (Called from main thread)

// Initialize a new PNCamera struct.
PNCamera pn_camera_new()
{
    PNCamera cam;
    cam.mode = UNINITIALIZED;
    cam.desired_mode = IDLE;
    cam.temperature = 0;
    cam.readout_time = 0;
    cam.simulated = false;
    cam.safe_to_stop_acquiring = false;
    pthread_mutex_init(&cam.read_mutex, NULL);

    return cam;
}

// Destroy a PNCamera struct.
void pn_camera_free(PNCamera *cam)
{
    pthread_mutex_destroy(&cam->read_mutex);
}


#pragma mark Camera Routines (Called from camera thread)
extern PNCamera *camera;

// Set the camera mode to be read by the other threads in a threadsafe manner
// Only to be used by camera implementations.
void set_mode(PNCameraMode mode)
{
    pthread_mutex_lock(&camera->read_mutex);
    camera->mode = mode;
    pthread_mutex_unlock(&camera->read_mutex);
}

// Request a new camera mode from another thread
void pn_camera_request_mode(PNCameraMode mode)
{
    pthread_mutex_lock(&camera->read_mutex);
    camera->desired_mode = mode;
    pthread_mutex_unlock(&camera->read_mutex);
}

void pn_camera_notify_safe_to_stop()
{
    pthread_mutex_lock(&camera->read_mutex);
    camera->safe_to_stop_acquiring = true;
    pthread_mutex_unlock(&camera->read_mutex);
}

bool pn_camera_is_simulated()
{
    return camera->simulated;
}

#pragma mark Simulated Camera Routines
static void *image_buffer = NULL;

// Stop an acquisition sequence
static void stop_acquiring_simulated()
{
    set_mode(ACQUIRE_STOP);
    free(image_buffer);
    pn_log("Acquisition sequence uninitialized");
    set_mode(IDLE);
}

// Main simulated camera thread loop
void *pn_simulated_camera_thread(void *_timer)
{
    TimerUnit *timer = (TimerUnit *)_timer;

    // Initialize the camera
    camera->simulated = true;
    camera->safe_to_stop_acquiring = false;
    pn_log("Initialising simulated camera");

    // Wait a bit to simulate hardware startup time
    millisleep(2000);
    pn_log("Camera initialized");
    set_mode(IDLE);

    // Loop and respond to user commands
    pthread_mutex_lock(&camera->read_mutex);
    PNCameraMode desired_mode = camera->desired_mode;
    bool safe_to_stop_acquiring = camera->safe_to_stop_acquiring;
    pthread_mutex_unlock(&camera->read_mutex);

    while (desired_mode != SHUTDOWN)
    {
        // Start Acquisition
        if (desired_mode == ACQUIRING && camera->mode == IDLE)
        {
            set_mode(ACQUIRE_START);

            camera->frame_height = 512;
            camera->frame_width = 512;

            // Create a buffer to write a simulated frame to
            image_buffer = (uint16_t*)malloc(512*512*2);

            // Delay a bit to simulate hardware startup time
            millisleep(2000);
            pn_log("Simulated acquisition run started");

            set_mode(ACQUIRING);
        }

        // Enter an intermediate waiting state while we wait for the
        // timer to say it is safe to stop the acquisition sequence
        if (desired_mode == IDLE && camera->mode == ACQUIRING)
            camera->mode = IDLE_WHEN_SAFE;

        // Stop acquisition
        if (camera->mode == IDLE_WHEN_SAFE && safe_to_stop_acquiring)
            stop_acquiring_simulated();

        // Check for new frame
        bool downloading = timer_camera_downloading(timer);

        if (camera->mode == ACQUIRING && downloading)
        {
            pn_log("Frame available @ %d", (int)time(NULL));

            // Do something with the frame data
            PNFrame frame;
            frame.width = camera->frame_width;
            frame.height = camera->frame_height;
            frame.data = image_buffer;
            queue_framedata(&frame);

            // There is no physical camera for the timer to monitor
            // so we must toggle this manually

            timer_set_simulated_camera_downloading(timer, false);
        }

        millisleep(100);
        pthread_mutex_lock(&camera->read_mutex);
        desired_mode = camera->desired_mode;
        safe_to_stop_acquiring = camera->safe_to_stop_acquiring;
        pthread_mutex_unlock(&camera->read_mutex);
    }

    // Shutdown camera
    if (camera->mode == ACQUIRING)
        stop_acquiring_simulated();

    pthread_exit(NULL);
}

