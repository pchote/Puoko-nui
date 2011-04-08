/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include "common.h"
#include "camera.h"

/* TODO: Standardise on using rs_bool? - at least be consistent with TRUE/true/FALSE/false */
PNCamera pn_camera_new()
{
	PNCamera cam;
	cam.handle = -1;
	cam.status = UNINITIALISED;
	cam.image_buffer = NULL;
	cam.image_buffer_size = 0;
	cam.binsize = 2;
    cam.shutdown = FALSE;

    cam.acquisition_thread = -1;
    pthread_mutex_init(&cam.status_mutex, NULL);
	return cam;
}

// TODO: Use this
void pn_camera_free(PNCamera *cam)
{
    pthread_mutex_destroy(&cam->status_mutex);
}

static void check_pvcam_error(const char * msg, int line)
{
	int error = pl_error_code();
	if (!error)
		return;

	char pvmsg[ERROR_MSG_LEN];
	pvmsg[0] = '\0';
	pl_error_message(error, pvmsg);

	pn_die("%s %d PVCAM error: %d = %s; %s\n", __FILE__, line, error, pvmsg, msg);
}


void pn_camera_shutdown(PNCamera *cam)
{
    if (cam->handle == SIMULATED)
    {
		free(cam->image_buffer);
    }
    else
    {
        // TODO: Be careful about shutting down while downloading
        /* Stop acquiring frames */
        void **retval = NULL;
        if (cam->status == ACQUIRING)
        {
            pthread_mutex_lock(&cam->status_mutex);        
            cam->status = ACQUIRE_STOP;
            pthread_mutex_unlock(&cam->status_mutex);

            pthread_join(cam->acquisition_thread, retval);
        }

	    /* Close the PVCAM lib (which in turn closes the camera) */
	    if (!pl_pvcam_uninit())
		    fprintf(stderr,"Error uninitialising PVCAM\n");
	    printf("PVCAM uninitialised\n");
    }
}

static rs_bool frame_available(PNCamera *cam)
{
	if (cam->handle == SIMULATED)
    {
        return cam->simulated_frame_available;
    }
    
    int16 status = READOUT_NOT_ACTIVE;
	uns32 bytesStored = 0, numFilledBuffers = 0;
	if (!pl_exp_check_cont_status(cam->handle, &status, &bytesStored, &numFilledBuffers))
		check_pvcam_error("Error querying camera status", __LINE__);

	return (status == FRAME_AVAILABLE);
}

void *pn_camera_initialisation_thread(void *_cam)
{
    rs_bool simulated = TRUE;
    PNCamera *cam = (PNCamera *)_cam;
	if (cam == NULL)
		pn_die("cam is null @ %s:%d\n", __FILE__, __LINE__);

	/* Open PVCAM and set the initial camera parameters */
    if (simulated)
    {
        cam->handle = SIMULATED;
        printf("Initialising simulated camera\n");

        pthread_mutex_lock(&cam->status_mutex);  
        cam->status = IDLE;
        pthread_mutex_unlock(&cam->status_mutex);  
    }
    else
    {
        pthread_mutex_lock(&cam->status_mutex);  
        cam->status = INITIALISING;
        
        if (!pl_pvcam_init())
		    check_pvcam_error("Could not initialise the PVCAM library (pl_pvcam_init)", __LINE__);
	
	    uns16 pversion;
	    if (!pl_pvcam_get_ver(&pversion))
		    check_pvcam_error("Cannot query pvcam version", __LINE__);

	    printf("PVCAM Version %d.%d.%d (0x%x) initialised\n",pversion>>8, (pversion & 0x00F0)>>4, pversion & 0x000F, pversion);

	    int16 numCams = 0;
	    if (!pl_cam_get_total(&numCams))
		    check_pvcam_error("Cannot query the number of cameras (pl_cam_get_total)", __LINE__);

	    printf("Found %d camera(s)\n", numCams);
	    if (numCams == 0)
		    pn_die("No cameras are available (pass --simulate to use simulated hardware).\n");

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

	    printf("Camera initialised\n");

        cam->status = IDLE;
        pthread_mutex_unlock(&cam->status_mutex);
    }

	pthread_exit(NULL);
}

void pn_camera_start_acquisition(PNCamera *cam)
{
    if (cam->acquisition_thread != -1)
        pn_die("cam->acquisition_thread is not null @ %s:%d\n", __FILE__, __LINE__);

    pthread_create(&cam->acquisition_thread, NULL, pn_camera_acquisition_thread, cam);
}

