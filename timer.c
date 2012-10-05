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
#ifdef USE_LIBFTDI
#   include <ftdi.h>
#else
#   include <ftd2xx.h>
#endif
#include "timer.h"
#include "main.h"
#include "preferences.h"
#include "platform.h"
#include "camera.h"

// Private struct implementation
struct TimerUnit
{
    pthread_t timer_thread;
    bool thread_initialized;

    bool simulated;
    int simulated_exptime;
    int simulated_remaining;
    bool simulated_send_shutdown;

#ifdef USE_LIBFTDI
    struct ftdi_context *context;
#else
    FT_HANDLE handle;
#endif

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


void *pn_timer_thread(void *timer);
void *pn_simulated_timer_thread(void *args);
#pragma mark Creation and Destruction (Called from main thread)

// Initialize a new TimerUnit struct.
TimerUnit *timer_new(bool simulate_hardware)
{
    TimerUnit *timer = calloc(1, sizeof(struct TimerUnit));
    if (!timer)
        return NULL;

    timer->simulated = simulate_hardware;
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

void timer_spawn_thread(TimerUnit *timer, ThreadCreationArgs *args)
{
    if (timer->simulated)
        pthread_create(&timer->timer_thread, NULL, pn_simulated_timer_thread, (void *)args);
    else
        pthread_create(&timer->timer_thread, NULL, pn_timer_thread, (void *)args);

    timer->thread_initialized = true;
}

#pragma mark Timer Routines (Called from Timer thread)

// Trigger a fatal error
// Sets the error message and kills the thread
static void fatal_timer_error(TimerUnit *timer, char *format, ...)
{
    va_list args;
    va_start(args, format);
    int len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    char *fatal_error = malloc((len + 1)*sizeof(char));
    if (fatal_error)
    {
        va_start(args, format);
        vsnprintf(fatal_error, len + 1, format, args);
        va_end(args);
    }
    else
        pn_log("Failed to allocate memory for fatal error");

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
#ifdef USE_LIBFTDI
    timer->context = ftdi_new();
    if (timer->context == NULL)
        fatal_timer_error(timer, "Error creating timer context");

    if (ftdi_init(timer->context) < 0)
        fatal_timer_error(timer, "Error initializing timer context: %s", timer->context->error_str);

    // Open the first available FTDI device, under the
    // assumption that it is the timer
    while (!timer->shutdown)
    {
        if (ftdi_usb_open(timer->context, 0x0403, 0x6001) == 0)
            break;

        pn_log("Waiting for timer...");
        millisleep(500);
    }

    if (ftdi_set_baudrate(timer->context, 250000) < 0)
        fatal_timer_error(timer, "Error setting timer baudrate: %s", timer->context->error_str);

    if (ftdi_set_line_property(timer->context, BITS_8, STOP_BIT_1, NONE) < 0)
        fatal_timer_error(timer, "Error setting timer data frame properties: %s", timer->context->error_str);

    if (ftdi_setflowctrl(timer->context, SIO_DISABLE_FLOW_CTRL) < 0)
        fatal_timer_error(timer, "Error setting timer flow control: %s", timer->context->error_str);

    // the latency in milliseconds before partially full bit buffers are sent.
    if (ftdi_set_latency_timer(timer->context, 1) < 0)
        fatal_timer_error(timer, "Error setting timer read timeout: %s", timer->context->error_str);
#else
    // Open the first available FTDI device, under the
    // assumption that it is the timer
    while (!timer->shutdown)
    {
        if (FT_Open(0, &timer->handle) == FT_OK)
            break;

        pn_log("Waiting for timer...");
        millisleep(1000);
    }

    // Set baud rate to 250k (matches internal USB<->UART rate within the timer)
    if (FT_SetBaudRate(timer->handle, 250000) != FT_OK)
        fatal_timer_error(timer, "Error setting timer baudrate");

    // Set data frame: 8N1
    if (FT_SetDataCharacteristics(timer->handle, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE) != FT_OK)
        fatal_timer_error(timer, "Error setting timer data characteristics");

    // Set read timeout to 1ms, write timeout unchanged
    if (FT_SetTimeouts(timer->handle, 1, 0) != FT_OK)
        fatal_timer_error(timer, "Error setting timer read timeout");
#endif
}

// Close the usb connection to the timer
static void uninitialize_timer(TimerUnit *timer)
{
    pn_log("Closing timer");
#ifdef USE_LIBFTDI
    if (ftdi_usb_close(timer->context) < 0)
        fatal_timer_error(timer, "Error closing timer connection");

    ftdi_deinit(timer->context);
    ftdi_free(timer->context);
    timer->context = NULL;
#else
    FT_Close(timer->handle);
#endif
}

static void log_raw_data(unsigned char *data, int len)
{
    char *msg = (char *)malloc((3*len+1)*sizeof(char));
    if (msg == NULL)
    {
        pn_log("ERROR: Memory allocation error in log_raw_data. Ignoring log message");
        return;
    }

    for (unsigned char i = 0; i < len; i++)
        snprintf(msg+3*i, 4, "%02x ", data[i]);
    pn_log(msg);
    free(msg);
}

static void write_data(TimerUnit *timer)
{
    pthread_mutex_lock(&timer->sendbuffer_mutex);
    if (timer->send_length > 0)
    {
#ifdef USE_LIBFTDI
        if (ftdi_write_data(timer->context, timer->send_buffer, timer->send_length) == timer->send_length)
            timer->send_length = 0;
        else
            pn_log("Error sending send buffer");
#else
        DWORD bytes_written;
        FT_STATUS status = FT_Write(timer->handle, timer->send_buffer, timer->send_length, &bytes_written);
        if (status == FT_OK && bytes_written == timer->send_length)
            timer->send_length = 0;
        else
            pn_log("Error sending timer buffer");
#endif
    }
    pthread_mutex_unlock(&timer->sendbuffer_mutex);
}

static void read_data(TimerUnit *timer, uint8_t read_buffer[256], uint8_t *bytes_read)
{
#ifdef USE_LIBFTDI
    *bytes_read = ftdi_read_data(timer->context, read_buffer, 255);
    if (bytes_read < 0)
        fatal_timer_error(timer, "Timer I/O error");
#else
    DWORD read;
    if (FT_Read(timer->handle, read_buffer, 255, &read) != FT_OK)
        fatal_timer_error(timer, "Timer I/O error");

    *bytes_read = (uint8_t)read;
#endif
}

static void parse_packet(TimerUnit *timer, uint8_t *packet, uint8_t packet_length)
{
    uint8_t packet_type = packet[2];
    uint8_t data_length = packet[3];
    uint8_t *data = &packet[4];
    uint8_t data_checksum = packet[packet_length - 3];

    // Check that the packet ends correctly
    if (packet[packet_length - 2] != '\r' || packet[packet_length - 1] != '\n')
    {
        pn_log("Invalid packet length, expected %d", packet_length);
        log_raw_data(packet, packet_length);
        return;
    }

    // Verify packet checksum
    unsigned char csm = checksum(data, data_length);
    if (csm != data_checksum)
    {
        pn_log("Invalid packet checksum. Got 0x%02x, expected 0x%02x", csm, data_checksum);
        return;
    }

    // Handle packet
    switch (packet_type)
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
            };
            pthread_mutex_unlock(&timer->read_mutex);
            break;
        }
        case DOWNLOADTIME:
        {
            TimerTimestamp *t = malloc(sizeof(TimerTimestamp));
            if (!t)
            {
                pn_log("Error allocating TimerTimestamp. Discarding trigger");
                break;
            }

            t->year = (data[5] & 0x00FF) | ((data[6] << 8) & 0xFF00);
            t->month = data[4];
            t->day = data[3];
            t->hours = data[0];
            t->minutes = data[1];
            t->seconds = data[2];
            t->locked = data[7];
            t->remaining_exposure = 0;
            pn_log("Trigger: %04d-%02d-%02d %02d:%02d:%02d (%d)", t->year, t->month, t->day, t->hours, t->minutes, t->seconds, t->locked);

            // Pass ownership to main thread
            queue_trigger(t);

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
}

// Main timer thread loop
void *pn_timer_thread(void *_args)
{
    ThreadCreationArgs *args = (ThreadCreationArgs *)_args;
    TimerUnit *timer = args->timer;

    // Store received bytes in a 256 byte circular buffer indexed by an unsigned char
    // This ensures the correct circular behavior on over/underflow
    uint8_t input_buf[256];
    uint8_t write_index = 0;
    uint8_t read_index = 0;

    // Current packet storage
    uint8_t packet[256];
    uint8_t packet_length = 0;
    uint8_t packet_expected_length = 0;
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
        write_data(timer);

        if (timer->shutdown)
            break;

        // Check for new data
        uint8_t bytes_read;
        uint8_t read_buffer[256];
        read_data(timer, read_buffer, &bytes_read);

        // Copy received bytes into the circular input buffer
        for (uint8_t i = 0; i < bytes_read; i++)
            input_buf[write_index++] = read_buffer[i];

        // Sync to the start of a data packet
        for (; packet_type == UNKNOWN_PACKET && read_index != write_index; read_index++)
        {
            if (input_buf[(uint8_t)(read_index - 3)] == '$' &&
                input_buf[(uint8_t)(read_index - 2)] == '$' &&
                input_buf[(uint8_t)(read_index - 1)] != '$')
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

        // Packet ready to parse
        if (packet_length == packet_expected_length && packet_type != UNKNOWN_PACKET)
        {
            parse_packet(timer, packet, packet_length);
            packet_length = 0;
            packet_expected_length = 0;
            packet_type = UNKNOWN_PACKET;
        }
    }

    // Uninitialize the timer connection and exit
    uninitialize_timer(timer);
    return NULL;
}

// Main simulated timer thread loop
void *pn_simulated_timer_thread(void *_args)
{
    ThreadCreationArgs *args = (ThreadCreationArgs *)_args;
    TimerUnit *timer = args->timer;

    // Initialization
    pn_log("Simulating GPS");
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

                TimerTimestamp *t = malloc(sizeof(TimerTimestamp));
                if (!t)
                {
                    pn_log("Error allocating TimerTimestamp. Discarding trigger");
                    break;
                }

                t->year = pc_time->tm_year + 1900;
                t->month = pc_time->tm_mon;
                t->day = pc_time->tm_wday;
                t->hours = pc_time->tm_hour;
                t->minutes = pc_time->tm_min;
                t->seconds = pc_time->tm_sec;
                t->locked = true;
                t->remaining_exposure = 0;
                pn_log("Simulated Trigger: %04d-%02d-%02d %02d:%02d:%02d (%d)", t->year, t->month, t->day, t->hours, t->minutes, t->seconds, t->locked);

                // Pass ownership to main thread
                queue_trigger(t);

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
            };

