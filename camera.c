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
	cam._binsize = 1;
	return cam;
}

/* TODO: Things to implement?
 *
 * PARAM_CCS_STATUS - query camera status
 * PARAM_SHTR_OPEN_MODE = OPEN_NO_CHANGE - tells the camera to never send any shutter signals (use instead of setting shutter time to 0?
 * PARAM_TEMP - get the camera temperature in degrees c * 100
 */

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
		if (!pl_pvcam_init())
			check_pvcam_error("Could not initialise the PVCAM library (pl_pvcam_init)", __LINE__);
		
		uns16 pversion;
		if (!pl_pvcam_get_ver(&pversion))
			check_pvcam_error("Cannot query pvcam version", __LINE__);

		printf("Initialising PVCAM Version %d.%d.%d (0x%x)\n",pversion>>8, (pversion & 0x00F0)>>4, pversion & 0x000F, pversion);
		cam->pvcam_inited = TRUE;

		int16 numCams = 0;
		if (!pl_cam_get_total(&numCams))
			check_pvcam_error("Cannot query the number of cameras (pl_cam_get_total)", __LINE__);

		printf("Found %d camera(s)\n", numCams);
		if (numCams == 0)
		{
			printf("No cameras are available (pass --simulate to use simulated hardware).\n");
			exit(1);
		}

		/* Get the camera name (assume that we only have one camera) */
		char cameraName[CAM_NAME_LEN];
		if (!pl_cam_get_name(0, cameraName))
			check_pvcam_error("Cannot open the camera as could not get the camera name (pl_cam_get_name)", __LINE__);

		/* Open the camera */
		if (!pl_cam_open(cameraName, &cam->handle, OPEN_EXCLUSIVE))
			check_pvcam_error("Cannot open the camera. Are you running as root?", __LINE__);

		/* Print the camera firmware version if available */
		char fwver_buf[16] = "Unknown";
		uns16 fwver;
		rs_bool avail = FALSE;
		if (!pl_get_param(cam->handle, PARAM_CAM_FW_VERSION, ATTR_AVAIL, (void *)&avail))
			check_pvcam_error("Error querying camera fw version", __LINE__);

		if (avail)
		{
			if (pl_get_param(cam->handle, PARAM_CAM_FW_VERSION, ATTR_CURRENT, (void *)&fwver))
				check_pvcam_error("Error querying camera fw version", __LINE__);
			sprintf(fwver_buf, "%d.%d (0x%x)", fwver >> 8, fwver & 0x00FF, fwver);
		}
		printf("Opened camera `%s`: Firmware version %s\n", cameraName, fwver_buf);

		/* Check camera status */
		if (!pl_cam_get_diags(cam->handle))
			check_pvcam_error("Cannot open the camera as diagnostics check indicated problem (pl_cam_get_diags)", __LINE__);

		/* Set camera parameters */
		uns16 shtr = 0;
		if (!pl_set_param(cam->handle, PARAM_SHTR_CLOSE_DELAY, (void*) &shtr))
			check_pvcam_error("Cannot open the camera as there was a problem setting the shutter close delay (pl_set_param[PARAM_SHTR_CLOSE_DELAY])", __LINE__);

		int param = OUTPUT_NOT_SCAN;
		if (!pl_set_param(cam->handle, PARAM_LOGIC_OUTPUT, (void*) &param))
			check_pvcam_error("Cannot open the camera as there was a problem setting the logic output (pl_set_param[OUTPUT_NOT_SCAN])", __LINE__);

		param = EDGE_TRIG_POS;
		if (!pl_set_param(cam->handle, PARAM_EDGE_TRIGGER, (void*) &param))
			check_pvcam_error("Cannot open the camera as there was a problem setting the edge trigger (pl_set_param[PARAM_EDGE_TRIGGER])", __LINE__);
	   
		param = MAKE_FRAME_TRANSFER;
		if (!pl_set_param(cam->handle, PARAM_FORCE_READOUT_MODE, (void*) &param))
			check_pvcam_error("Cannot open the camera as there was a problem setting the readout mode (pl_set_param[MAKE_FRAME_TRANSFER])", __LINE__);
	}

	cam->status = IDLE;
	pthread_exit(NULL);
}

void rangahau_camera_close(RangahauCamera *cam)
{
	check_camera(cam);
	printf("Closing camera\n");
	/* Simulated camera doesn't need cleanup */
	if (cam->simulated)
	{
		/* sleep for 1 second to simulate camera shutdown time */
		sleep(1);
		return;
	}

	if (cam->status == ACTIVE)
	{
		if (!pl_exp_stop_cont(cam->handle, CCS_CLEAR))
			check_pvcam_error("Cannot close the camera as there was a problem stopping the exposure (pl_exp_stop_cont)", __LINE__);
		if (!pl_exp_finish_seq(cam->handle, cam->image_buffer, 0))
			check_pvcam_error("Cannot close the camera as there was a problem finishing the exposure sequence (pl_exp_finish_seq)", __LINE__);
		if (!pl_exp_uninit_seq())
			check_pvcam_error("Cannot close the camera as there was a problem uninitialising the sequence (pl_exp_uninit_seq)", __LINE__);
	}

	/* Close the PVCAM lib (which in turn closes the camera) */
	if (cam->pvcam_inited && !pl_pvcam_uninit())
		check_pvcam_error("Cannot close the camera as there was a problem uninitialising the PVCAM library (pl_pvcam_uninit)", __LINE__);
}

