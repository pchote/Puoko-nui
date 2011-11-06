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
#include "gps.h"
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
    cam.simulated = FALSE;
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
extern PNCamera *camera;
extern PNGPS *gps;

// Set the camera mode to be read by the other threads in a threadsafe manner
static void set_mode(PNCameraMode mode)
{
    pthread_mutex_lock(&camera->read_mutex);
    camera->mode = mode;
    pthread_mutex_unlock(&camera->read_mutex);
}

// Decide what to do with an acquired frame
static void frame_downloaded(PNFrame *frame)
{
    // When starting a run, the first frame will not be exposed
    // for the correct time, so we discard it
    if (camera->first_frame)
    {
        pn_log("Discarding first frame");
        camera->first_frame = FALSE;
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
static void pvcam_error(const char *msg, int line)
{
    int error = pl_error_code();
    if (!error)
        return;

    char pvmsg[ERROR_MSG_LEN];
    pvmsg[0] = '\0';
    pl_error_message(error, pvmsg);

    asprintf(&camera->fatal_error, "FATAL: %s %d PVCAM error: %d = %s; %s\n", __FILE__, line, error, pvmsg, msg);    
    pthread_exit(NULL);
}

// Sample the camera temperature to be read by the other threads in a threadsafe manner
static void read_temperature()
{
    int16 temp;
    if (pl_get_param(camera->handle, PARAM_TEMP, ATTR_CURRENT, &temp ))
        pvcam_error("Error querying temperature", __LINE__);

    pthread_mutex_lock(&camera->read_mutex);
    camera->temperature = (float)temp/100;
    pthread_mutex_unlock(&camera->read_mutex);
}

// Check whether a frame is available
static rs_bool frame_available()
{
    int16 status = READOUT_NOT_ACTIVE;
    uns32 bytesStored = 0, numFilledBuffers = 0;
    if (!pl_exp_check_cont_status(camera->handle, &status, &bytesStored, &numFilledBuffers))
        pvcam_error("Error querying camera status", __LINE__);

    return (status == FRAME_AVAILABLE);
}

// Initialize PVCAM and the camera hardware
static void initialize_camera()
{
    set_mode(INITIALISING);

    if (!pl_pvcam_init())
        pvcam_error("Could not initialize the PVCAM library (pl_pvcam_init)", __LINE__);

    uns16 pversion;
    if (!pl_pvcam_get_ver(&pversion))
        pvcam_error("Cannot query pvcam version", __LINE__);

    pn_log("PVCAM Version %d.%d.%d (0x%x) initialized",pversion>>8, (pversion & 0x00F0)>>4, pversion & 0x000F, pversion);

    int16 numCams = 0;
    if (!pl_cam_get_total(&numCams))
        pvcam_error("Cannot query the number of cameras (pl_cam_get_total)", __LINE__);

    pn_log("Found %d camera(s)", numCams);
    if (numCams == 0)
    {
        camera->fatal_error = strdup("Camera not found");
        pthread_exit(NULL);
    }

    // Get the camera name (assume that we only have one camera)
    char cameraName[CAM_NAME_LEN];
    if (!pl_cam_get_name(0, cameraName))
        pvcam_error("Cannot open the camera as could not get the camera name (pl_cam_get_name)", __LINE__);

    // Open the camera
    if (!pl_cam_open(cameraName, &camera->handle, OPEN_EXCLUSIVE))
        pvcam_error("Cannot open the camera. Are you running as root?", __LINE__);

    // Print the camera firmware version if available
    char fwver_buf[16] = "Unknown";
    uns16 fwver;
    rs_bool avail = FALSE;
    if (!pl_get_param(camera->handle, PARAM_CAM_FW_VERSION, ATTR_AVAIL, (void *)&avail))
        pvcam_error("Error querying camera fw version", __LINE__);

    if (avail)
    {
        if (pl_get_param(camera->handle, PARAM_CAM_FW_VERSION, ATTR_CURRENT, (void *)&fwver))
            pvcam_error("Error querying camera fw version", __LINE__);
        sprintf(fwver_buf, "%d.%d (0x%x)", fwver >> 8, fwver & 0x00FF, fwver);
    }
    pn_log("Opened camera `%s`: Firmware version %s", cameraName, fwver_buf);

    // Check camera status
    if (!pl_cam_get_diags(camera->handle))
        pvcam_error("Camera failed diagnostic checks", __LINE__);

    // Set camera parameters
    uns16 shtr = 0;
    if (!pl_set_param(camera->handle, PARAM_SHTR_CLOSE_DELAY, (void*) &shtr))
        pvcam_error("Error setting PARAM_SHTR_CLOSE_DELAY]", __LINE__);

    int param = OUTPUT_NOT_SCAN;
    if (!pl_set_param(camera->handle, PARAM_LOGIC_OUTPUT, (void*) &param))
        pvcam_error("Error setting OUTPUT_NOT_SCAN", __LINE__);

    // Trigger on positive edge of the download pulse
    param = EDGE_TRIG_NEG;
    if (!pl_set_param(camera->handle, PARAM_EDGE_TRIGGER, (void*) &param))
        pvcam_error("Error setting PARAM_EDGE_TRIGGER", __LINE__);

    // Use custom frame-transfer readout mode
    param = MAKE_FRAME_TRANSFER;
    if (!pl_set_param(camera->handle, PARAM_FORCE_READOUT_MODE, (void*) &param))
        pvcam_error("Error setting PARAM_FORCE_READOUT_MODE", __LINE__);

    // Set temperature
    param = -5000; // -50 deg C
    //param = -4000; // -40 deg C
    //param = 0; // 0 deg C
    if (!pl_set_param(camera->handle, PARAM_TEMP_SETPOINT, (void*) &param))
        pvcam_error("Error setting PARAM_TEMP_SETPOINT", __LINE__);

    // Set readout speed
    param = 0; // 100kHz
    //param = 1; // 1Mhz
    if (!pl_set_param(camera->handle, PARAM_SPDTAB_INDEX, (void*) &param))
        pvcam_error("Error setting PARAM_SPDTAB_INDEX", __LINE__);

    camera->first_frame = TRUE;

    pn_log("Camera initialized");
    set_mode(IDLE);
}

// Start an acquisition sequence
static void start_acquiring()
{
    set_mode(ACQUIRE_START);

    pn_log("Starting acquisition run...");
    if (!pl_get_param(camera->handle, PARAM_SER_SIZE, ATTR_DEFAULT, (void *)&camera->frame_width))
        pvcam_error("Error querying camera width", __LINE__);

    if (!pl_get_param(camera->handle, PARAM_PAR_SIZE, ATTR_DEFAULT, (void *)&camera->frame_height))
        pvcam_error("Error querying camera height", __LINE__);

    pn_log("Pixel binning factor: %d", camera->binsize);

    rgn_type region;
    region.s1 = 0;                   // x start ('serial' direction)
    region.s2 = camera->frame_width-1;  // x end
    region.sbin = camera->binsize;      // x binning (1 = no binning)
    region.p1 = 0;                   // y start ('parallel' direction)
    region.p2 = camera->frame_height-1; // y end
    region.pbin = camera->binsize;      // y binning (1 = no binning)

    // Divide the chip size by the bin size to find the frame dimensions
    camera->frame_height /= camera->binsize;
    camera->frame_width /= camera->binsize;

    // Init exposure control libs
    if (!pl_exp_init_seq())
        pvcam_error("pl_exp_init_seq failed", __LINE__);

    // Set exposure mode: expose entire chip, expose on sync pulses (exposure time unused), overwrite buffer
    if (!pl_exp_setup_cont(camera->handle, 1, &region, STROBED_MODE, 0, &camera->image_buffer_size, CIRC_NO_OVERWRITE))
        pvcam_error("pl_exp_setup_cont failed", __LINE__);

    // Create a buffer big enough to hold 1 image
    camera->image_buffer = (uns16*)malloc( camera->image_buffer_size );

    // Start waiting for sync pulses to trigger exposures
    if (!pl_exp_start_cont(camera->handle, camera->image_buffer, camera->image_buffer_size))
        pvcam_error("pl_exp_start_cont failed", __LINE__);

    pn_log("Acquisition run started");

    // Sample initial temperature
    read_temperature();

    set_mode(ACQUIRING);
}

// Stop an acquisition sequence
static void stop_acquiring()
{
    set_mode(ACQUIRE_STOP);

    // Clear any buffered frames
    void_ptr camera_frame;
    while (frame_available())
    {
        pl_exp_get_oldest_frame(camera->handle, &camera_frame);
        pl_exp_unlock_oldest_frame(camera->handle);
        pn_log("Discarding buffered frame");
    }

    if (!pl_exp_stop_cont(camera->handle, CCS_HALT))
        pn_log("Error stopping sequence");

    if (!pl_exp_finish_seq(camera->handle, camera->image_buffer, 0))
        pn_log("Error finishing sequence");

    if (!pl_exp_uninit_seq())
        pn_log("Error uninitialising sequence");

    free(camera->image_buffer);
    pn_log("Acquisition sequence uninitialized");

    set_mode(IDLE);
}

// Main camera thread loop
void *pn_camera_thread(void *_unused)
{
    // Initialize the camera
    initialize_camera();

    // Loop and respond to user commands
    struct timespec wait = {0,1e8};
    int temp_ticks = 0;

    pthread_mutex_lock(&camera->read_mutex);
    PNCameraMode desired_mode = camera->desired_mode;
    pthread_mutex_unlock(&camera->read_mutex);

    while (desired_mode != SHUTDOWN)
    {
        // Start/stop acquisition
        if (desired_mode == ACQUIRING && camera->mode == IDLE)
            start_acquiring();

        if (desired_mode == ACQUIRE_WAIT && camera->mode == ACQUIRING)
            camera->mode = ACQUIRE_WAIT;

        if (desired_mode == IDLE && camera->mode == ACQUIRE_WAIT)
            stop_acquiring();

        // Check for new frame
        if (camera->mode == ACQUIRING && frame_available())
        {
            pn_log("Frame available @ %d", (int)time(NULL));
            void_ptr camera_frame;
            if (!pl_exp_get_oldest_frame(camera->handle, &camera_frame))
                pvcam_error("Error retrieving oldest frame", __LINE__);

            // Do something with the frame data
            PNFrame frame;            
            frame.width = camera->frame_width;
            frame.height = camera->frame_height;
            frame.data = camera_frame;
            frame_downloaded(&frame);

            // Unlock the frame buffer for reuse
            if (!pl_exp_unlock_oldest_frame(camera->handle))
                pvcam_error("Error unlocking oldest frame", __LINE__);
        }

        // Check temperature
        if (++temp_ticks >= 50)
        {
            temp_ticks = 0;
            read_temperature();
        }
        nanosleep(&wait, NULL);

        pthread_mutex_lock(&camera->read_mutex);
        desired_mode = camera->desired_mode;
        pthread_mutex_unlock(&camera->read_mutex);
    }

    // Shutdown camera
    if (camera->mode == ACQUIRING || camera->mode == ACQUIRE_WAIT)
        stop_acquiring();

    // Close the PVCAM lib (which in turn closes the camera)
    if (camera->mode == IDLE)
    {    
        if (!pl_pvcam_uninit())
            pn_log("Error uninitialising PVCAM");
        pn_log("PVCAM uninitialized");
    }

    pthread_exit(NULL);
}


#pragma mark Simulated Camera Routines

// Stop an acquisition sequence
static void stop_acquiring_simulated()
{
    set_mode(ACQUIRE_STOP);
    free(camera->image_buffer);
    pn_log("Acquisition sequence uninitialized");
    set_mode(IDLE);
}

// Main simulated camera thread loop
void *pn_simulated_camera_thread(void *_unused)
{
    // Initialize the camera
    camera->simulated = TRUE;
    camera->first_frame = TRUE;
    pn_log("Initialising simulated camera");

    // Wait a bit to simulate hardware startup time
    sleep(2);
    pn_log("Camera initialized");
    set_mode(IDLE);

    // Loop and respond to user commands
    struct timespec wait = {0,1e8};

    pthread_mutex_lock(&camera->read_mutex);
    PNCameraMode desired_mode = camera->desired_mode;
    pthread_mutex_unlock(&camera->read_mutex);

    while (desired_mode != SHUTDOWN)
    {
        // Start Acquisition
        if (desired_mode == ACQUIRING && camera->mode == IDLE)
        {
            set_mode(ACQUIRE_START);

            camera->frame_height = 512;
            camera->frame_width = 512;

            // Create a buffer to write a simulated frame to
            camera->image_buffer_size = 512*512*2;
            camera->image_buffer = (uns16*)malloc( camera->image_buffer_size );

            // Delay a bit to simulate hardware startup time
            sleep(2);
            pn_log("Simulated acquisition run started");

            set_mode(ACQUIRING);
        }

        // Enter an intermediate waiting state while we wait for the
        // timer to say it is safe to stop the acquisition sequence
        if (desired_mode == ACQUIRE_WAIT && camera->mode == ACQUIRING)
            camera->mode = ACQUIRE_WAIT;

        // Stop acquisition
        if (desired_mode == IDLE && camera->mode == ACQUIRE_WAIT)
            stop_acquiring_simulated();

        // Check for new frame
        pthread_mutex_lock(&gps->read_mutex);
        int downloading = gps->camera_downloading;
        pthread_mutex_unlock(&gps->read_mutex);

        if (camera->mode == ACQUIRING && downloading)
        {
            pn_log("Frame available @ %d", (int)time(NULL));

            // Do something with the frame data
            PNFrame frame;
            frame.width = camera->frame_width;
            frame.height = camera->frame_height;
            frame.data = camera->image_buffer;
            frame_downloaded(&frame);

            // There is no physical camera for the timer to monitor
            // so we must toggle this manually
            pthread_mutex_lock(&gps->read_mutex);
            gps->camera_downloading = FALSE;
            pthread_mutex_unlock(&gps->read_mutex);
        }

        nanosleep(&wait, NULL);
        pthread_mutex_lock(&camera->read_mutex);
        desired_mode = camera->desired_mode;
        pthread_mutex_unlock(&camera->read_mutex);
    }

    // Shutdown camera
    if (camera->mode == ACQUIRING)
        stop_acquiring_simulated();

    pthread_exit(NULL);
}

