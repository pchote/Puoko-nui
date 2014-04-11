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
#include "timer.h"
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

    // First postscan and bias column in CCD pixels
    uint16_t postscan_start;
    uint16_t bias_start;

    // Image and bias regions in window pixels
    bool has_image_region;
    bool has_bias_region;
    uint16_t image_region[4];
    uint16_t bias_region[4];

    // String descriptions to store in frame headers
    char *current_port_desc;
    char *current_speed_desc;
    char *current_gain_desc;

    double readout_time;
    double vertical_shift_us;
};

static char *gain_names[] = {"Low", "Medium", "High"};

static void log_pvcam_error()
{
    int error = pl_error_code();
    char pvmsg[ERROR_MSG_LEN];
    pvmsg[0] = '\0';
    pl_error_message(error, pvmsg);

    pn_log("PVCAM error: %d = %s.", error, pvmsg);
}

static int frame_available(struct internal *internal, bool *available)
{
    int16 status = READOUT_NOT_ACTIVE;
    uns32 bytesStored = 0, numFilledBuffers = 0;
    if (!pl_exp_check_cont_status(internal->handle, &status, &bytesStored, &numFilledBuffers))
    {
        pn_log("Failed to query camera status.");
        log_pvcam_error();
        return CAMERA_ERROR;
    }

    *available = (status == FRAME_AVAILABLE);
    return CAMERA_OK;
}

