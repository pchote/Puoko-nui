/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "camera.h"

void check_pvcam_error(const char * msg, int line)
{
	int error = pl_error_code();
	if (!error)
		return;

	char pvmsg[ERROR_MSG_LEN];
	pvmsg[0] = '\0';
	pl_error_message(error, pvmsg);

	printf("%s %d PVCAM error: %d = %s; %s\n", __FILE__, line, error, pvmsg, msg);
	exit(1);
}

void check_camera(RangahauCamera *cam)
{
	if (cam == NULL)
	{
		printf("cam is null @ %s:%d\n", __FILE__, __LINE__);
		exit(1);
	}
}

RangahauCamera rangahau_camera_new(boolean simulate)
{
	RangahauCamera cam;
	cam.pvcam_inited = FALSE;
	cam.handle = -1;
	cam.simulated = simulate;
	cam.status = INITIALISING;
	cam.image_buffer = NULL;
	cam.image_buffer_size = 0;
	return cam;
}

void *rangahau_camera_init(void *_cam)
{
	RangahauCamera *cam = (RangahauCamera *)_cam;
	check_camera(cam);
	
	if (cam->simulated)
	{
		/* sleep for 1 second to simulate camera startup time */
		sleep(1);
	}
	else
	{
		/* Init PVCAM library */
		pl_pvcam_init();
		check_pvcam_error("Cannot open the camera as could not initialise the PVCAM library (pl_pvcam_init)", __LINE__);
		cam->pvcam_inited = TRUE;

		/* Get the camera name */
		char cameraName[CAM_NAME_LEN];
		pl_cam_get_name(0, cameraName);
		check_pvcam_error("Cannot open the camera as could not get the camera name (pl_cam_get_name)", __LINE__);

		/* Open the camera */
		pl_cam_open(cameraName, &cam->handle, OPEN_EXCLUSIVE);
		check_pvcam_error("Cannot open the camera (pl_cam_open)", __LINE__);

		/* Check camera status */
		pl_cam_get_diags(cam->handle);
		check_pvcam_error("Cannot open the camera as diagnostics check indicated problem (pl_cam_get_diags)", __LINE__);

		/* Set camera parameters */
		long param = 0;
		pl_set_param(cam->handle, PARAM_SHTR_CLOSE_DELAY, (void*) &param);
		check_pvcam_error("Cannot open the camera as there was a problem setting the shutter close delay (pl_set_param[PARAM_SHTR_CLOSE_DELAY])", __LINE__);

		param = OUTPUT_NOT_SCAN;
		pl_set_param(cam->handle, PARAM_LOGIC_OUTPUT, (void*) &param);
		check_pvcam_error("Cannot open the camera as there was a problem setting the logic output (pl_set_param[OUTPUT_NOT_SCAN])", __LINE__);

		param = EDGE_TRIG_POS;
		pl_set_param(cam->handle, PARAM_EDGE_TRIGGER, (void*) &param);
		check_pvcam_error("Cannot open the camera as there was a problem setting the edge trigger (pl_set_param[PARAM_EDGE_TRIGGER])", __LINE__);
	   
		param = MAKE_FRAME_TRANSFER;
		pl_set_param(cam->handle, PARAM_FORCE_READOUT_MODE, (void*) &param);
		check_pvcam_error("Cannot open the camera as there was a problem setting the readout mode (pl_set_param[MAKE_FRAME_TRANSFER])", __LINE__);
	}
	cam->status = IDLE;
	pthread_exit(NULL);
}

void rangahau_camera_close(RangahauCamera *cam)
{
	check_camera(cam);

	/* Simulated camera doesn't need cleanup */
	if (cam->simulated)
	{
		/* sleep for 1 second to simulate camera shutdown time */
		sleep(1);
		return;
	}

	if (cam->status == ACQUIRING)
	{
		pl_exp_stop_cont(cam->handle, CCS_CLEAR);
		check_pvcam_error("Cannot close the camera as there was a problem stopping the exposure (pl_exp_stop_cont)", __LINE__);
		pl_exp_finish_seq(cam->handle, cam->image_buffer, 0);
		check_pvcam_error("Cannot close the camera as there was a problem finishing the exposure sequence (pl_exp_finish_seq)", __LINE__);
		pl_exp_uninit_seq();
		check_pvcam_error("Cannot close the camera as there was a problem uninitialising the sequence (pl_exp_uninit_seq)", __LINE__);
	}

	if (cam->handle >= 0)
	{
		pl_cam_close(cam->handle);
		check_pvcam_error("Cannot close the camera as there was a problem closing the camera (pl_cam_close)", __LINE__);
	}

	if (cam->pvcam_inited)
	{
    	pl_pvcam_uninit();
		check_pvcam_error("Cannot close the camera as there was a problem uninitialising the PVCAM library (pl_pvcam_uninit)", __LINE__);
	}
}

