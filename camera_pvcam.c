/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include "main.h"
#include "camera.h"
#include "preferences.h"
#include "platform.h"

#include <master.h>
#include <pvcam.h>

// Custom frame transfer mode options
#define PARAM_FORCE_READOUT_MODE ((CLASS2<<16) + (TYPE_UNS32<<24) + 326)
enum ForceReadOut {
    ALWAYS_CHECK_EXP,
    MAKE_FULL,
    MAKE_FRAME_TRANSFER,
    MAKE_AUTOMATIC
};

// Holds the state of a camera
struct internal
{
    int16 handle;
    uns32 frame_size;
    uns16 *frame_buffer;

    uint16_t ccd_width;
    uint16_t ccd_height;

    uint16_t frame_width;
    uint16_t frame_height;
    bool first_frame;
};

static char *gain_names[] = {"Low", "Medium", "High"};

static void fatal_pvcam_error(char *format, ...)
{
    va_list args;
    va_start(args, format);
    int len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    char *fatal_error = malloc((len + 1)*sizeof(char));
    if (fatal_error)
    {
        va_start(args, format);
        vsnprintf(fatal_error, len + 1, format, args);
        va_end(args);
    }
    else
        pn_log("Failed to allocate memory for fatal error.");

    int error = pl_error_code();
    char pvmsg[ERROR_MSG_LEN];
    pvmsg[0] = '\0';
    pl_error_message(error, pvmsg);

    pn_log("PVCAM error: %d = %s.", error, pvmsg);
    trigger_fatal_error(fatal_error);
    pthread_exit(NULL);
}

static void fatal_error(char *format, ...)
{
    va_list args;
    va_start(args, format);
    int len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    char *fatal_error = malloc((len + 1)*sizeof(char));
    if (fatal_error)
    {
        va_start(args, format);
        vsnprintf(fatal_error, len + 1, format, args);
        va_end(args);
    }
    else
        pn_log("Failed to allocate memory for fatal error.");

    trigger_fatal_error(fatal_error);
    pthread_exit(NULL);
}

static bool frame_available(struct internal *internal)
{
    int16 status = READOUT_NOT_ACTIVE;
    uns32 bytesStored = 0, numFilledBuffers = 0;
    if (!pl_exp_check_cont_status(internal->handle, &status, &bytesStored, &numFilledBuffers))
        fatal_pvcam_error("Failed to query camera status.");

    return (status == FRAME_AVAILABLE);
}

#define get_param(handle, param, attrib, value) _get_param(handle, param, #param, attrib, #attrib, (void*)value)
static void _get_param(int16 handle, uns32 param, char *param_name, int16 attrib, char *attrib_name, void_ptr value)
{
    if (!pl_get_param(handle, param, attrib, value))
        fatal_pvcam_error("Failed to query %s %s", param_name, attrib_name);
}

#define set_param(handle, param, value) _set_param(handle, param, #param, (void*)value)
static void _set_param(int16 handle, uns32 param, char *param_name, void_ptr value)
{
    if (!pl_set_param(handle, param, value))
        fatal_pvcam_error("Failed to set %s", param_name);
}

