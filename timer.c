/*
 * Copyright 2010-2012 Paul Chote
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
#include "timer.h"
#include "common.h"
#include "preferences.h"
#include "platform.h"
#include "camera.h"

// Private struct implementation
struct TimerUnit
{
    bool simulated;
    int simulated_exptime;
    int simulated_remaining;
    bool simulated_send_shutdown;

    struct usb_device *device;
    struct ftdi_context *context;
    bool shutdown;
    TimerTimestamp current_timestamp;
    bool camera_downloading;

    unsigned char send_buffer[256];
    unsigned char send_length;
    pthread_mutex_t read_mutex;
    pthread_mutex_t sendbuffer_mutex;
};

// Command packet types
typedef enum
{
    CURRENTTIME = 'A',
    DOWNLOADTIME = 'B',
    DEBUG_STRING = 'C',
    DEBUG_RAW = 'D',
    START_EXPOSURE = 'E',
    STOP_EXPOSURE = 'F',
    RESET = 'G',
    DOWNLOADCOMPLETE = 'H',
    SIMULATE_CAMERA = 'I',
    UNKNOWN_PACKET = 0
} TimerUnitPacketType;

#pragma mark Creation and Destruction (Called from main thread)

// Initialize a new TimerUnit struct.
TimerUnit *timer_new()
{
    TimerUnit *timer = malloc(sizeof(struct TimerUnit));
    if (!timer)
        trigger_fatal_error("Malloc failed while allocating timer");

    timer->simulated = false;
    timer->simulated_exptime = 0;
    timer->simulated_remaining = 0;
    timer->simulated_send_shutdown = false;

    timer->device = NULL;
    timer->context = NULL;
    timer->shutdown = false;
    timer->current_timestamp.valid = false;
    timer->current_timestamp.locked = false;
    timer->camera_downloading = 0;
    timer->send_length = 0;

    pthread_mutex_init(&timer->read_mutex, NULL);
    pthread_mutex_init(&timer->sendbuffer_mutex, NULL);

    return timer;
}

// Destroy a PNCamera struct.
void timer_free(TimerUnit *timer)
{
    pthread_mutex_destroy(&timer->read_mutex);
    pthread_mutex_destroy(&timer->sendbuffer_mutex);
}

#pragma mark Timer Routines (Called from Timer thread)

// Check the ftdi return code for a fatal error
// If an error occured, set the error message and kill the thread
static void check_ftdi(TimerUnit *timer, const char *message, char* file, int line, int status)
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
static void fatal_timer_error(TimerUnit *timer, char *msg, ...)
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
static void queue_send_byte(TimerUnit *timer, unsigned char b)
{
    // Hard-loop until the send buffer empties
    // Should never happen in normal operation
    while (timer->send_length >= 255);

    pthread_mutex_lock(&timer->sendbuffer_mutex);
    timer->send_buffer[timer->send_length++] = b;
    pthread_mutex_unlock(&timer->sendbuffer_mutex);
}

// Wrap an array of bytes in a data packet and send it to the timer
static void queue_data(TimerUnit *timer, unsigned char type, unsigned char *data, unsigned char length)
{
    // Data packet starts with $$ followed by packet type
    queue_send_byte(timer, '$');
    queue_send_byte(timer, '$');
    queue_send_byte(timer, type);

    // Length of data section
    queue_send_byte(timer, length);

    // Packet data
    for (unsigned char i = 0; i < length; i++)
        queue_send_byte(timer, data[i]);

    // Checksum
    queue_send_byte(timer, checksum(data,length));

    // Data packet ends with linefeed and carriage return
    queue_send_byte(timer, '\r');
    queue_send_byte(timer, '\n');
}

// Initialize the usb connection to the timer
static void initialize_timer(TimerUnit *timer)
{
    struct ftdi_device_list* devices = NULL;
    const int vendorId = 0x0403;  // FTDI USB vendor identifier
    const int productId = 0x6001; // FT232 USB product identifier

    // Get the list of FTDI devices on the system
    int numDevices = ftdi_usb_find_all(NULL, &devices, vendorId, productId);
    check_ftdi(timer, "ftdi_usb_find_all() returned an error code", __FILE__, __LINE__, numDevices);
    pn_log("Found %d FTDI device(s)", numDevices);

    // Assume that the first device is the timer unit
    if (numDevices == 0)
    {
        ftdi_list_free(&devices);
        fatal_timer_error(timer, "Timer not found");
    }

    timer->device = devices->dev;
    ftdi_list_free(&devices);

    pn_log("Opened FTDI device `%s`", timer->device->filename);

    if (timer->context != NULL)
        fatal_timer_error(timer, "device %s is already open", timer->device->filename);

    timer->context = ftdi_new();
    if (timer->context == NULL)
        fatal_timer_error(timer, "ftdi_new failed");

    int status = ftdi_init(timer->context);
    check_ftdi(timer, timer->context->error_str, __FILE__, __LINE__, status);

    // Prepare the device for use with libftdi library calls.
    status = ftdi_usb_open_dev(timer->context, timer->device);
    check_ftdi(timer, timer->context->error_str, __FILE__, __LINE__, status);

    //ftdi_enable_bitbang(pContext, 0xFF);

    status = ftdi_set_baudrate(timer->context, 250000);
    check_ftdi(timer, timer->context->error_str, __FILE__, __LINE__, status);

    status = ftdi_set_line_property(timer->context, BITS_8, STOP_BIT_1, NONE);
    check_ftdi(timer, timer->context->error_str, __FILE__, __LINE__, status);

    status = ftdi_setflowctrl(timer->context, SIO_DISABLE_FLOW_CTRL);
    check_ftdi(timer, timer->context->error_str, __FILE__, __LINE__, status);

    unsigned char latency = 1; // the latency in milliseconds before partially full bit buffers are sent.
    status = ftdi_set_latency_timer(timer->context, latency);
    check_ftdi(timer, timer->context->error_str, __FILE__, __LINE__, status);
}

// Close the usb connection to the timer
static void uninitialize_timer(TimerUnit *timer)
{
    pn_log("Closing device %s", timer->device->filename);
    if (timer->context == NULL)
        pn_log("device %s is already closed @ %s:%d", timer->device->filename, __FILE__, __LINE__);

    int status = ftdi_usb_close(timer->context);
    check_ftdi(timer, "ftdi_usb_close() returned an error code", __FILE__, __LINE__, status);

    ftdi_deinit(timer->context);
    ftdi_free(timer->context);
    timer->context = NULL;
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
void *pn_timer_thread(void *_timer)
{
    TimerUnit *timer = (TimerUnit *)_timer;

    // Store recieved bytes in a 256 byte circular buffer indexed by an unsigned char
    // This ensures the correct circular behavior on over/underflow
    unsigned char input_buf[256];
    unsigned char write_index = 0;
    unsigned char read_index = 0;

    // Current packet storage
    unsigned char packet[256];
    unsigned char packet_length = 0;
    unsigned char packet_expected_length = 0;
    TimerUnitPacketType packet_type = UNKNOWN_PACKET;

    // Initialization
    initialize_timer(timer);

    // Reset timer to its idle state
    queue_data(timer, RESET, NULL, 0);

    // Loop until shutdown, parsing incoming data
    while (true)
    {
        millisleep(100);

        // Reset the timer before shutting down
        if (timer->shutdown)
            queue_data(timer, RESET, NULL, 0);

        // Send any data in the send buffer
        pthread_mutex_lock(&timer->sendbuffer_mutex);
        if (timer->send_length > 0)
        {
            if (ftdi_write_data(timer->context, timer->send_buffer, timer->send_length) != timer->send_length)
                pn_log("Error sending send buffer");
            timer->send_length = 0;
        }
        pthread_mutex_unlock(&timer->sendbuffer_mutex);

        if (timer->shutdown)
            break;

        // Grab any data accumulated by ftdi
        unsigned char recvbuf[256];
        int ret = ftdi_read_data(timer->context, recvbuf, 256);
        if (ret < 0)
            fatal_timer_error(timer, "Bad response from timer. return code 0x%x",ret);

        // Copy recieved bytes into the circular input buffer
        for (int i = 0; i < ret; i++)
            input_buf[write_index++] = recvbuf[i];

        // Sync to the start of a data packet
        for (; packet_type == UNKNOWN_PACKET && read_index != write_index; read_index++)
        {
            if (input_buf[(unsigned char)(read_index - 3)] == '$' &&
                input_buf[(unsigned char)(read_index - 2)] == '$' &&
                input_buf[(unsigned char)(read_index - 1)] != '$')
            {
                packet_type = input_buf[(unsigned char)(read_index - 1)];
                packet_expected_length = input_buf[read_index] + 7;
                read_index -= 3;

                //pn_log("sync packet 0x%02x, length %d", packet_type, packet_expected_length);
                break;
            }
        }

        // Copy packet data into a buffer for parsing
        for (; read_index != write_index && packet_length != packet_expected_length; read_index++)
            packet[packet_length++] = input_buf[read_index];

        // Waiting for more data
        if (packet_type == UNKNOWN_PACKET || packet_length < packet_expected_length)
            continue;

        //
        // End of packet
        //
        unsigned char data_length = packet[3];
        unsigned char *data = &packet[4];
        unsigned char data_checksum = packet[packet_length - 3];

        // Check that the packet ends correctly
        if (packet[packet_length - 2] != '\r' || packet[packet_length - 1] != '\n')
        {
            pn_log("Invalid packet length, expected %d", packet_expected_length);
            log_raw_data(packet, packet_expected_length);
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
        switch(packet_type)
        {
            case CURRENTTIME:
            {
                pthread_mutex_lock(&timer->read_mutex);
                timer->current_timestamp = (TimerTimestamp)
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
                pthread_mutex_unlock(&timer->read_mutex);
                break;
            }
            case DOWNLOADTIME:
            {
                TimerTimestamp t = (TimerTimestamp)
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
                pthread_mutex_lock(&timer->read_mutex);
                timer->camera_downloading = true;
                pthread_mutex_unlock(&timer->read_mutex);
                break;
            }
            case DOWNLOADCOMPLETE:
            {
                pthread_mutex_lock(&timer->read_mutex);
                timer->camera_downloading = false;
                pthread_mutex_unlock(&timer->read_mutex);
                break;
            }
            case DEBUG_STRING:
            {
                packet[packet_length - 3] = '\0';
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
                pn_log("Unknown packet type %02x", packet_type);
            }
        }

    resetpacket:
        packet_length = 0;
        packet_expected_length = 0;
        packet_type = UNKNOWN_PACKET;
    }

    // Uninitialize the timer connection and exit
    uninitialize_timer(timer);
    pthread_exit(NULL);
}

// Main simulated timer thread loop
void *pn_simulated_timer_thread(void *_timer)
{
    TimerUnit *timer = (TimerUnit *)_timer;

    // Initialization
    pn_log("Simulating GPS");
    timer->simulated = true;
    timer->simulated_remaining = timer->simulated_exptime = 0;
    time_t last_unixtime = time(NULL);

    // Loop until shutdown, parsing incoming data
    while (!timer->shutdown)
    {
        millisleep(100);

        pthread_mutex_lock(&timer->read_mutex);
        bool send_shutdown = timer->simulated_send_shutdown;
        timer->simulated_send_shutdown = false;
        pthread_mutex_unlock(&timer->read_mutex);
        if (send_shutdown)
        {
            timer->simulated_exptime = 0;
            timer->simulated_remaining = 0;
            pn_camera_notify_safe_to_stop();
        }

        time_t cur_unixtime = time(NULL);
        if (cur_unixtime != last_unixtime)
        {
            struct tm *pc_time = gmtime(&cur_unixtime);
            if (timer->simulated_exptime > 0)
                timer->simulated_remaining -= (int)(cur_unixtime - last_unixtime);

            // Will behave badly if remaining < 0, but this should never happen
            if (timer->simulated_remaining <= 0 && timer->simulated_exptime > 0)
            {
                timer->simulated_remaining = timer->simulated_exptime;
                TimerTimestamp t = (TimerTimestamp)
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

                pthread_mutex_lock(&timer->read_mutex);
                timer->camera_downloading = true;
                pthread_mutex_unlock(&timer->read_mutex);
            }

            pthread_mutex_lock(&timer->read_mutex);
            timer->current_timestamp = (TimerTimestamp)
            {
                .year = pc_time->tm_year + 1900,
                .month = pc_time->tm_mon,
                .day = pc_time->tm_wday,
                .hours = pc_time->tm_hour,
                .minutes = pc_time->tm_min,
                .seconds = pc_time->tm_sec,
                .locked = 1,
                .remaining_exposure = timer->simulated_remaining,
                .valid = true
            };

            pthread_mutex_unlock(&timer->read_mutex);

            last_unixtime = cur_unixtime;
        }
    }

    pn_log("Closing simulated GPS");
    pthread_exit(NULL);
}

#pragma mark Timer communication Routines (Called from any thread)

// Start an exposure sequence with a specified exposure time
void timer_start_exposure(TimerUnit *timer, unsigned char exptime)
{
    pn_log("Starting exposure @ %ds", exptime);
    if (timer->simulated)
        timer->simulated_remaining = timer->simulated_exptime = exptime;
    else
    {
        unsigned char simulate_camera = pn_camera_is_simulated() || !pn_preference_char(TIMER_MONITOR_LOGIC_OUT);
        if (simulate_camera)
            pn_log("WARNING: USING SIMULATED CAMERA STATUS");

        queue_data(timer, SIMULATE_CAMERA, &simulate_camera, 1);
        queue_data(timer, START_EXPOSURE, &exptime, 1);
    }
}

// Stop the current exposure sequence
void timer_stop_exposure(TimerUnit *timer)
{
    pn_log("Stopping exposure");
    if (timer->simulated)
    {
        pthread_mutex_lock(&timer->read_mutex);
        timer->simulated_send_shutdown = true;
        pthread_mutex_unlock(&timer->read_mutex);
    }
    else
        queue_data(timer, STOP_EXPOSURE, NULL, 0);
}

// Returns the status of the camera logic output
bool timer_camera_downloading(TimerUnit *timer)
{
    pthread_mutex_lock(&timer->read_mutex);
    bool downloading = timer->camera_downloading;
    pthread_mutex_unlock(&timer->read_mutex);
    return downloading;
}

TimerTimestamp timer_current_timestamp(TimerUnit *timer)
{
    pthread_mutex_lock(&timer->read_mutex);
    TimerTimestamp ts = timer->current_timestamp;
    pthread_mutex_unlock(&timer->read_mutex);
    return ts;
}

void timer_request_shutdown(TimerUnit *timer)
{
    timer->shutdown = true;
}

// Callback to notify that the simulated camera has read out
// TODO: This is a crappy abstraction
void timer_set_simulated_camera_downloading(TimerUnit *timer, bool downloading)
{
    pthread_mutex_lock(&timer->read_mutex);
    timer->camera_downloading = downloading;
    pthread_mutex_unlock(&timer->read_mutex);
}

// Utility routine to subtract a number of seconds from a TimerTimestamp
TimerTimestamp pn_timestamp_normalize(TimerTimestamp ts)
{
    // Let gmtime/timegm do the hard work of normalizing the time
    struct tm a = {ts.seconds, ts.minutes, ts.hours, ts.day, ts.month, ts.year - 1900,0,0,0};
    normalize_tm(&a);

    // Construct a new timestamp to return
    TimerTimestamp ret = ts;
    ret.seconds = a.tm_sec;
    ret.minutes = a.tm_min;
    ret.hours = a.tm_hour;
    ret.day = a.tm_mday;
    ret.month = a.tm_mon;
    ret.year = a.tm_year + 1900;

    return ret;
}
