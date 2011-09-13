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
extern void shutdown_camera();
extern time_t timegm(struct tm *);

#pragma mark Creation and Destruction (Called from main thread)

// Initialize a new PNGPS struct.
PNGPS pn_gps_new()
{
	PNGPS ret;
	ret.context = NULL;
    ret.shutdown = FALSE;
    ret.current_timestamp.valid = FALSE;
    ret.current_timestamp.locked = FALSE;
    ret.download_timestamp.valid = FALSE;
    ret.download_timestamp.locked = FALSE;
    ret.send_length = 0;
    ret.simulated = FALSE;
    ret.camera_downloading = 0;
    ret.fatal_error = NULL;

	pthread_mutex_init(&ret.read_mutex, NULL);
    pthread_mutex_init(&ret.sendbuffer_mutex, NULL);

	return ret;
}

// Destroy a PNCamera struct.
void pn_gps_free(PNGPS *gps)
{
    if (gps->fatal_error)
        free(gps->fatal_error);

    pthread_mutex_destroy(&gps->read_mutex);
    pthread_mutex_destroy(&gps->sendbuffer_mutex);
}

#pragma mark Timer Routines (Called from Timer thread)

// Check the ftdi return code for a fatal error
// If an error occured, set the error message and kill the thread
static void check_ftdi(PNGPS *gps, const char *message, char* file, int line, int status)
{
	if (status >= 0)
        return;

    asprintf(&gps->fatal_error, "FATAL: %s line %d : %s, status = %d\n", file, line, message, status);
    pthread_exit(NULL);
}

// Trigger a fatal error
// Sets the error message and kills the thread
static void gps_error(PNGPS *gps, char *msg, ...)
{
	va_list args;
	va_start(args, msg);
    vasprintf(&gps->fatal_error, msg, args);
	va_end(args);

    pthread_exit(NULL);
}

// Calculate the checksum of a series of bytes by xoring them together
static unsigned char checksum(unsigned char *data, unsigned char length)
{
    unsigned char csm = data[0];
    for (unsigned char i = 1; i < length; i++)
        csm ^= data[i];
    return csm;
}

// Queue a raw byte to be sent to the timer
// Should only be called by queue_data
static void queue_send_byte(PNGPS *gps, unsigned char b)
{
    // Hard-loop until the send buffer empties
    // Should never happen in normal operation
    while (gps->send_length >= 255);

    pthread_mutex_lock(&gps->sendbuffer_mutex);
    gps->send_buffer[gps->send_length++] = b;
    pthread_mutex_unlock(&gps->sendbuffer_mutex);
}

// Wrap an array of bytes in a data packet and send it to the gps
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

// Initialize the usb connection to the timer
static void initialize_timer(PNGPS *gps)
{
	struct ftdi_device_list* devices = NULL;
	const int vendorId = 0x0403;  // FTDI USB vendor identifier
	const int productId = 0x6001; // FT232 USB product identifier

	// Get the list of FTDI devices on the system
	int numDevices = ftdi_usb_find_all(NULL, &devices, vendorId, productId);
	check_ftdi(gps, "ftdi_usb_find_all() returned an error code", __FILE__, __LINE__, numDevices);
	pn_log("Found %d FTDI device(s)", numDevices);

	// Assume that the first device is the gps unit
	if (numDevices == 0)
	{
  		ftdi_list_free(&devices);
		gps_error(gps, "FATAL: GPS unit unavailable");
	}

	gps->device = devices->dev;
  	ftdi_list_free(&devices);

	pn_log("Opened FTDI device `%s`", gps->device->filename);

	if (gps->context != NULL)
		gps_error(gps, "FATAL: device %s is already open @ %s:%d", gps->device->filename, __FILE__, __LINE__);

	gps->context = ftdi_new();
    if (gps->context == NULL)
        gps_error(gps, "FATAL: ftdi_new failed");

	int status = ftdi_init(gps->context);
	check_ftdi(gps, gps->context->error_str, __FILE__, __LINE__, status);

	// Prepare the device for use with libftdi library calls.
	status = ftdi_usb_open_dev(gps->context, gps->device);
	check_ftdi(gps, gps->context->error_str, __FILE__, __LINE__, status);

	//ftdi_enable_bitbang(pContext, 0xFF);

	status = ftdi_set_baudrate(gps->context, 115200);
	check_ftdi(gps, gps->context->error_str, __FILE__, __LINE__, status);

	status = ftdi_set_line_property(gps->context, BITS_8, STOP_BIT_1, NONE);
	check_ftdi(gps, gps->context->error_str, __FILE__, __LINE__, status);

	status = ftdi_setflowctrl(gps->context, SIO_DISABLE_FLOW_CTRL);
	check_ftdi(gps, gps->context->error_str, __FILE__, __LINE__, status);

	unsigned char latency = 1; // the latency in milliseconds before partially full bit buffers are sent.
	status = ftdi_set_latency_timer(gps->context, latency);
	check_ftdi(gps, gps->context->error_str, __FILE__, __LINE__, status);
}

