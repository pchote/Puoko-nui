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
    struct Camera * const camera;
    struct TimerUnit * const timer;
    struct FrameManager *const frame;
    struct PreviewScript * const preview;
    struct ReductionScript * const reduction;
} Modules;

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
    int32_t exposure_progress; // for current time
} TimerTimestamp;

// Represents an aquired frame
typedef struct
{
    uint16_t width;
    uint16_t height;
    uint16_t *data;
    double temperature;
    TimerTimestamp downloaded_time;

    bool has_timestamp;
    double timestamp;
    double readout_time;
    double vertical_shift_us;

    bool has_image_region;
    bool has_bias_region;
    uint16_t image_region[4];
    uint16_t bias_region[4];

    char *port_desc;
    char *speed_desc;
    char *gain_desc;

    bool has_em_gain;
    double em_gain;

    bool has_exposure_shortcut;
    uint16_t exposure_shortcut_ms;
} CameraFrame;

void pn_log(const char * format, ...);
void queue_framedata(CameraFrame *frame);
void queue_trigger(TimerTimestamp *timestamp);
void clear_queued_data(bool reset_first);
#endif
