/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    struct Camera *camera;
    struct TimerUnit *timer;
} ThreadCreationArgs;

// Represents a timestamp from the GPS
// Signed ints to allow subtracting times without hidden gotchas
typedef struct
{
    int32_t year;
    int32_t month;
    int32_t day;
    int32_t hours;
    int32_t minutes;
    int32_t seconds;
    int32_t milliseconds;
    bool locked;
    int32_t remaining_exposure; // for current time
} TimerTimestamp;

// Represents an aquired frame
typedef struct
{
    uint16_t width;
    uint16_t height;
    uint16_t *data;
    double temperature;
    TimerTimestamp downloaded_time;
} CameraFrame;

void pn_log(const char * format, ...);
void queue_framedata(CameraFrame *frame);
void queue_trigger(TimerTimestamp *timestamp);
void clear_queued_data();

void trigger_fatal_error(char *message);
#endif

