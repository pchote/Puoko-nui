/*
 * Copyright 2010-2012 Paul Chote
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


#ifdef USE_PVCAM
void *pn_pvcam_camera_thread(void *);
#endif
#ifdef USE_PICAM
void *pn_picam_camera_thread(void *);
#endif
void *pn_simulated_camera_thread(void *);

#pragma mark Creation and Destruction (Called from main thread)

// Initialize a new PNCamera struct.
PNCamera *pn_camera_new(bool simulate_hardware)
{
    PNCamera *camera = malloc(sizeof(PNCamera));
    if (!camera)
        trigger_fatal_error("Malloc failed while allocating timer");

    camera->mode = UNINITIALIZED;
    camera->desired_mode = IDLE;
    camera->temperature = 0;
    camera->readout_time = 0;
    camera->simulated = simulate_hardware;
    camera->safe_to_stop_acquiring = false;
    camera->thread_initialized = false;
    pthread_mutex_init(&camera->read_mutex, NULL);

    return camera;
}

// Destroy a PNCamera struct.
void pn_camera_free(PNCamera *camera)
{
    pthread_mutex_destroy(&camera->read_mutex);
}

void pn_camera_spawn_thread(PNCamera *camera, ThreadCreationArgs *args)
{
    if (camera->simulated)
        pthread_create(&camera->camera_thread, NULL, pn_simulated_camera_thread, (void *)args);
    else
    {
#ifdef USE_PVCAM
        pthread_create(&camera->camera_thread, NULL, pn_pvcam_camera_thread, (void *)args);
#elif defined USE_PICAM
		pthread_create(&camera->camera_thread, NULL, pn_picam_camera_thread, (void *)args);
#else
        pthread_create(&camera->camera_thread, NULL, pn_simulated_camera_thread, (void *)args);
#endif
    }
    camera->thread_initialized = true;
}

// Tell camera to shutdown and block until it completes
void pn_camera_shutdown(PNCamera *camera)
{
    pthread_mutex_lock(&camera->read_mutex);
    camera->desired_mode = SHUTDOWN;
    pthread_mutex_unlock(&camera->read_mutex);

    void **retval = NULL;
    if (camera->thread_initialized)
        pthread_join(camera->camera_thread, retval);

    camera->thread_initialized = false;
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
void pn_camera_start_exposure()
{
    pthread_mutex_lock(&camera->read_mutex);
    camera->desired_mode = ACQUIRING;
    pthread_mutex_unlock(&camera->read_mutex);
}

void pn_camera_stop_exposure()
{
    pthread_mutex_lock(&camera->read_mutex);
    camera->desired_mode = IDLE;
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
    // Not touched by camera thread, so safe to access without locks
    return camera->simulated;
}

float pn_camera_temperature()
{
    pthread_mutex_lock(&camera->read_mutex);
    float temperature = camera->temperature;
    pthread_mutex_unlock(&camera->read_mutex);
    return temperature;
}

float pn_camera_readout_time()
{
    pthread_mutex_lock(&camera->read_mutex);
    float readout_time = camera->readout_time;
    pthread_mutex_unlock(&camera->read_mutex);
    return readout_time;
}

// TODO: This is a temporary measure - callers shouldn't
// know about PNCameraMode
PNCameraMode pn_camera_mode()
{
    pthread_mutex_lock(&camera->read_mutex);
    PNCameraMode mode = camera->mode;
    pthread_mutex_unlock(&camera->read_mutex);
    return mode;
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
void *pn_simulated_camera_thread(void *_args)
{
    ThreadCreationArgs *args = (ThreadCreationArgs *)_args;
    TimerUnit *timer = args->timer;
    PNCamera *camera = args->camera;

    // Initialize the camera
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

    return NULL;
}