void *pn_camera_acquisition_thread(void *_cam)
{
    PNCamera *cam = (PNCamera *)_cam;
	struct timespec wait = {0,1e8};
	if (cam == NULL)
		pn_die("cam is null @ %s:%d\n", __FILE__, __LINE__);

    if (cam->status != IDLE)
        pn_die("cam is not ready @ %s:%d\n", __FILE__, __LINE__);

    pthread_mutex_lock(&cam->status_mutex);  
    cam->status = ACQUIRE_START;
    pthread_mutex_unlock(&cam->status_mutex);

	/* Initialise the acquisition sequence */
    if (cam->handle == SIMULATED)
    {
        cam->frame_height = 512;
        cam->frame_width = 512;
        cam->simulated_frame_available = FALSE;

        // Create a buffer to write a simulated frame to
        cam->image_buffer_size = 512*512*2;
        cam->image_buffer = (uns16*)malloc( cam->image_buffer_size );

        printf("Simulated acquisition run started\n");

        cam->temperature = 0;
    }
    else
    {
        printf("Starting acquisition run...\n");
        if (!pl_get_param(cam->handle, PARAM_SER_SIZE, ATTR_DEFAULT, (void *)&cam->frame_width))
	        check_pvcam_error("Error querying camera width", __LINE__);

        if (!pl_get_param(cam->handle, PARAM_PAR_SIZE, ATTR_DEFAULT, (void *)&cam->frame_height))
	        check_pvcam_error("Error querying camera height", __LINE__);

        rgn_type region;
        region.s1 = 0;                   /* x start ('serial' direction) */
        region.s2 = cam->frame_width-1;  /* x end */
        region.sbin = cam->binsize;      /* x binning (1 = no binning) */
        region.p1 = 0;                   /* y start ('parallel' direction) */
        region.p2 = cam->frame_height-1; /* y end */
        region.pbin = cam->binsize;      /* y binning (1 = no binning) */

        /* Divide the chip size by the bin size to find the frame dimensions */
        cam->frame_height /= cam->binsize;
        cam->frame_width /= cam->binsize;

        /* Init exposure control libs */
        if (!pl_exp_init_seq())
	        check_pvcam_error("pl_exp_init_seq failed", __LINE__);

        /* Set exposure mode: expose entire chip, expose on sync pulses (exposure time unused), overwrite buffer */
        uns32 frame_size = 0;
        if (!pl_exp_setup_cont(cam->handle, 1, &region, STROBED_MODE, 0, &frame_size, CIRC_NO_OVERWRITE))
	        check_pvcam_error("pl_exp_setup_cont failed", __LINE__);

        /* Create a buffer big enough to hold 4 images */
        cam->image_buffer_size = frame_size * 4;
        cam->image_buffer = (uns16*)malloc( cam->image_buffer_size );

        /* Start waiting for sync pulses to trigger exposures */
        if (!pl_exp_start_cont(cam->handle, cam->image_buffer, cam->image_buffer_size))
	        check_pvcam_error("pl_exp_start_cont failed", __LINE__);

        printf("Acquisition run started\n");
        
        /* Get initial temperature */
        pl_get_param(cam->handle, PARAM_TEMP, ATTR_CURRENT, &cam->temperature );
    }    

    if (cam->status == ACQUIRE_START)
    {
        pthread_mutex_lock(&cam->status_mutex);  
        cam->status = ACQUIRING;
        pthread_mutex_unlock(&cam->status_mutex);

	    /* Poll the camera for frames at 10Hz for frames
	     * and at ~0.2Hz for temperature */
	    uns32 temp_ticks = 0;
	    while (cam->status == ACQUIRING)
	    {
		    while (cam->status == ACQUIRING && ++temp_ticks <= 50 && !frame_available(cam))
			    nanosleep(&wait, NULL);

            if (cam->status != ACQUIRING)
                break;

		    /* Retrieve frame data */
		    if (frame_available(cam))
		    {
			    printf("Frame available @ %d\n", (int)time(NULL));
			    void_ptr camera_frame;
                if (cam->handle == SIMULATED)
                    camera_frame = cam->image_buffer;
                else if (!pl_exp_get_oldest_frame(cam->handle, &camera_frame))
				    check_pvcam_error("Error retrieving oldest frame", __LINE__);

			    /* Do something with the frame data */
			    PNFrame frame;			
			    frame.width = cam->frame_width;
			    frame.height = cam->frame_height;
			    frame.data = camera_frame;
			    cam->on_frame_available(&frame);
		

			    /* Unlock the frame buffer for reuse */
                if (cam->handle == SIMULATED)
                    cam->simulated_frame_available = FALSE;
			    else if (!pl_exp_unlock_oldest_frame(cam->handle))
				    check_pvcam_error("Error unlocking oldest frame", __LINE__);
		    }

		    /* Query camera temperature */
		    if (cam->handle != SIMULATED && temp_ticks >= 50)
		    {
			    temp_ticks = 0;
			    pl_get_param(cam->handle, PARAM_TEMP, ATTR_CURRENT, &cam->temperature );
		    }
	    }

	    /* Finish the acquisition sequence */
	    if (cam->handle != SIMULATED)
	    {
		    if (!pl_exp_stop_cont(cam->handle, CCS_HALT))
			    fprintf(stderr,"Error stopping sequence\n");

		    if (!pl_exp_finish_seq(cam->handle, cam->image_buffer, 0))
			    fprintf(stderr,"Error finishing sequence\n");

		    if (!pl_exp_uninit_seq())
			    fprintf(stderr,"Error uninitialising sequence\n");
	
		    free(cam->image_buffer);
		    printf("Acquisition sequence uninitialised\n");
	    }
	}

    pthread_mutex_lock(&cam->status_mutex);  
    cam->status = IDLE;
    pthread_mutex_unlock(&cam->status_mutex);

	pthread_exit(NULL);
}

