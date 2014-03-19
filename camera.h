/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef CAMERA_H
#define CAMERA_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

#define CAMERA_OK 0
#define CAMERA_ALLOCATION_FAILED -1
#define CAMERA_ERROR -2
#define CAMERA_INITIALIZATION_ABORTED -3

struct camera_gain_option
{
    char *name;
};

struct camera_speed_option
{
    char *name;
    struct camera_gain_option *gain;
    uint8_t gain_count;
};

struct camera_port_option
{
    char *name;
    struct camera_speed_option *speed;
    uint8_t speed_count;
};

typedef struct Camera Camera;

typedef enum
{
    UNINITIALIZED,
    INITIALISING,
    IDLE,
    ACQUIRE_START,
    ACQUIRING,
    IDLE_WHEN_SAFE,
    ACQUIRE_STOP,
    SHUTDOWN
} PNCameraMode;

Camera *camera_new(bool simulate_hardware);
void camera_free(Camera *camera);
void camera_spawn_thread(Camera *camera, const Modules *modules);
void camera_notify_shutdown(Camera *camera);
bool camera_thread_alive(Camera *camera);
void camera_join_thread(Camera *camera);

void camera_set_mode(Camera *camera, PNCameraMode mode);
void camera_notify_safe_to_stop(Camera *camera);
bool camera_is_simulated(Camera *camera);
void camera_start_exposure(Camera *camera, bool shutter_open);
void camera_stop_exposure(Camera *camera);

double camera_temperature(Camera *camera);
double camera_readout_time(Camera *camera);
PNCameraMode camera_mode(Camera *camera);
PNCameraMode camera_desired_mode(Camera *camera);
void camera_update_settings(Camera *camera);
bool camera_supports_readout_display(Camera *camera);
bool camera_supports_shutter_disabling(Camera *camera);
bool camera_supports_bias_acquisition(Camera *camera);
void camera_normalize_trigger(Camera *camera, TimerTimestamp *trigger);

void camera_simulate_frame(Camera *camera);

// Warning: These are not thread safe, but this is only touched by the camera
// thread during startup, when the main thread is designed to not call these
void camera_ccd_region(Camera *camera, uint16_t region[4]);
uint8_t camera_port_options(Camera *camera, struct camera_port_option **options);

#endif
