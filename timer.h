/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <time.h>
#include <stdbool.h>
#include <pthread.h>

#ifndef GPS_H
#define GPS_H

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

typedef struct TimerUnit TimerUnit;

TimerUnit *timer_new();
void timer_free(TimerUnit *timer);
void *pn_timer_thread(void *timer);
void *pn_simulated_timer_thread(void *timer);

void timer_start_exposure(TimerUnit *timer, unsigned char exptime);
void timer_stop_exposure(TimerUnit *timer);
bool timer_camera_downloading(TimerUnit *timer);
TimerTimestamp timer_current_timestamp(TimerUnit *timer);
void timer_request_shutdown(TimerUnit *timer);
void timer_set_simulated_camera_downloading(TimerUnit *timer, bool downloading);

TimerTimestamp pn_timestamp_normalize(TimerTimestamp ts);

#endif
