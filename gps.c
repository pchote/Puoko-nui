/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ftdi.h>
#include "gps.h"

void check_ftdi(const char *message, char* file, int line, int status)
{
	if (status >= 0)
		return;
	printf("%s line %d : %s, status = %d\n", file, line, message, status);
	exit(1);
}


void check_gps(RangahauGPS *gps, char *file, int line)
{
	if (gps == NULL)
	{
		printf("gps is null @ %s:%d\n", file, line);
		exit(1);
	}
}

/* Initialise a gps object with a valid usb device */
RangahauGPS rangahau_gps_new()
{
	printf("Enumerating FTDI devices\n");	
	struct ftdi_device_list* devices = NULL;
	const int vendorId = 0x0403;  /* The USB vendor identifier for the FTDI company */
	const int productId = 0x6001; /* USB product identifier for the FT232 device */

	/* Get the list of FTDI devices on the system */
	int numDevices = ftdi_usb_find_all(NULL, &devices, vendorId, productId);
	check_ftdi("ftdi_usb_find_all() returned an error code", __FILE__, __LINE__, numDevices);
	
	printf("Found %d devices:\n", numDevices);
	struct ftdi_device_list* cur = devices;
	while (cur != NULL)
	{
		printf("\t%s\n",cur->dev->filename);
		cur = cur->next;
	}

	/* Assume that the first device is the gps unit */
	if (!numDevices)
	{
		printf("Could not find the GPS unit\n");
  		ftdi_list_free(&devices);
		exit(1);
	}

	RangahauGPS ret;
	ret.device = devices->dev;
	ret.context = NULL;
  	ftdi_list_free(&devices);
	return ret;
}

void rangahau_gps_free(RangahauGPS *gps)
{
	check_gps(gps, __FILE__, __LINE__);
}

/* Open the usb gps device and prepare it for reading/writing */
void rangahau_gps_init(RangahauGPS *gps)
{
	check_gps(gps, __FILE__, __LINE__);	
	printf("Opening device %s\n", gps->device->filename);

	if (gps->context != NULL)
	{
		printf("device %s is already open @ %s:%d\n", gps->device->filename, __FILE__, __LINE__);
		exit(1);
	}
	gps->context = ftdi_new();
	int status = ftdi_init(gps->context);
	check_ftdi("ftdi_init() returned an error code", __FILE__, __LINE__, status);

	// Prepare the device for use with libftdi library calls.
	status = ftdi_usb_open_dev(gps->context, gps->device);
	check_ftdi("ftdi_usb_open_dev() returned an error code", __FILE__, __LINE__, status);

	//ftdi_enable_bitbang(pContext, 0xFF);

	status = ftdi_set_baudrate(gps->context, 115200);
	check_ftdi("ftdi_set_baudrate() returned an error code", __FILE__, __LINE__, status);

	status = ftdi_set_line_property(gps->context, BITS_8, STOP_BIT_1, NONE);
	check_ftdi("ftdi_set_line_property() returned an error code", __FILE__, __LINE__, status);

	status = ftdi_setflowctrl(gps->context, SIO_DISABLE_FLOW_CTRL);
	check_ftdi("ftdi_setflowctrl() returned an error code", __FILE__, __LINE__, status);

	unsigned char latency = 1; // the latency in milliseconds before partially full bit buffers are sent.
	status = ftdi_set_latency_timer(gps->context, latency);
	check_ftdi("ftdi_set_latency_timer() returned an error code", __FILE__, __LINE__, status);
}

/* Close the usb gps device */
void rangahau_gps_uninit(RangahauGPS *gps)
{
	check_gps(gps, __FILE__, __LINE__);
	printf("Closing device %s\n", gps->device->filename);
	if (gps->context == NULL)
	{
		printf("device %s is already closed @ %s:%d\n", gps->device->filename, __FILE__, __LINE__);
		exit(1);
	}
	int status = ftdi_usb_close(gps->context);
	check_ftdi("ftdi_usb_close() returned an error code", __FILE__, __LINE__, status);

	ftdi_deinit(gps->context);
	ftdi_free(gps->context);
	gps->context = NULL;
}

/* Send a series of bytes to the gps device */
void rangahau_gps_write(RangahauGPS *gps, unsigned char *bytes, int numBytes)
{
	check_gps(gps, __FILE__, __LINE__);
	if (ftdi_write_data(gps->context, bytes, numBytes) != numBytes)
	{
		printf("Error writing data @ %s:%d\n", __FILE__, __LINE__);
		exit(1);
	}
}

/* Read a series of bytes from a gps device */
void rangahau_gps_read(RangahauGPS *gps, unsigned char *bytes, int maxBytes, int *numBytes)
{
	check_gps(gps, __FILE__, __LINE__);
	if ((*numBytes = ftdi_read_data(gps->context, bytes, maxBytes)) < 0)
	{
		printf("Error reading data @ %s:%d\n", __FILE__, __LINE__);
		exit(1);
	}	
}
