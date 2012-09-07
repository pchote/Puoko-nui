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
typedef struct
{
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
} PNCamera;

PNCamera *pn_camera_new();
void pn_camera_free(PNCamera *cam);
void *pn_simulated_camera_thread(void *);

void set_mode(PNCameraMode mode);
void pn_camera_notify_safe_to_stop();
bool pn_camera_is_simulated();
void pn_camera_start_exposure();
void pn_camera_stop_exposure();
void pn_camera_shutdown();

float pn_camera_temperature();
float pn_camera_readout_time();
PNCameraMode pn_camera_mode();

#ifdef USE_PVCAM
void *pn_pvcam_camera_thread(void *);
#endif
#ifdef USE_PICAM
void *pn_picam_camera_thread(void *);
#endif

#endif
