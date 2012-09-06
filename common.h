/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#ifndef COMMON_H
#define COMMON_H

#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

// Represents an aquired frame
typedef struct
{
    uint16_t width;
    uint16_t height;
    uint16_t *data; // Pointer to the start of the frame data
} PNFrame;

// Represents a timestamp from the GPS
typedef struct
{
    int year;
    int month;
    int day;
    int hours;
    int minutes;
    int seconds;
    bool locked;
    int remaining_exposure; // for current time
    bool valid; // true before initialisation and if the download time has been used
} TimerTimestamp;

void pn_log(const char * format, ...);
void queue_framedata(PNFrame *frame);
void queue_trigger_timestamp(TimerTimestamp timestamp);
void trigger_fatal_error(char *message);
#endif

