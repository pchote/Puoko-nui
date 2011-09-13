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
#include <string.h>

#include "common.h"
#include "camera.h"
#include "preferences.h"

#pragma mark Creation and Destruction (Called from main thread)

// Initialize a new PNCamera struct.
PNCamera pn_camera_new()
{
	PNCamera cam;
	cam.handle = -1;
	cam.mode = UNINITIALIZED;
	cam.desired_mode = IDLE;
	cam.image_buffer = NULL;
	cam.image_buffer_size = 0;
	cam.binsize = 2;
    cam.temperature = 0;
    cam.fatal_error = NULL;
    cam.first_frame = TRUE;
    pthread_mutex_init(&cam.read_mutex, NULL);

	return cam;
}

// Destroy a PNCamera struct.
void pn_camera_free(PNCamera *cam)
{
    if (cam->fatal_error)
        free(cam->fatal_error);
    pthread_mutex_destroy(&cam->read_mutex);
}


#pragma mark Camera Routines (Called from camera thread)

// Set the camera mode to be read by the other threads in a threadsafe manner
static void set_mode(PNCamera *cam, PNCameraMode mode)
{
    pthread_mutex_lock(&cam->read_mutex);
    cam->mode = mode;
    pthread_mutex_unlock(&cam->read_mutex);
}

// Decide what to do with an acquired frame
static void frame_downloaded(PNCamera *cam, PNFrame *frame)
{
	// When starting a run, the first frame will not be exposed
    // for the correct time, so we discard it
	if (cam->first_frame)
    {
        pn_log("Discarding first frame");
        cam->first_frame = FALSE;
        return;
    }

	pn_log("Frame downloaded");
	if (pn_preference_char(SAVE_FRAMES) && pn_preference_allow_save())
	{
		pn_save_frame(frame);
        pn_preference_increment_framecount();
	}

	// Display the frame in ds9
	pn_preview_frame(frame);
}

#pragma mark Real Camera Routines

// Generate a fatal error based on the pvcam error
static void pvcam_error(PNCamera *cam, const char *msg, int line)
{
	int error = pl_error_code();
	if (!error)
		return;

	char pvmsg[ERROR_MSG_LEN];
	pvmsg[0] = '\0';
	pl_error_message(error, pvmsg);

    asprintf(&cam->fatal_error, "FATAL: %s %d PVCAM error: %d = %s; %s\n", __FILE__, line, error, pvmsg, msg);    
	pthread_exit(NULL);
}

// Sample the camera temperature to be read by the other threads in a threadsafe manner
static void read_temperature(PNCamera *cam)
{
    int16 temp;
    if (pl_get_param(cam->handle, PARAM_TEMP, ATTR_CURRENT, &temp ))
        pvcam_error(cam, "Error querying temperature", __LINE__);

    pthread_mutex_lock(&cam->read_mutex);
    cam->temperature = (float)temp/100;
    pthread_mutex_unlock(&cam->read_mutex);
}

// Check whether a frame is available
static rs_bool frame_available(PNCamera *cam)
{
    int16 status = READOUT_NOT_ACTIVE;
	uns32 bytesStored = 0, numFilledBuffers = 0;
	if (!pl_exp_check_cont_status(cam->handle, &status, &bytesStored, &numFilledBuffers))
		pvcam_error(cam, "Error querying camera status", __LINE__);

	return (status == FRAME_AVAILABLE);
}

