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
#include <signal.h>
#include <string.h>

#include "main.h"
#include "camera.h"
#include "timer.h"
#include "preferences.h"
#include "platform.h"

#include "camera_simulated.h"
#ifdef USE_PVCAM
#include "camera_pvcam.h"
#endif
#ifdef USE_PICAM
#include "camera_picam.h"
#endif

enum camera_type {PVCAM, PICAM, SIMULATED};

struct Camera
{
    enum camera_type type;
    void *internal;

    pthread_t camera_thread;
    bool thread_alive;

    pthread_mutex_t read_mutex;
    PNCameraMode desired_mode;
    PNCameraMode mode;
    bool desired_shutter;
    bool safe_to_stop_acquiring;

    struct camera_port_option *port_options;
    uint8_t port_count;
    double readout_time;
    double temperature;
    uint16_t ccd_region[4];

    bool camera_settings_dirty;

    int (*initialize)(Camera *, void **);
    int (*update_camera_settings)(Camera *, void *, double *);
    int (*port_table)(Camera *, void *, struct camera_port_option **, uint8_t *);
    int (*uninitialize)(Camera *, void *);
    int (*tick)(Camera *, void *, PNCameraMode);
    int (*start_acquiring)(Camera *, void *, bool);
    int (*stop_acquiring)(Camera *, void *);
    int (*read_temperature)(Camera *, void *, double *);
    int (*query_ccd_region)(Camera *, void *, uint16_t[4]);

    bool (*supports_readout_display)(Camera *, void *);
    bool (*supports_shutter_disabling)(Camera *, void *);
    bool (*supports_bias_acquisition)(Camera *, void *);
    void (*normalize_trigger)(Camera *, void *, TimerTimestamp *);
};

#define HOOK(type, suffix) camera->suffix = camera_##type##_##suffix
#define HOOK_FUNCTIONS(type)              \
{                                         \
    HOOK(type, initialize);               \
    HOOK(type, update_camera_settings);   \
    HOOK(type, port_table);               \
    HOOK(type, uninitialize);             \
    HOOK(type, tick);                     \
    HOOK(type, start_acquiring);          \
    HOOK(type, stop_acquiring);           \
    HOOK(type, read_temperature);         \
    HOOK(type, query_ccd_region);         \
    HOOK(type, supports_readout_display); \
    HOOK(type, supports_shutter_disabling); \
    HOOK(type, supports_bias_acquisition); \
    HOOK(type, normalize_trigger);        \
}

Camera *camera_new(bool simulate_hardware)
{
    Camera *camera = calloc(1, sizeof(Camera));
    if (!camera)
        return NULL;

    camera->mode = UNINITIALIZED;
    camera->desired_mode = IDLE;
    pthread_mutex_init(&camera->read_mutex, NULL);

    camera->type = SIMULATED;
    if (!simulate_hardware)
    {
#ifdef USE_PVCAM
        camera->type = PVCAM;
#elif defined USE_PICAM
        camera->type = PICAM;
#else
        camera->type = SIMULATED;
#endif
    }

    switch (camera->type)
    {
#ifdef USE_PVCAM
        case PVCAM: HOOK_FUNCTIONS(pvcam); break;
#endif
#ifdef USE_PICAM
        case PICAM: HOOK_FUNCTIONS(picam); break;
#endif
        default:
        case SIMULATED: HOOK_FUNCTIONS(simulated); break;
    }
    return camera;
}

void camera_free(Camera *camera)
{
    pthread_mutex_destroy(&camera->read_mutex);
}

static void set_mode(Camera *camera, PNCameraMode mode)
{
    pthread_mutex_lock(&camera->read_mutex);
    camera->mode = mode;
    pthread_mutex_unlock(&camera->read_mutex);
}

