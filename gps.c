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
#include <sys/types.h>
#include <ftdi.h>
#include "common.h"
#include "preferences.h"
#include "gps.h"

#pragma mark Creation and Destruction (Called from main thread)

// Initialize a new PNGPS struct.
PNGPS pn_gps_new()
{
    PNGPS ret;
    ret.context = NULL;
    ret.shutdown = false;
    ret.send_length = 0;
    ret.camera_downloading = 0;
    ret.current_timestamp.valid = false;
    ret.current_timestamp.locked = false;
    ret.simulated = false;

    pthread_mutex_init(&ret.read_mutex, NULL);
    pthread_mutex_init(&ret.sendbuffer_mutex, NULL);

    return ret;
}

// Destroy a PNCamera struct.
void pn_gps_free(PNGPS *gps)
{
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

    char *fatal_error;
    asprintf(&fatal_error, "FATAL: %s line %d : %s, status = %d\n", file, line, message, status);
    trigger_fatal_error(fatal_error);

    pthread_exit(NULL);
}

// Trigger a fatal error
// Sets the error message and kills the thread
static void gps_error(PNGPS *gps, char *msg, ...)
{
    char *fatal_error;
    va_list args;
    va_start(args, msg);
    vasprintf(&fatal_error, msg, args);
    va_end(args);

    trigger_fatal_error(fatal_error);
    pthread_exit(NULL);
}

// Calculate the checksum of a series of bytes by xoring them together
static unsigned char checksum(unsigned char *data, unsigned char length)
{
    unsigned char csm = 0;
    for (unsigned char i = 0; i < length; i++)
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
    // Data packet starts with $$ followed by packet type
    queue_send_byte(gps, '$');
    queue_send_byte(gps, '$');
    queue_send_byte(gps, type);

    // Length of data section
    queue_send_byte(gps, length);

    // Packet data
    for (unsigned char i = 0; i < length; i++)
        queue_send_byte(gps, data[i]);

    // Checksum
    queue_send_byte(gps, checksum(data,length));

    // Data packet ends with linefeed and carriage return
    queue_send_byte(gps, '\r');
    queue_send_byte(gps, '\n');
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
        gps_error(gps, "Timer not found");
    }

    gps->device = devices->dev;
    ftdi_list_free(&devices);

    pn_log("Opened FTDI device `%s`", gps->device->filename);

    if (gps->context != NULL)
        gps_error(gps, "device %s is already open", gps->device->filename);

    gps->context = ftdi_new();
    if (gps->context == NULL)
        gps_error(gps, "ftdi_new failed");

    int status = ftdi_init(gps->context);
    check_ftdi(gps, gps->context->error_str, __FILE__, __LINE__, status);

    // Prepare the device for use with libftdi library calls.
    status = ftdi_usb_open_dev(gps->context, gps->device);
    check_ftdi(gps, gps->context->error_str, __FILE__, __LINE__, status);

    //ftdi_enable_bitbang(pContext, 0xFF);

    status = ftdi_set_baudrate(gps->context, 250000);
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

static void log_raw_data(unsigned char *data, int len)
{
    char *msg = (char *)malloc((3*len+1)*sizeof(char));
    if (msg == NULL)
        pn_log("Memory allocation error in log_raw_data");

    for (unsigned char i = 0; i < len; i++)
        sprintf(msg+3*i, "%02x ", data[i]);
    pn_log(msg);
    free(msg);
}