// Initialize PVCAM and the camera hardware
static void initialize_camera(PNCamera *cam)
{
    set_mode(cam, INITIALISING);

    if (!pl_pvcam_init())
        pvcam_error(cam, "Could not initialize the PVCAM library (pl_pvcam_init)", __LINE__);

    uns16 pversion;
    if (!pl_pvcam_get_ver(&pversion))
        pvcam_error(cam, "Cannot query pvcam version", __LINE__);

    pn_log("PVCAM Version %d.%d.%d (0x%x) initialized",pversion>>8, (pversion & 0x00F0)>>4, pversion & 0x000F, pversion);

    int16 numCams = 0;
    if (!pl_cam_get_total(&numCams))
        pvcam_error(cam, "Cannot query the number of cameras (pl_cam_get_total)", __LINE__);

    pn_log("Found %d camera(s)", numCams);
    if (numCams == 0)
    {
        cam->fatal_error = strdup("FATAL: No cameras are available (pass --simulate-camera to use simulated hardware).");    
        pthread_exit(NULL);
    }

    // Get the camera name (assume that we only have one camera)
    char cameraName[CAM_NAME_LEN];
    if (!pl_cam_get_name(0, cameraName))
        pvcam_error(cam, "Cannot open the camera as could not get the camera name (pl_cam_get_name)", __LINE__);

    // Open the camera
    if (!pl_cam_open(cameraName, &cam->handle, OPEN_EXCLUSIVE))
        pvcam_error(cam, "Cannot open the camera. Are you running as root?", __LINE__);

    // Print the camera firmware version if available
    char fwver_buf[16] = "Unknown";
    uns16 fwver;
    rs_bool avail = FALSE;
    if (!pl_get_param(cam->handle, PARAM_CAM_FW_VERSION, ATTR_AVAIL, (void *)&avail))
        pvcam_error(cam, "Error querying camera fw version", __LINE__);

    if (avail)
    {
        if (pl_get_param(cam->handle, PARAM_CAM_FW_VERSION, ATTR_CURRENT, (void *)&fwver))
            pvcam_error(cam, "Error querying camera fw version", __LINE__);
        sprintf(fwver_buf, "%d.%d (0x%x)", fwver >> 8, fwver & 0x00FF, fwver);
    }
    pn_log("Opened camera `%s`: Firmware version %s", cameraName, fwver_buf);

    // Check camera status
    if (!pl_cam_get_diags(cam->handle))
        pvcam_error(cam, "Camera failed diagnostic checks", __LINE__);

    // Set camera parameters
    uns16 shtr = 0;
    if (!pl_set_param(cam->handle, PARAM_SHTR_CLOSE_DELAY, (void*) &shtr))
        pvcam_error(cam, "Error setting PARAM_SHTR_CLOSE_DELAY]", __LINE__);

    int param = OUTPUT_NOT_SCAN;
    if (!pl_set_param(cam->handle, PARAM_LOGIC_OUTPUT, (void*) &param))
        pvcam_error(cam, "Error setting OUTPUT_NOT_SCAN", __LINE__);

    // Trigger on positive edge of the download pulse
    param = EDGE_TRIG_NEG;
    if (!pl_set_param(cam->handle, PARAM_EDGE_TRIGGER, (void*) &param))
        pvcam_error(cam, "Error setting PARAM_EDGE_TRIGGER", __LINE__);

    // Use custom frame-transfer readout mode
    param = MAKE_FRAME_TRANSFER;
    if (!pl_set_param(cam->handle, PARAM_FORCE_READOUT_MODE, (void*) &param))
        pvcam_error(cam, "Error setting PARAM_FORCE_READOUT_MODE", __LINE__);

    // Set temperature
    param = -5000; // -50 deg C
    //param = -4000; // -40 deg C
    //param = 0; // 0 deg C
    if (!pl_set_param(cam->handle, PARAM_TEMP_SETPOINT, (void*) &param))
        pvcam_error(cam, "Error setting PARAM_TEMP_SETPOINT", __LINE__);

    // Set readout speed
    param = 0; // 100kHz
    //param = 1; // 1Mhz
    if (!pl_set_param(cam->handle, PARAM_SPDTAB_INDEX, (void*) &param))
        pvcam_error(cam, "Error setting PARAM_SPDTAB_INDEX", __LINE__);

    cam->first_frame = TRUE;

    pn_log("Camera initialized");
    set_mode(cam, IDLE);
}

