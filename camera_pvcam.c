/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#include "main.h"
#include "camera.h"
#include "preferences.h"
#include "platform.h"

#include <master.h>
#include <pvcam.h>

extern PNCamera *camera;

static int16 handle = -1;
static void *image_buffer = NULL;
static uns32 image_buffer_size = 0;

// Custom frame transfer mode options
#define PARAM_FORCE_READOUT_MODE ((CLASS2<<16) + (TYPE_UNS32<<24) + 326)
enum ForceReadOut {
    ALWAYS_CHECK_EXP,
    MAKE_FULL,
    MAKE_FRAME_TRANSFER,
    MAKE_AUTOMATIC
};

// Used to determine and discard the first frame
// corresponding to the exposure before the first
// trigger. This allows us to count all download timestamps
// as the beginning of the corresponding frame
bool first_frame;

// Generate a fatal error based on the pvcam error
static void pvcam_error(const char *msg)
{
    int error = pl_error_code();
    if (!error)
        return;

    char pvmsg[ERROR_MSG_LEN];
    pvmsg[0] = '\0';
    pl_error_message(error, pvmsg);

    pn_log("PVCAM error: %d = %s", error, pvmsg);
    trigger_fatal_error(msg);
    pthread_exit(NULL);
}

// Sample the camera temperature to be read by the other threads in a threadsafe manner
static void read_temperature()
{
    int16 temp;
    if (pl_get_param(handle, PARAM_TEMP, ATTR_CURRENT, &temp ))
        pvcam_error("Error querying temperature");

    pthread_mutex_lock(&camera->read_mutex);
    camera->temperature = (float)temp/100;
    pthread_mutex_unlock(&camera->read_mutex);
}

// Check whether a frame is available
static bool frame_available()
{
    int16 status = READOUT_NOT_ACTIVE;
    uns32 bytesStored = 0, numFilledBuffers = 0;
    if (!pl_exp_check_cont_status(handle, &status, &bytesStored, &numFilledBuffers))
        pvcam_error("Error querying camera status");

    return (status == FRAME_AVAILABLE);
}


static void set_readout_port()
{
    char str[100];
    int32 value;

    // Set readout port by constraint index
    uns32 port_count;
	if (!pl_get_param(handle, PARAM_READOUT_PORT, ATTR_COUNT, (void*)&port_count))
        pvcam_error("Error querying readout port count");

    pn_log("Available Readout Ports:");
    for (size_t i = 0; i < port_count; i++)
    {
        if (!pl_get_enum_param(handle, PARAM_READOUT_PORT, i, &value, str, 100))
            pvcam_error("Error querying readout port value");
        pn_log("   %d = %s", i, str);
    }

    // Can only configure cameras with multiple ports
    if (port_count == 1)
        return;

    const size_t readport_id = pn_preference_char(CAMERA_READPORT_MODE);
    if (readport_id < port_count)
    {
        if (!pl_set_param(handle, PARAM_READOUT_PORT, (void*)&readport_id))
            pvcam_error("Error setting readout port");

        if (!pl_get_enum_param(handle, PARAM_READOUT_PORT, readport_id, &value, str, 100))
            pvcam_error("Error querying readout port value");
        pn_log("Readout Port set to %s", str);
    }
    else
        pn_log("Invalid Readout Port requested: %d", readport_id);
}

