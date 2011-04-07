/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <ftdi.h>
#include "common.h"
#include "gps.h"

void check_ftdi(const char *message, char* file, int line, int status)
{
	if (status < 0)
		pn_die("%s line %d : %s, status = %d\n", file, line, message, status);
}


void check_gps(PNGPS *gps, char *file, int line)
{
	if (gps == NULL)
		pn_die("gps is null @ %s:%d\n", file, line);
}

/* Initialise a gps object with a valid usb device */
PNGPS pn_gps_new()
{
	struct ftdi_device_list* devices = NULL;
	const int vendorId = 0x0403;  /* The USB vendor identifier for the FTDI company */
	const int productId = 0x6001; /* USB product identifier for the FT232 device */

	/* Get the list of FTDI devices on the system */
	int numDevices = ftdi_usb_find_all(NULL, &devices, vendorId, productId);
	check_ftdi("ftdi_usb_find_all() returned an error code", __FILE__, __LINE__, numDevices);
	
	printf("Found %d FTDI device(s)\n", numDevices);

	/* Assume that the first device is the gps unit */
	if (numDevices == 0)
	{
  		ftdi_list_free(&devices);		
		pn_die("No GPS available (pass --simulate to use simulated hardware).\n");
	}

	PNGPS ret;
	ret.device = devices->dev;
	ret.context = NULL;
    ret.shutdown = FALSE;
  	ftdi_list_free(&devices);
	return ret;
}

void pn_gps_free(PNGPS *gps)
{
	check_gps(gps, __FILE__, __LINE__);
	pthread_mutex_destroy(&gps->commLock);
}

/* Open the usb gps device and prepare it for reading/writing */
void pn_gps_init(PNGPS *gps)
{
	pthread_mutex_init(&gps->commLock, NULL);
	pthread_mutex_lock(&gps->commLock);
	check_gps(gps, __FILE__, __LINE__);	
	printf("Opened FTDI device `%s`\n", gps->device->filename);

	if (gps->context != NULL)
		pn_die("device %s is already open @ %s:%d\n", gps->device->filename, __FILE__, __LINE__);

	gps->context = ftdi_new();
    if (gps->context == NULL)
        pn_die("ftdi_new failed");


	int status = ftdi_init(gps->context);
	check_ftdi(gps->context->error_str, __FILE__, __LINE__, status);

	// Prepare the device for use with libftdi library calls.
	status = ftdi_usb_open_dev(gps->context, gps->device);
	check_ftdi(gps->context->error_str, __FILE__, __LINE__, status);

	//ftdi_enable_bitbang(pContext, 0xFF);

	status = ftdi_set_baudrate(gps->context, 115200);
	check_ftdi(gps->context->error_str, __FILE__, __LINE__, status);

	status = ftdi_set_line_property(gps->context, BITS_8, STOP_BIT_1, NONE);
	check_ftdi(gps->context->error_str, __FILE__, __LINE__, status);

	status = ftdi_setflowctrl(gps->context, SIO_DISABLE_FLOW_CTRL);
	check_ftdi(gps->context->error_str, __FILE__, __LINE__, status);

	unsigned char latency = 1; // the latency in milliseconds before partially full bit buffers are sent.
	status = ftdi_set_latency_timer(gps->context, latency);
	check_ftdi(gps->context->error_str, __FILE__, __LINE__, status);
	pthread_mutex_unlock(&gps->commLock);
}

/* Close the usb gps device */
void pn_gps_uninit(PNGPS *gps)
{
	pthread_mutex_lock(&gps->commLock);	
	check_gps(gps, __FILE__, __LINE__);
	printf("Closing device %s\n", gps->device->filename);
	if (gps->context == NULL)
		printf("device %s is already closed @ %s:%d\n", gps->device->filename, __FILE__, __LINE__);

	int status = ftdi_usb_close(gps->context);
	check_ftdi("ftdi_usb_close() returned an error code", __FILE__, __LINE__, status);

	ftdi_deinit(gps->context);
	ftdi_free(gps->context);
	gps->context = NULL;
	pthread_mutex_unlock(&gps->commLock);
}

