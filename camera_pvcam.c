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

char *gain_names[3] = {"Low Gain", "Medium Gain", "High Gain"};

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

    pn_log("PVCAM error: %d = %s.", error, pvmsg);
    trigger_fatal_error(msg);
    pthread_exit(NULL);
}

// Sample the camera temperature to be read by the other threads in a threadsafe manner
static void read_temperature()
{
    int16 temp;
    if (pl_get_param(handle, PARAM_TEMP, ATTR_CURRENT, &temp ))
        pvcam_error("Failed to query temperature.");

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
        pvcam_error("Failed to query camera status.");

    return (status == FRAME_AVAILABLE);
}

static void load_readout_settings()
{
    uns32 p_count;
	if (!pl_get_param(handle, PARAM_READOUT_PORT, ATTR_COUNT, (void*)&p_count))
        pvcam_error("Failed to query readout port count.");

    camera->port_count = p_count;
    camera->ports = calloc(camera->port_count, sizeof(struct camera_readout_port));
    if (!camera->ports)
        trigger_fatal_error("Failed to allocate memory for readout ports.");

    char str[100];
    for (uint8_t i = 0; i < camera->port_count; i++)
    {
        struct camera_readout_port *port = &camera->ports[i];
        int32 value;
        if (!pl_get_enum_param(handle, PARAM_READOUT_PORT, i, &value, str, 100))
            pvcam_error("Failed to query readout port value.");

        port->id = i;
        port->name = strdup(str);

        // Set the active port then query readout speeds
        if (camera->port_count > 1 && !pl_set_param(handle, PARAM_READOUT_PORT, (void*)&i))
            pvcam_error("Failed to set readout port.");

        int16 speed_min, speed_max;
        if (!pl_get_param(handle, PARAM_SPDTAB_INDEX, ATTR_MIN, (void*)&speed_min));
            pvcam_error("Failed to query readout min.");
        if (!pl_get_param(handle, PARAM_SPDTAB_INDEX, ATTR_MAX, (void*)&speed_max));
            pvcam_error("Failed to query readout max.");

        port->speed_count = speed_max - speed_min + 1;
        port->speeds = calloc(port->speed_count, sizeof(struct camera_readout_speed));
        if (!port->speeds)
            trigger_fatal_error("Failed to allocate memory for readout speeds.");

        for (uint8_t j = 0; j <= speed_max - speed_min; j++)
        {
            struct camera_readout_speed *speed = &camera->ports[i].speeds[j];
            if (!pl_set_param(handle, PARAM_SPDTAB_INDEX, (int16[]){speed_min + j}))
                pvcam_error("Failed to set readout mode.");

            int16 gain_min, gain_max, pix_time;
            if (!pl_get_param(handle, PARAM_GAIN_INDEX, ATTR_MIN, (void*)&gain_min));
                pvcam_error("Failed to query gain min.");
            if (!pl_get_param(handle, PARAM_GAIN_INDEX, ATTR_MAX, (void*)&gain_max));
                pvcam_error("Failed to query gain max.");
            if (!pl_get_param(handle, PARAM_PIX_TIME, ATTR_CURRENT, (void*)&pix_time))
                pvcam_error("Failed to query pixel time.");

            snprintf(str, 100, "%0.1f MHz", 1.0e3/pix_time);
            speed->id = speed_min + j;
            speed->name = strdup(str);

            speed->gain_count = gain_max - gain_min + 1;
            speed->gains = calloc(speed->gain_count, sizeof(struct camera_readout_gain));
            if (!speed->gains)
                trigger_fatal_error("Failed to allocate memory for readout gains.");

            for (uint8_t k = 0; k <= gain_max - gain_min; k++)
            {
                struct camera_readout_gain *gain = &camera->ports[i].speeds[j].gains[k];
                gain->id = k;
                gain->name = (gain_max - gain_min == 2) ? strdup(gain_names[k]) : strdup("Unknown");
            }
        }
    }
}

static void set_readout_port()
{
    char str[100];
    int32 value;

    // Set readout port by constraint index
    uns32 port_count;
	if (!pl_get_param(handle, PARAM_READOUT_PORT, ATTR_COUNT, (void*)&port_count))
        pvcam_error("Failed to query readout port count.");

    pn_log("Available Readout Ports:");
    for (size_t i = 0; i < port_count; i++)
    {
        if (!pl_get_enum_param(handle, PARAM_READOUT_PORT, i, &value, str, 100))
            pvcam_error("Failed to query readout port value.");
        pn_log("   %d = %s", i, str);
    }

    // Can only configure cameras with multiple ports
    if (port_count == 1)
        return;

    const size_t readport_id = pn_preference_char(CAMERA_READPORT_MODE);
    if (readport_id < port_count)
    {
        if (!pl_set_param(handle, PARAM_READOUT_PORT, (void*)&readport_id))
            pvcam_error("Failed to set readout port.");

        if (!pl_get_enum_param(handle, PARAM_READOUT_PORT, readport_id, &value, str, 100))
            pvcam_error("Failed to set readout port value.");
        pn_log("Readout Port set to %s.", str);
    }
    else
        pn_log("Invalid Readout Port: %d. Ignoring value.", readport_id);
}

