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

// Source for the relaystart, relaystop and flushinput programs.
// Functionality set by makefile defines
int main(int argc, char *argv[])
{    
#ifdef ENTER_RELAY 
    uint8_t data[] = {'$','$','R',0,0,'\r','\n'};
    const uint16_t length = 7;
#else
    const uint16_t length = 256;
    uint8_t data[length];
    memset(data, 0, length);
#endif

#ifdef USE_LIBFTDI
    // Initialize
    struct ftdi_context *context = ftdi_new();
    if (context == NULL)
        fprintf(stderr, "Error creating timer context\n");

    if (ftdi_init(context) < 0)
        fprintf(stderr, "Error initializing timer context: %s\n", context->error_str);

    if (ftdi_usb_open(context, 0x0403, 0x6001) != 0)
        fprintf(stderr, "Timer not found\n");

    if (ftdi_set_baudrate(context, BAUDRATE) < 0)
        fprintf(stderr, "Error setting timer baudrate: %s\n", context->error_str);

    if (ftdi_set_line_property(context, BITS_8, STOP_BIT_1, NONE) < 0)
        fprintf(stderr, "Error setting timer data frame properties: %s\n", context->error_str);

    if (ftdi_setflowctrl(context, SIO_DISABLE_FLOW_CTRL) < 0)
        fprintf(stderr, "Error setting timer flow control: %s\n", context->error_str);

    // Send data
    if (ftdi_write_data(context, data, length) != length)
        fprintf(stderr, "Failed to send buffered data.\n");

    // Shutdown
    if (ftdi_usb_close(context) < 0)
        fprintf(stderr, "Error closing timer connection\n");

    ftdi_deinit(context);
    ftdi_free(context);

#else
    // Initialize
    FT_HANDLE handle;
    if (FT_Open(0, &handle) != FT_OK)
        fprintf(stderr, "Timer not found\n");

    if (FT_SetBaudRate(handle, BAUDRATE) != FT_OK)
        fprintf(stderr, "Error setting timer baudrate\n");

    if (FT_SetDataCharacteristics(handle, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE) != FT_OK)
        fprintf(stderr, "Error setting timer data characteristics\n");

    DWORD bytes_written;
    FT_STATUS status = FT_Write(handle, data, length, &bytes_written);
    if (status != FT_OK && bytes_written == length)
        fprintf(stderr, "Failed to send buffered data.\n");

    FT_Close(handle);
#endif

    return 0;
}