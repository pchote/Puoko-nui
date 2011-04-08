/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
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
#include "gps.h"
#include "preferences.h"

PNView view;
PNCamera camera;
PNGPS gps;
PNPreferences prefs;

/* Write frame data to a fits file */
void pn_save_frame(PNFrame *frame)
{
	fitsfile *fptr;
	int status = 0;

	char filepath[PATH_MAX];
	/* Append a ! to the filepath to force overwriting of existing files */
	sprintf(filepath, "!%s/%s-%04d.fits.gz", prefs.output_directory, prefs.run_prefix, prefs.run_number);
	printf("Saving frame %s\n", filepath);
	
	/* Create a new fits file */
	fits_create_file(&fptr, filepath, &status);

	/* Create the primary array image (16-bit short integer pixels */
	long size[2] = { frame->width, frame->height };
	fits_create_img(fptr, USHORT_IMG, 2, size, &status);

	/* Write header keys */
	fits_update_key(fptr, TSTRING, "RUN", (void *)prefs.run_prefix, "name of this run", &status);
	
	char *object;
	switch (prefs.object_type)
	{
		case OBJECT_DARK:
			object = "DARK";
		break;
		case OBJECT_FLAT:
			object = "FLAT";
		break;
		default:
		case OBJECT_TARGET:
			object = prefs.object_name;
		break;
	}

	fits_update_key(fptr, TSTRING, "OBJECT", (void *)object, "Object name", &status);
	fits_update_key(fptr, TLONG, "EXPTIME", &prefs.exposure_time, "Actual integration time (sec)", &status);
	fits_update_key(fptr, TSTRING, "OBSERVER", (void *)prefs.observers, "Observers", &status);
	fits_update_key(fptr, TSTRING, "OBSERVAT", (void *)prefs.observatory, "Observatory", &status);
	fits_update_key(fptr, TSTRING, "TELESCOP", (void *)prefs.telescope, "Telescope name", &status);
	fits_update_key(fptr, TSTRING, "PROGRAM", "puoko-nui", "Data acquistion program", &status);
	fits_update_key(fptr, TSTRING, "INSTRUME", "puoko-nui", "Instrument", &status);

	char datebuf[15];
	char gpstimebuf[15];

    /* Get the last download pulse time from the gps */
    pthread_mutex_lock(&gps.downloadtime_mutex);
    PNGPSTimestamp end = gps.download_timestamp;
    rs_bool was_valid = end.valid;
    gps.download_timestamp.valid = FALSE;
    pthread_mutex_unlock(&gps.downloadtime_mutex);
	
	/* synctime gives the *end* of the exposure. The start of the exposure
	 * is found by subtracting the exposure time */
	PNGPSTimestamp start = end;
	pn_timestamp_subtract_seconds(&start, prefs.exposure_time);

	sprintf(datebuf, "%04d-%02d-%02d", start.year, start.month, start.day);
	fits_update_key(fptr, TSTRING, "UTC-DATE", datebuf, "Exposure start date (GPS)", &status);

	sprintf(gpstimebuf, "%02d:%02d:%02d.%03d", start.hours, start.minutes, start.seconds, start.milliseconds);
	fits_update_key(fptr, TSTRING, "UTC-BEG", gpstimebuf, "Exposure start time (GPS)", &status);

	sprintf(gpstimebuf, "%02d:%02d:%02d.%03d", end.hours, end.minutes, end.seconds, end.milliseconds);
    fits_update_key(fptr, TSTRING, "UTC-END", gpstimebuf, "Exposure end time (GPS)", &status);
	fits_update_key(fptr, TLOGICAL, "GPS-LOCK", &start.locked, "GPS time locked", &status);

    /* The timestamp may not be valid (spurious downloads, etc) */
    if (!was_valid)
	    fits_update_key(fptr, TLOGICAL, "GPS-VALID", &was_valid, "GPS timestamp has been used already", &status);

	char timebuf[15];
	time_t pcend = time(NULL);
	time_t pcstart = pcend - prefs.exposure_time;

	strftime(timebuf, 15, "%Y-%m-%d", gmtime(&pcstart));
	fits_update_key(fptr, TSTRING, "PC-DATE", (void *)timebuf, "Exposure start date (PC)", &status);

	strftime(timebuf, 15, "%H:%M:%S", gmtime(&pcstart));
	fits_update_key(fptr, TSTRING, "PC-BEG", (void *)timebuf, "Exposure start time (PC)", &status);
	
	strftime(timebuf, 15, "%H:%M:%S", gmtime(&pcend));
	fits_update_key(fptr, TSTRING, "PC-END", (void *)timebuf, "Exposure end time (PC)", &status);

    /* Camera temperature */
    sprintf(timebuf, "%0.02f",(float)camera.temperature/100);
	fits_update_key(fptr, TSTRING, "CCD-TEMP", (void *)timebuf, "CCD temperature at end of exposure in deg C", &status);

	/* Write the frame data to the image */
	fits_write_img(fptr, TUSHORT, 1, frame->width*frame->height, frame->data, &status);

	fits_close_file(fptr, &status);

	/* print out any error messages */
	fits_report_error(stderr, status);
    char cmd[PATH_MAX];
    sprintf(cmd,"./frame_available.sh %s&",filepath);
    system(cmd);
}

