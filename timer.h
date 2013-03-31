/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef GPS_H
#define GPS_H

#include <time.h>
#include <stdbool.h>
#include "main.h"

typedef enum
{
    TIMER_IDLE,
    TIMER_WAITING,
    TIMER_ALIGN,
    TIMER_EXPOSING,
    TIMER_READOUT
} TimerMode;

typedef enum
{
    GPS_UNAVAILABLE = 0,
    GPS_SYNCING     = 1,
    GPS_ACTIVE      = 2
} TimerGPSStatus;

typedef struct TimerUnit TimerUnit;

TimerUnit *timer_new(bool simulate_hardware);
void timer_free(TimerUnit *timer);
void timer_spawn_thread(TimerUnit *timer, const Modules *modules);
void timer_join_thread(TimerUnit *timer);
void timer_notify_shutdown(TimerUnit *timer);
bool timer_thread_alive(TimerUnit *timer);

void timer_start_exposure(TimerUnit *timer, uint16_t exptime, bool use_monitor);
void timer_stop_exposure(TimerUnit *timer);
TimerMode timer_mode(TimerUnit *timer);
TimerTimestamp timer_current_timestamp(TimerUnit *timer);
TimerGPSStatus timer_gps_status(TimerUnit *timer);

void timestamp_normalize(TimerTimestamp *ts);
double timestamp_to_unixtime(TimerTimestamp *ts);

#endif