// Readout speed and gain
static void set_speed_table()
{
    int16 speed_min, speed_max;
    if (!pl_get_param(handle, PARAM_SPDTAB_INDEX, ATTR_MIN, (void*)&speed_min));
        pvcam_error("Failed to query readout min.");
    if (!pl_get_param(handle, PARAM_SPDTAB_INDEX, ATTR_MAX, (void*)&speed_max));
        pvcam_error("Failed to query readout max.");

    pn_log("Available Readout Modes:");
    for (int16 i = speed_min; i <= speed_max; i++)
    {
        if (!pl_set_param(handle, PARAM_SPDTAB_INDEX, (void*)&i))
            pvcam_error("Failed to set readout mode.");

        int16 gain_min, gain_max, pix_time;
        if (!pl_get_param(handle, PARAM_GAIN_INDEX, ATTR_MIN, (void*)&gain_min));
            pvcam_error("Failed to query gain min.");
        if (!pl_get_param(handle, PARAM_GAIN_INDEX, ATTR_MAX, (void*)&gain_max));
            pvcam_error("Failed to query gain max.");
        if (!pl_get_param(handle, PARAM_PIX_TIME, ATTR_CURRENT, (void*)&pix_time))
            pvcam_error("Failed to query pixel time.");

        pn_log("   %d = %0.1f MHz; Gain ids %d-%d", i, 1.0e3/pix_time, gain_min, gain_max);
    }

    const int16 readspeed_id = pn_preference_char(CAMERA_READSPEED_MODE);
    const int16 gain_id = pn_preference_char(CAMERA_GAIN_MODE);
    if (speed_min <= readspeed_id && readspeed_id <= speed_max)
    {
        if (!pl_set_param(handle, PARAM_SPDTAB_INDEX, (void*)&readspeed_id))
            pvcam_error("Failed to set readout mode.");

        int16 gain_min, gain_max, pix_time;
        if (!pl_get_param(handle, PARAM_GAIN_INDEX, ATTR_MIN, (void*)&gain_min));
            pvcam_error("Failed to query gain min.");
        if (!pl_get_param(handle, PARAM_GAIN_INDEX, ATTR_MAX, (void*)&gain_max));
            pvcam_error("Failed to query gain max.");
        if (!pl_get_param(handle, PARAM_PIX_TIME, ATTR_CURRENT, (void*)&pix_time))
            pvcam_error("Failed to query pixel time.");

        pn_log("Set readout speed to %0.1f MHz.", 1.0e3/pix_time);

        if (gain_min <= gain_id && gain_id <= gain_max)
        {
            if (!pl_set_param(handle, PARAM_GAIN_INDEX, (void*)&gain_id))
                pvcam_error("Failed to set gain.");

            pn_log("Set gain index to %d.", gain_id);
        }
        else
            pn_log("Invalid gain index: %d. Ignoring value.", gain_id);
    }
    else
        pn_log("Invalid Readout speed index: %d. Ignoring value.", readspeed_id);
}

// Initialize PVCAM and the camera hardware
static void initialize_camera()
{
    set_mode(INITIALISING);

    if (!pl_pvcam_init())
        pvcam_error("Failed to initialize PVCAM.");

    uns16 pversion;
    if (!pl_pvcam_get_ver(&pversion))
        pvcam_error("Failed to query PVCAM version.");

    pn_log("PVCAM Version %d.%d.%d.", pversion>>8, (pversion & 0x00F0)>>4, pversion & 0x000F, pversion);

    int16 numCams = 0;
    if (!pl_cam_get_total(&numCams))
        pvcam_error("Failed to query cameras.");

    if (numCams == 0)
    {
        trigger_fatal_error(strdup("Camera not found."));
        pthread_exit(NULL);
    }

    // Get the camera name (assume that we only have one camera)
    char cameraName[CAM_NAME_LEN];
    if (!pl_cam_get_name(0, cameraName))
        pvcam_error("Failed to query camera name.");

    // Open the camera
    if (!pl_cam_open(cameraName, &handle, OPEN_EXCLUSIVE))
        pvcam_error("Failed to open camera. Are permissions correct?");

    pn_log("Camera ID: \"%s\".", cameraName);

    // Check camera status
    if (!pl_cam_get_diags(handle))
        pvcam_error("Camera failed diagnostic checks.");

    // Set camera parameters
    if (!pl_set_param(handle, PARAM_SHTR_CLOSE_DELAY, (void*) &(uns16){0}))
        pvcam_error("Failed to set PARAM_SHTR_CLOSE_DELAY.");

    if (!pl_set_param(handle, PARAM_LOGIC_OUTPUT, (void*) &(int){OUTPUT_NOT_SCAN}))
        pvcam_error("Failed to set OUTPUT_NOT_SCAN.");

    // Trigger on positive edge of the download pulse
    if (!pl_set_param(handle, PARAM_EDGE_TRIGGER, (void*) &(int){EDGE_TRIG_NEG}))
        pvcam_error("Failed to set PARAM_EDGE_TRIGGER.");

    // Use custom frame-transfer readout mode
    if (!pl_set_param(handle, PARAM_FORCE_READOUT_MODE, (void*) &(int){MAKE_FRAME_TRANSFER}))
        pvcam_error("Failed to set PARAM_FORCE_READOUT_MODE.");

    // Set temperature
    if (!pl_set_param(handle, PARAM_TEMP_SETPOINT, (void*) &(int){pn_preference_int(CAMERA_TEMPERATURE)}))
        pvcam_error("Failed to set PARAM_TEMP_SETPOINT.");

    load_readout_settings();

    pn_log("Camera is now idle.");
    set_mode(IDLE);
}

