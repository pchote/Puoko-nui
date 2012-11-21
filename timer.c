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
#include <sys/time.h>
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
    uint16_t simulated_total;
    uint16_t simulated_progress;
    bool simulated_send_shutdown;

#ifdef USE_LIBFTDI
    struct ftdi_context *context;
#else
    FT_HANDLE handle;
#endif

    bool shutdown;
    TimerTimestamp current_timestamp;
    TimerMode mode;

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
    STATUSMODE = 'H',
    SIMULATE_CAMERA = 'I',
    UNKNOWN_PACKET = 0
} TimerUnitPacketType;


void *timer_thread(void *timer);
void *simulated_timer_thread(void *args);
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

void timer_free(TimerUnit *timer)
{
    pthread_mutex_destroy(&timer->read_mutex);
    pthread_mutex_destroy(&timer->sendbuffer_mutex);
}

void timer_spawn_thread(TimerUnit *timer, ThreadCreationArgs *args)
{
    if (timer->simulated)
        pthread_create(&timer->timer_thread, NULL, simulated_timer_thread, (void *)args);
    else
        pthread_create(&timer->timer_thread, NULL, timer_thread, (void *)args);

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
        pn_log("Failed to allocate memory for fatal error.");

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
    // Data to send timer on startup to exit
    // relay / upgrade mode and purge input buffer
    const uint16_t reset_data_length = 257;
    uint8_t reset_data[257];
    memset(reset_data, 0, reset_data_length);
    reset_data[0] = 'E';

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

    if (timer->shutdown)
        return;

    if (ftdi_set_baudrate(timer->context, pn_preference_int(TIMER_BAUD_RATE)) < 0)
        fatal_timer_error(timer, "Error setting timer baudrate: %s", timer->context->error_str);

    if (ftdi_set_line_property(timer->context, BITS_8, STOP_BIT_1, NONE) < 0)
        fatal_timer_error(timer, "Error setting timer data frame properties: %s", timer->context->error_str);

    if (ftdi_setflowctrl(timer->context, SIO_DISABLE_FLOW_CTRL) < 0)
        fatal_timer_error(timer, "Error setting timer flow control: %s", timer->context->error_str);

    // the latency in milliseconds before partially full bit buffers are sent.
    if (ftdi_set_latency_timer(timer->context, 1) < 0)
        fatal_timer_error(timer, "Error setting timer read timeout: %s", timer->context->error_str);

    // Purge any data in the chip receive buffer
    if (ftdi_usb_purge_rx_buffer(timer->context))
        fatal_timer_error(timer, "Error purging timer rx buffer: %s", timer->context->error_str);

    // Send reset data
    if (ftdi_write_data(timer->context, reset_data, reset_data_length) != reset_data_length)
        fatal_timer_error(timer, "Error sending reset data: %s", timer->context->error_str);

    // Purge any data in the chip transmit buffer
    if (ftdi_usb_purge_tx_buffer(timer->context))
        fatal_timer_error(timer, "Error purging timer tx buffer: %s", timer->context->error_str);

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

    if (timer->shutdown)
        return;

    if (FT_SetBaudRate(timer->handle, pn_preference_int(TIMER_BAUD_RATE)) != FT_OK)
        fatal_timer_error(timer, "Error setting timer baudrate");

    // Set data frame: 8N1
    if (FT_SetDataCharacteristics(timer->handle, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE) != FT_OK)
        fatal_timer_error(timer, "Error setting timer data characteristics");

    // Set read timeout to 1ms, write timeout unchanged
    if (FT_SetTimeouts(timer->handle, 1, 0) != FT_OK)
        fatal_timer_error(timer, "Error setting timer read timeout");

    // Purge any data in the chip receive buffer
    if (FT_Purge(timer->handle, FT_PURGE_RX) != FT_OK)
        fatal_timer_error(timer, "Error purging timer buffers");

    // Send reset data
    DWORD bytes_written;
    FT_STATUS status = FT_Write(timer->handle, reset_data, reset_data_length, &bytes_written);
    if (status != FT_OK || bytes_written != reset_data_length)
        fatal_timer_error(timer, "Error sending reset data");

    // Purge any data in the chip transmit buffer
    if (FT_Purge(timer->handle, FT_PURGE_TX) != FT_OK)
        fatal_timer_error(timer, "Error purging timer buffers");

#endif

    queue_data(timer, RESET, NULL, 0);
    pn_log("Timer is now active.");
}

