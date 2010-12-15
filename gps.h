/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#ifndef GPS_H
#define GPS_H

/* datalink escape byte */
#define DLE 0x10
/* End of text byte */
#define ETX 0x03

typedef enum
{
	ECHO = 0x01,
	GETGPSTIME = 0x23,
	GETEXPOSURETIME = 0x24,
	SETEXPOSURETIME = 0x44,
	GETSYNCTIME = 0x25,
} RangahauGPSRequest;

typedef enum
{
	NO_ERROR = 1<<0,
	PACKET_ID_INVALID = 1<<1,
	UTC_ACCESS_ON_UPDATE = 1<<2,
	EOF_ACCESS_ON_UPDATE = 1<<3,
	PACKET_8FAB_LENGTH_WRONG = 1<<4,
	GPS_TIME_NOT_LOCKED = 1<<5,
	GPS_SERIAL_LOST = 1<<7
} RangahauGPSError;

typedef struct
{
	RangahauGPSRequest type;
	unsigned char data[1024];
	int datalength;	
	unsigned char error;
} RangahauGPSResponse;

typedef struct
{
	struct usb_device *device;
	struct ftdi_context *context;
} RangahauGPS;

RangahauGPS rangahau_gps_new();
void rangahau_gps_free(RangahauGPS *gps);
void rangahau_gps_init(RangahauGPS *gps);
void rangahau_gps_uninit(RangahauGPS *gps);

void ranaghau_gps_ping_device(RangahauGPS *gps);
#endif
