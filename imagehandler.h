/*
 * Copyright 2010, 2011, 2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <stdint.h>
#include "gps.h"

#ifndef IMAGEHANDLER_H
#define IMAGEHANDLER_H

// Represents an aquired frame
typedef struct
{
    uint16_t width;
    uint16_t height;
    uint16_t *data; // Pointer to the start of the frame data
} PNFrame;

void pn_save_frame(PNFrame *frame, PNGPSTimestamp timestamp);
void pn_preview_frame(PNFrame *frame, PNGPSTimestamp timestamp);
void launch_ds9();

#endif
