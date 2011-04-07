/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <pthread.h>
#include <master.h>
#ifndef GPS_H
#define GPS_H

/* datalink escape byte */
#define DLE 0x10
/* End of text byte */
#define ETX 0x03


/* GPS command types */
typedef enum
{
	CURRENTTIME = 0xA1,
	DOWNLOADTIME = 0xA2,
	DEBUG = 0xA3,
	EXPOSURE = 0xA4,
} PNGPSRequest;

/* Represents a timestamp from the GPS */
typedef struct
{
	int year;
	int month;
	int day;
	int hours;
	int minutes;
	int seconds;
    int milliseconds;
    rs_bool locked;
    int remaining_exposure; // for current time
    rs_bool valid; // true before initialisation and if the download time has been used
} PNGPSTimestamp;

/* Represents the GPS hardware */
typedef struct
{
	struct usb_device *device;
	struct ftdi_context *context;
    rs_bool shutdown;
    PNGPSTimestamp current_timestamp;
    pthread_mutex_t currenttime_mutex;
    PNGPSTimestamp download_timestamp;
    pthread_mutex_t downloadtime_mutex;
} PNGPS;

PNGPS pn_gps_new();
void pn_gps_free(PNGPS *gps);
void pn_gps_init(PNGPS *gps);
void pn_gps_uninit(PNGPS *gps);

rs_bool pn_gps_get_exposetime(PNGPS *gps, int *outbuf);
rs_bool pn_gps_set_exposetime(PNGPS *gps, int exptime);

void pn_timestamp_subtract_seconds(PNGPSTimestamp *ts, int seconds);

void *pn_gps_thread(void *_gps);
#endif