void *camera_pvcam_initialize(Camera *camera, ThreadCreationArgs *args)
{
    struct internal *internal = calloc(1, sizeof(struct internal));
    if (!internal)
        return NULL;

    if (!pl_pvcam_init())
        fatal_pvcam_error("Failed to initialize PVCAM.");

    uns16 pversion;
    if (!pl_pvcam_get_ver(&pversion))
        fatal_pvcam_error("Failed to query PVCAM version.");

    pn_log("PVCAM Version %d.%d.%d.", pversion>>8, (pversion & 0x00F0)>>4, pversion & 0x000F, pversion);

    int16 numCams = 0;
    if (!pl_cam_get_total(&numCams))
        fatal_pvcam_error("Failed to query cameras.");

    if (numCams == 0)
        fatal_error(strdup("Camera not found."));

    // Get the camera name (assume that we only have one camera)
    char cameraName[CAM_NAME_LEN];
    if (!pl_cam_get_name(0, cameraName))
        fatal_pvcam_error("Failed to query camera name.");

    // Open the camera
    if (!pl_cam_open(cameraName, &internal->handle, OPEN_EXCLUSIVE))
        fatal_pvcam_error("Failed to open camera. Are permissions correct?");

    pn_log("Camera ID: \"%s\".", cameraName);

    // Check camera status
    if (!pl_cam_get_diags(internal->handle))
        fatal_pvcam_error("Camera failed diagnostic checks.");

    set_param(internal->handle, PARAM_SHTR_CLOSE_DELAY, &(uns16){0});
    set_param(internal->handle, PARAM_LOGIC_OUTPUT, &(int){OUTPUT_NOT_SCAN});
    set_param(internal->handle, PARAM_EDGE_TRIGGER, &(int){EDGE_TRIG_NEG});
    set_param(internal->handle, PARAM_FORCE_READOUT_MODE, &(int){MAKE_FRAME_TRANSFER});


    get_param(internal->handle, PARAM_SER_SIZE, ATTR_DEFAULT, &internal->ccd_width);
    get_param(internal->handle, PARAM_PAR_SIZE, ATTR_DEFAULT, &internal->ccd_height);

    if (pn_preference_char(CAMERA_OVERSCAN_ENABLED))
    {
        // Enable custom chip so we can add a bias strip
        set_param(internal->handle, PARAM_CUSTOM_CHIP, &(rs_bool){true});

        // Increase frame width by the requested amount
        internal->ccd_width += pn_preference_char(CAMERA_OVERSCAN_SKIP_COLS) + pn_preference_char(CAMERA_OVERSCAN_BIAS_COLS);
        set_param(internal->handle, PARAM_SER_SIZE, &(uns16){internal->ccd_width});

        // Remove postscan - this is handled by the additional overscan readouts
        // PVCAM seems to have mislabeled *SCAN and *MASK
        set_param(internal->handle, PARAM_POSTMASK, &(uns16){0});
    }

    // Init exposure control libs
    if (!pl_exp_init_seq())
        fatal_pvcam_error("Failed to initialize exposure sequence.");

    return internal;
}

double camera_pvcam_update_camera_settings(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;

    // Validate requested settings
    uns32 port_count;
    uint8_t port_id = pn_preference_char(CAMERA_READPORT_MODE);
    get_param(internal->handle, PARAM_READOUT_PORT, ATTR_COUNT, &port_count);
    if (port_id >= port_count)
    {
        pn_log("Invalid port index: %d. Reset to %d.", port_id, 0);
        pn_preference_set_char(CAMERA_READPORT_MODE, 0);
        port_id = 0;
    }

    if (port_count > 1)
        set_param(internal->handle, PARAM_READOUT_PORT, &port_id);

    uint8_t speed_id = pn_preference_char(CAMERA_READSPEED_MODE);
    int16 speed_min, speed_max;
    get_param(internal->handle, PARAM_SPDTAB_INDEX, ATTR_MIN, &speed_min);
    get_param(internal->handle, PARAM_SPDTAB_INDEX, ATTR_MAX, &speed_max);

    int16 speed_value = speed_id + speed_min;
    if (speed_value > speed_max)
    {
        pn_log("Invalid speed index: %d. Reset to %d.", speed_id, 0);
        pn_preference_set_char(CAMERA_READSPEED_MODE, 0);
        speed_value = speed_min;
    }
    set_param(internal->handle, PARAM_SPDTAB_INDEX, &speed_value);

    uint8_t gain_id = pn_preference_char(CAMERA_GAIN_MODE);
    int16 gain_min, gain_max, pix_time;
    get_param(internal->handle, PARAM_GAIN_INDEX, ATTR_MIN, &gain_min);
    get_param(internal->handle, PARAM_GAIN_INDEX, ATTR_MAX, &gain_max);
    get_param(internal->handle, PARAM_PIX_TIME, ATTR_CURRENT, &pix_time);

    int16 gain_value = gain_id + gain_min;
    if (gain_value > gain_max)
    {
        pn_log("Invalid gain index: %d. Reset to %d.", gain_id, 0);
        pn_preference_set_char(CAMERA_GAIN_MODE, 0);
        gain_value = gain_min;
    }
    set_param(internal->handle, PARAM_GAIN_INDEX, &gain_value);

    set_param(internal->handle, PARAM_TEMP_SETPOINT, &(int){pn_preference_int(CAMERA_TEMPERATURE)});

    // Set readout area
    uint8_t pixel_size = pn_preference_char(CAMERA_PIXEL_SIZE);
    rgn_type region;
    region.s1 = 0;
    region.s2 = internal->ccd_width-1;
    region.sbin = pixel_size;
    region.p1 = 0;
    region.p2 = internal->ccd_height-1;
    region.pbin = pixel_size;

    internal->frame_height = internal->ccd_width / pixel_size;
    internal->frame_width = internal->ccd_height / pixel_size;

    // Set exposure mode: expose entire chip, expose on sync pulses (exposure time unused), overwrite buffer
    if (!pl_exp_setup_cont(internal->handle, 1, &region, STROBED_MODE, 0, &internal->frame_size, CIRC_NO_OVERWRITE))
        fatal_pvcam_error("Failed to setup exposure sequence.");

    // Query readout time
    flt64 readout_time;
    get_param(internal->handle, PARAM_READOUT_TIME, ATTR_CURRENT, &readout_time);

    // convert from ms to s
    readout_time /= 1000;
    pn_log("Camera readout time is now %.2fs", readout_time);
    uint8_t exposure_time = pn_preference_char(EXPOSURE_TIME);
    if (exposure_time < readout_time)
    {
        unsigned char new_exposure = (unsigned char)(ceil(readout_time));
        pn_preference_set_char(EXPOSURE_TIME, new_exposure);
        pn_log("Increasing EXPOSURE_TIME to %d seconds.", new_exposure);
    }
    return readout_time;
}