/* Send a command to the gps device
 * Returns false if the write failed */
bool pn_gps_send_command(PNGPS *gps, PNGPSRequest type)
{
	check_gps(gps, __FILE__, __LINE__);
	unsigned char send[5] = {DLE, type, 0, DLE, ETX};
	return (ftdi_write_data(gps->context, send, 5) == 5);
}

/* Send a command to the gps device with a data payload
 * Returns false if the write failed */
bool pn_gps_send_command_with_data(PNGPS *gps, PNGPSRequest type, const char data[], int numBytes)
{
/*
	check_gps(gps, __FILE__, __LINE__);
	
	int length = 0;
	unsigned char send[GPS_PACKET_LENGTH];
	send[length++] = DLE;
	send[length++] = type;
	send[length++] = 0;
	for (int i = 0; i < numBytes; i++)
	{
		send[length++] = data[i];
		/ * Add a padding byte if necessary * /
		if (data[i] == DLE)
			send[length++] = DLE;
	}
	send[length++] = DLE;
	send[length++] = ETX;

	// Flush the read buffer
	unsigned char recvbuf[GPS_PACKET_LENGTH];
	while(ftdi_read_data(gps->context, recvbuf, GPS_PACKET_LENGTH));

	// Send the request
	return (ftdi_write_data(gps->context, send, length) == length);
*/
    return TRUE;
}

/* Read the gps response to a command
 * Give up after a timeout of <timeout> ms.
 * Returns the number of bytes read */
PNGPSResponse pn_gps_read(PNGPS *gps, int timeoutms)
{
	PNGPSResponse response;
	response.type = 0;
	response.error = REQUEST_TIMEOUT;
	response.datalength = 0;
	response.data[0] = (int)NULL;	
    return response;
/*
	check_gps(gps, __FILE__, __LINE__);

	PNGPSResponse response;
	response.type = 0;
	response.error = REQUEST_TIMEOUT;
	response.datalength = 0;
	response.data[0] = (int)NULL;		
	unsigned char recvbuf[GPS_PACKET_LENGTH];
	unsigned char totalbuf[GPS_PACKET_LENGTH];
	int recievedBytes = 0;
	int parsedBytes = 0;

	struct timeval curtime;
	gettimeofday(&curtime, NULL);

	/ * Calculate timeout time * /
	long int tus = curtime.tv_usec + 1000*timeoutms;
	struct timeval timeout;
	timeout.tv_sec = curtime.tv_sec + tus / 1000000;
	timeout.tv_usec = curtime.tv_usec + tus % 1000000;

	/ * Poll for data until we reach the end of the packet, or timeout * /
	while (curtime.tv_sec < timeout.tv_sec ||
		(curtime.tv_sec == timeout.tv_sec && curtime.tv_usec < timeout.tv_usec))
	{
		int ret = ftdi_read_data(gps->context, recvbuf, GPS_PACKET_LENGTH);
		if (ret < 0)
			pn_die("Bad response from gps. return code 0x%x",ret);

		for (int i = 0; i < ret && recievedBytes < GPS_PACKET_LENGTH; i++)
			totalbuf[recievedBytes++] = recvbuf[i];

		/ * Parse the packet header * /
		if (parsedBytes == 0 && recievedBytes > 2)
		{
			if (totalbuf[0] != DLE)
			{
				totalbuf[recievedBytes] = 0;
				pn_die("Malformed packed: Expected 0x%02x, got 0x%02x.\nData: `%s`", DLE, totalbuf[0], totalbuf);
			}
			response.type = totalbuf[1];
			response.error = totalbuf[2];
			parsedBytes += 3;
		}

		/ * Need at least 2 bytes available to parse the payload
		 * so that we can strip padding bytes * /
		while (recievedBytes - parsedBytes >= 2)
		{
			/ * Reached the end of the packet * /
			if (totalbuf[parsedBytes] == DLE && totalbuf[parsedBytes + 1] == ETX)
				return response;

			/ * Found a padding byte - strip it from the payload * /			
			if (totalbuf[parsedBytes] == DLE && totalbuf[parsedBytes + 1] == DLE)
				parsedBytes++;
			
			/ * Add byte to data bucket and shift terminator * /
			response.data[response.datalength++] = totalbuf[parsedBytes++];
			response.data[response.datalength] = 0;		
		}
		gettimeofday(&curtime, NULL);
	}

	printf("gps request timed out after %dms. Have data `%s`\n", timeoutms, response.data);
	return response;
    */
}

