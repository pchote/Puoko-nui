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
    TimerUnit *timer;
    uint16_t frame_width;
    uint16_t frame_height;
};

static char *speed_names[] = {"Slow", "Fast"};
static char *gain_names[] = {"S Low", "S Medium", "S High", "F Low", "F Medium", "F High"};

void *camera_simulated_initialize(Camera *camera, ThreadCreationArgs *args)
{
    struct internal *internal = calloc(1, sizeof(struct internal));
    if (!internal)
        return NULL;

    internal->timer = args->timer;

    return internal;
}

double camera_simulated_update_camera_settings(Camera *camera, void *_internal)
{
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

    // Set readout area
    uint16_t ww = pn_preference_int(CAMERA_WINDOW_WIDTH);
    if (ww < 1 || ww > 512)
    {
        pn_log("Invalid window width: %d. Reset to %d.", ww, 512);
        ww = 512;
        pn_preference_set_int(CAMERA_WINDOW_WIDTH, ww);
    }

    uint16_t wh = pn_preference_int(CAMERA_WINDOW_HEIGHT);
    if (wh < 1 || wh > 512)
    {
        pn_log("Invalid window height: %d. Reset to %d.", wh, 512);
        wh = 512;
        pn_preference_set_int(CAMERA_WINDOW_HEIGHT, wh);
    }

    uint16_t wx = pn_preference_int(CAMERA_WINDOW_X);
    if (wx + ww > 512)
    {
        pn_log("Invalid window x: %d. Reset to %d.", wx, 0);
        wx = 0;
        pn_preference_set_int(CAMERA_WINDOW_X, wx);
    }

    uint16_t wy = pn_preference_int(CAMERA_WINDOW_Y);
    if (wy + wh > 512)
    {
        pn_log("Invalid window y: %d. Reset to %d.", wy, 0);
        wy = 0;
        pn_preference_set_int(CAMERA_WINDOW_Y, wy);
    }

    uint8_t bin = pn_preference_char(CAMERA_BINNING);
    if (bin == 0 || bin > ww || bin > wh)
    {
        pn_log("Invalid binning: %d. Reset to %d.", bin, 1);
        bin = 1;
        pn_preference_set_char(CAMERA_BINNING, bin);
    }

    return 0;
}

uint8_t camera_simulated_port_table(Camera *camera, void *internal, struct camera_port_option **ports)
{
    struct camera_port_option *port = calloc(1, sizeof(struct camera_port_option));
    if (!port)
        trigger_fatal_error(strdup("Failed to allocate readout port options"));

    port->name = strdup("Normal");
    port->speed_count = 2;
    port->speed = calloc(port->speed_count, sizeof(struct camera_speed_option));
    if (!port->speed)
        trigger_fatal_error(strdup("Failed to allocate readout speed options"));

    for (uint8_t i = 0; i < 2; i++)
    {
        struct camera_speed_option *s = &port->speed[i];
        s->name = strdup(speed_names[i]);
        s->gain_count = 3;
        s->gain = calloc(s->gain_count, sizeof(struct camera_gain_option));
        if (!s->gain)
            trigger_fatal_error(strdup("Failed to allocate readout gain options"));

        for (uint8_t j = 0; j < s->gain_count; j++)
        {
            struct camera_gain_option *g = &s->gain[j];
            g->name = strdup(gain_names[i*s->gain_count + j]);
        }
    }

    *ports = port;
    return 1;
}

void camera_simulated_query_ccd_region(Camera *camera, void *internal, uint16_t region[4])
{
     region[0] = 0;
     region[1] = 511;
     region[2] = 0;
     region[3] = 511;
}

void camera_simulated_uninitialize(Camera *camera, void *internal)
{
    free(internal);
}

void camera_simulated_start_acquiring(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;

    // TODO: Take preferences for binning into account
    internal->frame_height = 512;
    internal->frame_width = 512;

    // Wait a bit to simulate hardware delays
    millisleep(2000);
}

void camera_simulated_stop_acquiring(Camera *camera, void *_internal)
{
    // Wait a bit to simulate hardware delays
    millisleep(1000);
}

double camera_simulated_read_temperature(Camera *camera, void *internal)
{
    return 0;
}

void camera_simulated_tick(Camera *camera, void *_internal, PNCameraMode current_mode, double current_temperature)
{
    struct internal *internal = _internal;

    if (current_mode == ACQUIRING && timer_mode(internal->timer) == TIMER_READOUT)
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
                    frame->data[i] = rand();

                // Add orientation squares to top corners of frame
                for (size_t j = 20; j < 30; j++)
                    for (size_t i = 20; i < 30; i++)
                    {
                        frame->data[(internal->frame_height - j)*internal->frame_width + i] = 0;
                        frame->data[(internal->frame_height - j)*internal->frame_width +
                                    internal->frame_width - i] = 65535;
                    }
                frame->width = internal->frame_width;
                frame->height = internal->frame_height;
                frame->temperature = current_temperature;
                queue_framedata(frame);
            }
            else
                pn_log("Failed to allocate CameraFrame->data. Discarding frame.");
        }
        else
            pn_log("Failed to allocate CameraFrame. Discarding frame.");

        // There is no physical camera for the timer to monitor
        // so we must toggle this manually
        timer_set_simulated_camera_downloading(internal->timer, false);
    }
}