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

#include "common.h"
#include "view.h"
#include "camera.h"
#include "acquisitionthread.h"
#include "gps.h"

RangahauView view;
RangahauAcquisitionThreadInfo acquisition_info;
pthread_t acquisition_thread;
RangahauCamera camera;
RangahauGPS gps;

/* Write frame data to a fits file */
void rangahau_save_frame(RangahauFrame frame, const char *filepath)
{
	fitsfile *fptr;
	int status = 0;

	printf("Saving frame to %s\n", filepath);

	/* Collect the various frame data we want to write */
	long exposure = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(view.exptime_entry));
	
	/* Append a ! to the filepath to force overwriting of existing files */
	char *file = (char *)malloc((strlen(filepath)+2)*sizeof(char));
	sprintf(file, "!%s",filepath);
	printf("%s\n",file);

	/* Create a new fits file */
	fits_create_file(&fptr, file, &status);

	/* Create the primary array image (16-bit short integer pixels */
	long size[2] = { frame.width, frame.height };
	fits_create_img(fptr, USHORT_IMG, 2, size, &status);

	/* Write header keys */
	fits_update_key(fptr, TSTRING, "RUN", (void *)gtk_entry_get_text(GTK_ENTRY(view.run_entry)), "name of this run", &status);
	
	char *object;
	switch (gtk_combo_box_get_active(GTK_COMBO_BOX(view.target_combobox)))
	{
		case OBJECT_DARK:
			object = "DARK";
		break;
		case OBJECT_FLAT:
			object = "FLAT";
		break;
		default:
		case OBJECT_TARGET:
			object = (char *)gtk_entry_get_text(GTK_ENTRY(view.target_entry));
		break;
	}

	fits_update_key(fptr, TSTRING, "OBJECT", (void *)object, "Object name", &status);
	fits_update_key(fptr, TLONG, "EXPTIME", &exposure, "Actual integration time (sec)", &status);
	fits_update_key(fptr, TSTRING, "OBSERVER", (void *)gtk_entry_get_text(GTK_ENTRY(view.observers_entry)), "Observers", &status);
	fits_update_key(fptr, TSTRING, "OBSERVAT", (void *)gtk_entry_get_text(GTK_ENTRY(view.observatory_entry)), "Observatory", &status);
	fits_update_key(fptr, TSTRING, "TELESCOP", (void *)gtk_entry_get_text(GTK_ENTRY(view.telescope_entry)), "Telescope name", &status);
	fits_update_key(fptr, TSTRING, "PROGRAM", "rangahau", "Data acquistion program", &status);
	fits_update_key(fptr, TSTRING, "INSTRUME", "puoko-nui", "Instrument", &status);

	RangahauGPSTimestamp ts;
	char datebuf[15];
	char gpstimebuf[15];

	/* Get the synctime. Failure is not an option! */
	while (!rangahau_gps_get_synctime(&gps, 2000, &ts));

	sprintf(datebuf, "%04d-%02d-%02d", ts.year, ts.month, ts.day);
	sprintf(gpstimebuf, "%02d:%02d:%02d.%3d", ts.hours, ts.minutes, ts.seconds, ts.milliseconds);

	fits_update_key(fptr, TSTRING, "UTC-DATE", datebuf, "GPS Exposure start date (UTC)", &status);
	fits_update_key(fptr, TSTRING, "UTC-END", gpstimebuf, "GPS Exposure end time (UTC)", &status);

	/* synctime gives the *end* of the exposure. The start of the exposure
	 * is found by subtracting the exposure time */
	rangahau_timestamp_subtract_seconds(&ts, exposure);
	sprintf(gpstimebuf, "%02d:%02d:%02d.%3d", ts.hours, ts.minutes, ts.seconds, ts.milliseconds);
	fits_update_key(fptr, TSTRING, "UTC-TIME", gpstimebuf, "GPS Exposure start time (UTC)", &status);
	//fits_update_key(fptr, TSTRING, "GPS-CLOCK", "TODO", "GPS clock status", &status);

	char timebuf[15];
	time_t t = time(NULL) - exposure;
	strftime(timebuf, 15, "%Y-%m-%d", gmtime(&t));
	fits_update_key(fptr, TSTRING, "PC-DATE", (void *)timebuf, "PC Exposure start date (UTC)", &status);
	strftime(timebuf, 15, "%H:%M:%S", gmtime(&t));
	fits_update_key(fptr, TSTRING, "PC-TIME", (void *)timebuf, "PC Exposure start time (UTC)", &status);


	/* Write the frame data to the image */
	fits_write_img(fptr, TUSHORT, 1, frame.width*frame.height, frame.data, &status);

	fits_close_file(fptr, &status);

	/* print out any error messages */
	fits_report_error(stderr, status);

	free(file);
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
		rangahau_die("Error: couldn't allocate fitsbuf\n");

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
	/* TODO: Get the last gps sync pulse time, subtract exptime to find frame start */
	
	printf("Frame downloaded\n");
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(view.save_checkbox)))
	{
		/* Build the file path to save to */
		int framenum = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(view.frame_entry));
		const char *destination = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(view.destination_btn));
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
		rangahau_set_camera_editable(&view, FALSE);	
		/* Set the exposure time */
		int exptime = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(view.exptime_entry));
		rangahau_gps_set_exposetime(&gps, exptime);
		
		/* Check that it was set correctly */		
		int buf;
		rangahau_gps_get_exposetime(&gps, &buf);
		if (buf != exptime)
			rangahau_die("Error setting exposure time. Expected %d, was %d\n", exptime, buf);	
		
		printf("Set exposure time to %d\n", exptime);

		/* Start acquisition thread */
		acquisition_info.camera = &camera;
		acquisition_info.binsize = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(view.binsize_entry));
		acquisition_info.cancelled = FALSE;
		acquisition_info.on_frame_available = rangahau_frame_downloaded_cb;
		
		pthread_create(&acquisition_thread, NULL, rangahau_acquisition_thread, (void *)&acquisition_info);

		gtk_button_set_label(GTK_BUTTON(widget), "Stop Acquisition");
	}
	else
	{
		/* Stop aquisition thread */
		acquisition_info.cancelled = TRUE;
		gtk_button_set_label(GTK_BUTTON(widget), "Start Acquisition");
		rangahau_set_camera_editable(&view, TRUE);	
    }
}

int main( int argc, char *argv[] )
{
	gtk_init(&argc, &argv);
	boolean simulate = FALSE;
	/* Parse the commandline args */
	for (int i = 0; i < argc; i++)
		if (strcmp(argv[i], "--simulate") == 0)
			simulate = TRUE;

	/* Initialise the gps */
	gps = rangahau_gps_new();
	rangahau_gps_init(&gps);
	view.gps = &gps;

	/* Initialise the camera.
	 * Run in a separate thread to avoid blocking the gui. */
	camera = rangahau_camera_new(simulate);
	view.camera = &camera;
	pthread_t camera_init;
	pthread_create(&camera_init, NULL, rangahau_camera_init, (void *)&camera);

	/* Initialise the gui and start the event loop */
	rangahau_init_gui(&view, startstop_pressed);
	gtk_main();

	rangahau_shutdown();
	return 0;
}

void rangahau_shutdown()
{
	/* Stop the acquisition thread */
	acquisition_info.cancelled = TRUE;

	/* Close the camera */
	rangahau_camera_close(&camera);

	/* Close the gps */
	rangahau_gps_uninit(&gps);
	rangahau_gps_free(&gps);
}

void rangahau_die(const char * format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");

	rangahau_shutdown();
	exit(1);
}
