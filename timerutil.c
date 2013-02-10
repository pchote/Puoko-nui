/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
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

int main(int argc, char *argv[])
{
    if (argc == 3)
    {
        ssize_t error;
        struct serial_port *port = serial_port_open(argv[1], 9600, &error);
        if (!port)
        {
            printf("Timer error %zd: %s\n", error, serial_port_error_string(error));
            return 1;
        }

        // Force a second hardware reset to clear relay mode flag if it was enabled
        serial_port_set_dtr(port, true);
        millisleep(500);
        serial_port_set_dtr(port, false);

        // Wait for bootloader timeout
        millisleep(1000);

        ssize_t ret;
        if (strcmp(argv[2], "relay") == 0)
        {
            // TODO: Fix syncing to the first packet and remove this
            ret = serial_port_write(port, (uint8_t []){'$','$','R',0, 0,'\r','\n'}, 7);
            ret = serial_port_write(port, (uint8_t []){'$','$','R',0, 0,'\r','\n'}, 7);
        }
        if (ret < 0)
        {
            printf("Write error %zd: %s\n", ret, serial_port_error_string(ret));
            serial_port_close(port);
            return 1;
        }

        serial_port_close(port);
        return 0;
    }

    printf("Example usage:\n");
    printf("  timerutil <port> relay\n");

    return 1;
}