// Main camera thread loop
static void *camera_thread(void *_modules)
{
    const Modules *modules = _modules;
    Camera *camera = modules->camera;

    // Initialize hardware, etc
    set_mode(camera, INITIALISING);
    int ret = camera->initialize(camera, &camera->internal);
    if (ret == CAMERA_INITIALIZATION_ABORTED)
    {
        pn_log("Camera initialization aborted.");
		goto initialization_failure;
    }
    else if (ret != CAMERA_OK)
    {
        pn_log("Failed to initialize camera.");
        goto initialization_failure;
    }

    if (camera->port_table(camera, camera->internal, &camera->port_options, &camera->port_count) != CAMERA_OK)
    {
        pn_log("Failed to query port table");
        goto setup_failure;
    }

    if (camera->query_ccd_region(camera, camera->internal, camera->ccd_region) != CAMERA_OK)
    {
        pn_log("Failed to query ccd region");
        goto setup_failure;
    }

    double readout;
    if (camera->update_camera_settings(camera, camera->internal, &readout) != CAMERA_OK)
    {
        pn_log("Failed to update camera settings");
        goto setup_failure;
    }
    pthread_mutex_lock(&camera->read_mutex);
    camera->readout_time = readout;
    pthread_mutex_unlock(&camera->read_mutex);

    pn_log("Camera is now idle.");
    set_mode(camera, IDLE);

    // Loop and respond to user commands
    time_t last_temperature_check = 0;

    pthread_mutex_lock(&camera->read_mutex);
    PNCameraMode desired_mode = camera->desired_mode;
    bool safe_to_stop_acquiring = camera->safe_to_stop_acquiring;
    pthread_mutex_unlock(&camera->read_mutex);

    PNCameraMode current_mode;
    while (desired_mode != SHUTDOWN)
    {
        current_mode = camera_mode(camera);
        pthread_mutex_lock(&camera->read_mutex);
        bool camera_settings_dirty = camera->camera_settings_dirty;
        bool desired_shutter = camera->desired_shutter;
        pthread_mutex_unlock(&camera->read_mutex);

        if (current_mode == IDLE && camera_settings_dirty)
        {
            double readout;
            if (camera->update_camera_settings(camera, camera->internal, &readout) != CAMERA_OK)
            {
                pn_log("Failed to update camera settings");
                goto failure;
            }

            pthread_mutex_lock(&camera->read_mutex);
            camera->readout_time = readout;
            camera->camera_settings_dirty = false;
            pthread_mutex_unlock(&camera->read_mutex);
        }

        // Start/stop acquisition
        if (desired_mode == ACQUIRING && current_mode == IDLE)
        {
            set_mode(camera, ACQUIRE_START);
            pn_log("Camera is preparing for acquisition.");

            if (camera->start_acquiring(camera, camera->internal, desired_shutter) != CAMERA_OK)
            {
                pn_log("Failed to start camera acquisition");
                goto failure;
            }
            pn_log("Camera is now acquiring.");
            set_mode(camera, ACQUIRING);

            pthread_mutex_lock(&camera->read_mutex);
            camera->safe_to_stop_acquiring = false;
            pthread_mutex_unlock(&camera->read_mutex);
        }

        // Intermediate mode - waiting for the timer to tell us that
        // the hardware is ready to accept a stop acquisition command
        if (desired_mode == IDLE && current_mode == ACQUIRING)
        {
            set_mode(camera, IDLE_WHEN_SAFE);
            pn_log("Camera is waiting for safe shutdown.");
        }

        // Stop acquisition
        if (camera->mode == IDLE_WHEN_SAFE && safe_to_stop_acquiring)
        {
            set_mode(camera, ACQUIRE_STOP);

            if (camera->stop_acquiring(camera, camera->internal) != CAMERA_OK)
            {
                pn_log("Failed to stop camera acquisition");
                goto failure;
            }

            pn_log("Camera is now idle.");
            set_mode(camera, IDLE);
        }

        // Check for new frames, etc
        if (camera->tick(camera, camera->internal, current_mode) != CAMERA_OK)
        {
            pn_log("Camera tick failed");
            goto failure;
        }

        // Check temperature
        time_t t = time(NULL);
        if (difftime(t, last_temperature_check) > 5)
        {
            last_temperature_check = t;
            double temperature;
            if (camera->read_temperature(camera, camera->internal, &temperature)!= CAMERA_OK)
            {
                pn_log("Failed to query camera temperature");
                goto failure;
            }

            pthread_mutex_lock(&camera->read_mutex);
            camera->temperature = temperature;
            pthread_mutex_unlock(&camera->read_mutex);
        }
        millisleep(100);

        pthread_mutex_lock(&camera->read_mutex);
        desired_mode = camera->desired_mode;
        safe_to_stop_acquiring = camera->safe_to_stop_acquiring;
        pthread_mutex_unlock(&camera->read_mutex);
    }

failure:
    // Attempt to shutdown camera
    current_mode = camera_mode(camera);

    if (current_mode == ACQUIRING || current_mode == IDLE_WHEN_SAFE)
    {
        if (camera->stop_acquiring(camera, camera->internal) != CAMERA_OK)
            pn_log("Failed to stop camera acquisition");
        else
            pn_log("Camera is now idle.");
    }

setup_failure:
    // Uninitialize hardware, etc
    if (camera->uninitialize(camera, camera->internal) != CAMERA_OK)
        pn_log("Failed to uninitialize camera");

initialization_failure:
    pn_log("Camera uninitialized.");

    camera->thread_alive = false;
    return NULL;
}

