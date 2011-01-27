/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <master.h>
#include <pvcam.h>

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
	INITIALISING,
	ACTIVE,
	SHUTDOWN,
} RangahauCameraStatus;

typedef enum
{
	INIT_UNINITIALISED = 0,
	INIT_OPEN = 1,
	INIT_AQUIRING = 2
} RangahauCameraInitStatus;

/* Represents an aquired frame */
typedef struct
{
	uns16 width;
	uns16 height;
	uns16 *data; /* Pointer to the start of the frame data */
} RangahauFrame;


/* Holds the state of a camera */
typedef struct
{
	/* read/write */
	uns16 binsize;
	rs_bool acquire_frames;
	void (*on_frame_available)(RangahauFrame *frame);
	rs_bool shutdown;

	/* read only */
	RangahauCameraStatus status;
	uns16 frame_width;
	uns16 frame_height;

	/* internal use only */
	int16 handle;
	void *image_buffer;
	uns32 image_buffer_size;
	RangahauCameraInitStatus init_status;
} RangahauCamera;

RangahauCamera rangahau_camera_new();
void *rangahau_camera_thread(void *cam);

#endif