void pn_preview_frame(PNFrame *frame)
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
		pn_die("Error: couldn't allocate fitsbuf\n");

	/* Create a new fits file in memory */
	fits_create_memfile(&fptr, &fitsbuf, &fitssize, 2880, realloc, &status);

	/* Create the primary array image (16-bit short integer pixels */
	long size[2] = { frame->width, frame->height };
	fits_create_img(fptr, USHORT_IMG, 2, size, &status);

	/* Write frame data to the OBJECT header for ds9 to display */
	char buf[128];
	sprintf(buf, "Exposure @ %d", (int)time(NULL));
	fits_update_key(fptr, TSTRING, "OBJECT", &buf, NULL, &status);

	/* Write the frame data to the image */
	fits_write_img(fptr, TUSHORT, 1, frame->width*frame->height, frame->data, &status);
	fits_close_file(fptr, &status);

	/* print out any error messages */
	if (status)
		fits_report_error(stderr, status);
	else /* Tell ds9 to draw the new image via XPA */
		XPASet(NULL, "ds9", "fits", NULL, fitsbuf, fitssize, NULL, NULL, 1);
	
	free(fitsbuf);
}

rs_bool first_frame = FALSE;
/* Called when the acquisition thread has downloaded a frame
 * Note: this runs in the acquisition thread *not* the main thread. */
void pn_frame_downloaded_cb(PNFrame *frame)
{
	/* When starting a run, the first frame will not have a valid exposure time */
	if (first_frame)
    {
        first_frame = FALSE;
        return;
    }
    
	printf("Frame downloaded\n");
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(view.save_checkbox)))
	{
		pn_save_frame(frame);

		/* Increment the next frame */
		prefs.run_number++;
		pn_save_preferences(&prefs, "preferences.dat");
	}

	/* Display the frame in ds9 */
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(view.display_checkbox)))
		pn_preview_frame(frame);
}

void simulate_camera_download()
{
    if (camera.handle == SIMULATED)
        camera.simulated_frame_available = TRUE;
}

/*
 * Start or stop acquiring frames in response to user input
 */
static void startstop_pressed(GtkWidget *widget, gpointer data)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) 
	{
		pn_set_camera_editable(&view, FALSE);
		pn_update_camera_preferences(&view);
	
		/* Set the exposure time */
		int exptime = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(view.exptime_entry));
		pn_gps_set_exposetime(&gps, exptime);
		
		/* Check that it was set correctly */		
		int buf;
		pn_gps_get_exposetime(&gps, &buf);
		if (buf != exptime)
			pn_die("Error setting exposure time. Expected %d, was %d\n", exptime, buf);	
		
		printf("Set exposure time to %d\n", exptime);
        first_frame = TRUE;

		/* Start acquisition */
        camera.desired_mode = ACQUIRING;
		gtk_button_set_label(GTK_BUTTON(widget), "Stop Acquisition");
	}
	else
	{
		/* Stop aquisition thread */
		camera.desired_mode = IDLE;
		gtk_button_set_label(GTK_BUTTON(widget), "Start Acquisition");
		pn_set_camera_editable(&view, TRUE);	
    }
}

pthread_t gps_thread;
pthread_t camera_thread;
int main( int argc, char *argv[] )
{
	gtk_init(&argc, &argv);
	pn_load_preferences(&prefs, "preferences.dat");
	pn_save_preferences(&prefs, "preferences.dat");

	/*	
	boolean simulate = FALSE;
	/ * Parse the commandline args * /
	for (int i = 0; i < argc; i++)
		if (strcmp(argv[i], "--simulate") == 0)
			simulate = TRUE;
	*/

	/* Initialise the gps on its own thread*/
	gps = pn_gps_new();

	pthread_create(&gps_thread, NULL, pn_gps_thread, (void *)&gps);

	view.gps = &gps;
	view.prefs = &prefs;

	/* Initialise the camera on its own thread */
	camera = pn_camera_new();
	camera.on_frame_available = pn_frame_downloaded_cb;


	pthread_create(&camera_thread, NULL, pn_camera_thread, (void *)&camera);

	/* Initialise the gui and start the event loop */
	view.camera = &camera;
	pn_init_gui(&view, startstop_pressed);
	gtk_main();

	/* Shutdown hardware cleanly before exiting */
	pn_shutdown();
	return 0;
}

void pn_shutdown()
{
	/* camera and gps shutdown are done in their own threads */
    camera.desired_mode = SHUTDOWN;
    gps.shutdown = TRUE;

	void **retval = NULL;
	pthread_join(camera_thread, retval);
    pthread_join(gps_thread, retval);
}

void pn_die(const char * format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");

	pn_shutdown();
	exit(1);
}