uint8_t camera_pvcam_port_table(Camera *camera, void *_internal, struct camera_port_option **out_ports)
{
    struct internal *internal = _internal;
    uns32 port_count;
    get_param(internal->handle, PARAM_READOUT_PORT, ATTR_COUNT, &port_count);

    struct camera_port_option *ports = calloc(port_count, sizeof(struct camera_port_option));
    if (!ports)
        fatal_error("Failed to allocate memory for %d readout ports.", port_count);

    char str[100];
    int32 value;
    for (uint8_t i = 0; i < port_count; i++)
    {
        struct camera_port_option *port = &ports[i];
        if (!pl_get_enum_param(internal->handle, PARAM_READOUT_PORT, i, &value, str, 100))
            fatal_pvcam_error("Failed to query PARAM_READOUT_PORT");
        port->name = strdup(str);

        // Set the active port then query readout speeds
        if (port_count > 1)
            set_param(internal->handle, PARAM_READOUT_PORT, &i);

        int16 speed_min, speed_max;
        get_param(internal->handle, PARAM_SPDTAB_INDEX, ATTR_MIN, &speed_min);
        get_param(internal->handle, PARAM_SPDTAB_INDEX, ATTR_MAX, &speed_max);

        port->speed_count = speed_max - speed_min + 1;
        port->speed = calloc(port->speed_count, sizeof(struct camera_speed_option));
        if (!port->speed)
            fatal_error("Failed to allocate memory for %d readout speeds.", port->speed_count);

        for (uint8_t j = 0; j <= speed_max - speed_min; j++)
        {
            struct camera_speed_option *speed = &port->speed[j];
            set_param(internal->handle, PARAM_SPDTAB_INDEX, &(int16){speed_min + j});

            int16 gain_min, gain_max, pix_time;
            get_param(internal->handle, PARAM_GAIN_INDEX, ATTR_MIN, &gain_min);
            get_param(internal->handle, PARAM_GAIN_INDEX, ATTR_MAX, &gain_max);
            get_param(internal->handle, PARAM_PIX_TIME, ATTR_CURRENT, &pix_time);

            snprintf(str, 100, "%0.1f MHz", 1.0e3/pix_time);
            speed->name = strdup(str);

            speed->gain_count = gain_max - gain_min + 1;
            speed->gain = calloc(speed->gain_count, sizeof(struct camera_gain_option));
            if (!speed->gain)
                fatal_error("Failed to allocate memory for readout gains.");

            for (uint8_t k = 0; k <= gain_max - gain_min; k++)
            {
                struct camera_gain_option *gain = &speed->gain[k];
                gain->name = (gain_max - gain_min == 2) ? strdup(gain_names[k]) : strdup("Unknown");
            }
        }
    }

    *out_ports = ports;
    return port_count;
}

