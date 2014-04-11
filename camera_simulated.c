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

#include "camera_simulated.h"
#include "main.h"
#include "camera.h"
#include "timer.h"
#include "preferences.h"
#include "platform.h"

// Holds the state of a camera
struct internal
{
    uint16_t frame_width;
    uint16_t frame_height;

    // Number of queued frames to generate
    bool acquiring;
    size_t queued_frames;
    pthread_mutex_t queue_mutex;
    TimerTimestamp bias_last_updated;

    // String descriptions to store in frame headers
    char *current_port_desc;
    char *current_speed_desc;
    char *current_gain_desc;
};

static char *speed_names[] = {"Slow", "Fast"};
static char *gain_names[] = {"S Low", "S Medium", "S High", "F Low", "F Medium", "F High"};

int camera_simulated_initialize(Camera *camera, void **out_internal)
{
    struct internal *internal = calloc(1, sizeof(struct internal));
    if (!internal)
        return CAMERA_ALLOCATION_FAILED;

    pthread_mutex_init(&internal->queue_mutex, NULL);

    internal->frame_height = 512;
    internal->frame_width = 512;

    *out_internal = internal;
    return CAMERA_OK;
}

int camera_simulated_update_camera_settings(Camera *camera, void *_internal, double *out_readout_time)
{
    struct internal *internal = _internal;

    uint8_t port_id = pn_preference_char(CAMERA_READPORT_MODE);
    if (port_id > 0)
    {
        pn_log("Invalid port index: %d. Reset to %d.", port_id, 0);
        pn_preference_set_char(CAMERA_READPORT_MODE, 0);
    }

    uint8_t speed_id = pn_preference_char(CAMERA_READSPEED_MODE);
    if (speed_id > 1)
    {
        pn_log("Invalid speed index: %d. Reset to %d.", speed_id, 0);
        pn_preference_set_char(CAMERA_READSPEED_MODE, 0);
    }

    uint8_t gain_id = pn_preference_char(CAMERA_GAIN_MODE);
    if (gain_id > 2)
    {
        pn_log("Invalid gain index: %d. Reset to %d.", gain_id, 0);
        pn_preference_set_char(CAMERA_GAIN_MODE, 0);
    }

    internal->current_port_desc = "Normal";
    internal->current_speed_desc = speed_names[speed_id];
    internal->current_gain_desc = gain_names[speed_id*3 + gain_id];

    // Set readout area
    uint16_t ww = pn_preference_int(CAMERA_WINDOW_WIDTH);
    if (ww < 1 || ww > internal->frame_width)
    {
        pn_log("Invalid window width: %d. Reset to %d.", ww, internal->frame_width);
        ww = internal->frame_width;
        pn_preference_set_int(CAMERA_WINDOW_WIDTH, ww);
    }

    uint16_t wh = pn_preference_int(CAMERA_WINDOW_HEIGHT);
    if (wh < 1 || wh > internal->frame_height)
    {
        pn_log("Invalid window height: %d. Reset to %d.", wh, internal->frame_height);
        wh = internal->frame_height;
        pn_preference_set_int(CAMERA_WINDOW_HEIGHT, wh);
    }

    uint16_t wx = pn_preference_int(CAMERA_WINDOW_X);
    if (wx + ww > internal->frame_width)
    {
        pn_log("Invalid window x: %d. Reset to %d.", wx, 0);
        wx = 0;
        pn_preference_set_int(CAMERA_WINDOW_X, wx);
    }

    uint16_t wy = pn_preference_int(CAMERA_WINDOW_Y);
    if (wy + wh > internal->frame_height)
    {
        pn_log("Invalid window y: %d. Reset to %d.", wy, 0);
        wy = 0;
        pn_preference_set_int(CAMERA_WINDOW_Y, wy);
    }

    uint8_t bin = pn_preference_char(CAMERA_BINNING);
    if (bin == 0 || bin > internal->frame_width || bin > internal->frame_height)
    {
        pn_log("Invalid binning: %d. Reset to %d.", bin, 1);
        bin = 1;
        pn_preference_set_char(CAMERA_BINNING, bin);
    }

    *out_readout_time = 0;
    return CAMERA_OK;
}

