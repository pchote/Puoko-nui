/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "serial.h"

#ifdef _WIN32
#   include <windows.h>
#else
#   include <sys/time.h>
#endif

static void millisleep(int ms)
{
#if (defined _WIN32 || defined _WIN64)
    Sleep(ms);
#else
    nanosleep(&(struct timespec){ms / 1000, (ms % 1000)*1e6}, NULL);
#endif
}

int send_config_string(const char *device, uint32_t baud, char *str, size_t len)
{
    ssize_t error;
    struct serial_port *port = serial_new(device, baud, &error);
    if (!port)
    {
        printf("Timer error %zd: %s\n", error, serial_error_string(error));
        return 1;
    }

    // Reset twice to cancel relay mode if it was activated
    serial_set_dtr(port, true);
    millisleep(100);
    serial_set_dtr(port, false);
    millisleep(100);
    serial_set_dtr(port, true);
    millisleep(100);
    serial_set_dtr(port, false);

    // Wait for bootloader to timeout
    millisleep(5000);

    // Send synchronization packet
    if ((error = serial_write(port, (uint8_t *)"$$S\x00\x00\r\n", 7)) < 0)
    {
        printf("Write error %zd: %s\n", error, serial_error_string(error));
        return 1;
    }

    if (str && (error = serial_write(port, (uint8_t *)str, len)) < 0)
    {
        printf("Write error %zd: %s\n", error, serial_error_string(error));
        return 1;
    }

    serial_free(port);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc == 3)
    {
        const char *device = argv[1];
        uint32_t baud = 9600;

        if (strcmp(argv[2], "relay") == 0)
            return send_config_string(device, baud, "$$R\x00\x00\r\n", 7);

        if (strcmp(argv[2], "reset") == 0)
            return send_config_string(device, baud, NULL, 0);

        printf("Unknown mode: %s\n", argv[2]);
    }

    printf("Example usage:\n");
    printf("  timerutil <port> relay\n");
    printf("  timerutil <port> reset\n");

    return 1;
}
