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
    bool thread_initialized;

    pthread_mutex_t read_mutex;
    PNCameraMode desired_mode;
    PNCameraMode mode;
    bool safe_to_stop_acquiring;

    struct camera_port_option *port_options;
    uint8_t port_count;
    double readout_time;
    double temperature;
    uint16_t ccd_region[4];

    bool camera_settings_dirty;

    void *(*initialize)(Camera *, ThreadCreationArgs *);
    double (*update_camera_settings)(Camera *, void *);
    uint8_t (*port_table)(Camera *, void *, struct camera_port_option **);
    void (*uninitialize)(Camera *, void *);
    void (*tick)(Camera *, void *, PNCameraMode, double);
    void (*start_acquiring)(Camera *, void *);
    void (*stop_acquiring)(Camera *, void *);
    double (*read_temperature)(Camera *, void *);
    void (*query_ccd_region)(Camera *, void *, uint16_t[4]);
};

#define HOOK(type, suffix) camera->suffix = camera_##type##_##suffix
#define HOOK_FUNCTIONS(type)            \
{                                       \
    HOOK(type, initialize);             \
    HOOK(type, update_camera_settings); \
    HOOK(type, port_table);             \
    HOOK(type, uninitialize);           \
    HOOK(type, tick);                   \
    HOOK(type, start_acquiring);        \
    HOOK(type, stop_acquiring);         \
    HOOK(type, read_temperature);       \
    HOOK(type, query_ccd_region);       \
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

static void set_desired_mode(Camera *camera, PNCameraMode mode)
{
    pthread_mutex_lock(&camera->read_mutex);
    camera->desired_mode = mode;
    pthread_mutex_unlock(&camera->read_mutex);
}

// Main camera thread loop
static void *camera_thread(void *_args)
{
    ThreadCreationArgs *args = (ThreadCreationArgs *)_args;
    Camera *camera = args->camera;

    // Initialize hardware, etc
    set_mode(camera, INITIALISING);
    camera->internal = camera->initialize(camera, args);
    if (!camera->internal)
    {
        if (camera_desired_mode(camera) == SHUTDOWN)
            pn_log("Camera initialization aborted.");
        else
            trigger_fatal_error(strdup("Failed to initialize camera."));
        return NULL;
    }

    camera->port_count = camera->port_table(camera, camera->internal, &camera->port_options);
    camera->query_ccd_region(camera, camera->internal, camera->ccd_region);

    double readout = camera->update_camera_settings(camera, camera->internal);
    pthread_mutex_lock(&camera->read_mutex);
    camera->readout_time = readout;
    pthread_mutex_unlock(&camera->read_mutex);

    pn_log("Camera is now idle.");
    set_mode(camera, IDLE);

    // Loop and respond to user commands
    int temp_ticks = 0;

    pthread_mutex_lock(&camera->read_mutex);
    PNCameraMode desired_mode = camera->desired_mode;
    bool safe_to_stop_acquiring = camera->safe_to_stop_acquiring;
    pthread_mutex_unlock(&camera->read_mutex);

    while (desired_mode != SHUTDOWN)
    {
        PNCameraMode current_mode = camera_mode(camera);
        pthread_mutex_lock(&camera->read_mutex);
        bool camera_settings_dirty = camera->camera_settings_dirty;
        pthread_mutex_unlock(&camera->read_mutex);

        if (current_mode == IDLE && camera_settings_dirty)
        {
            double readout = camera->update_camera_settings(camera, camera->internal);
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
            camera->start_acquiring(camera, camera->internal);
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
            camera->stop_acquiring(camera, camera->internal);
            pn_log("Camera is now idle.");
            set_mode(camera, IDLE);
        }

        // Check for new frames, etc
        camera->tick(camera, camera->internal, current_mode, camera->temperature);

        // Check temperature
        if (++temp_ticks >= 50)
        {
            temp_ticks = 0;
            double temperature = camera->read_temperature(camera, camera->internal);
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

    // Shutdown camera
    PNCameraMode current_mode = camera_mode(camera);

    if (current_mode == ACQUIRING || current_mode == IDLE_WHEN_SAFE)
    {
        camera->stop_acquiring(camera, camera->internal);
        pn_log("Camera is now idle.");
    }

    // Uninitialize hardware, etc
    camera->uninitialize(camera, camera->internal);
    pn_log("Camera uninitialized.");

    return NULL;
}

void camera_spawn_thread(Camera *camera, ThreadCreationArgs *args)
{
    pthread_create(&camera->camera_thread, NULL, camera_thread, (void *)args);
    camera->thread_initialized = true;
}

void camera_shutdown(Camera *camera)
{
    set_desired_mode(camera, SHUTDOWN);

    void **retval = NULL;
    if (camera->thread_initialized)
        pthread_join(camera->camera_thread, retval);

    camera->thread_initialized = false;
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

void camera_start_exposure(Camera *camera)
{
    set_desired_mode(camera, ACQUIRING);
}

void camera_stop_exposure(Camera *camera)
{
    set_desired_mode(camera, IDLE);
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
    PNCameraMode mode = camera->mode;
    pthread_mutex_unlock(&camera->read_mutex);
    return mode;
}

void camera_update_settings(Camera *camera)
{
    pthread_mutex_lock(&camera->read_mutex);
    camera->camera_settings_dirty = true;
    pthread_mutex_unlock(&camera->read_mutex);
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

const char *camera_port_desc(Camera *camera)
{
    uint8_t pid = pn_preference_char(CAMERA_READPORT_MODE);
    if (pid > camera->port_count)
        return "Unknown";
    return camera->port_options[pid].name;
}

const char *camera_speed_desc(Camera *camera)
{
    uint8_t pid = pn_preference_char(CAMERA_READPORT_MODE);
    uint8_t sid = pn_preference_char(CAMERA_READSPEED_MODE);
    if (pid >= camera->port_count ||
        sid >= camera->port_options[pid].speed_count)
        return "Unknown";
    return camera->port_options[pid].speed[sid].name;
}

const char *camera_gain_desc(Camera *camera)
{
    uint8_t pid = pn_preference_char(CAMERA_READPORT_MODE);
    uint8_t sid = pn_preference_char(CAMERA_READSPEED_MODE);
    uint8_t gid = pn_preference_char(CAMERA_GAIN_MODE);
    if (pid >= camera->port_count ||
        sid >= camera->port_options[pid].speed_count ||
        gid >= camera->port_options[pid].speed[sid].gain_count)
        return "Unknown";
    return camera->port_options[pid].speed[sid].gain[gid].name;
}
