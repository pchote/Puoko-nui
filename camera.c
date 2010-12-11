/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/
#include <stdlib.h>
#include <stdio.h>
#include "camera.h"

void check_pvcam_error(const char * msg, int line)
{
	int error = pl_error_code();
	if (!error)
		return;

	char pvmsg[ERROR_MSG_LEN];
	pvmsg[0] = '\0';
	pl_error_message(error, pvmsg);

	printf("%s %d PVCAM error: %d = %s; %s", __FILE__, line, error, pvmsg, msg);
	exit(1);
}

RangahauCamera rangahau_camera_new()
{
	RangahauCamera cam;
	cam.handle = -1;

	/* Init PVCAM library */
	pl_pvcam_init();
	check_pvcam_error("Cannot open the camera as could not initialise the PVCAM library (pl_pvcam_init)", __LINE__);
	cam.pvcam_inited = TRUE;

	/* Get the camera name */
	char cameraName[CAM_NAME_LEN];
	pl_cam_get_name(0, cameraName);
	check_pvcam_error("Cannot open the camera as could not get the camera name (pl_cam_get_name)", __LINE__);

	/* Open the camera */
	pl_cam_open(cameraName, &cam.handle, OPEN_EXCLUSIVE);
	check_pvcam_error("Cannot open the camera (pl_cam_open)", __LINE__);

	/* Check camera status */
	pl_cam_get_diags(cam.handle);
	check_pvcam_error("Cannot open the camera as diagnostics check indicated problem (pl_cam_get_diags)", __LINE__);

	/* Set camera parameters */
	long param = 0;
	pl_set_param(cam.handle, PARAM_SHTR_CLOSE_DELAY, (void*) &param);
	check_pvcam_error("Cannot open the camera as there was a problem setting the shutter close delay (pl_set_param[PARAM_SHTR_CLOSE_DELAY])", __LINE__);

	param = OUTPUT_NOT_SCAN;
	pl_set_param(cam.handle, PARAM_LOGIC_OUTPUT, (void*) &param);
	check_pvcam_error("Cannot open the camera as there was a problem setting the logic output (pl_set_param[OUTPUT_NOT_SCAN])", __LINE__);

	param = EDGE_TRIG_POS;
	pl_set_param(cam.handle, PARAM_EDGE_TRIGGER, (void*) &param);
	check_pvcam_error("Cannot open the camera as there was a problem setting the edge trigger (pl_set_param[PARAM_EDGE_TRIGGER])", __LINE__);
   
	param = MAKE_FRAME_TRANSFER;
	pl_set_param(cam.handle, PARAM_FORCE_READOUT_MODE, (void*) &param);
	check_pvcam_error("Cannot open the camera as there was a problem setting the readout mode (pl_set_param[MAKE_FRAME_TRANSFER])", __LINE__);

	return cam;
}

void rangahau_camera_close(RangahauCamera *cam)
{
	if (cam == NULL)
	{
		printf("cam is null @ %s:%d\n", __FILE__, __LINE__);
		//exit(1);
	}

	if (cam->acquiring)
	{
		pl_exp_stop_cont(cam->handle, CCS_CLEAR);
		check_pvcam_error("Cannot close the camera as there was a problem stopping the exposure (pl_exp_stop_cont)", __LINE__);
		// TODO: finish sequence once we have a pixel buffer to reference (created when the exposure run is started)
		//pl_exp_finish_seq(cam->handle, pPixelBuffer, 0);
		//check_pvcam_error("Cannot close the camera as there was a problem finishing the exposure sequence (pl_exp_finish_seq)", __LINE__);
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

	// TODO: Free any memory associated with the frame buffer
}

void rangahau_camera_start_acquisition(RangahauCamera *cam)
{

}

boolean rangahau_camera_image_available(RangahauCamera *cam)
{
	if (cam == NULL)
	{
		printf("cam is null @ %s:%d\n", __FILE__, __LINE__);
		//exit(1);
	}
	return FALSE;
}
