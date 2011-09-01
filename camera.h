/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <master.h>
#include <pvcam.h>
#include <pthread.h>

#ifndef CAMERA_H
#define CAMERA_H

/* Custom frame transfer mode options */
#define PARAM_FORCE_READOUT_MODE ((CLASS2<<16) + (TYPE_UNS32<<24) + 326)
enum ForceReadOut {
    ALWAYS_CHECK_EXP,
    MAKE_FULL,
    MAKE_FRAME_TRANSFER,
    MAKE_AUTOMATIC
};

/* Represents the current state of the camera */
typedef enum
{
    UNINITIALISED,
	INITIALISING,
    IDLE,
    ACQUIRE_START,
	ACQUIRING,
	DOWNLOADING,
    ACQUIRE_WAIT,
    ACQUIRE_STOP,
	SHUTDOWN,
} PNCameraMode;

/* Represents an aquired frame */
typedef struct
{
	uns16 width;
	uns16 height;
	uns16 *data; /* Pointer to the start of the frame data */
} PNFrame;

#define SIMULATED -10

/* Holds the state of a camera */
typedef struct
{
	/* read/write */
	uns16 binsize;
	void (*on_frame_available)(PNFrame *frame);
    PNCameraMode desired_mode;

	/* read only */
	PNCameraMode mode;
	uns16 frame_width;
	uns16 frame_height;
	float temperature;

	/* internal use only */
	int16 handle;
	void *image_buffer;
	uns32 image_buffer_size;
    rs_bool simulated_frame_available;
    pthread_mutex_t read_mutex;
} PNCamera;

PNCamera pn_camera_new();
void *pn_camera_thread(void *_cam);
#endif
