/*
 * Copyright 2010, 2011, 2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <stdint.h>
#include "timer.h"

#ifndef IMAGEHANDLER_H
#define IMAGEHANDLER_H

// Represents an aquired frame
typedef struct
{
    uint16_t width;
    uint16_t height;
    uint16_t *data; // Pointer to the start of the frame data
} PNFrame;

void pn_run_startup_script();
void pn_run_preview_script(const char *filepath);
void pn_run_saved_script(const char *filepath);

const char *pn_save_frame(PNFrame *frame, TimerTimestamp timestamp);
void pn_save_preview(PNFrame *frame, TimerTimestamp timestamp);

#endif
