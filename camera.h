/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef CAMERA_H
#define CAMERA_H

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include "common.h"

// Represents the current state of the camera
typedef enum
{
    UNINITIALIZED,
    INITIALISING,
    IDLE,
    ACQUIRE_START,
    ACQUIRING,
    DOWNLOADING,
    IDLE_WHEN_SAFE,
    ACQUIRE_STOP,
    SHUTDOWN
} PNCameraMode;

// Holds the state of a camera
struct PNCamera
{
    pthread_t camera_thread;
    bool thread_initialized;

    // read/write
    PNCameraMode desired_mode;

    // read only
    bool simulated;
    PNCameraMode mode;
    uint16_t frame_width;
    uint16_t frame_height;
    float temperature;
    float readout_time;
    bool safe_to_stop_acquiring;

    pthread_mutex_t read_mutex;
};

typedef struct PNCamera PNCamera;

PNCamera *pn_camera_new(bool simulate_hardware);
void pn_camera_free(PNCamera *camera);
void pn_camera_spawn_thread(PNCamera *camera, ThreadCreationArgs *args);
void pn_camera_shutdown(PNCamera *camera);

void set_mode(PNCameraMode mode);
void pn_camera_notify_safe_to_stop();
bool pn_camera_is_simulated();
void pn_camera_start_exposure();
void pn_camera_stop_exposure();

float pn_camera_temperature();
float pn_camera_readout_time();
PNCameraMode pn_camera_mode();

#endif