// Readout speed and gain
static void set_speed_table()
{
    int16 speed_min, speed_max;
    if (!pl_get_param(handle, PARAM_SPDTAB_INDEX, ATTR_MIN, (void*)&speed_min));
        pvcam_error("Error querying readout speed value");
    if (!pl_get_param(handle, PARAM_SPDTAB_INDEX, ATTR_MAX, (void*)&speed_max));
        pvcam_error("Error querying readout speed value");

    pn_log("Available Readout Modes:");
    for (int16 i = speed_min; i <= speed_max; i++)
    {
        if (!pl_set_param(handle, PARAM_SPDTAB_INDEX, (void*)&i))
            pvcam_error("Error setting readout value");

        int16 gain_min, gain_max, pix_time;
        if (!pl_get_param(handle, PARAM_GAIN_INDEX, ATTR_MIN, (void*)&gain_min));
            pvcam_error("Error querying gain value");
        if (!pl_get_param(handle, PARAM_GAIN_INDEX, ATTR_MAX, (void*)&gain_max));
            pvcam_error("Error querying gain value");
        if (!pl_get_param(handle, PARAM_PIX_TIME, ATTR_CURRENT, (void*)&pix_time))
            pvcam_error("Error querying pixel time");

        pn_log("   %d = %0.1f MHz; Gain ids %d-%d", i, 1.0e3/pix_time, gain_min, gain_max);
    }

    const int16 readspeed_id = pn_preference_char(CAMERA_READSPEED_MODE);
    const int16 gain_id = pn_preference_char(CAMERA_GAIN_MODE);
    if (speed_min <= readspeed_id && readspeed_id <= speed_max)
    {
        if (!pl_set_param(handle, PARAM_SPDTAB_INDEX, (void*)&readspeed_id))
            pvcam_error("Error setting readout value");

        int16 gain_min, gain_max, pix_time;
        if (!pl_get_param(handle, PARAM_GAIN_INDEX, ATTR_MIN, (void*)&gain_min));
            pvcam_error("Error querying gain value");
        if (!pl_get_param(handle, PARAM_GAIN_INDEX, ATTR_MAX, (void*)&gain_max));
            pvcam_error("Error querying gain value");
        if (!pl_get_param(handle, PARAM_PIX_TIME, ATTR_CURRENT, (void*)&pix_time))
            pvcam_error("Error querying pixel time");

        pn_log("Set readout speed to %0.1f MHz", 1.0e3/pix_time);

        if (gain_min <= gain_id && gain_id <= gain_max)
        {
            if (!pl_set_param(handle, PARAM_GAIN_INDEX, (void*)&gain_id))
                pvcam_error("Error setting gain value");

            pn_log("Set gain index to %d", gain_id);
        }
        else
            pn_log("Invalid gain index requested: %d", gain_id);
    }
    else
        pn_log("Invalid Readout speed index requested: %d", readspeed_id);
}

// Initialize PVCAM and the camera hardware
static void initialize_camera()
{
    set_mode(INITIALISING);

    if (!pl_pvcam_init())
        pvcam_error("Could not initialize PVCAM library");

    uns16 pversion;
    if (!pl_pvcam_get_ver(&pversion))
        pvcam_error("Cannot query pvcam version");

    pn_log("PVCAM Version %d.%d.%d (0x%x) initialized", pversion>>8, (pversion & 0x00F0)>>4, pversion & 0x000F, pversion);

    int16 numCams = 0;
    if (!pl_cam_get_total(&numCams))
        pvcam_error("Cannot query the number of cameras");

    pn_log("Found %d camera(s)", numCams);
    if (numCams == 0)
    {
        trigger_fatal_error(strdup("Camera not found"));
        pthread_exit(NULL);
    }

    // Get the camera name (assume that we only have one camera)
    char cameraName[CAM_NAME_LEN];
    if (!pl_cam_get_name(0, cameraName))
        pvcam_error("Cannot open the camera as could not get the camera name");

    // Open the camera
    if (!pl_cam_open(cameraName, &handle, OPEN_EXCLUSIVE))
        pvcam_error("Cannot open the camera. Are you running as root?");

    // Print the camera firmware version if available
    char fwver_buf[16] = "Unknown";
    uns16 fwver;
    rs_bool avail = FALSE;
    if (!pl_get_param(handle, PARAM_CAM_FW_VERSION, ATTR_AVAIL, (void *)&avail))
        pvcam_error("Error querying camera fw version");

    if (avail)
    {
        if (pl_get_param(handle, PARAM_CAM_FW_VERSION, ATTR_CURRENT, (void *)&fwver))
            pvcam_error("Error querying camera fw version");
        sprintf(fwver_buf, "%d.%d (0x%x)", fwver >> 8, fwver & 0x00FF, fwver);
    }
    pn_log("Opened camera `%s`: Firmware version %s", cameraName, fwver_buf);

    // Check camera status
    if (!pl_cam_get_diags(handle))
        pvcam_error("Camera failed diagnostic checks");

    // Set camera parameters
    if (!pl_set_param(handle, PARAM_SHTR_CLOSE_DELAY, (void*) &(uns16){0}))
        pvcam_error("Error setting PARAM_SHTR_CLOSE_DELAY");

    if (!pl_set_param(handle, PARAM_LOGIC_OUTPUT, (void*) &(int){OUTPUT_NOT_SCAN}))
        pvcam_error("Error setting OUTPUT_NOT_SCAN");

    // Trigger on positive edge of the download pulse
    if (!pl_set_param(handle, PARAM_EDGE_TRIGGER, (void*) &(int){EDGE_TRIG_NEG}))
        pvcam_error("Error setting PARAM_EDGE_TRIGGER");

    // Use custom frame-transfer readout mode
    if (!pl_set_param(handle, PARAM_FORCE_READOUT_MODE, (void*) &(int){MAKE_FRAME_TRANSFER}))
        pvcam_error("Error setting PARAM_FORCE_READOUT_MODE");

    // Set temperature
    if (!pl_set_param(handle, PARAM_TEMP_SETPOINT, (void*) &(int){pn_preference_int(CAMERA_TEMPERATURE)}))
        pvcam_error("Error setting PARAM_TEMP_SETPOINT");

    set_readout_port();
    set_speed_table();

    pn_log("Camera initialized");
    set_mode(IDLE);
}