void camera_spawn_thread(Camera *camera, const Modules *modules)
{
    camera->thread_alive = true;
    if (pthread_create(&camera->camera_thread, NULL, camera_thread, (void *)modules))
    {
        pn_log("Failed to create camera thread");
        camera->thread_alive = false;
    }
}

void camera_notify_shutdown(Camera *camera)
{
    pthread_mutex_lock(&camera->read_mutex);
    camera->desired_mode = SHUTDOWN;
    pthread_mutex_unlock(&camera->read_mutex);
}

void camera_join_thread(Camera *camera)
{
    void **retval = NULL;
    if (camera->thread_alive)
        pthread_join(camera->camera_thread, retval);
}

bool camera_thread_alive(Camera *camera)
{
    return camera->thread_alive;
}

void camera_notify_safe_to_stop(Camera *camera)
{
    pthread_mutex_lock(&camera->read_mutex);
    camera->safe_to_stop_acquiring = true;
    pthread_mutex_unlock(&camera->read_mutex);
}

bool camera_is_simulated(Camera *camera)
{
    return (camera->type == SIMULATED);
}

void camera_start_exposure(Camera *camera, bool shutter_open)
{
    pthread_mutex_lock(&camera->read_mutex);
    camera->desired_mode = ACQUIRING;
    camera->desired_shutter = shutter_open;
    pthread_mutex_unlock(&camera->read_mutex);
}

void camera_stop_exposure(Camera *camera)
{
    pthread_mutex_lock(&camera->read_mutex);
    camera->desired_mode = IDLE;
    pthread_mutex_unlock(&camera->read_mutex);
}

double camera_temperature(Camera *camera)
{
    pthread_mutex_lock(&camera->read_mutex);
    double temperature = camera->temperature;
    pthread_mutex_unlock(&camera->read_mutex);
    return temperature;
}

double camera_readout_time(Camera *camera)
{
    pthread_mutex_lock(&camera->read_mutex);
    double readout = camera->readout_time;
    pthread_mutex_unlock(&camera->read_mutex);
    return readout;
}

PNCameraMode camera_mode(Camera *camera)
{
    pthread_mutex_lock(&camera->read_mutex);
    PNCameraMode mode = camera->mode;
    pthread_mutex_unlock(&camera->read_mutex);
    return mode;
}

PNCameraMode camera_desired_mode(Camera *camera)
{
    pthread_mutex_lock(&camera->read_mutex);
    PNCameraMode mode = camera->desired_mode;
    pthread_mutex_unlock(&camera->read_mutex);
    return mode;
}

void camera_update_settings(Camera *camera)
{
    pthread_mutex_lock(&camera->read_mutex);
    camera->camera_settings_dirty = true;
    pthread_mutex_unlock(&camera->read_mutex);
}

bool camera_supports_readout_display(Camera *camera)
{
    return camera->supports_readout_display(camera, camera->internal);
}

bool camera_supports_shutter_disabling(Camera *camera)
{
    return camera->supports_shutter_disabling(camera, camera->internal);
}

bool camera_supports_bias_acquisition(Camera *camera)
{
    return camera->supports_bias_acquisition(camera, camera->internal);
}

void camera_normalize_trigger(Camera *camera, TimerTimestamp *trigger)
{
    camera->normalize_trigger(camera, camera->internal, trigger);
}

// Warning: These are not thread safe, but this is only touched by the camera
// thread during startup, when the main thread is designed to not call these
void camera_ccd_region(Camera *camera, uint16_t region[4])
{
    memcpy(region, camera->ccd_region, 4*sizeof(uint16_t));
}

uint8_t camera_port_options(Camera *camera, struct camera_port_option **options)
{
    *options = camera->port_options;
    return camera->port_count;
}

void camera_simulate_frame(Camera *camera)
{
    if (camera->type != SIMULATED)
        return;

    camera_simulated_trigger_frame(camera, camera->internal);
}