// Start an acquisition sequence
static void start_acquiring()
{
    set_mode(ACQUIRE_START);

    pn_log("Camera is preparing for acquisition.");
    set_readout_port();
    set_speed_table();

    if (!pl_get_param(handle, PARAM_SER_SIZE, ATTR_DEFAULT, (void *)&camera->frame_width))
        pvcam_error("Failed to query CCD width.");

    if (!pl_get_param(handle, PARAM_PAR_SIZE, ATTR_DEFAULT, (void *)&camera->frame_height))
        pvcam_error("Failed to query CCD height.");

    unsigned char superpixel_size = pn_preference_char(CAMERA_PIXEL_SIZE);

    if (pn_preference_char(CAMERA_OVERSCAN_ENABLED))
    {
        // Enable custom chip so we can add a bias strip
        if (!pl_set_param(handle, PARAM_CUSTOM_CHIP, (void*) &(rs_bool){TRUE}))
            pvcam_error("Failed to set PARAM_CUSTOM_CHIP.");

        // Increase frame width by the requested amount
        camera->frame_width += pn_preference_char(CAMERA_OVERSCAN_SKIP_COLS) + pn_preference_char(CAMERA_OVERSCAN_BIAS_COLS);
        if (!pl_set_param(handle, PARAM_SER_SIZE, (void*) &(uns16){camera->frame_width}))
            pvcam_error("Failed to set PARAM_SER_SIZE.");

        // Remove postscan - this is handled by the additional overscan readouts
        // PVCAM seems to have mislabeled *SCAN and *MASK
        if (!pl_set_param(handle, PARAM_POSTMASK, (void*) &(uns16){0}))
            pvcam_error("Failed to set PARAM_POSTMASK.");
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

    pn_log("Pixel size set to %dx%d.", superpixel_size, superpixel_size);

    // Init exposure control libs
    if (!pl_exp_init_seq())
        pvcam_error("Failed to initialize exposure sequence.");

    // Set exposure mode: expose entire chip, expose on sync pulses (exposure time unused), overwrite buffer
    if (!pl_exp_setup_cont(handle, 1, &region, STROBED_MODE, 0, &image_buffer_size, CIRC_NO_OVERWRITE))
        pvcam_error("Failed to setup exposure sequence.");

    // Create a buffer big enough to hold 1 image
    image_buffer = (uns16*)malloc(image_buffer_size);
    if (image_buffer == NULL)
        trigger_fatal_error("Failed to allocate frame buffer.");

    // Start waiting for sync pulses to trigger exposures
    if (!pl_exp_start_cont(handle, image_buffer, image_buffer_size))
        pvcam_error("Failed to start exposure sequence.");

    // Sample initial temperature
    read_temperature();

    first_frame = true;
    camera->safe_to_stop_acquiring = false;
    pn_log("Camera is now acquiring.");
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
        pn_log("Discarding buffered frame.");
    }

    if (!pl_exp_stop_cont(handle, CCS_HALT))
        pn_log("Failed to stop exposure sequence.");

    if (!pl_exp_finish_seq(handle, image_buffer, 0))
        pn_log("Failed to finish exposure sequence.");

    if (!pl_exp_uninit_seq())
        pn_log("Failed to uninitialize exposure sequence.");

    free(image_buffer);
    pn_log("Camera is now idle.");
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
            void_ptr camera_frame;
            if (!pl_exp_get_oldest_frame(handle, &camera_frame))
                pvcam_error("Error retrieving oldest frame.");

            // PVCAM triggers end the frame, and so the first frame
            // will consist of the sync and align time period.
            // Discard this frame.
            if (first_frame)
            {
                pn_log("Discarding pre-exposure readout.");
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
                        pn_log("Failed to allocate CameraFrame->data. Discarding frame.");
                }
                else
                    pn_log("Failed to allocate CameraFrame. Discarding frame.");
            }

            // Unlock the frame buffer for reuse
            if (!pl_exp_unlock_oldest_frame(handle))
                pvcam_error("Failed to unlock oldest frame.");
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
            pn_log("Failed to uninitialize PVCAM.");
        pn_log("PVCAM uninitialized.");
    }

    return NULL;
}