// Start an acquisition sequence
static void start_acquiring()
{
    set_mode(ACQUIRE_START);

    pn_log("Starting acquisition run...");
    if (!pl_get_param(handle, PARAM_SER_SIZE, ATTR_DEFAULT, (void *)&camera->frame_width))
        pvcam_error("Error querying camera width");

    if (!pl_get_param(handle, PARAM_PAR_SIZE, ATTR_DEFAULT, (void *)&camera->frame_height))
        pvcam_error("Error querying camera height");

    unsigned char superpixel_size = pn_preference_char(CAMERA_PIXEL_SIZE);

    if (pn_preference_char(CAMERA_OVERSCAN_ENABLED))
    {
        // Enable custom chip so we can add a bias strip
        if (!pl_set_param(handle, PARAM_CUSTOM_CHIP, (void*) &(rs_bool){TRUE}))
            pvcam_error("Error setting PARAM_CUSTOM_CHIP");

        // Increase frame width by the requested amount
        camera->frame_width += pn_preference_char(CAMERA_OVERSCAN_SKIP_COLS) + pn_preference_char(CAMERA_OVERSCAN_BIAS_COLS);
        if (!pl_set_param(handle, PARAM_SER_SIZE, (void*) &(uns16){camera->frame_width}))
            pvcam_error("Error setting PARAM_SER_SIZE");

        // Remove postscan - this is handled by the additional overscan readouts
        // PVCAM seems to have mislabeled *SCAN and *MASK
        if (!pl_set_param(handle, PARAM_POSTMASK, (void*) &(uns16){0}))
            pvcam_error("Error setting PARAM_POSTMASK");
    }

    rgn_type region;
    region.s1 = 0;                      // x start ('serial' direction)
    region.s2 = camera->frame_width-1;  // x end
    region.sbin = superpixel_size;      // x binning (1 = no binning)
    region.p1 = 0;                      // y start ('parallel' direction)
    region.p2 = camera->frame_height-1; // y end
    region.pbin = superpixel_size;      // y binning (1 = no binning)

    // Divide the chip size by the bin size to find the frame dimensions
    camera->frame_height /= superpixel_size;
    camera->frame_width /= superpixel_size;

    pn_log("Pixel size set to %dx%d", superpixel_size, superpixel_size);

    // Init exposure control libs
    if (!pl_exp_init_seq())
        pvcam_error("pl_exp_init_seq failed");

    // Set exposure mode: expose entire chip, expose on sync pulses (exposure time unused), overwrite buffer
    if (!pl_exp_setup_cont(handle, 1, &region, STROBED_MODE, 0, &image_buffer_size, CIRC_NO_OVERWRITE))
        pvcam_error("pl_exp_setup_cont failed");

    // Create a buffer big enough to hold 1 image
    image_buffer = (uns16*)malloc(image_buffer_size);

    // Start waiting for sync pulses to trigger exposures
    if (!pl_exp_start_cont(handle, image_buffer, image_buffer_size))
        pvcam_error("pl_exp_start_cont failed");

    pn_log("Acquisition run started");

    // Sample initial temperature
    read_temperature();

    first_frame = true;
    camera->safe_to_stop_acquiring = false;
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
        pl_exp_get_oldest_frame(handle, &camera_frame);
        pl_exp_unlock_oldest_frame(handle);
        pn_log("Discarding buffered frame");
    }

    if (!pl_exp_stop_cont(handle, CCS_HALT))
        pn_log("Error stopping sequence");

    if (!pl_exp_finish_seq(handle, image_buffer, 0))
        pn_log("Error finishing sequence");

    if (!pl_exp_uninit_seq())
        pn_log("Error uninitialising sequence");

    free(image_buffer);
    pn_log("Acquisition sequence uninitialized");

    set_mode(IDLE);
}

