/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#ifdef USE_LIBFTDI
#   include <ftdi.h>
#else
#   include <ftd2xx.h>
#endif

static int send_data(uint8_t *data, uint16_t length)
{
#ifdef USE_LIBFTDI
    // Initialize
    struct ftdi_context *context = ftdi_new();
    if (context == NULL)
    {
        fprintf(stderr, "Error creating timer context\n");
        return 1;
    }

    if (ftdi_init(context) < 0)
    {
        fprintf(stderr, "Error initializing timer context: %s\n", context->error_str);
        return 1;
    }

    if (ftdi_usb_open(context, 0x0403, 0x6001) != 0)
    {
        fprintf(stderr, "Timer not found\n");
        return 1;
    }

    if (ftdi_set_baudrate(context, 9600) < 0)
    {
        fprintf(stderr, "Error setting timer baudrate: %s\n", context->error_str);
        return 1;
    }

    if (ftdi_set_line_property(context, BITS_8, STOP_BIT_1, NONE) < 0)
    {
        fprintf(stderr, "Error setting timer data frame properties: %s\n", context->error_str);
        return 1;
    }

    if (ftdi_setflowctrl(context, SIO_DISABLE_FLOW_CTRL) < 0)
    {
        fprintf(stderr, "Error setting timer flow control: %s\n", context->error_str);
        return 1;
    }

    // Send data
    if (ftdi_write_data(context, data, length) != length)
    {
        fprintf(stderr, "Failed to send buffered data.\n");
        return 1;
    }

    // Shutdown
    if (ftdi_usb_close(context) < 0)
    {
        fprintf(stderr, "Error closing timer connection\n");
        return 1;
    }

    ftdi_deinit(context);
    ftdi_free(context);
#else
    // Initialize
    FT_HANDLE handle;
    if (FT_Open(0, &handle) != FT_OK)
    {
        fprintf(stderr, "Timer not found\n");
        return 1;
    }

    if (FT_SetBaudRate(handle, 9600) != FT_OK)
    {
        fprintf(stderr, "Error setting timer baudrate\n");
        return 1;
    }

    if (FT_SetDataCharacteristics(handle, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE) != FT_OK)
    {
        fprintf(stderr, "Error setting timer data characteristics\n");
        return 1;
    }

    DWORD bytes_written;
    FT_STATUS status = FT_Write(handle, data, length, &bytes_written);
    if (status != FT_OK && bytes_written == length)
    {
        fprintf(stderr, "Failed to send buffered data.\n");
        return 1;
    }

    FT_Close(handle);
#endif

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc == 2)
    {
        if (strcmp(argv[1], "--relay") == 0)
            return send_data((uint8_t []){'$','$','R',0, 0,'\r','\n'}, 7);

        if (strcmp(argv[1], "--upgrade") == 0)
            return send_data((uint8_t []){'$','$','U',0, 0,'\r','\n'}, 7);

        if (strcmp(argv[1], "--flush") == 0)
        {
            // Send a single 'E' to exit upgrade mode
            // followed by 256 0's to force a soft-restart
            uint8_t data[257];
            memset(data, 0, 257);
            data[0] = 'E';
            return send_data(data, 257);
        }
    }

    printf("Example usage:\n");
    printf("  timerutil --relay\n");
    printf("  timerutil --upgrade\n");
    printf("  timerutil --flush\n");

    return 1;
}