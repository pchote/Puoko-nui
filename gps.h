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
} PNGPSTimestamp;

typedef struct PNGPS PNGPS;

PNGPS *pn_gps_new();
void pn_gps_free(PNGPS *gps);
void *pn_timer_thread(void *gps);
void *pn_simulated_timer_thread(void *gps);

void pn_gps_start_exposure(PNGPS *gps, unsigned char exptime);
void pn_gps_stop_exposure(PNGPS *gps);
bool pn_gps_camera_downloading(PNGPS *gps);
PNGPSTimestamp pn_gps_current_timestamp(PNGPS *gps);
void pn_gps_request_shutdown(PNGPS *gps);
void pn_gps_set_simulated_camera_downloading(PNGPS *gps, bool downloading);

PNGPSTimestamp pn_timestamp_normalize(PNGPSTimestamp ts);

#endif