// Close the usb connection to the timer
static void uninitialize_timer(PNGPS *gps)
{
	pn_log("Closing device %s", gps->device->filename);
	if (gps->context == NULL)
		pn_log("device %s is already closed @ %s:%d", gps->device->filename, __FILE__, __LINE__);

	int status = ftdi_usb_close(gps->context);
	check_ftdi(gps, "ftdi_usb_close() returned an error code", __FILE__, __LINE__, status);

	ftdi_deinit(gps->context);
	ftdi_free(gps->context);
	gps->context = NULL;
}

// Main timer thread loop
void *pn_timer_thread(void *_gps)
{
    PNGPS *gps = (PNGPS *)_gps;

    // Store recieved bytes in a 256 byte circular buffer indexed by an unsigned char
    // This ensures the correct circular behavior on over/underflow
	unsigned char recvbuf[256];
	unsigned char totalbuf[256];
	unsigned char writeIndex = 0;
	unsigned char readIndex = 0;
    rs_bool synced = FALSE;

    // Current packet storage
    unsigned char gps_packet[256];
    unsigned char gps_packet_length = 0;

	// Initialize the gps
    initialize_timer(gps);

    // Send two reset packets to the timer.
    // The first is used to sync the incoming data and discarded
    unsigned char unused = 0;
    queue_data(gps, RESET, &unused, 1);
    queue_data(gps, RESET, &unused, 1);

    // Loop until shutdown, parsing incoming data
	struct timespec wait = {0,1e8};
    while (!gps->shutdown)
	{
		nanosleep(&wait, NULL);

        // Send any data in the send buffer
        pthread_mutex_lock(&gps->sendbuffer_mutex);
        if (gps->send_length > 0)
        {
	        if (ftdi_write_data(gps->context, gps->send_buffer, gps->send_length) != gps->send_length)
                pn_log("Error sending send buffer");
            gps->send_length = 0;
        }
        pthread_mutex_unlock(&gps->sendbuffer_mutex);

        // Grab any data accumulated by ftdi
	    int ret = ftdi_read_data(gps->context, recvbuf, 256);
	    if (ret < 0)
		    gps_error(gps, "FATAL: Bad response from timer. return code 0x%x",ret);

        // Copy recieved bytes into the buffer
        for (int i = 0; i < ret; i++)
            totalbuf[writeIndex++] = recvbuf[i];

        // Sync to the end of the previous packet and the start of the next
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
            // Ignore DLE padding byte
            if (totalbuf[readIndex] == DLE && totalbuf[readIndex-1] == DLE)
                readIndex++;

            gps_packet[gps_packet_length++] = totalbuf[readIndex];

            // Packet Format:
            // <DLE> <data length> <type> <data 0> ... <data length-1> <checksum> <DLE> <ETX>

            rs_bool reset = FALSE;
            // Reached the end of the packet
            if (gps_packet_length > 2 && gps_packet_length == gps_packet[1] + 6)
            {
                // Validate packet checksum
                if (checksum(&gps_packet[3], gps_packet[1]) == gps_packet[gps_packet_length - 3])
                {
                    // Parse data packet
                    switch(gps_packet[2])
                    {
                        case CURRENTTIME:
	                        pthread_mutex_lock(&gps->read_mutex);
                            gps->current_timestamp.year = (gps_packet[8] & 0x00FF) | ((gps_packet[9] << 8) & 0xFF00);
                            gps->current_timestamp.month = gps_packet[7];
                            gps->current_timestamp.day = gps_packet[6];
                            gps->current_timestamp.hours = gps_packet[3];
                            gps->current_timestamp.minutes = gps_packet[4];
                            gps->current_timestamp.seconds = gps_packet[5];
                            gps->current_timestamp.locked = gps_packet[10];
                            gps->current_timestamp.remaining_exposure = gps_packet[11];
                            gps->current_timestamp.valid = TRUE;

                            pthread_mutex_unlock(&gps->read_mutex);
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
                            pthread_mutex_lock(&gps->read_mutex);
                            gps->download_timestamp.year = (gps_packet[8] & 0x00FF) | ((gps_packet[9] << 8) & 0xFF00);
                            gps->download_timestamp.month = gps_packet[7];
                            gps->download_timestamp.day = gps_packet[6];
                            gps->download_timestamp.hours = gps_packet[3];
                            gps->download_timestamp.minutes = gps_packet[4];
                            gps->download_timestamp.seconds = gps_packet[5];
                            gps->download_timestamp.locked = gps_packet[10];
                            gps->download_timestamp.valid = TRUE;
                            pn_log("Download: %04d-%02d-%02d %02d:%02d:%02d (%d)", (gps_packet[8] & 0x00FF) | ((gps_packet[9] << 8) & 0xFF00), // Year
                                                                      gps_packet[7],   // Month
                                                                      gps_packet[6],   // Day
                                                                      gps_packet[3],   // Hour
                                                                      gps_packet[4],   // Minute
                                                                      gps_packet[5],   // Second
                                                                      gps_packet[10]); // Locked

                            // Mark the camera as downloading for UI feedback and shutdown purposes
                            gps->camera_downloading = TRUE;
                            pthread_mutex_unlock(&gps->read_mutex);

                            // Trigger a fake download if the camera is simulated
                            simulate_camera_download();
                        break;
                        case DOWNLOADCOMPLETE:
                            pthread_mutex_lock(&gps->read_mutex);
                            gps->camera_downloading = FALSE;
                            pthread_mutex_unlock(&gps->read_mutex);
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

                // Reset for next packet
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

	// Uninitialize the timer connection and exit
	uninitialize_timer(gps);
	pthread_exit(NULL);
}

// Main simulated timer thread loop
void *pn_simulated_timer_thread(void *_gps)
{
    PNGPS *gps = (PNGPS *)_gps;

	// Initialization
    pn_log("Simulating GPS");
    gps->simulated = TRUE;
    gps->simulated_remaining = gps->simulated_exptime = 0;
    time_t last_unixtime = time(NULL);

    // Loop until shutdown, parsing incoming data
	struct timespec wait = {0,1e8};
    while (!gps->shutdown)
	{
		nanosleep(&wait, NULL);

        time_t cur_unixtime = time(NULL);
        if (cur_unixtime != last_unixtime)
        {
            struct tm *t = gmtime(&cur_unixtime);
            if (gps->simulated_exptime > 0)
                gps->simulated_remaining -= (int)(cur_unixtime - last_unixtime);

            // Will behave badly if remaining < 0, but this should never happen
            if (gps->simulated_remaining <= 0 && gps->simulated_exptime > 0)
            {
                gps->simulated_remaining = gps->simulated_exptime;
                pthread_mutex_lock(&gps->read_mutex);
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
                pthread_mutex_unlock(&gps->read_mutex);

                simulate_camera_download();
            }

            pthread_mutex_lock(&gps->read_mutex);
            gps->current_timestamp.year = t->tm_year + 1900;
            gps->current_timestamp.month = t->tm_mon;
            gps->current_timestamp.day = t->tm_wday;
            gps->current_timestamp.hours = t->tm_hour;
            gps->current_timestamp.minutes = t->tm_min;
            gps->current_timestamp.seconds = t->tm_sec;
            gps->current_timestamp.locked = 1;
            gps->current_timestamp.remaining_exposure = gps->simulated_remaining;
            gps->current_timestamp.valid = TRUE;
            pthread_mutex_unlock(&gps->read_mutex);

            last_unixtime = cur_unixtime;
        }
    }

    pn_log("Closing simulated GPS");
	pthread_exit(NULL);
}

#pragma mark Timer communication Routines (Called from any thread)

// Start an exposure sequence with a specified exposure time
void pn_gps_start_exposure(PNGPS *gps, unsigned char exptime)
{
    pn_log("Starting exposure @ %ds", exptime);
    if (gps->simulated)
        gps->simulated_remaining = gps->simulated_exptime = exptime;
    else
        queue_data(gps, START_EXPOSURE, &exptime, 1);
}

// Stop the current exposure sequence
void pn_gps_stop_exposure(PNGPS *gps)
{
    pn_log("Stopping exposure");
    unsigned char unused = 0;

    if (gps->simulated)
        shutdown_camera();
    else
        queue_data(gps, STOP_EXPOSURE, &unused, 1);
}

// Utility routine to subtract a number of seconds from a PNGPSTimestamp
PNGPSTimestamp pn_timestamp_subtract_seconds(PNGPSTimestamp ts, int seconds)
{
    // Let gmtime/timegm do the hard work of normalizing the time
	struct tm a = {ts.seconds - seconds, ts.minutes, ts.hours, ts.day, ts.month, ts.year - 1900,0,0,0};
	time_t b = timegm(&a);
	gmtime_r(&b, &a);

    // Construct a new timestamp to return
    PNGPSTimestamp ret = ts;
	ret.seconds = a.tm_sec;
	ret.minutes = a.tm_min;
	ret.hours = a.tm_hour;
	ret.day = a.tm_mday;
	ret.month = a.tm_mon;
	ret.year = a.tm_year + 1900;

    return ret;
}

