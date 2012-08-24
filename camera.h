/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef CAMERA_H
#define CAMERA_H

// Custom frame transfer mode options
#define PARAM_FORCE_READOUT_MODE ((CLASS2<<16) + (TYPE_UNS32<<24) + 326)
enum ForceReadOut {
    ALWAYS_CHECK_EXP,
    MAKE_FULL,
    MAKE_FRAME_TRANSFER,
    MAKE_AUTOMATIC
};

// Represents the current state of the camera
typedef enum
{
    UNINITIALIZED,
    INITIALISING,
    IDLE,
    ACQUIRE_START,
    ACQUIRING,
    DOWNLOADING,
    ACQUIRE_WAIT,
    ACQUIRE_STOP,
    SHUTDOWN,
} PNCameraMode;

// Represents an aquired frame
typedef struct
{
    uint16_t width;
    uint16_t height;
    uint16_t *data; // Pointer to the start of the frame data
} PNFrame;

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
    char *fatal_error;
    bool first_frame;

    pthread_mutex_t read_mutex;
} PNCamera;

PNCamera pn_camera_new();
void pn_camera_free(PNCamera *cam);
void *pn_simulated_camera_thread(void *);

void set_mode(PNCameraMode mode);
void frame_downloaded(PNFrame *frame);

#ifdef USE_PVCAM
void *pn_pvcam_camera_thread(void *);
#endif
#ifdef USE_PICAM
void *pn_picam_camera_thread(void *);
#endif

#endif
