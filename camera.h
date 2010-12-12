/*
* Copyright 2007-2010 The Authors (see AUTHORS)
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
	IDLE,
	ACQUIRING
} RangahauCameraStatus;

/* Holds the state of a camera */
typedef struct
{
	boolean pvcam_inited;
	short handle;
	RangahauCameraStatus status;
	boolean simulated;
	void *image_buffer;
	uns32 image_buffer_size;
} RangahauCamera;

/* Represents an aquired frame */
typedef struct
{
	int width;
	int height;
	short *data; /* Pointer to the start of the frame data */
} RangahauFrame;


RangahauCamera rangahau_camera_new(boolean simulate);
void *rangahau_camera_init(void *cam);
void rangahau_camera_start_acquisition(RangahauCamera *cam);
void rangahau_camera_stop_acquisition(RangahauCamera *cam);
boolean rangahau_camera_image_available(RangahauCamera *cam);
RangahauFrame rangahau_camera_latest_frame(RangahauCamera *cam);

#endif