// Main camera thread loop
void *pn_pvcam_camera_thread(void *_args)
{
    ThreadCreationArgs *args = (ThreadCreationArgs *)_args;
    PNCamera *camera = args->camera;

    // Initialize the camera
    initialize_camera();

    // Loop and respond to user commands
    int temp_ticks = 0;

    pthread_mutex_lock(&camera->read_mutex);
    PNCameraMode desired_mode = camera->desired_mode;
    bool safe_to_stop_acquiring = camera->safe_to_stop_acquiring;
    pthread_mutex_unlock(&camera->read_mutex);

    while (desired_mode != SHUTDOWN)
    {
        // Start/stop acquisition
        if (desired_mode == ACQUIRING && camera->mode == IDLE)
            start_acquiring();

        if (desired_mode == IDLE && camera->mode == ACQUIRING)
            camera->mode = IDLE_WHEN_SAFE;

        // Stop acquisition
        if (camera->mode == IDLE_WHEN_SAFE && safe_to_stop_acquiring)
            stop_acquiring();

        // Check for new frame
        if (camera->mode == ACQUIRING && frame_available())
        {
            pn_log("Frame available @ %d", (int)time(NULL));
            void_ptr camera_frame;
            if (!pl_exp_get_oldest_frame(handle, &camera_frame))
                pvcam_error("Error retrieving oldest frame");

            // PVCAM triggers end the frame, and so the first frame
            // will consist of the sync and align time period.
            // Discard this frame.
            if (first_frame)
            {
                pn_log("Discarding pre-exposure readout");
                first_frame = false;
            }
            else
            {
                // Copy frame data and pass ownership to main thread
                CameraFrame *frame = malloc(sizeof(CameraFrame));
                if (frame)
                {
                    size_t frame_bytes = camera->frame_width*camera->frame_height*sizeof(uint16_t);
                    frame->data = malloc(frame_bytes);
                    if (frame->data)
                    {
                        memcpy(frame->data, camera_frame, frame_bytes);
                        frame->width = camera->frame_width;
                        frame->height = camera->frame_height;
                        frame->temperature = camera->temperature;
                        queue_framedata(frame);
                    }
                    else
                        pn_log("Error allocating CameraFrame->data. Discarding frame");
                }
                else
                    pn_log("Error allocating CameraFrame. Discarding frame");
            }

            // Unlock the frame buffer for reuse
            if (!pl_exp_unlock_oldest_frame(handle))
                pvcam_error("Error unlocking oldest frame");
        }

        // Check temperature
        if (++temp_ticks >= 50)
        {
            temp_ticks = 0;
            read_temperature();
        }
        millisleep(100);

        pthread_mutex_lock(&camera->read_mutex);
        desired_mode = camera->desired_mode;
        safe_to_stop_acquiring = camera->safe_to_stop_acquiring;
        pthread_mutex_unlock(&camera->read_mutex);
    }

    // Shutdown camera
    if (camera->mode == ACQUIRING || camera->mode == IDLE_WHEN_SAFE)
        stop_acquiring();

    // Close the PVCAM lib (which in turn closes the camera)
    if (camera->mode == IDLE)
    {    
        if (!pl_pvcam_uninit())
            pn_log("Error uninitialising PVCAM");
        pn_log("PVCAM uninitialized");
    }

    return NULL;
}