            pthread_mutex_unlock(&timer->read_mutex);

            last_unixtime = cur_unixtime;
        }
    }

    pn_log("Closing simulated GPS");
    return NULL;
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

void timer_shutdown(TimerUnit *timer)
{
    timer->shutdown = true;
    void **retval = NULL;
    if (timer->thread_initialized)
        pthread_join(timer->timer_thread, retval);

    timer->thread_initialized = false;
}

// Callback to notify that the simulated camera has read out
// TODO: This is a crappy abstraction
void timer_set_simulated_camera_downloading(TimerUnit *timer, bool downloading)
{
    pthread_mutex_lock(&timer->read_mutex);
    timer->camera_downloading = downloading;
    pthread_mutex_unlock(&timer->read_mutex);
}

// Ensure all time components are within their allowed range
void timestamp_normalize(TimerTimestamp *ts)
{
    // Let gmtime/timegm do the hard work of normalizing the time
    struct tm a = {ts->seconds, ts->minutes, ts->hours, ts->day, ts->month, ts->year - 1900,0,0,0};
    normalize_tm(&a);

    // Construct a new timestamp to return
    ts->seconds = a.tm_sec;
    ts->minutes = a.tm_min;
    ts->hours = a.tm_hour;
    ts->day = a.tm_mday;
    ts->month = a.tm_mon;
    ts->year = a.tm_year + 1900;
}

time_t timestamp_to_time_t(TimerTimestamp *ts)
{
    struct tm t = {ts->seconds, ts->minutes, ts->hours, ts->day, ts->month, ts->year - 1900,0,0,0};
    return struct_tm_to_time_t(&t);
}