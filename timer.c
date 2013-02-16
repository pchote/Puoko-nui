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
#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include "timer.h"
#include "main.h"
#include "preferences.h"
#include "platform.h"
#include "camera.h"
#include "serial.h"


// Timer message protocol definitions
#define MAX_DATA_LENGTH 200
enum packet_state {HEADERA = 0, HEADERB, TYPE, LENGTH, DATA, CHECKSUM, FOOTERA, FOOTERB};
enum packet_type
{
    TIMESTAMP = 'A',
    TRIGGER = 'B',
    MESSAGE = 'C',
    MESSAGE_RAW = 'D',
    START_EXPOSURE = 'E',
    STOP_EXPOSURE = 'F',
    STATUS = 'H',
    ENABLE_RELAY = 'R',
};

struct __attribute__((__packed__)) packet_time
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint16_t milliseconds;
    uint8_t locked;
    uint16_t exposure_progress;
};

enum __attribute__((__packed__)) packet_timingmode {TIME_SECONDS, TIME_MILLISECONDS};
struct __attribute__((__packed__)) packet_startexposure
{
    uint8_t use_monitor;
    enum packet_timingmode timing_mode;
    uint16_t exposure;
};

struct __attribute__((__packed__)) packet_status
{
    // Mirrors (unpacked) TimerMode enum
    uint8_t timer;

    // Mirrors (unpacked) TimerGPSStatus enum
    uint8_t gps;
};

struct __attribute__((__packed__)) packet_message
{
    uint8_t length;
    char str[MAX_DATA_LENGTH-1];
};

struct timer_packet
{
    enum packet_state state;
    enum packet_type type;
    uint8_t length;
    uint8_t progress;
    uint8_t checksum;

    union
    {
        // Extra byte allows us to always null-terminate strings for display
        uint8_t bytes[MAX_DATA_LENGTH+1];
        struct packet_time time;
        struct packet_status status;
        struct packet_message message;
    } data;
};

// Private struct implementation
struct TimerUnit
{
    pthread_t timer_thread;
    bool thread_alive;

    bool simulated;
    uint16_t simulated_total;
    uint16_t simulated_progress;
    bool simulated_send_shutdown;

    struct serial_port *port;

    bool shutdown;
    TimerTimestamp current_timestamp;
    TimerMode mode;
    TimerGPSStatus gps_status;

    unsigned char send_buffer[256];
    unsigned char send_length;
    pthread_mutex_t read_mutex;
    pthread_mutex_t write_mutex;
};

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
    pthread_mutex_init(&timer->write_mutex, NULL);

    return timer;
}

void timer_free(TimerUnit *timer)
{
    pthread_mutex_destroy(&timer->read_mutex);
    pthread_mutex_destroy(&timer->write_mutex);
    free(timer);
}

void timer_spawn_thread(TimerUnit *timer, ThreadCreationArgs *args)
{
    void *(*start_routine)(void *) = timer->simulated ? simulated_timer_thread : timer_thread;

    timer->thread_alive = true;
    if (pthread_create(&timer->timer_thread, NULL, start_routine, (void *)args))
    {
        pn_log("Failed to create timer thread");
        timer->thread_alive = false;
    }
}

void timer_join_thread(TimerUnit *timer)
{
    void **retval = NULL;
    if (timer->thread_alive)
        pthread_join(timer->timer_thread, retval);
}

void timer_notify_shutdown(TimerUnit *timer)
{
    timer->shutdown = true;
}

bool timer_thread_alive(TimerUnit *timer)
{
    return timer->thread_alive;
}

#pragma mark Timer Routines (Called from Timer thread)

// Queue a raw byte to be sent to the timer
// Should only be called by queue_data
static void queue_send_byte(TimerUnit *timer, unsigned char b)
{
    // Hard-loop until the send buffer empties
    // Should never happen in normal operation
    while (timer->send_length >= 255);

    pthread_mutex_lock(&timer->write_mutex);
    timer->send_buffer[timer->send_length++] = b;
    pthread_mutex_unlock(&timer->write_mutex);
}

// Wrap an array of bytes in a data packet and send it to the timer
static void queue_data(TimerUnit *timer, enum packet_type type, void *data, uint8_t length)
{
    // Data packet starts with $$ followed by packet type
    queue_send_byte(timer, '$');
    queue_send_byte(timer, '$');
    queue_send_byte(timer, type);

    // Length of data section
    queue_send_byte(timer, length);

    // Packet data
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < length; i++)
    {
        checksum ^= ((uint8_t *)data)[i];
        queue_send_byte(timer, ((uint8_t *)data)[i]);
    }

    // Checksum
    queue_send_byte(timer, checksum);

    // Data packet ends with linefeed and carriage return
    queue_send_byte(timer, '\r');
    queue_send_byte(timer, '\n');
}

