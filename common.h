/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <stdarg.h>
#include "platform.h"
#include "camera.h"
#include "gps.h"

#ifndef COMMON_H
#define COMMON_H

void pn_log(const char * format, ...);
void queue_framedata(PNFrame *frame);
void queue_trigger_timestamp(PNGPSTimestamp timestamp);
void trigger_fatal_error(char *message);
#endif