/* Ping the gps device
 * Returns false on error after printing the error to stderr */
bool ranaghau_gps_ping_device(PNGPS *gps)
{
	/*
    check_gps(gps, __FILE__, __LINE__);

	pthread_mutex_lock(&gps->commLock);
	pn_gps_send_command(gps, ECHO);
	PNGPSResponse response = pn_gps_read(gps, 20);
	pthread_mutex_unlock(&gps->commLock);

	if (!(response.type == ECHO && response.error == NO_ERROR))
	{
		fprintf(stderr, "ping failed (error 0x%02x)\n", response.error);
		return false;
	}
    */
	return true;
}

/* Query the last gps pulse time
 * Returns false on error after printing the error to stderr */
bool pn_gps_get_gpstime(PNGPS *gps, int timeoutMillis, PNGPSTimestamp *timestamp)
{
    /*	
    check_gps(gps, __FILE__, __LINE__);
	
	PNGPSResponse response;
	PNGPSTimestamp ret;
	do
	{
		pthread_mutex_lock(&gps->commLock);
		pn_gps_send_command(gps, GETGPSTIME);
		response = pn_gps_read(gps, timeoutMillis);
		pthread_mutex_unlock(&gps->commLock);

		if (response.type == GETGPSTIME && (response.error == NO_ERROR || response.error == GPS_TIME_NOT_LOCKED))
		{
            ret.locked = (response.error == NO_ERROR);
			if (sscanf((const char *)response.data, "%d:%d:%d:%d:%d:%d:%d", &ret.year, &ret.month, &ret.day, &ret.hours, &ret.minutes, &ret.seconds, &ret.milliseconds) == 7)
			{
				*timestamp = ret;
				return true;
			}

			fprintf(stderr, "Malformed time string: `%s`\n",(char *)response.data);
		}
	} while (response.error & UTC_ACCESS_ON_UPDATE);

	if (response.error)
		fprintf(stderr, "gettime failed (error 0x%02x)\n", response.error);

	return false;
    */
    PNGPSTimestamp ret;
	sscanf("1234:12:12:12:34:56:000", "%d:%d:%d:%d:%d:%d:%d", &ret.year, &ret.month, &ret.day, &ret.hours, &ret.minutes, &ret.seconds, &ret.milliseconds);
    *timestamp = ret;
    return true;
}

/* Query the last sync pulse time
 * outbuf is assumed to be of the correct length (>= 25)
 * Returns false on error after printing the error to stderr */
bool pn_gps_get_synctime(PNGPS *gps, int timeoutMillis, PNGPSTimestamp *timestamp)
{
    /*	
    check_gps(gps, __FILE__, __LINE__);
	
	PNGPSResponse response;
	PNGPSTimestamp ret;
	do
	{
		pthread_mutex_lock(&gps->commLock);
		pn_gps_send_command(gps, GETSYNCTIME);
		response = pn_gps_read(gps, timeoutMillis);
		pthread_mutex_unlock(&gps->commLock);
		if (response.type == GETSYNCTIME && (response.error == NO_ERROR || response.error == GPS_TIME_NOT_LOCKED))
		{
			ret.locked = (response.error == NO_ERROR);
			if (sscanf((const char *)response.data, "%d:%d:%d:%d:%d:%d:%d", &ret.year, &ret.month, &ret.day, &ret.hours, &ret.minutes, &ret.seconds, &ret.milliseconds) == 7)
			{
				*timestamp = ret;
				return true;
			}
            
			fprintf(stderr, "Malformed time string: `%s`\n",(char *)response.data);
		}
	} while (response.error & EOF_ACCESS_ON_UPDATE);

	if (response.error)
		fprintf(stderr, "synctime failed (error 0x%02x)\n", response.error);

	return false;
    */
    PNGPSTimestamp ret;
	sscanf("1234:12:12:12:34:56:000", "%d:%d:%d:%d:%d:%d:%d", &ret.year, &ret.month, &ret.day, &ret.hours, &ret.minutes, &ret.seconds, &ret.milliseconds);
    *timestamp = ret;
    return true;
}

