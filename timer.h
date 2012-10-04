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
#include <pthread.h>
#include "main.h"

typedef struct TimerUnit TimerUnit;

TimerUnit *timer_new(bool simulate_hardware);
void timer_free(TimerUnit *timer);
void timer_spawn_thread(TimerUnit *timer, ThreadCreationArgs *args);

void timer_start_exposure(TimerUnit *timer, unsigned char exptime);
void timer_stop_exposure(TimerUnit *timer);
bool timer_camera_downloading(TimerUnit *timer);
TimerTimestamp timer_current_timestamp(TimerUnit *timer);
void timer_shutdown(TimerUnit *timer);
void timer_set_simulated_camera_downloading(TimerUnit *timer, bool downloading);

void timestamp_normalize(TimerTimestamp *ts);
time_t timestamp_to_time_t(TimerTimestamp *ts);

#endif