// Start an acquisition sequence
static void start_acquiring(PNCamera *cam)
{
    set_mode(cam, ACQUIRE_START);

    pn_log("Starting acquisition run...");
    if (!pl_get_param(cam->handle, PARAM_SER_SIZE, ATTR_DEFAULT, (void *)&cam->frame_width))
        pvcam_error(cam, "Error querying camera width", __LINE__);

    if (!pl_get_param(cam->handle, PARAM_PAR_SIZE, ATTR_DEFAULT, (void *)&cam->frame_height))
        pvcam_error(cam, "Error querying camera height", __LINE__);

    pn_log("Pixel binning factor: %d", cam->binsize);

    rgn_type region;
    region.s1 = 0;                   // x start ('serial' direction)
    region.s2 = cam->frame_width-1;  // x end
    region.sbin = cam->binsize;      // x binning (1 = no binning)
    region.p1 = 0;                   // y start ('parallel' direction)
    region.p2 = cam->frame_height-1; // y end
    region.pbin = cam->binsize;      // y binning (1 = no binning)

    // Divide the chip size by the bin size to find the frame dimensions
    cam->frame_height /= cam->binsize;
    cam->frame_width /= cam->binsize;

    // Init exposure control libs
    if (!pl_exp_init_seq())
        pvcam_error(cam, "pl_exp_init_seq failed", __LINE__);

    // Set exposure mode: expose entire chip, expose on sync pulses (exposure time unused), overwrite buffer
    uns32 frame_size = 0;
    if (!pl_exp_setup_cont(cam->handle, 1, &region, STROBED_MODE, 0, &frame_size, CIRC_NO_OVERWRITE))
        pvcam_error(cam, "pl_exp_setup_cont failed", __LINE__);

    // Create a buffer big enough to hold 4 images
    cam->image_buffer_size = frame_size * 4;
    cam->image_buffer = (uns16*)malloc( cam->image_buffer_size );

    // Start waiting for sync pulses to trigger exposures
    if (!pl_exp_start_cont(cam->handle, cam->image_buffer, cam->image_buffer_size))
        pvcam_error(cam, "pl_exp_start_cont failed", __LINE__);

    pn_log("Acquisition run started");

    // Sample initial temperature
    read_temperature(cam);

    set_mode(cam, ACQUIRING);
}

// Stop an acquisition sequence
static void stop_acquiring(PNCamera *cam)
{
    set_mode(cam, ACQUIRE_STOP);

    // Clear any buffered frames
    void_ptr camera_frame;
    while (frame_available(cam))
    {
        pl_exp_get_oldest_frame(cam->handle, &camera_frame);
        pl_exp_unlock_oldest_frame(cam->handle);
        pn_log("Discarding buffered frame");
    }

    if (!pl_exp_stop_cont(cam->handle, CCS_HALT))
        pn_log("Error stopping sequence");

    if (!pl_exp_finish_seq(cam->handle, cam->image_buffer, 0))
        pn_log("Error finishing sequence");

    if (!pl_exp_uninit_seq())
        pn_log("Error uninitialising sequence");

    free(cam->image_buffer);
    pn_log("Acquisition sequence uninitialized");

    set_mode(cam, IDLE);
}

// Main camera thread loop
void *pn_camera_thread(void *_cam)
{
    PNCamera *cam = (PNCamera *)_cam;

    // Initialize the camera
    initialize_camera(cam);

    // Loop and respond to user commands
	struct timespec wait = {0,1e8};
    int temp_ticks = 0;

    pthread_mutex_lock(&cam->read_mutex);
    PNCameraMode desired_mode = cam->desired_mode;
    pthread_mutex_unlock(&cam->read_mutex);

    while (desired_mode != SHUTDOWN)
    {
        // Start/stop acquisition
        if (desired_mode == ACQUIRING && cam->mode == IDLE)
            start_acquiring(cam);

        if (desired_mode == ACQUIRE_WAIT && cam->mode == ACQUIRING)
            cam->mode = ACQUIRE_WAIT;

        if (desired_mode == IDLE && cam->mode == ACQUIRE_WAIT)
            stop_acquiring(cam);

        // Check for new frame
        if (cam->mode == ACQUIRING && frame_available(cam))
        {
            pn_log("Frame available @ %d", (int)time(NULL));
		    void_ptr camera_frame;
            if (!pl_exp_get_oldest_frame(cam->handle, &camera_frame))
			    pvcam_error(cam, "Error retrieving oldest frame", __LINE__);

		    // Do something with the frame data
		    PNFrame frame;			
		    frame.width = cam->frame_width;
		    frame.height = cam->frame_height;
		    frame.data = camera_frame;
		    frame_downloaded(cam, &frame);

		    // Unlock the frame buffer for reuse
            if (!pl_exp_unlock_oldest_frame(cam->handle))
			    pvcam_error(cam, "Error unlocking oldest frame", __LINE__);
        }

        // Check temperature
	    if (++temp_ticks >= 50)
	    {
		    temp_ticks = 0;
            read_temperature(cam);
	    }
        nanosleep(&wait, NULL);

        pthread_mutex_lock(&cam->read_mutex);
        desired_mode = cam->desired_mode;
        pthread_mutex_unlock(&cam->read_mutex);
    }

    // Shutdown camera
    if (cam->mode == ACQUIRING || cam->mode == ACQUIRE_WAIT)
        stop_acquiring(cam);

    // Close the PVCAM lib (which in turn closes the camera)
    if (cam->mode == IDLE)
    {	
        if (!pl_pvcam_uninit())
	        pn_log("Error uninitialising PVCAM");
	    pn_log("PVCAM uninitialized");
    }

	pthread_exit(NULL);
}


