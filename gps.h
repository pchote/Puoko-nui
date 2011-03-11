/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <stdbool.h>
#include <pthread.h>

#ifndef GPS_H
#define GPS_H

/* datalink escape byte */
#define DLE 0x10
/* End of text byte */
#define ETX 0x03

/* Maximum gps packet length */
#define GPS_PACKET_LENGTH 255

/* GPS command types */
typedef enum
{
	ECHO = 0x01,
	GETGPSTIME = 0x23,
	GETSYNCTIME = 0x25,
	GETEXPOSURETIME = 0x24,
	SETEXPOSURETIME = 0x44,
} PNGPSRequest;

/* GPS error types */
typedef enum
{
	REQUEST_TIMEOUT = 0,
	NO_ERROR = 1<<0,
	PACKET_ID_INVALID = 1<<1,
	UTC_ACCESS_ON_UPDATE = 1<<2,
	EOF_ACCESS_ON_UPDATE = 1<<3,
	PACKET_8FAB_LENGTH_WRONG = 1<<4,
	GPS_TIME_NOT_LOCKED = 1<<5,
	GPS_SERIAL_LOST = 1<<7
} PNGPSError;

/* Represents a response packet from the GPS */
typedef struct
{
	PNGPSRequest type;
	unsigned char data[GPS_PACKET_LENGTH];
	int datalength;	
	unsigned char error;
} PNGPSResponse;

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
    bool locked;
} PNGPSTimestamp;

/* Represents the GPS hardware */
typedef struct
{
	struct usb_device *device;
	struct ftdi_context *context;
	pthread_mutex_t commLock;
} PNGPS;

PNGPS pn_gps_new();
void pn_gps_free(PNGPS *gps);
void pn_gps_init(PNGPS *gps);
void pn_gps_uninit(PNGPS *gps);

bool pn_gps_send_command(PNGPS *gps, PNGPSRequest type);
PNGPSResponse pn_gps_read(PNGPS *gps, int timeoutms);

bool ranaghau_gps_ping_device(PNGPS *gps);
bool pn_gps_get_gpstime(PNGPS *gps, int timeoutMillis, PNGPSTimestamp *timestamp);
bool pn_gps_get_synctime(PNGPS *gps, int timeoutMillis, PNGPSTimestamp *timestamp);
bool pn_gps_get_exposetime(PNGPS *gps, int *outbuf);
bool pn_gps_set_exposetime(PNGPS *gps, int exptime);

void pn_timestamp_subtract_seconds(PNGPSTimestamp *ts, int seconds);
#endif