// Close the usb connection to the timer
static void uninitialize_timer(TimerUnit *timer)
{
    pn_log("Shutting down timer.");
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
    char *msg = (char *)malloc((3*len+7)*sizeof(char));
    strcpy(msg, "Data: ");
    if (msg == NULL)
    {
        pn_log("Failed to allocate log_raw_data. Ignoring message");
        return;
    }
    for (unsigned char i = 0; i < len; i++)
        snprintf(&msg[3*i + 6], 4, "%02x ", data[i]);

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
            pn_log("Failed to send buffered data.");
#else
        DWORD bytes_written;
        FT_STATUS status = FT_Write(timer->handle, timer->send_buffer, timer->send_length, &bytes_written);
        if (status == FT_OK && bytes_written == timer->send_length)
            timer->send_length = 0;
        else
            pn_log("Failed to send buffered data.");
#endif
    }
    pthread_mutex_unlock(&timer->sendbuffer_mutex);
}

static void read_data(TimerUnit *timer, uint8_t *read_buffer, uint8_t *bytes_read)
{
#ifdef USE_LIBFTDI
    int read = ftdi_read_data(timer->context, read_buffer, 255);
    if (read < 0)
        fatal_timer_error(timer, "Timer I/O error.");
#else
    DWORD read;
    if (FT_Read(timer->handle, read_buffer, 255, &read) != FT_OK)
        fatal_timer_error(timer, "Timer I/O error.");
#endif
    *bytes_read = (uint8_t)read;
}

static void parse_packet(TimerUnit *timer, Camera *camera, uint8_t *packet, uint8_t packet_length)
{
    uint8_t packet_type = packet[2];
    uint8_t data_length = packet[3];
    uint8_t *data = &packet[4];
    uint8_t data_checksum = packet[packet_length - 3];

    // Check that the packet ends correctly
    if (packet[packet_length - 2] != '\r' || packet[packet_length - 1] != '\n')
    {
        pn_log("Invalid packet length, expected %d.", packet_length);
        log_raw_data(packet, packet_length);
        return;
    }

    // Verify packet checksum
    unsigned char csm = checksum(data, data_length);
    if (csm != data_checksum)
    {
        pn_log("Invalid packet checksum. Got 0x%02x, expected 0x%02x.", csm, data_checksum);
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
                .year = data[0] | data[1] << 8,
                .month = data[2],
                .day = data[3],
                .hours = data[4],
                .minutes = data[5],
                .seconds = data[6],
                .milliseconds = data[7] | data[8] << 8,
                .locked = data[9],
                .exposure_progress = data[10] | data[11] << 8
            };
            pthread_mutex_unlock(&timer->read_mutex);
            break;
        }
        case DOWNLOADTIME:
        {
            TimerTimestamp *t = malloc(sizeof(TimerTimestamp));
            if (!t)
            {
                pn_log("Failed to allocate TimerTimestamp. Discarding trigger");
                break;
            }

            t->year = data[0] | data[1] << 8;
            t->month = data[2];
            t->day = data[3];
            t->hours = data[4];
            t->minutes = data[5];
            t->seconds = data[6];
            t->milliseconds = data[7] | data[8] << 8;
            t->locked = data[9];
            t->exposure_progress = 0;

            // The timer sends unnormalized timestamps, where milliseconds may
            // be greater than 1000.
            timestamp_normalize(t);

            // Update current time
            pthread_mutex_lock(&timer->read_mutex);
            timer->current_timestamp = *t;
            pthread_mutex_unlock(&timer->read_mutex);

            // Pass ownership to main thread
            queue_trigger(t);
            break;
        }
        case STATUSMODE:
        {
            TimerMode mode = (data_length == 0) ? TIMER_EXPOSING : data[0];
            pthread_mutex_lock(&timer->read_mutex);
            timer->mode = mode;
            pthread_mutex_unlock(&timer->read_mutex);
            break;
        }
        case DEBUG_STRING:
        {
            packet[packet_length - 3] = '\0';
            pn_log("Timer Debug: `%s`.", data);
            break;
        }
        case DEBUG_RAW:
        {
            log_raw_data(data, data_length);
            break;
        }
        case STOP_EXPOSURE:
        {
            pn_log("Timer reports camera ready to stop sequence.");
            camera_notify_safe_to_stop(camera);
            break;
        }
        default:
        {
            pn_log("Unknown packet type %02x.", packet_type);
        }
    }
}