static void unpack_timestamp(struct packet_time *pt, TimerTimestamp *tt)
{
    tt->year = pt->year;
    tt->month = pt->month;
    tt->day = pt->day;
    tt->hours = pt->hours;
    tt->minutes = pt->minutes;
    tt->seconds = pt->seconds;
    tt->milliseconds = pt->milliseconds;
    tt->locked = pt->locked;
    tt->exposure_progress = pt->exposure_progress;
}

static void parse_packet(TimerUnit *timer, Camera *camera, struct timer_packet *p)
{
    // Handle packet
    switch (p->type)
    {
        case TIMESTAMP:
            pthread_mutex_lock(&timer->read_mutex);
            unpack_timestamp(&p->data.time, &timer->current_timestamp);
            pthread_mutex_unlock(&timer->read_mutex);
            break;
        case TRIGGER:
        {
            TimerTimestamp *t = malloc(sizeof(TimerTimestamp));
            if (!t)
            {
                pn_log("Failed to allocate TimerTimestamp. Discarding trigger");
                break;
            }

            p->data.time.exposure_progress = 0;
            unpack_timestamp(&p->data.time, t);

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
        case STATUS:
            if (timer->gps_status != p->data.status.gps)
            {
                switch (p->data.status.gps)
                {
                case GPS_UNAVAILABLE:
                    pn_log("Timer: GPS Serial lost.");
                    break;
                case GPS_SYNCING:
                    pn_log("Timer: GPS Serial syncing.");
                    break;
                case GPS_ACTIVE:
                    pn_log("Timer: GPS Serial active.");
                    break;
                }
            }
            pthread_mutex_lock(&timer->read_mutex);
            timer->mode = p->data.status.timer;
            timer->gps_status = p->data.status.gps;
            pthread_mutex_unlock(&timer->read_mutex);
            break;
        case MESSAGE:
            p->data.message.str[p->data.message.length] = '\0';
            pn_log("Timer: %s", p->data.message.str);
            break;
        case MESSAGE_RAW:
        {
            char *msg = (char *)malloc((3*p->data.message.length+7)*sizeof(char));
            if (!msg)
            {
                pn_log("Timer warning: Failed to allocate log_raw_data. Ignoring message");
                break;
            }

            strcpy(msg, "Data: ");
            for (uint8_t i = 0; i < p->data.message.length; i++)
                snprintf(&msg[3*i + 6], 4, "%02x ", (uint8_t)p->data.message.str[i]);

            pn_log(msg);
            free(msg);
            break;
        }
        case STOP_EXPOSURE:
            pn_log("Timer reports camera ready to stop sequence.");
            camera_notify_safe_to_stop(camera);
            break;
        default:
            pn_log("Unknown packet type: %c", p->type);
    }
}

// Main timer thread loop
void *timer_thread(void *_args)
{
    ThreadCreationArgs *args = (ThreadCreationArgs *)_args;
    TimerUnit *timer = args->timer;

    // Opening the serial port triggers a hardware reset
    char *port_path = pn_preference_string(TIMER_SERIAL_PORT);
    int port_baud = pn_preference_int(TIMER_BAUD_RATE);
    pn_log("Initializing timer at %s with %d baud", port_path, port_baud);

    ssize_t error;
    struct serial_port *port = serial_new(port_path, port_baud, &error);
    free(port_path);

    if (!port)
    {
        pn_log("Timer initialization error (%zd): %s", error, serial_error_string(error));
        goto serial_error;
    }

    // Reset twice - the first reset may put the unit into relay mode
    serial_set_dtr(port, true);
    millisleep(100);
    serial_set_dtr(port, false);
    millisleep(100);
    serial_set_dtr(port, true);
    millisleep(100);
    serial_set_dtr(port, false);

    // Clear any buffered data from before the reset
    while (serial_read(port, &(uint8_t){0}, 1) > 0);

    // Wait for bootloader timeout
    millisleep(1000);

    struct timer_packet p = (struct timer_packet){.state = HEADERA};

    while (!timer->shutdown)
    {
        // Send any queued data
        pthread_mutex_lock(&timer->write_mutex);
        if (timer->send_length > 0)
        {
            ssize_t ret = serial_write(port, timer->send_buffer, timer->send_length);
            if (ret < 0)
            {
                pn_log("Timer write error (%zd): %s", ret, serial_error_string(ret));
                pthread_mutex_unlock(&timer->write_mutex);
                break;
            }

            if (ret != timer->send_length)
            {
                pn_log("Timer write error: only %zu of %u bytes written", ret, timer->send_length);
                pthread_mutex_unlock(&timer->write_mutex);
                break;
            }

            timer->send_length = 0;
        }
        pthread_mutex_unlock(&timer->write_mutex);

        // Check for new data
        uint8_t b;
        ssize_t status;
        while ((status = serial_read(port, &b, 1)))
        {
            if (status < 0)
            {
                pn_log("Timer read error (%zd): %s", status, serial_error_string(status));
                goto error;
            }

            switch (p.state)
            {
                case HEADERA:
                case HEADERB:
                    if (b == '$')
                        p.state++;
                    else
                        p.state = HEADERA;
                    break;
                case TYPE:
                    p.type = b;
                    p.state++;
                    break;
                case LENGTH:
                    p.length = b;
                    p.progress = 0;
                    p.checksum = 0;
                    if (p.length == 0)
                        p.state = CHECKSUM;
                    else if (p.length <= sizeof(p.data))
                        p.state++;
                    else
                    {
                        pn_log("Timer warning: ignoring long packet: %c (length %u)", p.type, p.length);
                        p.state = HEADERA;
                    }
                    break;
                case DATA:
                    p.checksum ^= b;
                    p.data.bytes[p.progress++] = b;
                    if (p.progress == p.length)
                        p.state++;
                    break;
                case CHECKSUM:
                    if (p.checksum == b)
                        p.state++;
                    else
                    {
                        pn_log("Timer warning: Packet checksum failed. Got 0x%02x, expected 0x%02x.", b, p.checksum);
                        p.state = HEADERA;
                    }
                    break;
                case FOOTERA:
                    if (b == '\r')
                        p.state++;
                    else
                    {
                        pn_log("Timer warning: Invalid packet end byte. Got 0x%02x, expected 0x%02x.", b, '\r');
                        p.state = HEADERA;
                    }
                    break;
                case FOOTERB:
                    if (b == '\n')
                        parse_packet(timer, args->camera, &p);
                    else
                        pn_log("Timer warning: Invalid packet end byte. Got 0x%02x, expected 0x%02x.", b, '\n');

                    p.state = HEADERA;
                    break;
            }
        }

        millisleep(100);
    }

error:
    pn_log("Shutting down timer.");
    serial_free(port);
serial_error:

    // Invalidate current time
    pthread_mutex_lock(&timer->read_mutex);
    timer->mode = TIMER_IDLE;
    pthread_mutex_unlock(&timer->read_mutex);

    timer->thread_alive = false;
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
    timer->gps_status = GPS_ACTIVE;

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
                if (pn_preference_char(TIMER_HIGHRES_TIMING))
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

    // Invalidate current time
    pthread_mutex_lock(&timer->read_mutex);
    timer->mode = TIMER_IDLE;
    pthread_mutex_unlock(&timer->read_mutex);

    timer->thread_alive = false;
    return NULL;
}

#pragma mark Timer communication Routines (Called from any thread)

// Start an exposure sequence with a specified exposure time
void timer_start_exposure(TimerUnit *timer, uint16_t exptime, bool use_monitor)
{
    bool highres = pn_preference_char(TIMER_HIGHRES_TIMING);
    pn_log("Starting %d %s exposures.", exptime, highres ? "ms" : "s");

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

        struct packet_startexposure data = (struct packet_startexposure)
        {
            .use_monitor = use_monitor,
            .timing_mode = highres ? TIME_MILLISECONDS : TIME_SECONDS,
            .exposure = exptime,
        };

        queue_data(timer, START_EXPOSURE, &data, sizeof(struct packet_startexposure));
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

TimerGPSStatus timer_gps_status(TimerUnit *timer)
{
    // gps_status is atomic, so no need for a mutex
    return timer->gps_status;
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
    struct tm a = {ts->seconds + ts->milliseconds / 1000, ts->minutes, ts->hours, ts->day, ts->month - 1, ts->year - 1900, 0, 0, 0};
    normalize_tm(&a);

    // Construct a new timestamp to return
    ts->milliseconds = ts->milliseconds % 1000;
    ts->seconds = a.tm_sec;
    ts->minutes = a.tm_min;
    ts->hours = a.tm_hour;
    ts->day = a.tm_mday;
    ts->month = a.tm_mon + 1;
    ts->year = a.tm_year + 1900;
}

double timestamp_to_unixtime(TimerTimestamp *ts)
{
    struct tm t = {ts->seconds, ts->minutes, ts->hours, ts->day, ts->month, ts->year - 1900,0,0,0};
    return struct_tm_to_time_t(&t) + ts->milliseconds / 1000.0;
}