// Main timer thread loop
void *pn_timer_thread(void *_gps)
{
    PNGPS *gps = (PNGPS *)_gps;

    // Store recieved bytes in a 256 byte circular buffer indexed by an unsigned char
    // This ensures the correct circular behavior on over/underflow
    unsigned char input_buf[256];
    unsigned char write_index = 0;
    unsigned char read_index = 0;

    // Current packet storage
    unsigned char gps_packet[256];
    unsigned char gps_packet_length = 0;
    unsigned char gps_packet_expected_length = 0;
    PNGPSPacketType gps_packet_type = UNKNOWN_PACKET;

    // Initialization
    initialize_timer(gps);

    // Reset timer to its idle state
    queue_data(gps, RESET, NULL, 0);

    // Loop until shutdown, parsing incoming data
    while (true)
    {
        millisleep(100);

        // Reset the timer before shutting down
        if (gps->shutdown)
            queue_data(gps, RESET, NULL, 0);

        // Send any data in the send buffer
        pthread_mutex_lock(&gps->sendbuffer_mutex);
        if (gps->send_length > 0)
        {
            if (ftdi_write_data(gps->context, gps->send_buffer, gps->send_length) != gps->send_length)
                pn_log("Error sending send buffer");
            gps->send_length = 0;
        }
        pthread_mutex_unlock(&gps->sendbuffer_mutex);

        if (gps->shutdown)
            break;

        // Grab any data accumulated by ftdi
        unsigned char recvbuf[256];
        int ret = ftdi_read_data(gps->context, recvbuf, 256);
        if (ret < 0)
            gps_error(gps, "Bad response from timer. return code 0x%x",ret);

        // Copy recieved bytes into the circular input buffer
        for (int i = 0; i < ret; i++)
            input_buf[write_index++] = recvbuf[i];

        // Sync to the start of a data packet
        for (; gps_packet_type == UNKNOWN_PACKET && read_index != write_index; read_index++)
        {
            if (input_buf[(unsigned char)(read_index - 3)] == '$' &&
                input_buf[(unsigned char)(read_index - 2)] == '$' &&
                input_buf[(unsigned char)(read_index - 1)] != '$')
            {
                gps_packet_type = input_buf[(unsigned char)(read_index - 1)];
                gps_packet_expected_length = input_buf[read_index] + 7;
                read_index -= 3;

                //pn_log("sync packet 0x%02x, length %d", gps_packet_type, gps_packet_expected_length);
                break;
            }
        }

        // Copy packet data into a buffer for parsing
        for (; read_index != write_index && gps_packet_length != gps_packet_expected_length; read_index++)
            gps_packet[gps_packet_length++] = input_buf[read_index];

        // Waiting for more data
        if (gps_packet_type == UNKNOWN_PACKET || gps_packet_length < gps_packet_expected_length)
            continue;

        //
        // End of packet
        //
        unsigned char data_length = gps_packet[3];
        unsigned char *data = &gps_packet[4];
        unsigned char data_checksum = gps_packet[gps_packet_length - 3];

        // Check that the packet ends correctly
        if (gps_packet[gps_packet_length - 2] != '\r' || gps_packet[gps_packet_length - 1] != '\n')
        {
            pn_log("Invalid packet length, expected %d", gps_packet_expected_length);
            log_raw_data(gps_packet, gps_packet_expected_length);
            goto resetpacket;
        }

        // Verify packet checksum
        unsigned char csm = checksum(data, data_length);
        if (csm != data_checksum)
        {
            pn_log("Invalid packet checksum. Got 0x%02x, expected 0x%02x", csm, data_checksum);
            goto resetpacket;
        }

        // Handle packet
        switch(gps_packet_type)
        {
            case CURRENTTIME:
            {
                pthread_mutex_lock(&gps->read_mutex);
                gps->current_timestamp = (PNGPSTimestamp)
                {
                    .year = (data[5] & 0x00FF) | ((data[6] << 8) & 0xFF00),
                    .month = data[4],
                    .day = data[3],
                    .hours = data[0],
                    .minutes = data[1],
                    .seconds = data[2],
                    .locked = data[7],
                    .remaining_exposure = data[8],
                    .valid = true
                };
                pthread_mutex_unlock(&gps->read_mutex);
                break;
            }
            case DOWNLOADTIME:
            {
                PNGPSTimestamp t = (PNGPSTimestamp)
                {
                    .year = (data[5] & 0x00FF) | ((data[6] << 8) & 0xFF00),
                    .month = data[4],
                    .day = data[3],
                    .hours = data[0],
                    .minutes = data[1],
                    .seconds = data[2],
                    .locked = data[7],
                    .valid = true
                };
                pn_log("Trigger: %04d-%02d-%02d %02d:%02d:%02d (%d)", t.year, t.month, t.day, t.hours, t.minutes, t.seconds, t.locked);
                queue_trigger_timestamp(t);

                // Mark the camera as downloading for UI feedback and shutdown purposes
                pthread_mutex_lock(&gps->read_mutex);
                gps->camera_downloading = true;
                pthread_mutex_unlock(&gps->read_mutex);
                break;
            }
            case DOWNLOADCOMPLETE:
            {
                pthread_mutex_lock(&gps->read_mutex);
                gps->camera_downloading = false;
                pthread_mutex_unlock(&gps->read_mutex);
                break;
            }
            case DEBUG_STRING:
            {
                gps_packet[gps_packet_length - 3] = '\0';
                pn_log("GPS Debug: `%s`", data);
                break;
            }
            case DEBUG_RAW:
            {
                log_raw_data(data, data_length);
                break;
            }
            case STOP_EXPOSURE:
            {
                pn_log("Timer reports safe to shutdown camera");
                pn_camera_notify_safe_to_stop();
                break;
            }
            default:
            {
                pn_log("Unknown packet type %02x", gps_packet_type);
            }
        }

    resetpacket:
        gps_packet_length = 0;
        gps_packet_expected_length = 0;
        gps_packet_type = UNKNOWN_PACKET;
    }

    // Uninitialize the timer connection and exit
    uninitialize_timer(gps);
    pthread_exit(NULL);
}