void camera_pvcam_uninitialize(Camera *camera, void *internal)
{
    if (!pl_exp_uninit_seq())
        pn_log("Failed to uninitialize exposure sequence.");

    if (!pl_pvcam_uninit())
        pn_log("Failed to uninitialize PVCAM.");

    free(internal);
    pn_log("PVCAM uninitialized.");
}

void camera_pvcam_start_acquiring(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;

    // Create a buffer big enough to hold 5 images.
    // PVCAM under win32 gives a DMA error if this is set to 1.
    uns32 buffer_size = 5*internal->frame_size;
    internal->frame_buffer = malloc(buffer_size*sizeof(uns16));
    if (!internal->frame_buffer)
        fatal_error("Failed to allocate frame buffer.");

    // Start waiting for sync pulses to trigger exposures
    if (!pl_exp_start_cont(internal->handle, internal->frame_buffer, buffer_size))
        fatal_pvcam_error("Failed to start exposure sequence.");

    internal->first_frame = true;
}

void camera_pvcam_stop_acquiring(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;

    // Clear any buffered frames
    void_ptr camera_frame;
    while (frame_available(internal))
    {
        pl_exp_get_oldest_frame(internal->handle, &camera_frame);
        pl_exp_unlock_oldest_frame(internal->handle);
        pn_log("Discarding buffered frame.");
    }

    if (!pl_exp_stop_cont(internal->handle, CCS_HALT))
        pn_log("Failed to stop exposure sequence.");

    if (!pl_exp_finish_seq(internal->handle, internal->frame_buffer, 0))
        pn_log("Failed to finish exposure sequence.");

    free(internal->frame_buffer);
}

double camera_pvcam_read_temperature(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;

    int16 temp;
    get_param(internal->handle, PARAM_TEMP, ATTR_CURRENT, &temp);
    return temp/100.0;
}

void camera_pvcam_tick(Camera *camera, void *_internal, PNCameraMode current_mode)
{
    struct internal *internal = _internal;

    // Check for new frame
    if (current_mode == ACQUIRING && frame_available(internal))
    {
        void_ptr camera_frame;
        if (!pl_exp_get_oldest_frame(internal->handle, &camera_frame))
            fatal_pvcam_error("Error retrieving oldest frame.");

        // PVCAM triggers end the frame, and so the first frame
        // will consist of the sync and align time period.
        // Discard this frame.
        if (internal->first_frame)
        {
            pn_log("Discarding pre-exposure readout.");
            internal->first_frame = false;
        }
        else
        {
            // Copy frame data and pass ownership to main thread
            CameraFrame *frame = malloc(sizeof(CameraFrame));
            if (frame)
            {
                size_t frame_bytes = internal->frame_width*internal->frame_height*sizeof(uint16_t);
                frame->data = malloc(frame_bytes);
                if (frame->data)
                {
                    memcpy(frame->data, camera_frame, frame_bytes);
                    frame->width = internal->frame_width;
                    frame->height = internal->frame_height;
                    frame->temperature = camera_pvcam_read_temperature(camera, _internal);
                    queue_framedata(frame);
                }
                else
                    pn_log("Failed to allocate CameraFrame->data. Discarding frame.");
            }
            else
                pn_log("Failed to allocate CameraFrame. Discarding frame.");
        }

        // Unlock the frame buffer for reuse
        if (!pl_exp_unlock_oldest_frame(internal->handle))
            fatal_pvcam_error("Failed to unlock oldest frame.");
    }
}