int _temp_exptime = 0;
/* Query the current exposure time
 * Returns false on error after printing the error to stderr */
bool pn_gps_get_exposetime(PNGPS *gps, int *outbuf)
{
/*
	check_gps(gps, __FILE__, __LINE__);

	pthread_mutex_lock(&gps->commLock);
	pn_gps_send_command(gps, GETEXPOSURETIME);
	PNGPSResponse response = pn_gps_read(gps, 1000);
	pthread_mutex_unlock(&gps->commLock);

	if (!(response.type == GETEXPOSURETIME && response.error == NO_ERROR))
	{
		fprintf(stderr,"exposetime failed (error 0x%02x)\n", response.error);
		return false;
	}

	/ * Parse the response * /
	if (sscanf((char *)response.data, "%d", outbuf) != 1)
	{
		fprintf(stderr,"Error parsing exposetime: `%s`\n", response.data);
		return false;
	}
*/
    *outbuf = _temp_exptime;
	return true;
}

/* Set the exposure time
 * Returns false on error after printing the error to stderr */
bool pn_gps_set_exposetime(PNGPS *gps, int exptime)
{
/*	
    check_gps(gps, __FILE__, __LINE__);
	if (exptime < 0 || exptime > 9999)
	{
		fprintf(stderr, "Exposure time `%d` is not between 0 and 9999\n", exptime);
		return false;
	}

	/ * gps expects exptime to be a 4 character ascii string * /
	char buf[5];
	sprintf(buf, "%04d",exptime);

	pthread_mutex_lock(&gps->commLock);
	pn_gps_send_command_with_data(gps, SETEXPOSURETIME, buf, 4);
	PNGPSResponse response = pn_gps_read(gps, 1000);
	pthread_mutex_unlock(&gps->commLock);

	if (!(response.type == SETEXPOSURETIME && response.error == NO_ERROR))
	{
		fprintf(stderr,"setexposetime failed (error 0x%02x)\n", response.error);
		return false;
	}
*/
    _temp_exptime = exptime;
	return true;
}

extern time_t timegm(struct tm *);

// Subtract a number of seconds from a given timestamp
void pn_timestamp_subtract_seconds(PNGPSTimestamp *ts, int seconds)
{
	/* Make use of the fact that mktime/timegm() normalizes time values to do
	 * the conversion for us while handling rollover cases */
	struct tm a = {ts->seconds - seconds, ts->minutes, ts->hours, ts->day, ts->month, ts->year - 1900,0,0,0};
	time_t b = timegm(&a);
	gmtime_r(&b, &a);
	ts->seconds = a.tm_sec;
	ts->minutes = a.tm_min;
	ts->hours = a.tm_hour;
	ts->day = a.tm_mday;
	ts->month = a.tm_mon;
	ts->year = a.tm_year + 1900;
}

static unsigned char checksum(unsigned char *data, unsigned char length)
{
    unsigned char csm = data[0];
    for (unsigned char i = 1; i < length; i++)
        csm ^= data[i];
    return csm;
}