int camera_simulated_port_table(Camera *camera, void *internal, struct camera_port_option **out_ports, uint8_t *out_port_count)
{
    struct camera_port_option *port = calloc(1, sizeof(struct camera_port_option));
    if (!port)
        return CAMERA_ALLOCATION_FAILED;

    port->name = strdup("Normal");
    port->speed_count = 2;
    port->speed = calloc(port->speed_count, sizeof(struct camera_speed_option));
    if (!port->speed)
        return CAMERA_ALLOCATION_FAILED;

    for (uint8_t i = 0; i < 2; i++)
    {
        struct camera_speed_option *s = &port->speed[i];
        s->name = strdup(speed_names[i]);
        s->gain_count = 3;
        s->gain = calloc(s->gain_count, sizeof(struct camera_gain_option));
        if (!s->gain)
            return CAMERA_ALLOCATION_FAILED;

        for (uint8_t j = 0; j < s->gain_count; j++)
        {
            struct camera_gain_option *g = &s->gain[j];
            g->name = strdup(gain_names[i*s->gain_count + j]);
        }
    }

    *out_ports = port;
    *out_port_count = 1;
    return CAMERA_OK;
}

int camera_simulated_query_ccd_region(Camera *camera, void *internal, uint16_t region[4])
{
    region[0] = 0;
    region[1] = 511;
    region[2] = 0;
    region[3] = 511;
    return CAMERA_OK;
}

int camera_simulated_uninitialize(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;
    pthread_mutex_destroy(&internal->queue_mutex);
    free(internal);
    return CAMERA_OK;
}

int camera_simulated_start_acquiring(Camera *camera, void *_internal, bool shutter_open)
{
    struct internal *internal = _internal;

    // Wait a bit to simulate hardware delays
    millisleep(2000);
    pn_log("%s simulated shutter.", shutter_open ? "Opened" : "Closed");
    internal->acquiring = true;
    return CAMERA_OK;
}

int camera_simulated_stop_acquiring(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;

    // Wait a bit to simulate hardware delays
    millisleep(1000);
    pn_log("Closed simulated shutter.");
    internal->acquiring = false;
    return CAMERA_OK;
}

int camera_simulated_read_temperature(Camera *camera, void *internal, double *out_temperature)
{
    *out_temperature = 0;
    return CAMERA_OK;
}

int camera_simulated_tick(Camera *camera, void *_internal, PNCameraMode current_mode)
{
    struct internal *internal = _internal;

    pthread_mutex_lock(&internal->queue_mutex);
    size_t queued = internal->queued_frames;
    internal->queued_frames = 0;

    if (internal->acquiring && pn_preference_char(TIMER_TRIGGER_MODE) == TRIGGER_BIAS)
    {
        // Simulate a new bias every 100ms
        TimerTimestamp bias_updated = system_time();
        double dt = (timestamp_to_unixtime(&bias_updated) - timestamp_to_unixtime(&internal->bias_last_updated));
        if (dt >= 0.1)
        {
            queued++;
            internal->bias_last_updated = bias_updated;
        }
    }
    pthread_mutex_unlock(&internal->queue_mutex);

    for (size_t i = 0; i < queued; i++)
    {
        // Copy frame data and pass ownership to main thread
        CameraFrame *frame = malloc(sizeof(CameraFrame));
        if (frame)
        {
            size_t frame_bytes = internal->frame_width*internal->frame_height*sizeof(uint16_t);
            frame->data = malloc(frame_bytes);

            if (frame->data)
            {
                // Fill frame with random numbers
                for (size_t i = 0; i < internal->frame_width*internal->frame_height; i++)
                    frame->data[i] = rand() % 10000;

                // Add orientation squares to top corners of frame
                for (size_t j = 20; j < 30; j++)
                    for (size_t i = 20; i < 30; i++)
                    {
                        frame->data[(internal->frame_height - j)*internal->frame_width + i] = 0;
                        frame->data[(internal->frame_height - j)*internal->frame_width +
                                    internal->frame_width - i] = 65535;

                        frame->data[(internal->frame_height/2 - j + 25)*internal->frame_width +
                                    internal->frame_width/2 - i + 25] = 20000;
                    }
                frame->width = internal->frame_width;
                frame->height = internal->frame_height;
                camera_simulated_read_temperature(camera, internal, &frame->temperature);

                frame->readout_time = 0;
                frame->vertical_shift_us = 0;

                frame->has_timestamp = false;
                frame->has_image_region = false;
                frame->has_bias_region = false;

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
    }

    return CAMERA_OK;
}

bool camera_simulated_supports_readout_display(Camera *camera, void *internal)
{
    return false;
}

bool camera_simulated_supports_shutter_disabling(Camera *camera, void *internal)
{
    return true;
}

bool camera_simulated_supports_bias_acquisition(Camera *camera, void *internal)
{
    return true;
}

void camera_simulated_normalize_trigger(Camera *camera, void *internal, TimerTimestamp *trigger)
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

void camera_simulated_trigger_frame(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;

    pthread_mutex_lock(&internal->queue_mutex);
    internal->queued_frames++;
    pthread_mutex_unlock(&internal->queue_mutex);
}