#define get_param(label, handle, param, attrib, value) do \
{ \
    int ret = _get_param(handle, param, #param, attrib, #attrib, (void*)value); \
    if (ret != CAMERA_OK) \
        goto label; \
} while(0)

static int _get_param(int16 handle, uns32 param, char *param_name, int16 attrib, char *attrib_name, void_ptr value)
{
    if (!pl_get_param(handle, param, attrib, value))
    {
        pn_log("Failed to query %s %s", param_name, attrib_name);
        log_pvcam_error();
        return CAMERA_ERROR;
    }
    return CAMERA_OK;
}

#define set_param(label, handle, param, value) do \
{ \
    int ret = _set_param(handle, param, #param, (void*)value); \
    if (ret != CAMERA_OK) \
        goto label; \
} while(0)

static int _set_param(int16 handle, uns32 param, char *param_name, void_ptr value)
{
    if (!pl_set_param(handle, param, value))
    {
        pn_log("Failed to set %s", param_name);
        log_pvcam_error();
        return CAMERA_ERROR;
    }
    return CAMERA_OK;
}

static int port_description(struct internal *internal, char **out_desc)
{
    char str[100];
    int32 value;
    uns32 port = 0;
    get_param(error, internal->handle, PARAM_READOUT_PORT, ATTR_CURRENT, &port);

    // PVCAM returns port = 1 for the Puoko-nui MicroMax camera, but port = 0
    // is the only valid choice for get_enum_parameter...
    // We assume that this off-by-one is also true for cameras with multiple
    // readout ports.
    port -= 1;

    if (!pl_get_enum_param(internal->handle, PARAM_READOUT_PORT, port, &value, str, 100))
        goto error;

    *out_desc = strdup(str);
    return CAMERA_OK;
error:
    pn_log("Failed to query port name for port %d", port);
    log_pvcam_error();
    return CAMERA_ERROR;
}

static int speed_description(struct internal *internal, char **out_desc)
{
    char str[100];
    int16 pix_time;

    get_param(error, internal->handle, PARAM_PIX_TIME, ATTR_CURRENT, &pix_time);
    snprintf(str, 100, "%0.1f MHz", 1.0e3/pix_time);
    *out_desc = strdup(str);

    return CAMERA_OK;
error:
    pn_log("Failed to query PARAM_PIX_TIME");
    log_pvcam_error();
    return CAMERA_ERROR;
}

static int gain_description(struct internal *internal, char **out_desc)
{
    int16 min, max, cur;
    get_param(error, internal->handle, PARAM_GAIN_INDEX, ATTR_MIN, &min);
    get_param(error, internal->handle, PARAM_GAIN_INDEX, ATTR_MAX, &max);
    get_param(error, internal->handle, PARAM_GAIN_INDEX, ATTR_CURRENT, &cur);

    *out_desc = (max - min == 2) ? strdup(gain_names[cur - min]) : strdup("Unknown");
    return CAMERA_OK;
error:
    pn_log("Failed to query gain parameters");
    log_pvcam_error();
    return CAMERA_ERROR;
}

static int initialize_camera(struct internal *internal)
{
    int16 numCams = 0;
    if (!pl_cam_get_total(&numCams))
    {
        pn_log("Failed to query cameras.");
        log_pvcam_error();
        return CAMERA_ERROR;
    }

    if (numCams == 0)
    {
        pn_log("No cameras found.");
        return CAMERA_ERROR;
    }

    // Get the camera name (assume that we only have one camera)
    char cameraName[CAM_NAME_LEN];
    if (!pl_cam_get_name(0, cameraName))
    {
        pn_log("Failed to query camera name.");
        log_pvcam_error();
        return CAMERA_ERROR;
    }

    // Open the camera
    if (!pl_cam_open(cameraName, &internal->handle, OPEN_EXCLUSIVE))
    {
        pn_log("Failed to open camera. Are permissions correct?");
        log_pvcam_error();
        return CAMERA_ERROR;
    }

    pn_log("Camera ID: \"%s\".", cameraName);

    // Check camera status
    if (!pl_cam_get_diags(internal->handle))
    {
        pn_log("Camera failed diagnostic checks.");
        log_pvcam_error();
        return CAMERA_ERROR;
    }

    set_param(error, internal->handle, PARAM_TEMP_SETPOINT, &(int){pn_preference_int(CAMERA_TEMPERATURE)});
    set_param(error, internal->handle, PARAM_SHTR_CLOSE_DELAY, &(uns16){0});
    set_param(error, internal->handle, PARAM_LOGIC_OUTPUT, &(int){OUTPUT_NOT_SCAN});
    set_param(error, internal->handle, PARAM_EDGE_TRIGGER, &(int){EDGE_TRIG_NEG});
    get_param(error, internal->handle, PARAM_SER_SIZE, ATTR_DEFAULT, &internal->ccd_width);
    get_param(error, internal->handle, PARAM_PAR_SIZE, ATTR_DEFAULT, &internal->ccd_height);

    uint8_t overscan = pn_preference_char(CAMERA_OVERSCAN_COLS);
    internal->postscan_start = internal->ccd_width;
    internal->bias_start = internal->postscan_start;

    if (overscan > 0)
    {
        // Enable custom chip so we can add a bias strip
        set_param(error, internal->handle, PARAM_CUSTOM_CHIP, &(rs_bool){true});

        // Query the number of masked pixels that are adjacent to the exposure region
        // PVCAM seems to have mislabeled *SCAN and *MASK
        uint16_t postscan;
        get_param(error, internal->handle, PARAM_POSTMASK, ATTR_DEFAULT, &postscan);
        set_param(error, internal->handle, PARAM_POSTMASK, &(uns16){0});
        internal->bias_start += postscan;

        // Increase the frame width to allow readout of the postscan pixels
        // and the requested number of overscan columns
        internal->ccd_width += postscan + overscan;
        set_param(error, internal->handle, PARAM_SER_SIZE, &(uns16){internal->ccd_width});
    }

    // Init exposure control libs
    if (!pl_exp_init_seq())
    {
        pn_log("Failed to initialize exposure sequence.");
        log_pvcam_error();
        return CAMERA_ERROR;
    }

    return CAMERA_OK;
error:
    return CAMERA_ERROR;
}

static void uninitialize_camera(struct internal *internal)
{
    if (!pl_exp_uninit_seq())
        pn_log("Failed to uninitialize exposure sequence.");

    if (!pl_cam_close(internal->handle))
        pn_log("Failed to close camera.");
}

int camera_pvcam_initialize(Camera *camera, void **out_internal)
{
    struct internal *internal = calloc(1, sizeof(struct internal));
    if (!internal)
        return CAMERA_ALLOCATION_FAILED;

    if (!pl_pvcam_init())
    {
        pn_log("Failed to initialize PVCAM.");
        log_pvcam_error();
        return CAMERA_ERROR;
    }

    uns16 pversion;
    if (!pl_pvcam_get_ver(&pversion))
    {
        pn_log("Failed to query PVCAM version.");
        log_pvcam_error();
        return CAMERA_ERROR;
    }

    pn_log("PVCAM Version %d.%d.%d.", pversion>>8, (pversion & 0x00F0)>>4, pversion & 0x000F, pversion);

    int ret = initialize_camera(internal);
    if (ret != CAMERA_OK)
        return ret;

    // Query vertical shift rate
    int16 vertical_shift_ns;
    get_param(error, internal->handle, PARAM_PAR_SHIFT_TIME, ATTR_CURRENT, &vertical_shift_ns);
    internal->vertical_shift_us = vertical_shift_ns / 1000.0f;

    *out_internal = internal;
    return CAMERA_OK;

error:
    return CAMERA_ERROR;
}

int camera_pvcam_update_camera_settings(Camera *camera, void *_internal, double *out_readout_time)
{
    struct internal *internal = _internal;

    // Validate requested settings
    uns32 port_count;
    uint8_t port_id = pn_preference_char(CAMERA_READPORT_MODE);
    get_param(error, internal->handle, PARAM_READOUT_PORT, ATTR_COUNT, &port_count);

    if (port_id >= port_count)
    {
        pn_log("Invalid port index: %d. Reset to %d.", port_id, 0);
        pn_preference_set_char(CAMERA_READPORT_MODE, 0);
        port_id = 0;
    }

    if (port_count > 1)
        set_param(error, internal->handle, PARAM_READOUT_PORT, &port_id);

    uint8_t speed_id = pn_preference_char(CAMERA_READSPEED_MODE);
    int16 speed_min, speed_max;
    get_param(error, internal->handle, PARAM_SPDTAB_INDEX, ATTR_MIN, &speed_min);
    get_param(error, internal->handle, PARAM_SPDTAB_INDEX, ATTR_MAX, &speed_max);

    int16 speed_value = speed_id + speed_min;
    if (speed_value > speed_max)
    {
        pn_log("Invalid speed index: %d. Reset to %d.", speed_id, 0);
        pn_preference_set_char(CAMERA_READSPEED_MODE, 0);
        speed_value = speed_min;
    }
    set_param(error, internal->handle, PARAM_SPDTAB_INDEX, &speed_value);

    uint8_t gain_id = pn_preference_char(CAMERA_GAIN_MODE);
    int16 gain_min, gain_max;
    get_param(error, internal->handle, PARAM_GAIN_INDEX, ATTR_MIN, &gain_min);
    get_param(error, internal->handle, PARAM_GAIN_INDEX, ATTR_MAX, &gain_max);

    int16 gain_value = gain_id + gain_min;
    if (gain_value > gain_max)
    {
        pn_log("Invalid gain index: %d. Reset to %d.", gain_id, 0);
        pn_preference_set_char(CAMERA_GAIN_MODE, 0);
        gain_value = gain_min;
    }

    set_param(error, internal->handle, PARAM_GAIN_INDEX, &gain_value);

    // Store port/speed/gain descriptions to store in frames
    free(internal->current_port_desc);
    if (port_description(internal, &internal->current_port_desc) != CAMERA_OK)
        goto error;

    free(internal->current_speed_desc);
    if (speed_description(internal, &internal->current_speed_desc) != CAMERA_OK)
        goto error;

    free(internal->current_gain_desc);
    if (gain_description(internal, &internal->current_gain_desc) != CAMERA_OK)
        goto error;

    set_param(error, internal->handle, PARAM_TEMP_SETPOINT, &(int){pn_preference_int(CAMERA_TEMPERATURE)});

    // Set readout area
    uint16_t ww = pn_preference_int(CAMERA_WINDOW_WIDTH);
    if (ww < 1 || ww > internal->ccd_width)
    {
        pn_log("Invalid window width: %d. Reset to %d.", ww, internal->ccd_width);
        ww = internal->ccd_width;
        pn_preference_set_int(CAMERA_WINDOW_WIDTH, ww);
    }

    uint16_t wh = pn_preference_int(CAMERA_WINDOW_HEIGHT);
    if (wh < 1 || wh > internal->ccd_height)
    {
        pn_log("Invalid window height: %d. Reset to %d.", wh, internal->ccd_height);
        wh = internal->ccd_height;
        pn_preference_set_int(CAMERA_WINDOW_HEIGHT, wh);
    }

    uint8_t bin = pn_preference_char(CAMERA_BINNING);
    if (bin < 1 || bin > ww || bin > wh)
    {
        pn_log("Invalid binning: %d. Reset to %d.", bin, 1);
        bin = 1;
        pn_preference_set_char(CAMERA_BINNING, bin);
    }

    uint16_t wx = pn_preference_int(CAMERA_WINDOW_X);
    if (wx + ww > internal->ccd_width)
    {
        pn_log("Invalid window x: %d. Reset to %d.", wx, 0);
        wx = 0;
        pn_preference_set_int(CAMERA_WINDOW_X, wx);
    }

    uint16_t wy = pn_preference_int(CAMERA_WINDOW_Y);
    if (wy + wh > internal->ccd_height)
    {
        pn_log("Invalid window y: %d. Reset to %d.", wy, 0);
        wy = 0;
        pn_preference_set_int(CAMERA_WINDOW_Y, wy);
    }

    rgn_type region;
    region.s1 = wx;
    region.s2 = wx + ww - 1;
    region.sbin = bin;
    region.p1 = wy;
    region.p2 = wy + wh - 1;
    region.pbin = bin;

    internal->frame_width = ww / bin;
    internal->frame_height = wh / bin;

    // Record image and bias regions (in window coordinates)
    // for inclusion in the FITS header
    internal->has_image_region = false;
    internal->has_bias_region = false;

    if (region.s2 >= internal->postscan_start)
    {
        internal->image_region[0] = 0;
        internal->image_region[1] = (internal->postscan_start - region.s1) / bin;
        internal->image_region[2] = 0;
        internal->image_region[3] = wh / bin;
        internal->has_image_region = true;
    }

    if (region.s2 >= internal->bias_start)
    {
        internal->bias_region[0] = (internal->bias_start - region.s1) / bin;
        internal->bias_region[1] = ww / bin;
        internal->bias_region[2] = 0;
        internal->bias_region[3] = wh / bin;
        internal->has_bias_region = true;
    }

    // Set exposure mode: expose entire chip, expose on sync pulses (exposure time unused), overwrite buffer
    // If bias mode is requested then ignore triggers and acquire as fast as possible

    uint8_t trigger_mode = pn_preference_char(TIMER_TRIGGER_MODE);
    if (trigger_mode != TRIGGER_BIAS)
        set_param(error, internal->handle, PARAM_FORCE_READOUT_MODE, &(int){MAKE_FRAME_TRANSFER});

    int16 pv_trigger_mode = trigger_mode == TRIGGER_BIAS ? TIMED_MODE : STROBED_MODE;
    if (!pl_exp_setup_cont(internal->handle, 1, &region, pv_trigger_mode, 0, &internal->frame_size, CIRC_NO_OVERWRITE))
    {
        pn_log("Failed to setup exposure sequence.");
        log_pvcam_error();
        return CAMERA_ERROR;
    }

    // Query readout time
    flt64 readout_time;
    get_param(error, internal->handle, PARAM_READOUT_TIME, ATTR_CURRENT, &readout_time);
    internal->readout_time = readout_time / 1000;

    double exposure_time = pn_preference_int(EXPOSURE_TIME);

    // Convert readout time from to the base exposure unit (s or ms) for comparison
    if (trigger_mode == TRIGGER_SECONDS)
        readout_time /= 1000;

    if (exposure_time < readout_time)
    {
        uint16_t new_exposure = (uint16_t)(ceil(readout_time));
        pn_preference_set_int(EXPOSURE_TIME, new_exposure);
        pn_log("Increasing EXPOSURE_TIME to %d.", new_exposure);
    }

    *out_readout_time = internal->readout_time;

    return CAMERA_OK;
error:
    return CAMERA_ERROR;
}

int camera_pvcam_port_table(Camera *camera, void *_internal, struct camera_port_option **out_ports, uint8_t *out_port_count)
{
    struct internal *internal = _internal;
    uns32 port_count;
    get_param(error, internal->handle, PARAM_READOUT_PORT, ATTR_COUNT, &port_count);

    struct camera_port_option *ports = calloc(port_count, sizeof(struct camera_port_option));
    if (!ports)
        return CAMERA_ALLOCATION_FAILED;

    for (uint8_t i = 0; i < port_count; i++)
    {
        struct camera_port_option *port = &ports[i];

        if (port_description(internal, &port->name) != CAMERA_OK)
            return CAMERA_ERROR;

        // Set the active port then query readout speeds
        if (port_count > 1)
            set_param(error, internal->handle, PARAM_READOUT_PORT, &i);

        int16 speed_min, speed_max;
        get_param(error, internal->handle, PARAM_SPDTAB_INDEX, ATTR_MIN, &speed_min);
        get_param(error, internal->handle, PARAM_SPDTAB_INDEX, ATTR_MAX, &speed_max);

        port->speed_count = speed_max - speed_min + 1;
        port->speed = calloc(port->speed_count, sizeof(struct camera_speed_option));
        if (!port->speed)
            return CAMERA_ALLOCATION_FAILED;

        for (uint8_t j = 0; j <= speed_max - speed_min; j++)
        {
            struct camera_speed_option *speed = &port->speed[j];
            set_param(error, internal->handle, PARAM_SPDTAB_INDEX, &(int16){speed_min + j});

            int16 gain_min, gain_max;
            get_param(error, internal->handle, PARAM_GAIN_INDEX, ATTR_MIN, &gain_min);
            get_param(error, internal->handle, PARAM_GAIN_INDEX, ATTR_MAX, &gain_max);

            if (speed_description(internal, &speed->name) != CAMERA_OK)
                return CAMERA_ERROR;

            speed->gain_count = gain_max - gain_min + 1;
            speed->gain = calloc(speed->gain_count, sizeof(struct camera_gain_option));
            if (!speed->gain)
                return CAMERA_ALLOCATION_FAILED;

            for (uint8_t k = 0; k <= gain_max - gain_min; k++)
            {
                set_param(error, internal->handle, PARAM_GAIN_INDEX, &(int16){gain_min + k});
                if (gain_description(internal, &speed->gain[k].name) != CAMERA_OK)
                    return CAMERA_ERROR;
            }
        }
    }

    *out_ports = ports;
    *out_port_count = port_count;
    return CAMERA_OK;
error:
    return CAMERA_ERROR;
}

int camera_pvcam_uninitialize(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;
    uninitialize_camera(internal);

    if (!pl_pvcam_uninit())
        pn_log("Failed to uninitialize camera.");

    free(internal);
    return CAMERA_OK;
}

int camera_pvcam_start_acquiring(Camera *camera, void *_internal, bool shutter_open)
{
    struct internal *internal = _internal;

    // Reinitialize camera from scratch to work around a driver bug/PVCAM that causes
    // several stale frames to be returned before the real frame data resulting in
    // frames that are recieved several exposure periods after their actual time
    pn_log("Reinitializing camera.");
    uninitialize_camera(internal);
    initialize_camera(internal);
    double readout_time;
    camera_pvcam_update_camera_settings(camera, internal, &readout_time);

    // Create a buffer large enough to hold multiple frames. PVCAM and the USB driver
    // tend to give frames in batches for very fast exposures, which need a bigger buffer.
    uns32 buffer_size = internal->frame_size*pn_preference_int(CAMERA_FRAME_BUFFER_SIZE);
    internal->frame_buffer = malloc(buffer_size*sizeof(uns16));
    if (!internal->frame_buffer)
        return CAMERA_ALLOCATION_FAILED;

    // Start waiting for sync pulses to trigger exposures
    if (!pl_exp_start_cont(internal->handle, internal->frame_buffer, buffer_size))
    {
        pn_log("Failed to start exposure sequence.");
        log_pvcam_error();
        return CAMERA_ERROR;
    }

    return CAMERA_OK;
}

int camera_pvcam_stop_acquiring(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;
    int ret = CAMERA_OK;

    // Clear any buffered frames
    void_ptr camera_frame;
    bool available;
    do
    {
        ret = frame_available(internal, &available);
        if (ret != CAMERA_OK)
            break;

        if (available)
        {
            pl_exp_get_oldest_frame(internal->handle, &camera_frame);
            pl_exp_unlock_oldest_frame(internal->handle);
            pn_log("Discarding buffered frame.");
        }
    } while (available);

    if (!pl_exp_stop_cont(internal->handle, CCS_HALT))
    {
        pn_log("Failed to stop exposure sequence.");
        log_pvcam_error();
        ret = CAMERA_ERROR;
    }

    if (!pl_exp_finish_seq(internal->handle, internal->frame_buffer, 0))
    {
        pn_log("Failed to finish exposure sequence.");
        log_pvcam_error();
        ret = CAMERA_ERROR;
    }

    free(internal->frame_buffer);
    return ret;
}

int camera_pvcam_read_temperature(Camera *camera, void *_internal, double *out_temperature)
{
    struct internal *internal = _internal;

    int16 temp;
    get_param(error, internal->handle, PARAM_TEMP, ATTR_CURRENT, &temp);
    *out_temperature = temp/100.0;

    return CAMERA_OK;
error:
    return CAMERA_ERROR;
}

int camera_pvcam_tick(Camera *camera, void *_internal, PNCameraMode current_mode)
{
    struct internal *internal = _internal;

    // Check for new frame
    while (current_mode == ACQUIRING)
    {
        bool available = false;
        int status = frame_available(internal, &available);
        if (status != CAMERA_OK || !available)
            return status;

        void_ptr camera_frame;
        if (!pl_exp_get_oldest_frame(internal->handle, &camera_frame))
        {
            pn_log("Error retrieving oldest frame.");
            log_pvcam_error();
            return CAMERA_ERROR;
        }

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
                camera_pvcam_read_temperature(camera, internal, &frame->temperature);
                frame->has_timestamp = false;

                frame->has_image_region = internal->has_image_region;
                if (frame->has_image_region)
                    memcpy(frame->image_region, internal->image_region, 4*sizeof(uint16_t));

                frame->has_bias_region = internal->has_bias_region;
                if (frame->has_bias_region)
                    memcpy(frame->bias_region, internal->bias_region, 4*sizeof(uint16_t));

                frame->readout_time = internal->readout_time;
                frame->vertical_shift_us = internal->vertical_shift_us;

                frame->port_desc = strdup(internal->current_port_desc);
                frame->speed_desc = strdup(internal->current_speed_desc);
                frame->gain_desc = strdup(internal->current_gain_desc);
                frame->has_em_gain = false;
                frame->has_exposure_shortcut = false;

                queue_framedata(frame);
            }
            else
                pn_log("Failed to allocate CameraFrame->data. Discarding frame.");
        }
        else
            pn_log("Failed to allocate CameraFrame. Discarding frame.");

        // Unlock the frame buffer for reuse
        if (!pl_exp_unlock_oldest_frame(internal->handle))
        {
            pn_log("Failed to unlock oldest frame.");
            log_pvcam_error();
            return CAMERA_ERROR;
        }
    }

    return CAMERA_OK;
}

int camera_pvcam_query_ccd_region(Camera *camera, void *_internal, uint16_t region[4])
{
    struct internal *internal = _internal;
    region[0] = 0;
    region[1] = internal->ccd_width - 1;
    region[2] = 0;
    region[3] = internal->ccd_height - 1;
    return CAMERA_OK;
}

bool camera_pvcam_supports_readout_display(Camera *camera, void *internal)
{
    return true;
}

bool camera_pvcam_supports_shutter_disabling(Camera *camera, void *internal)
{
    return false;
}

bool camera_pvcam_supports_bias_acquisition(Camera *camera, void *internal)
{
    return true;
}

void camera_pvcam_normalize_trigger(Camera *camera, void *internal, TimerTimestamp *trigger)
{
    // Convert trigger time from end of exposure to start of exposure
    uint16_t exposure = pn_preference_int(EXPOSURE_TIME);
    if (pn_preference_char(TIMER_TRIGGER_MODE) != TRIGGER_SECONDS)
    {
        trigger->seconds -= exposure / 1000;
        trigger->milliseconds -= exposure % 1000;
    }
    else
        trigger->seconds -= exposure;

    timestamp_normalize(trigger);
}