void *pn_gps_thread(void *_gps)
{
    PNGPS *gps = (PNGPS *)_gps;
	if (gps == NULL)
		pn_die("gps is null @ %s:%d\n", __FILE__, __LINE__);

    // Store recieved bytes in a 256 byte circular buffer indexed
    // by unsigned char. This ensures the correct circular behavior on over/underflow
	unsigned char recvbuf[256];
	unsigned char totalbuf[256];
	unsigned char writeIndex = 0;
	unsigned char readIndex = 0;
    rs_bool synced = FALSE;

    unsigned char gps_packet[256];
    unsigned char gps_packet_length = 0;

	/* Initialise the gps */
	if (!gps->shutdown)
        pn_gps_init(gps);
	
    // TODO: Loop while !shutdown parsing data
	struct timespec wait = {0,1e8};
    while (!gps->shutdown)
	{
		nanosleep(&wait, NULL);

	    int ret = ftdi_read_data(gps->context, recvbuf, 256);
	    if (ret < 0)
		    pn_die("Bad response from gps. return code 0x%x",ret);
        
        // Copy recieved bytes into the buffer
        for (int i = 0; i < ret; i++)
            totalbuf[writeIndex++] = recvbuf[i];

        // Sync to the start of a data frame
        for (; !synced && readIndex != writeIndex; readIndex++)
        {
            if (totalbuf[readIndex] != DLE &&
                totalbuf[readIndex-1] == DLE &&
                totalbuf[readIndex-2] == ETX &&
                totalbuf[readIndex-3] == DLE)
            {
                synced = TRUE;
                readIndex -= 1;
                break;
            }
        }

        // Haven't synced - wait for more data
        if (!synced)
            continue;

        
        // Copy data from storage buffer into parsing buffer and strip padding
        for (; readIndex != writeIndex; readIndex++)
        {
            // Strip DLE padding byte while being careful
            // about repeated 'real' DLE's in the data
            if (totalbuf[readIndex] == DLE && totalbuf[readIndex-1] == DLE)
                readIndex++;

            gps_packet[gps_packet_length++] = totalbuf[readIndex];

            // Format:
            // <DLE> <data length> <type> <data 0> ... <data length-1> <checksum> <DLE> <ETX>

            rs_bool reset = FALSE;
            // Reached the end of the packet
            if (gps_packet_length > 2 && gps_packet_length == gps_packet[1] + 6)
            {
                // Validate frame checksum
                if (checksum(&gps_packet[3], gps_packet[1]) == gps_packet[gps_packet_length - 3])
                {
                    // Parse data packet
                    switch(gps_packet[2])
                    {
                        case CURRENTTIME:
                            printf("Time: %04d-%02d-%02d %02d:%02d:%02d (%03d:%d)\n", (gps_packet[8] & 0x00FF) | ((gps_packet[9] << 8) & 0xFF00), // Year
                                                                      gps_packet[7],   // Month
                                                                      gps_packet[6],   // Day
                                                                      gps_packet[3],   // Hour
                                                                      gps_packet[4],   // Minute
                                                                      gps_packet[5],   // Second
                                                                      gps_packet[11],  // Exptime remaining
                                                                      gps_packet[10]); // Locked
                        break;
                        case DOWNLOADTIME:
                            printf("Download: %04d-%02d-%02d %02d:%02d:%02d (%d)\n", (gps_packet[8] & 0x00FF) | ((gps_packet[9] << 8) & 0xFF00), // Year
                                                                      gps_packet[7],   // Month
                                                                      gps_packet[6],   // Day
                                                                      gps_packet[3],   // Hour
                                                                      gps_packet[4],   // Minute
                                                                      gps_packet[5],   // Second
                                                                      gps_packet[10]); // Locked
                        break;
                        case DEBUG:

                        break;
                        case EXPOSURE:

                        break;
                    }
                }

                // Reset for next frame
                reset = TRUE;
            }

            // Something went wrong
            if (gps_packet_length >= 255)
                reset = TRUE;
    
            if (reset)
            {
                synced = FALSE;
                gps_packet_length = 0;
                break;
            }
        }
    }

	/* Close the gps */	
	pn_gps_uninit(gps);
	pn_gps_free(gps);

	pthread_exit(NULL);
}
