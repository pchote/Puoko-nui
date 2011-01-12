/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include "camera.h"
#include "acquisitionthread.h"

/* Starts an acquisition run and downloads frames on a different thread
 * Note: access to gtk objects should only be made from the main thread, so
 * the bin size is passed as an argument to this worker thread */
void *rangahau_acquisition_thread(void *_info)
{
	RangahauAcquisitionThreadInfo *info = (RangahauAcquisitionThreadInfo *)_info;
	info->active = TRUE;

	/* Tell the camera to start listening for sync pulses */
	rangahau_camera_start_acquisition(info->camera, info->binsize);

	/* Poll the camera for frames at 10Hz for frames */
	struct timespec wait = {0,1e8};
	while (!info->cancelled)
	{
		while (!info->cancelled && !rangahau_camera_image_available(info->camera))
			nanosleep(&wait, NULL);

		if (info->cancelled)
			break;

		/* Get the frame data. Note: this contains a reference
		 * to the native pvcam buffer, so may be overwritten beneath
		 * us if we are tardy copying the data */
		RangahauFrame frame = rangahau_camera_latest_frame(info->camera);
		if (info->cancelled)
			break;

		info->on_frame_available(frame);
	}

	/* Stop camera acquisition */
	rangahau_camera_stop_acquisition(info->camera);
	printf("Acquisition thread completed.\n");
	info->active = FALSE;

	pthread_exit(NULL);
}
