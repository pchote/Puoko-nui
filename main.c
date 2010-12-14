/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <time.h>
#include <pthread.h>
#include <fitsio.h>
#include <string.h>
#include <sys/select.h>
#include <xpa.h>

#include "view.h"
#include "camera.h"
#include "acquisitionthread.h"
#include "gps.h"

RangahauView view;
RangahauAcquisitionThreadInfo acquisition_info;
pthread_t acquisition_thread;
RangahauCamera camera;


/* Write frame data to a fits file */
void rangahau_save_frame(RangahauFrame frame, const char *filepath)
{
	fitsfile *fptr;
	int status = 0;

	printf("Saving frame to %s\n", filepath);

	/* Collect the various frame data we want to write */
	long exposure = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(view.exptime_entry));

	/* Create a new fits file */
	fits_create_file(&fptr, filepath, &status);

	/* Create the primary array image (16-bit short integer pixels */
	long size[2] = { frame.width, frame.height };
	fits_create_img(fptr, USHORT_IMG, 2, size, &status);

	/* Write header keys */
	fits_update_key(fptr, TLONG, "EXPOSURE", &exposure, "Total Exposure Time", &status);

	/* Write the frame data to the image */
	fits_write_img(fptr, TUSHORT, 1, frame.width*frame.height, frame.data, &status);

	fits_close_file(fptr, &status);

	/* print out any error messages */
	fits_report_error(stderr, status);
}

void rangahau_preview_frame(RangahauFrame frame)
{
	fitsfile *fptr;
	int status = 0;
	void *fitsbuf;

	/* Size of the memory buffer = 1024*1024*2 bytes 
	 * for pixels + 4096 for the header */
	/* TODO: What is a good number for this? */
	size_t fitssize = 2101248;

	/* Allocate a chunk of memory for the image */
	if(!(fitsbuf = malloc(fitssize)))
	{
		printf("Error: couldn't allocate fitsbuf\n");
		exit(1);
	}

	/* Create a new fits file in memory */
	fits_create_memfile(&fptr, &fitsbuf, &fitssize, 2880, realloc, &status);

	/* Create the primary array image (16-bit short integer pixels */
	long size[2] = { frame.width, frame.height };
	fits_create_img(fptr, USHORT_IMG, 2, size, &status);

	/* Write frame data to the OBJECT header for ds9 to display */
	char buf[128];
	sprintf(buf, "Exposure @ %d", (int)time(NULL));
	fits_update_key(fptr, TSTRING, "OBJECT", &buf, NULL, &status);

	/* Write the frame data to the image */
	fits_write_img(fptr, TUSHORT, 1, frame.width*frame.height, frame.data, &status);
	fits_close_file(fptr, &status);

	/* print out any error messages */
	if (status)
		fits_report_error(stderr, status);
	else /* Tell ds9 to draw the new image via XPA */
		XPASet(NULL, "ds9", "fits", NULL, fitsbuf, fitssize, NULL, NULL, 1);
	
	free(fitsbuf);
}

/* Called when the acquisition thread has downloaded a frame
 * Note: this runs in the acquisition thread *not* the main thread. */
void rangahau_frame_downloaded_cb(RangahauFrame frame)
{
	printf("Frame downloaded\n");
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(view.save_checkbox)))
	{
		/* Build the file path to save to */
		int framenum = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(view.frame_entry));
		const char *destination = gtk_entry_get_text(GTK_ENTRY(view.destination_entry));
		const char *prefix = gtk_entry_get_text(GTK_ENTRY(view.run_entry));
		char filepath[1024];
		sprintf(filepath, "%s/%s-%d.fits.gz",destination,prefix,framenum);

		/* Increment the next frame */
		gtk_spin_button_spin(GTK_SPIN_BUTTON(view.frame_entry), GTK_SPIN_STEP_FORWARD, 1);

		rangahau_save_frame(frame, filepath);		
	}

	/* Display the frame in ds9 */
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(view.display_checkbox)))
		rangahau_preview_frame(frame);
}

/*
 * Start or stop acquiring frames in response to user input
 */
static void startstop_pressed(GtkWidget *widget, gpointer data)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) 
	{
		rangahau_set_fields_editable(&view, FALSE);

		/* Start acquisition thread */
		acquisition_info.camera = &camera;
		acquisition_info.exptime = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(view.exptime_entry));
		acquisition_info.binsize = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(view.binsize_entry));
		acquisition_info.cancelled = FALSE;
		acquisition_info.on_frame_available = rangahau_frame_downloaded_cb;
		
		pthread_create(&acquisition_thread, NULL, rangahau_acquisition_thread, (void *)&acquisition_info);

		gtk_button_set_label(GTK_BUTTON(widget), "Stop Acquisition");
	}
	else
	{
		rangahau_set_fields_editable(&view, TRUE);

		/* Stop aquisition thread */
		acquisition_info.cancelled = TRUE;

		gtk_button_set_label(GTK_BUTTON(widget), "Start Acquisition");
    }
}

int main( int argc, char *argv[] )
{
	RangahauGPS gps = rangahau_gps_new();
	rangahau_gps_init(&gps);
	rangahau_gps_uninit(&gps);
	rangahau_gps_free(&gps);
	exit(1);

	gtk_init(&argc, &argv);
	boolean simulate = FALSE;
	/* Parse the commandline args */
	for (int i = 0; i < argc; i++)
		if (strcmp(argv[i], "--simulate") == 0)
			simulate = TRUE;

	/* Initialise the camera.
	 * Run in a separate thread to avoid blocking the gui. */
	camera = rangahau_camera_new(simulate);
	view.camera = &camera;
	pthread_t camera_init;
	pthread_create(&camera_init, NULL, rangahau_camera_init, (void *)&camera);

	/* Initialise the gui and start */
	rangahau_init_gui(&view, startstop_pressed);
	gtk_main();

	/* Close the camera before exiting */
	rangahau_camera_close(&camera);
	return 0;
}
