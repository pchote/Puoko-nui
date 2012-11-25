/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef FRAME_H
#define FRAME_H

#include "main.h"

void frame_process_transforms(CameraFrame *frame);
bool frame_save(CameraFrame *frame, TimerTimestamp timestamp, char *filepath);

#endif