// Main simulated timer thread loop
static bool simulated_send_shutdown = false;
void *pn_simulated_timer_thread(void *_gps)
{
    PNGPS *gps = (PNGPS *)_gps;

    // Initialization
    pn_log("Simulating GPS");
    gps->simulated = true;
    gps->simulated_remaining = gps->simulated_exptime = 0;
    time_t last_unixtime = time(NULL);

    // Loop until shutdown, parsing incoming data
    while (!gps->shutdown)
    {
        millisleep(100);

        pthread_mutex_lock(&gps->read_mutex);
        bool send_shutdown = simulated_send_shutdown;
        simulated_send_shutdown = false;
        pthread_mutex_unlock(&gps->read_mutex);
        if (send_shutdown)
        {
            gps->simulated_exptime = 0;
            gps->simulated_remaining = 0;
            pn_camera_notify_safe_to_stop();
        }

        time_t cur_unixtime = time(NULL);
        if (cur_unixtime != last_unixtime)
        {
            struct tm *pc_time = gmtime(&cur_unixtime);
            if (gps->simulated_exptime > 0)
                gps->simulated_remaining -= (int)(cur_unixtime - last_unixtime);

            // Will behave badly if remaining < 0, but this should never happen
            if (gps->simulated_remaining <= 0 && gps->simulated_exptime > 0)
            {
                gps->simulated_remaining = gps->simulated_exptime;
                PNGPSTimestamp t = (PNGPSTimestamp)
                {
                    .year = pc_time->tm_year + 1900,
                    .month = pc_time->tm_mon,
                    .day = pc_time->tm_wday,
                    .hours = pc_time->tm_hour,
                    .minutes = pc_time->tm_min,
                    .seconds = pc_time->tm_sec,
                    .locked = 1,
                    .remaining_exposure = 0,
                    .valid = true
                };
                pn_log("Simulated Trigger: %04d-%02d-%02d %02d:%02d:%02d (%d)", t.year, t.month, t.day, t.hours, t.minutes, t.seconds, t.locked);
                queue_trigger_timestamp(t);

                pthread_mutex_lock(&gps->read_mutex);
                gps->camera_downloading = true;
                pthread_mutex_unlock(&gps->read_mutex);
            }

            pthread_mutex_lock(&gps->read_mutex);
            gps->current_timestamp = (PNGPSTimestamp)
            {
                .year = pc_time->tm_year + 1900,
                .month = pc_time->tm_mon,
                .day = pc_time->tm_wday,
                .hours = pc_time->tm_hour,
                .minutes = pc_time->tm_min,
                .seconds = pc_time->tm_sec,
                .locked = 1,
                .remaining_exposure = gps->simulated_remaining,
                .valid = true
            };

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
    {
        unsigned char simulate_camera = pn_camera_is_simulated() || !pn_preference_char(TIMER_MONITOR_LOGIC_OUT);
        if (simulate_camera)
            pn_log("WARNING: USING SIMULATED CAMERA STATUS");

        queue_data(gps, SIMULATE_CAMERA, &simulate_camera, 1);
        queue_data(gps, START_EXPOSURE, &exptime, 1);
    }
}

// Stop the current exposure sequence
void pn_gps_stop_exposure(PNGPS *gps)
{
    pn_log("Stopping exposure");
    if (gps->simulated)
    {
        pthread_mutex_lock(&gps->read_mutex);
        simulated_send_shutdown = true;
        pthread_mutex_unlock(&gps->read_mutex);
    }
    else
        queue_data(gps, STOP_EXPOSURE, NULL, 0);
}

// Returns the status of the camera logic output
bool pn_gps_camera_downloading(PNGPS *gps)
{
    pthread_mutex_lock(&gps->read_mutex);
    bool downloading = gps->camera_downloading;
    pthread_mutex_unlock(&gps->read_mutex);
    return downloading;
}

PNGPSTimestamp pn_gps_current_timestamp(PNGPS *gps)
{
    pthread_mutex_lock(&gps->read_mutex);
    PNGPSTimestamp ts = gps->current_timestamp;
    pthread_mutex_unlock(&gps->read_mutex);
    return ts;
}

void pn_gps_request_shutdown(PNGPS *gps)
{
    gps->shutdown = true;
}

// Callback to notify that the simulated camera has read out
// TODO: This is a crappy abstraction
void pn_gps_set_simulated_camera_downloading(PNGPS *gps, bool downloading)
{
    pthread_mutex_lock(&gps->read_mutex);
    gps->camera_downloading = downloading;
    pthread_mutex_unlock(&gps->read_mutex);
}

// Utility routine to subtract a number of seconds from a PNGPSTimestamp
PNGPSTimestamp pn_timestamp_normalize(PNGPSTimestamp ts)
{
    // Let gmtime/timegm do the hard work of normalizing the time
    struct tm a = {ts.seconds, ts.minutes, ts.hours, ts.day, ts.month, ts.year - 1900,0,0,0};
    normalize_tm(&a);

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
