/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef CAMERA_PICAM_H
#define CAMERA_PICAM_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"
#include "camera.h"

int camera_picam_initialize(Camera *camera, void **internal);
int camera_picam_update_camera_settings(Camera *camera, void *internal, double *readout_time);
int camera_picam_port_table(Camera *camera, void *internal, struct camera_port_option **ports, uint8_t *port_count);
int camera_picam_uninitialize(Camera *camera, void *internal);
int camera_picam_start_acquiring(Camera *camera, void *internal, bool shutter_open);
int camera_picam_stop_acquiring(Camera *camera, void *internal);
int camera_picam_tick(Camera *camera, void *internal, PNCameraMode current_mode);
int camera_picam_read_temperature(Camera *camera, void *internal, double *temperature);
int camera_picam_query_ccd_region(Camera *camera, void *internal, uint16_t region[4]);

bool camera_picam_supports_readout_display(Camera *camera, void *internal);
bool camera_picam_supports_shutter_disabling(Camera *camera, void *internal);
bool camera_picam_supports_bias_acquisition(Camera *camera, void *internal);
void camera_picam_normalize_trigger(Camera *camera, void *internal, TimerTimestamp *trigger);

#endif
