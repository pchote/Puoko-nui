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

extern void simulate_camera_download();

void check_ftdi(const char *message, char* file, int line, int status)
{
	if (status < 0)
		pn_die("FATAL: %s line %d : %s, status = %d\n", file, line, message, status);
}


void check_gps(PNGPS *gps, char *file, int line)
{
	if (gps == NULL)
		pn_die("FATAL: gps is null @ %s:%d\n", file, line);
}

/* Initialise a gps object with a valid usb device */
PNGPS pn_gps_new(rs_bool simulate)
{
	PNGPS ret;
	ret.context = NULL;
    ret.shutdown = FALSE;
    ret.current_timestamp.valid = FALSE;
    ret.current_timestamp.valid = FALSE;
    ret.send_length = 0;
    ret.simulated = FALSE;

    if (simulate)
    {
        ret.simulated = TRUE;
        ret.simulated_remaining = ret.simulated_exptime = 0;
        ret.simulated_unixtime = time(NULL);
        return ret;
    }

	struct ftdi_device_list* devices = NULL;
	const int vendorId = 0x0403;  /* The USB vendor identifier for the FTDI company */
	const int productId = 0x6001; /* USB product identifier for the FT232 device */

	/* Get the list of FTDI devices on the system */
	int numDevices = ftdi_usb_find_all(NULL, &devices, vendorId, productId);
	check_ftdi("ftdi_usb_find_all() returned an error code", __FILE__, __LINE__, numDevices);
	
	pn_log("Found %d FTDI device(s)", numDevices);

	/* Assume that the first device is the gps unit */
	if (numDevices == 0)
	{
  		ftdi_list_free(&devices);
		pn_die("FATAL: GPS unit unavailable");
	}

	ret.device = devices->dev;
  	ftdi_list_free(&devices);
	return ret;
}

void pn_gps_free(PNGPS *gps)
{
	check_gps(gps, __FILE__, __LINE__);
    pthread_mutex_destroy(&gps->currenttime_mutex);
    pthread_mutex_destroy(&gps->downloadtime_mutex);
    pthread_mutex_destroy(&gps->sendbuffer_mutex);
}

/* Open the usb gps device and prepare it for reading/writing */
void pn_gps_init(PNGPS *gps)
{
	check_gps(gps, __FILE__, __LINE__);	
	pthread_mutex_init(&gps->currenttime_mutex, NULL);
    pthread_mutex_init(&gps->downloadtime_mutex, NULL);
    pthread_mutex_init(&gps->sendbuffer_mutex, NULL);

    if (gps->simulated)
    {
        pn_log("Simulating GPS");
        return;
    }
	pn_log("Opened FTDI device `%s`", gps->device->filename);

	if (gps->context != NULL)
		pn_die("FATAL: device %s is already open @ %s:%d", gps->device->filename, __FILE__, __LINE__);

	gps->context = ftdi_new();
    if (gps->context == NULL)
        pn_die("FATAL: ftdi_new failed");


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
}

/* Close the usb gps device */
void pn_gps_uninit(PNGPS *gps)
{
	check_gps(gps, __FILE__, __LINE__);
    if (gps->simulated)
    {
        pn_log("Closing simulated GPS");
        return;
    }

	pn_log("Closing device %s", gps->device->filename);
	if (gps->context == NULL)
		pn_log("device %s is already closed @ %s:%d", gps->device->filename, __FILE__, __LINE__);

	int status = ftdi_usb_close(gps->context);
	check_ftdi("ftdi_usb_close() returned an error code", __FILE__, __LINE__, status);

	ftdi_deinit(gps->context);
	ftdi_free(gps->context);
	gps->context = NULL;
}

static unsigned char checksum(unsigned char *data, unsigned char length)
{
    unsigned char csm = data[0];
    for (unsigned char i = 1; i < length; i++)
        csm ^= data[i];
    return csm;
}

static void queue_send_byte(PNGPS *gps, unsigned char b)
{
    // Hard-loop until the send buffer empties
    // Should never happen in normal operation
    while (gps->send_length >= 255);

    pthread_mutex_lock(&gps->sendbuffer_mutex);
    gps->send_buffer[gps->send_length++] = b;
    pthread_mutex_unlock(&gps->sendbuffer_mutex);
}