// Main timer thread loop
static void timer_loop(TimerUnit *timer, Camera *camera)
{
    // Store received bytes in a 256 byte circular buffer indexed by an unsigned char
    // This ensures the correct circular behavior on over/underflow
    uint8_t input_buf[256];
    memset(input_buf, 0, 256);
    uint8_t write_index = 0;
    uint8_t read_index = 0;

    // Current packet storage
    uint8_t packet[256];
    memset(packet, 0, 256);
    uint8_t packet_length = 0;
    uint8_t packet_expected_length = 0;
    TimerUnitPacketType packet_type = UNKNOWN_PACKET;

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
            return;

        // Check for new data
        uint8_t bytes_read;
        uint8_t read_buffer[256];
        read_data(timer, read_buffer, &bytes_read);

        // Copy received bytes into the circular input buffer
        for (uint8_t i = 0; i < bytes_read; i++)
            input_buf[write_index++] = read_buffer[i];

        while (read_index != write_index)
        {
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
                parse_packet(timer, camera, packet, packet_length);
                packet_length = 0;
                packet_expected_length = 0;
                packet_type = UNKNOWN_PACKET;
            }
        }
    }
}

// Main timer thread loop
void *timer_thread(void *_args)
{
    ThreadCreationArgs *args = (ThreadCreationArgs *)_args;
    TimerUnit *timer = args->timer;

    initialize_timer(timer);
    if (!timer->shutdown)
        timer_loop(timer, args->camera);

    uninitialize_timer(timer);
    return NULL;
}

// Main simulated timer thread loop
void *simulated_timer_thread(void *_args)
{
    ThreadCreationArgs *args = (ThreadCreationArgs *)_args;
    TimerUnit *timer = args->timer;
    Camera *camera = args->camera;

    // Initialization
    pn_log("Initializing simulated Timer.");
    timer->simulated_progress = timer->simulated_total = 0;

    TimerTimestamp last = system_time();

    // Loop until shutdown, parsing incoming data
    while (!timer->shutdown)
    {
        millisleep(1);

        pthread_mutex_lock(&timer->read_mutex);
        bool send_shutdown = timer->simulated_send_shutdown;
        timer->simulated_send_shutdown = false;
        pthread_mutex_unlock(&timer->read_mutex);
        if (send_shutdown)
        {
            timer->simulated_total = 0;
            timer->simulated_progress = 0;
            camera_notify_safe_to_stop(camera);
        }

        TimerTimestamp cur = system_time();

        if (cur.seconds != last.seconds ||
            cur.milliseconds != last.milliseconds)
        {
            if (timer->simulated_total > 0)
            {
                if (pn_preference_char(TIMER_MILLISECOND_MODE))
                    timer->simulated_progress += (cur.seconds - last.seconds)*1000 + (cur.milliseconds - last.milliseconds);
                else
                    timer->simulated_progress += cur.seconds - last.seconds;
            }

            if (timer->simulated_progress >= timer->simulated_total && timer->simulated_total > 0)
            {
                timer->simulated_progress -= timer->simulated_total;

                TimerTimestamp *t = malloc(sizeof(TimerTimestamp));
                if (!t)
                {
                    pn_log("Error allocating TimerTimestamp. Discarding trigger");
                    break;
                }

                *t = cur;

                // Pass ownership to main thread
                queue_trigger(t);

                pthread_mutex_lock(&timer->read_mutex);
                timer->mode = TIMER_READOUT;
                pthread_mutex_unlock(&timer->read_mutex);
            }

            cur.exposure_progress = timer->simulated_progress;
            pthread_mutex_lock(&timer->read_mutex);
            timer->current_timestamp = cur;
            pthread_mutex_unlock(&timer->read_mutex);

            last = cur;
        }
    }

    pn_log("Simulated Timer shutdown.");
    return NULL;
}