void rangahau_camera_start_acquisition(RangahauCamera *cam, uns16 binsize)
{
	check_camera(cam);
	printf("Starting acquisition\n");
	cam->status = INITIALISING;
	cam->_binsize = binsize;

	if (cam->simulated)
	{
		/* sleep for 1 second to simulate camera init time */
		sleep(1);
	}
	else
	{
		uns16 width = 0, height = 0;
		if (!pl_get_param(cam->handle, PARAM_SER_SIZE, ATTR_CURRENT, (void *)&width))
			check_pvcam_error("Error querying camera width", __LINE__);

		if (!pl_get_param(cam->handle, PARAM_PAR_SIZE, ATTR_CURRENT, (void *)&height))
			check_pvcam_error("Error querying camera height", __LINE__);

		rgn_type region;
		region.s1 = 0;        /* x start ('serial' direction) */
		region.s2 = width-1;  /* x end */
		region.sbin = binsize;      /* x binning (1 = no binning) */
		region.p1 = 0;        /* y start ('parallel' direction) */
		region.p2 = height-1; /* y end */
		region.pbin = binsize;      /* y binning (1 = no binning) */

		/* Init exposure control libs */
		if (!pl_exp_init_seq())
			check_pvcam_error("Error initialising sequence (pl_exp_init_seq)", __LINE__);

		/* Set exposure mode: expose entire chip, expose on sync pulses (exposure time unused), overwrite buffer */
		uns32 frame_size = 0;
		if (!pl_exp_setup_cont(cam->handle, 1, &region, STROBED_MODE, 0, &frame_size, CIRC_OVERWRITE))
			check_pvcam_error("Cannot start acquisition as there was a problem setting up continuous exposure", __LINE__);

		/* Create a buffer big enough to hold 4 images */
		cam->image_buffer_size = frame_size * 4;
		cam->image_buffer = (uns16*)malloc( cam->image_buffer_size );

		/* Start waiting for sync pulses to trigger exposures */
		if (!pl_exp_start_cont(cam->handle, cam->image_buffer, cam->image_buffer_size))
			check_pvcam_error("Cannot start acquisition as there was a problem starting continuous exposure", __LINE__);
	}

	cam->status = ACTIVE;
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
	else if (status == ACTIVE)
	{
		/* TODO: call pl_exp_abort(int16 hcam,int16 cam_state);? */

		if (!pl_exp_stop_cont(cam->handle, CCS_CLEAR))
			check_pvcam_error("Cannot stop acquisition as there was a problem stopping the exposure (pl_exp_stop_cont)", __LINE__);

		/* TODO: is this necessary? */
		if (!pl_exp_finish_seq(cam->handle, cam->image_buffer, 0))
			check_pvcam_error("Cannot stop acquisition as there was a problem finishing the exposure sequence (pl_exp_finish_seq)", __LINE__);

		if (!pl_exp_uninit_seq())
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
		/* Trigger a simulated download every 20 requests (~2 seconds) */		
		if (!(++simulate_count % 20))		
			simulate_count = 0;

		return (simulate_count == 0);
	}

	int16 status = READOUT_NOT_ACTIVE;
	uns32 bytesStored = 0, numFilledBuffers = 0;
	if (!pl_exp_check_cont_status(cam->handle, &status, &bytesStored, &numFilledBuffers))
		check_pvcam_error("Cannot determine whether an image is ready (pl_exp_check_cont_status)", __LINE__);

	/* TODO: Use status to update cam->status? */
	return (status == FRAME_AVAILABLE);
}

uns16 simulator_data[1024*1024];
RangahauFrame rangahau_camera_latest_frame(RangahauCamera *cam)
{
	check_camera(cam);

	if (cam->simulated)
	{
		RangahauFrame frame;
		frame.data = &simulator_data[0];
		frame.width = frame.height = 1024/cam->_binsize;

		/* Draw a linear ramp across the frame */
		for (int ii = 0, i=0; ii < frame.width; ii++)
			for (int jj = 0; jj < frame.height; jj++)
				simulator_data[i++] = ii+jj;
		return frame;
	}

	/* Retrieve frame data directly into a frame struct */
	RangahauFrame frame;
	frame.data = NULL; frame.width = frame.height = 0;
	if (!pl_exp_get_latest_frame(cam->handle, (void *)&frame.data))
		check_pvcam_error("Cannot get the latest frame (pl_exp_get_latest_frame)", __LINE__);

	if (frame.data == NULL)
	{
		printf("frame data is null @ %s:%d\n", __FILE__, __LINE__);
		exit(1);
	}

	if (!pl_get_param(cam->handle, PARAM_SER_SIZE, ATTR_CURRENT, (void *)&frame.width))
		check_pvcam_error("Error querying camera width", __LINE__);

	if (!pl_get_param(cam->handle, PARAM_PAR_SIZE, ATTR_CURRENT, (void *)&frame.height))
		check_pvcam_error("Error querying camera height", __LINE__);

	/* Divide the chip size by the bin size to find the frame dimensions */
	frame.width /= cam->_binsize;
	frame.height /= cam->_binsize;

	return frame;
}

