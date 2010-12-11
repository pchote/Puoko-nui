/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include "camera.h"
#include "acquisitionthread.h"

void *rangahau_acquisition_thread(void *_info)
{
	RangahauAcquisitionThreadInfo *info = (RangahauAcquisitionThreadInfo *)_info;

	info->active = TRUE;
	printf("Starting acquisition thread with exposure time of %d sec.\n", info->exptime);
	
	/* Poll the camera for frames at 10Hz */
	struct timespec wait;
	wait.tv_sec = 0;
	wait.tv_nsec = 1e8;

	/* Todo: set gps to use exposuretime? maybe do that in the controller. */

	/*
	 * Tell the camera to start listening for sync pulses from the gps,
	 * and exposing frames
	 */
	rangahau_camera_start_acquisition(info->camera);

	/* Dump any existing images that may have been buffered on the camera */
	//rangahau_camera_purge_images(info->camera);

	while (!info->cancelled)
	{
		while (!info->cancelled && !rangahau_camera_image_available(info->camera))
		{
			printf("Waiting for camera...\n");
			nanosleep(&wait, NULL);
		}

		if (info->cancelled)
			break;

		printf("Frame available!\n");
		/* TODO: Get the last gps sync pulse time, subtract exptime to find frame start */

		if (info->cancelled)
			break;

		/* Get the frame data. Note: this contains a reference
		 * to the native pvcam buffer, so may be overwritten beneath
		 * us if we are tardy copying the data */

		RangahauFrame frame = rangahau_camera_latest_frame(info->camera);
		if (info->cancelled)
			break;
		printf("Frame size: %d x %d\n",frame.width, frame.height);

		info->on_frame_available(frame);
	}

	/* Stop camera acquisition */
	rangahau_camera_stop_acquisition(info->camera);
	printf("Acquisition thread completed.\n");
	info->active = FALSE;

	pthread_exit(NULL);
}