#pragma mark Simulated Camera Routines

// Stop an acquisition sequence
static void stop_acquiring_simulated(PNCamera *cam)
{
    set_mode(cam, ACQUIRE_STOP);
    free(cam->image_buffer);
    pn_log("Acquisition sequence uninitialized");
    set_mode(cam, IDLE);
}

// Main simulated camera thread loop
void *pn_simulated_camera_thread(void *_cam)
{
    PNCamera *cam = (PNCamera *)_cam;

    // Initialize the camera
    cam->handle = SIMULATED;
    cam->first_frame = TRUE;
    pn_log("Initialising simulated camera");

    // Wait a bit to simulate hardware startup time
    sleep(2);
    pn_log("Camera initialized");
    set_mode(cam, IDLE);

    // Loop and respond to user commands
	struct timespec wait = {0,1e8};

    pthread_mutex_lock(&cam->read_mutex);
    PNCameraMode desired_mode = cam->desired_mode;
    pthread_mutex_unlock(&cam->read_mutex);

    while (desired_mode != SHUTDOWN)
    {
        // Start Acquisition
        if (desired_mode == ACQUIRING && cam->mode == IDLE)
        {
            set_mode(cam, ACQUIRE_START);

            cam->frame_height = 512;
            cam->frame_width = 512;
            cam->simulated_frame_available = FALSE;

            // Create a buffer to write a simulated frame to
            cam->image_buffer_size = 512*512*2;
            cam->image_buffer = (uns16*)malloc( cam->image_buffer_size );

            // Delay a bit to simulate hardware startup time
            sleep(2);
            pn_log("Simulated acquisition run started");

            set_mode(cam, ACQUIRING);
        }

        // Enter an intermediate waiting state while we wait for the
        // timer to say it is safe to stop the acquisition sequence
        if (desired_mode == ACQUIRE_WAIT && cam->mode == ACQUIRING)
            cam->mode = ACQUIRE_WAIT;

        // Stop acquisition
        if (desired_mode == IDLE && cam->mode == ACQUIRE_WAIT)
            stop_acquiring_simulated(cam);

        // Check for new frame
        if (cam->mode == ACQUIRING && cam->simulated_frame_available)
        {
            pn_log("Frame available @ %d", (int)time(NULL));

		    // Do something with the frame data
		    PNFrame frame;
		    frame.width = cam->frame_width;
		    frame.height = cam->frame_height;
		    frame.data = cam->image_buffer;
		    frame_downloaded(cam, &frame);
            cam->simulated_frame_available = FALSE;
        }

        nanosleep(&wait, NULL);
        pthread_mutex_lock(&cam->read_mutex);
        desired_mode = cam->desired_mode;
        pthread_mutex_unlock(&cam->read_mutex);
    }

    // Shutdown camera
    if (cam->mode == ACQUIRING)
        stop_acquiring_simulated(cam);

	pthread_exit(NULL);
}