static void queue_data(PNGPS *gps, unsigned char type, unsigned char *data, unsigned char length)
{
    queue_send_byte(gps, DLE);
    queue_send_byte(gps, length);
    queue_send_byte(gps, type);
    for (unsigned char i = 0; i < length; i++)
    {
        queue_send_byte(gps, data[i]);
        if (data[i] == DLE)
            queue_send_byte(gps, DLE);
    }
    queue_send_byte(gps, checksum(data,length));
    queue_send_byte(gps, DLE);
    queue_send_byte(gps, ETX);
}


/* Set the exposure time */
void pn_gps_start_exposure(PNGPS *gps, unsigned char exptime)
{
    pn_log("Starting exposure @ %ds",exptime);
    if (gps->simulated)
        gps->simulated_remaining = gps->simulated_exptime = exptime;
    else
        queue_data(gps, START_EXPOSURE, &exptime, 1);
}

extern void shutdown_camera();

void pn_gps_stop_exposure(PNGPS *gps)
{
    pn_log("Stopping exposure");
    unsigned char unused = 0;
    if (gps->simulated)
    {
        shutdown_camera();
    }
    else
        queue_data(gps, STOP_EXPOSURE, &unused, 1);
}

void pn_gps_reset(PNGPS *gps)
{
    unsigned char unused = 0;
    if (!gps->simulated)
        queue_data(gps, RESET, &unused, 1);
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

void *pn_gps_thread(void *_gps)
{
    PNGPS *gps = (PNGPS *)_gps;
	if (gps == NULL)
		pn_die("FATAL: gps is null @ %s:%d\n", __FILE__, __LINE__);

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
    {
        pn_gps_init(gps);

        // Ping the gps twice - the first will be ignored and used for syncing
        pn_gps_reset(gps);
        pn_gps_reset(gps);
    }

    // Loop until shutdown, parsing incoming data
	struct timespec wait = {0,1e8};
    while (!gps->shutdown)
	{
		nanosleep(&wait, NULL);

        if (gps->simulated)
        {
            time_t curunixtime = time(NULL);
            if (curunixtime != gps->simulated_unixtime)
            {
                struct tm *t = gmtime(&curunixtime);
                if (gps->simulated_exptime > 0)
                    gps->simulated_remaining -= (int)(curunixtime - gps->simulated_unixtime);

                // Will behave badly if remaining < 0, but this should never happen
                if (gps->simulated_remaining <= 0 && gps->simulated_exptime > 0)
                {
                    gps->simulated_remaining = gps->simulated_exptime;
                    pthread_mutex_lock(&gps->downloadtime_mutex);
                    gps->download_timestamp.year = t->tm_year + 1900;
                    gps->download_timestamp.month = t->tm_mon;
                    gps->download_timestamp.day = t->tm_wday;
                    gps->download_timestamp.hours = t->tm_hour;
                    gps->download_timestamp.minutes = t->tm_min;
                    gps->download_timestamp.seconds = t->tm_sec;
                    gps->download_timestamp.locked = 1;
                    gps->download_timestamp.remaining_exposure = 0;
                    gps->download_timestamp.valid = TRUE;
                    pn_log("Simulated Download: %04d-%02d-%02d %02d:%02d:%02d (%d)",
                           gps->download_timestamp.year, // Year
                           gps->download_timestamp.month,   // Month
                           gps->download_timestamp.day,   // Day
                           gps->download_timestamp.hours,   // Hour
                           gps->download_timestamp.minutes,   // Minute
                           gps->download_timestamp.seconds,   // Second
                           gps->download_timestamp.locked); // Locked
                    pthread_mutex_unlock(&gps->downloadtime_mutex);

                    simulate_camera_download();
                }

                pthread_mutex_lock(&gps->currenttime_mutex);
                gps->current_timestamp.year = t->tm_year + 1900;
                gps->current_timestamp.month = t->tm_mon;
                gps->current_timestamp.day = t->tm_wday;
                gps->current_timestamp.hours = t->tm_hour;
                gps->current_timestamp.minutes = t->tm_min;
                gps->current_timestamp.seconds = t->tm_sec;
                gps->current_timestamp.locked = 1;
                gps->current_timestamp.remaining_exposure = gps->simulated_remaining;
                gps->current_timestamp.valid = TRUE;
                pthread_mutex_unlock(&gps->currenttime_mutex);

                gps->simulated_unixtime = curunixtime;
            }
            continue;
        }

        // Send any data in the send buffer
        pthread_mutex_lock(&gps->sendbuffer_mutex);
        if (gps->send_length > 0)
        {
	        if (ftdi_write_data(gps->context, gps->send_buffer, gps->send_length) != gps->send_length)
                pn_log("Error sending send buffer");
            gps->send_length = 0;
        }
        pthread_mutex_unlock(&gps->sendbuffer_mutex);
        
        // Grab any data accumulated by the usb driver
	    int ret = ftdi_read_data(gps->context, recvbuf, 256);
	    if (ret < 0)
		    pn_die("FATAL: Bad response from gps. return code 0x%x",ret);
        
        // Copy recieved bytes into the buffer
        for (int i = 0; i < ret; i++)
            totalbuf[writeIndex++] = recvbuf[i];

        // Sync to the start of a data frame
        for (; !synced && readIndex != writeIndex; readIndex++)
        {
            if (// Start of new packet
                totalbuf[readIndex] != DLE &&
                totalbuf[(unsigned char)(readIndex-1)] == DLE &&
                // End of previous packet
                totalbuf[(unsigned char)(readIndex-2)] == ETX &&
                totalbuf[(unsigned char)(readIndex-3)] == DLE)
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
	                        pthread_mutex_lock(&gps->currenttime_mutex);
                            gps->current_timestamp.year = (gps_packet[8] & 0x00FF) | ((gps_packet[9] << 8) & 0xFF00);
                            gps->current_timestamp.month = gps_packet[7];
                            gps->current_timestamp.day = gps_packet[6];
                            gps->current_timestamp.hours = gps_packet[3];
                            gps->current_timestamp.minutes = gps_packet[4];
                            gps->current_timestamp.seconds = gps_packet[5];
                            gps->current_timestamp.locked = gps_packet[10];
                            gps->current_timestamp.remaining_exposure = gps_packet[11];
                            gps->current_timestamp.valid = TRUE;

                            pthread_mutex_unlock(&gps->currenttime_mutex);
/*
                            pn_log("Time: %04d-%02d-%02d %02d:%02d:%02d (%03d:%d)", (gps_packet[8] & 0x00FF) | ((gps_packet[9] << 8) & 0xFF00), // Year
                                                                      gps_packet[7],   // Month
                                                                      gps_packet[6],   // Day
                                                                      gps_packet[3],   // Hour
                                                                      gps_packet[4],   // Minute
                                                                      gps_packet[5],   // Second
                                                                      gps_packet[11],  // Exptime remaining
                                                                      gps_packet[10]); // Locked
*/
                        break;
                        case DOWNLOADTIME:
                            pthread_mutex_lock(&gps->downloadtime_mutex);
                            gps->download_timestamp.year = (gps_packet[8] & 0x00FF) | ((gps_packet[9] << 8) & 0xFF00);
                            gps->download_timestamp.month = gps_packet[7];
                            gps->download_timestamp.day = gps_packet[6];
                            gps->download_timestamp.hours = gps_packet[3];
                            gps->download_timestamp.minutes = gps_packet[4];
                            gps->download_timestamp.seconds = gps_packet[5];
                            gps->download_timestamp.locked = gps_packet[10];
                            gps->download_timestamp.valid = TRUE;
                            pthread_mutex_unlock(&gps->downloadtime_mutex);
                            pn_log("Download: %04d-%02d-%02d %02d:%02d:%02d (%d)", (gps_packet[8] & 0x00FF) | ((gps_packet[9] << 8) & 0xFF00), // Year
                                                                      gps_packet[7],   // Month
                                                                      gps_packet[6],   // Day
                                                                      gps_packet[3],   // Hour
                                                                      gps_packet[4],   // Minute
                                                                      gps_packet[5],   // Second
                                                                      gps_packet[10]); // Locked

                            // Trigger a fake download if the camera is simulated
                            simulate_camera_download();
                        break;
                        case DEBUG_STRING:
                            gps_packet[gps_packet[1]+3] = '\0';
                            pn_log("GPS Debug: `%s`", &gps_packet[3]);
                        break;
                        case DEBUG_RAW:
                            pn_log("GPS Debug: ");
                            for (unsigned char i = 0; i < gps_packet[1]; i++)
                                pn_log("0x%02x ", gps_packet[3+i]);
                        break;
                        case STOP_EXPOSURE:
                            pn_log("Timer reports safe to shutdown camera");
                            shutdown_camera();
                        break;
                        default:
                            pn_log("Unknown packet");
                    }
                }

                // Reset for next frame
                reset = TRUE;
            }

            // Something went wrong
            if (gps_packet_length >= 255)
            {
                pn_log("Packet length overrun\n");
                reset = TRUE;
            }    
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