void rangahau_camera_start_acquisition(RangahauCamera *cam)
{
	check_camera(cam);
	printf("Starting acquisition\n");
	cam->status = INITIALISING;

	if (cam->simulated)
	{
		/* sleep for 1 second to simulate camera init time */
		sleep(1);
	}
	else
	{
		int width = 0;
		int height = 0;

		pl_get_param(cam->handle, PARAM_SER_SIZE, ATTR_CURRENT, (void *)&width);
		pl_get_param(cam->handle, PARAM_PAR_SIZE, ATTR_CURRENT, (void *)&height);

		rgn_type region;
		region.s1 = 0;        // x start ('serial' direction).
		region.s2 = width-1;  // x end.
		region.sbin = 1;      // x binning (1 = no binning).
		region.p1 = 0;        // y start ('parallel' direction).
		region.p2 = height-1; // y end.
		region.pbin = 1;      // y binning (1 = no binning).

		/* Init exposure control libs */
		pl_exp_init_seq();

		/* Set exposure mode: expose entire chip, expose on sync pulses (exposure time unused), overwrite buffer */
		unsigned long frame_size = 0;
		pl_exp_setup_cont(cam->handle, 1, &region, STROBED_MODE, 0, &frame_size, CIRC_OVERWRITE);
		check_pvcam_error("Cannot start acquisition as there was a problem setting up continuous exposure", __LINE__);

		/* Create a buffer big enough to hold 5 images */
		cam->image_buffer_size = frame_size * 5;
		cam->image_buffer = (uns16*)malloc( cam->image_buffer_size );

		/* Start waiting for sync pulses */
		pl_exp_start_cont(cam->handle, cam->image_buffer, cam->image_buffer_size);
		check_pvcam_error("Cannot start acquisition as there was a problem starting continuous exposure", __LINE__);
	}

	cam->status = ACQUIRING;
}

void rangahau_camera_stop_acquisition(RangahauCamera *cam)
{
	check_camera(cam);
	
	int status = cam->status;
	cam->status = INITIALISING;

	printf("Stopping acquisition\n");
	if (cam->simulated)
	{
		/* sleep for 1 second to simulate camera init time */
		sleep(1);
	}
	else if (status == ACQUIRING)
	{
		pl_exp_stop_cont(cam->handle, CCS_CLEAR);
		check_pvcam_error("Cannot stop acquisition as there was a problem stopping the exposure (pl_exp_stop_cont)", __LINE__);
		pl_exp_finish_seq(cam->handle, cam->image_buffer, 0);
		check_pvcam_error("Cannot stop acquisition as there was a problem finishing the exposure sequence (pl_exp_finish_seq)", __LINE__);
		pl_exp_uninit_seq();
		check_pvcam_error("Cannot stop acquisition as there was a problem uninitialising the sequence (pl_exp_uninit_seq)", __LINE__);
		free(cam->image_buffer);
	}
	cam->status = IDLE;
}

int simulate_count = 0;
boolean rangahau_camera_image_available(RangahauCamera *cam)
{
	check_camera(cam);

	if (cam->simulated)
	{
		/* Trigger a simulated download every 10 requests (~1 second) */		
		if (!(++simulate_count % 10))		
			simulate_count = 0;

		return (simulate_count == 0);
	}

	short status = ACQUISITION_IN_PROGRESS; // Indicates whether the a frame is available for readout or not.
	unsigned long bytesStored = 0;       // The number of bytes currently stored in the buffer.
	unsigned long numFilledBuffers = 0;  // The number of times the buffer has been filled.
	pl_exp_check_cont_status(cam->handle, &status, &bytesStored, &numFilledBuffers);
	check_pvcam_error("Cannot determine whether an image is ready (pl_exp_check_cont_status)", __LINE__);

	/* TODO: Use status to update cam->status? */
	return (status == FRAME_AVAILABLE);
}

short simulator_data[1024*1024];
RangahauFrame rangahau_camera_latest_frame(RangahauCamera *cam)
{
	check_camera(cam);

	if (cam->simulated)
	{
		RangahauFrame frame;
		frame.data = &simulator_data[0];
		frame.width = frame.height = 1024;

		/* Draw a linear ramp across the frame */
		for (int ii = 0; ii < frame.width; ii++)
			for (int jj = 0; jj < frame.height; jj++)
				simulator_data[ii + jj*frame.width] = ii+jj;

		return frame;
	}

	/* Retrieve frame data directly into a frame struct */
	RangahauFrame frame;
	frame.data = NULL; frame.width = frame.height = -1;
	pl_exp_get_latest_frame(cam->handle, (void *)&frame.data);
	check_pvcam_error("Cannot get the latest frame (pl_exp_get_latest_frame)", __LINE__);
	if (frame.data == NULL)
	{
		printf("frame data is null @ %s:%d\n", __FILE__, __LINE__);
		exit(1);
	}
	pl_get_param(cam->handle, PARAM_SER_SIZE, ATTR_CURRENT, (void *)&frame.width);
	pl_get_param(cam->handle, PARAM_PAR_SIZE, ATTR_CURRENT, (void *)&frame.height);
	return frame;
}

