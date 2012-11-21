/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef CAMERA_PICAM_H
#define CAMERA_PICAM_H

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include "main.h"
#include "camera.h"

void *camera_picam_initialize(Camera *camera, ThreadCreationArgs *args);
double camera_picam_update_camera_settings(Camera *camera, void *internal);
uint8_t camera_picam_port_table(Camera *camera, void *internal, struct camera_port_option **ports);
void camera_picam_uninitialize(Camera *camera, void *internal);
void camera_picam_start_acquiring(Camera *camera, void *internal);
void camera_picam_stop_acquiring(Camera *camera, void *internal);
void camera_picam_tick(Camera *camera, void *internal, PNCameraMode current_mode, double current_temperature);
double camera_picam_read_temperature(Camera *camera, void *internal);
void camera_picam_query_ccd_region(Camera *camera, void *internal, uint16_t region[4]);
bool camera_picam_supports_readout_display(Camera *camera, void *internal);

#endif
