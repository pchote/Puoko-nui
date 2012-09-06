/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <time.h>
#include <stdbool.h>
#include <pthread.h>

#ifndef GPS_H
#define GPS_H

// Datalink escape byte
#define DLE 0x10
// End of text byte
#define ETX 0x03

// GPS command types
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
} PNGPSPacketType;

// Represents a timestamp from the GPS
typedef struct
{
    int year;
    int month;
    int day;
    int hours;
    int minutes;
    int seconds;
    bool locked;
    int remaining_exposure; // for current time
    bool valid; // true before initialisation and if the download time has been used
} PNGPSTimestamp;

// Represents the GPS hardware
typedef struct
{
    bool simulated;
    int simulated_exptime;
    int simulated_remaining;
    char *fatal_error;

    struct usb_device *device;
    struct ftdi_context *context;
    bool shutdown;
    PNGPSTimestamp current_timestamp;
    bool camera_downloading;
    pthread_mutex_t read_mutex;

    unsigned char send_buffer[256];
    unsigned char send_length;
    pthread_mutex_t sendbuffer_mutex;
} PNGPS;

PNGPS pn_gps_new();
void pn_gps_free(PNGPS *gps);
void *pn_timer_thread(void *);
void *pn_simulated_timer_thread(void *);

void pn_gps_start_exposure(unsigned char exptime);
void pn_gps_stop_exposure();
bool pn_gps_camera_downloading();

void pn_gps_set_simulated_camera_downloading(bool downloading);
PNGPSTimestamp pn_timestamp_normalize(PNGPSTimestamp ts);

#endif