#pragma mark Timer communication Routines (Called from any thread)

// Start an exposure sequence with a specified exposure time
void timer_start_exposure(TimerUnit *timer, uint16_t exptime, bool use_monitor)
{
    pn_log("Starting %d %s exposures.", exptime,
           pn_preference_char(TIMER_MILLISECOND_MODE) ? "ms" : "s");

    if (timer->simulated)
    {
        pthread_mutex_lock(&timer->read_mutex);
        timer->simulated_progress = 0;
        timer->simulated_total = exptime;
        timer->mode = TIMER_EXPOSING;
        pthread_mutex_unlock(&timer->read_mutex);
    }
    else
    {
        if (!use_monitor)
            pn_log("WARNING: Timer monitor is disabled.");

        uint8_t simulate_camera = !use_monitor;
        uint8_t data[2] = {exptime & 0xFF, (exptime >> 8) & 0xFF};

        queue_data(timer, SIMULATE_CAMERA, &simulate_camera, 1);
        queue_data(timer, START_EXPOSURE, data, 2);
    }
}

// Stop the current exposure sequence
void timer_stop_exposure(TimerUnit *timer)
{
    pn_log("Stopping exposures.");
    if (timer->simulated)
    {
        pthread_mutex_lock(&timer->read_mutex);
        timer->simulated_send_shutdown = true;
        timer->mode = TIMER_IDLE;
        pthread_mutex_unlock(&timer->read_mutex);
    }
    else
        queue_data(timer, STOP_EXPOSURE, NULL, 0);
}

TimerMode timer_mode(TimerUnit *timer)
{
    pthread_mutex_lock(&timer->read_mutex);
    TimerMode mode = timer->mode;
    pthread_mutex_unlock(&timer->read_mutex);
    return mode;
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
    timer->mode = downloading ? TIMER_READOUT : TIMER_EXPOSING;
    pthread_mutex_unlock(&timer->read_mutex);
}

// Ensure all time components are within their allowed range
void timestamp_normalize(TimerTimestamp *ts)
{
    // Normalize milliseconds manually
    while (ts->milliseconds < 0)
    {
        ts->milliseconds += 1000;
        ts->seconds--;
    }

    // Let gmtime/timegm normalize the rest
    struct tm a = {ts->seconds + ts->milliseconds / 1000, ts->minutes, ts->hours, ts->day, ts->month, ts->year - 1900,0,0,0};
    normalize_tm(&a);

    // Construct a new timestamp to return
    ts->milliseconds = ts->milliseconds % 1000;
    ts->seconds = a.tm_sec;
    ts->minutes = a.tm_min;
    ts->hours = a.tm_hour;
    ts->day = a.tm_mday;
    ts->month = a.tm_mon;
    ts->year = a.tm_year + 1900;
}

double timestamp_to_unixtime(TimerTimestamp *ts)
{
    struct tm t = {ts->seconds, ts->minutes, ts->hours, ts->day, ts->month, ts->year - 1900,0,0,0};
    return struct_tm_to_time_t(&t) + ts->milliseconds / 1000.0;
}