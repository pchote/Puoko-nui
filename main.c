/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <fitsio.h>
#include <string.h>
#include <sys/select.h>
#include <xpa.h>
#include "common.h"
#include "camera.h"
#include "gps.h"
#include "preferences.h"
#include "ui.h"

PNCamera camera;
PNGPS gps;

/* A quick and dirty method for opening ds9
 * Beware of race conditions: it will take some time
 * between calling this, and ds9 actually being available */
static void launch_ds9()
{
    char *names[1];
    char *errs[1];
    int valid = XPAAccess(NULL, "Puoko-nui", NULL, NULL, names, errs, 1);
    if (errs[0] != NULL)
    {
        valid = 0;
        free(errs[0]);
    }
    if (names[0]) free(names[0]);

    if (!valid)
        system("ds9 -title Puoko-nui&");
}

/* Write frame data to a fits file */
void pn_save_frame(PNFrame *frame)
{
	fitsfile *fptr;
	int status = 0;

	/* Saving will fail if a file with the same name already exists */
    char *output_dir = pn_preference_string(OUTPUT_DIR);
    char *run_prefix = pn_preference_string(RUN_PREFIX);
    int run_number = pn_preference_int(RUN_NUMBER);

    char *filepath;
	asprintf(&filepath, "%s/%s-%04d.fits.gz", output_dir, run_prefix, run_number);
    pn_log("Saving frame %s", filepath);
	
	/* Create a new fits file */
	if (fits_create_file(&fptr, filepath, &status))
    {
        pn_log("Unable to save file. fitsio error %d", status);
        return;
    }

	/* Create the primary array image (16-bit short integer pixels */
	long size[2] = { frame->width, frame->height };
	fits_create_img(fptr, USHORT_IMG, 2, size, &status);

	/* Write header keys */
	fits_update_key(fptr, TSTRING, "RUN", (void *)run_prefix, "name of this run", &status);
	
    free(output_dir);
    free(run_prefix);

    char *object_name = pn_preference_string(OBJECT_NAME);
	fits_update_key(fptr, TSTRING, "OBJECT", (void *)object_name, "Object name", &status);
    free(object_name);

    int exposure_time = pn_preference_char(EXPOSURE_TIME);
	fits_update_key(fptr, TLONG, "EXPTIME", &exposure_time, "Actual integration time (sec)", &status);

    char *observers = pn_preference_string(OBSERVERS);
	fits_update_key(fptr, TSTRING, "OBSERVER", (void *)observers, "Observers", &status);
    free(observers);

    char *observatory = pn_preference_string(OBSERVATORY);
	fits_update_key(fptr, TSTRING, "OBSERVAT", (void *)observatory, "Observatory", &status);
	free(observatory);

    char *telescope = pn_preference_string(TELESCOPE);
    fits_update_key(fptr, TSTRING, "TELESCOP", (void *)telescope, "Telescope name", &status);
	free(telescope);

    fits_update_key(fptr, TSTRING, "PROGRAM", "puoko-nui", "Data acquistion program", &status);
	fits_update_key(fptr, TSTRING, "INSTRUME", "puoko-nui", "Instrument", &status);

	char datebuf[15];
	char gpstimebuf[15];

    /* Get the last download pulse time from the gps */
    pthread_mutex_lock(&gps.read_mutex);
    PNGPSTimestamp end = gps.download_timestamp;
    rs_bool was_valid = end.valid;
    gps.download_timestamp.valid = FALSE;
    pthread_mutex_unlock(&gps.read_mutex);
	
	/* synctime gives the *end* of the exposure. The start of the exposure
	 * is found by subtracting the exposure time */
	PNGPSTimestamp start = end;
	pn_timestamp_subtract_seconds(&start, exposure_time);

	sprintf(datebuf, "%04d-%02d-%02d", start.year, start.month, start.day);
	fits_update_key(fptr, TSTRING, "UTC-DATE", datebuf, "Exposure start date (GPS)", &status);

	sprintf(gpstimebuf, "%02d:%02d:%02d", start.hours, start.minutes, start.seconds);
	fits_update_key(fptr, TSTRING, "UTC-BEG", gpstimebuf, "Exposure start time (GPS)", &status);

	sprintf(gpstimebuf, "%02d:%02d:%02d", end.hours, end.minutes, end.seconds);
    fits_update_key(fptr, TSTRING, "UTC-END", gpstimebuf, "Exposure end time (GPS)", &status);
	fits_update_key(fptr, TLOGICAL, "GPS-LOCK", &start.locked, "GPS time locked", &status);

    /* The timestamp may not be valid (spurious downloads, etc) */
    if (!was_valid)
	    fits_update_key(fptr, TLOGICAL, "GPS-VALID", &was_valid, "GPS timestamp has been used already", &status);

	char timebuf[15];
	time_t pcend = time(NULL);
	time_t pcstart = pcend - exposure_time;

	strftime(timebuf, 15, "%Y-%m-%d", gmtime(&pcstart));
	fits_update_key(fptr, TSTRING, "PC-DATE", (void *)timebuf, "Exposure start date (PC)", &status);

	strftime(timebuf, 15, "%H:%M:%S", gmtime(&pcstart));
	fits_update_key(fptr, TSTRING, "PC-BEG", (void *)timebuf, "Exposure start time (PC)", &status);
	
	strftime(timebuf, 15, "%H:%M:%S", gmtime(&pcend));
	fits_update_key(fptr, TSTRING, "PC-END", (void *)timebuf, "Exposure end time (PC)", &status);

    /* Camera temperature */
    pthread_mutex_lock(&camera.read_mutex);
    sprintf(timebuf, "%0.02f", camera.temperature);
    pthread_mutex_unlock(&camera.read_mutex);

	fits_update_key(fptr, TSTRING, "CCD-TEMP", (void *)timebuf, "CCD temperature at end of exposure in deg C", &status);

	/* Write the frame data to the image */
	fits_write_img(fptr, TUSHORT, 1, frame->width*frame->height, frame->data, &status);

	fits_close_file(fptr, &status);

	/* print out any error messages */
	fits_report_error(stderr, status);
    char cmd[PATH_MAX];
    sprintf(cmd,"./frame_available.sh %s&",filepath);
    system(cmd);

    free(filepath);
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
	fitsbuf = malloc(fitssize);

	/* Create a new fits file in memory */
	fits_create_memfile(&fptr, &fitsbuf, &fitssize, 2880, realloc, &status);

	/* Create the primary array image (16-bit short integer pixels */
	long size[2] = { frame->width, frame->height };
	fits_create_img(fptr, USHORT_IMG, 2, size, &status);

	/* Write frame data to the OBJECT header for ds9 to display */
    pthread_mutex_lock(&gps.read_mutex);
    PNGPSTimestamp end = gps.download_timestamp;
    pthread_mutex_unlock(&gps.read_mutex);

	char buf[128];
    sprintf(buf, "Exposure ending %04d-%02d-%02d %02d:%02d:%02d",
            end.year, end.month, end.day,
            end.hours, end.minutes, end.seconds);

    fits_update_key(fptr, TSTRING, "OBJECT", &buf, NULL, &status);

	/* Write the frame data to the image */
	fits_write_img(fptr, TUSHORT, 1, frame->width*frame->height, frame->data, &status);
	fits_close_file(fptr, &status);

	/* print out any error messages */
	if (status)
		fits_report_error(stderr, status);
	else /* Tell ds9 to draw the new image via XPA */
		if (0 == XPASet(NULL, "Puoko-nui", "fits", NULL, fitsbuf, fitssize, NULL, NULL, 1))
            launch_ds9(); // Launch a new instance of ds9 to be available for the next frame
	
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

	pn_log("Frame downloaded");
	if (pn_preference_char(SAVE_FRAMES) && pn_preference_allow_save())
	{
		pn_save_frame(frame);
        pn_preference_increment_framecount();
	}

	/* Display the frame in ds9 */
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

void shutdown_camera()
{
    pthread_mutex_lock(&camera.read_mutex);
    camera.desired_mode = IDLE;
    pthread_mutex_unlock(&camera.read_mutex);
}

static pthread_t gps_thread;
static rs_bool gps_thread_initialized = FALSE;
static pthread_t camera_thread;
static rs_bool camera_thread_initialized = FALSE;

FILE *logFile;
int main( int argc, char *argv[] )
{
    // Open the log file for writing
    time_t start = time(NULL);
    char namebuf[32];
    strftime(namebuf, 32, "%Y%m%d-%H%M%S.log", gmtime(&start));
    logFile = fopen(namebuf, "w");
    init_log_gui();

    launch_ds9();	
	pn_init_preferences("preferences.dat");

	rs_bool simulate_camera = FALSE;
    rs_bool simulate_gps = FALSE;
    rs_bool disable_pixel_binning = FALSE;

	// Parse the commandline args
	for (int i = 0; i < argc; i++)
	{
    	if (strcmp(argv[i], "--simulate-camera") == 0)
			simulate_camera = TRUE;

        if (strcmp(argv[i], "--simulate-gps") == 0)
			simulate_gps = TRUE;

        if (strcmp(argv[i], "--disable-binning") == 0)
			disable_pixel_binning = TRUE;
    }

	/* Initialise the gps on its own thread*/
	gps = pn_gps_new(simulate_gps);

	pthread_create(&gps_thread, NULL, pn_gps_thread, (void *)&gps);
    gps_thread_initialized = TRUE;

	/* Initialise the camera on its own thread */
	camera = pn_camera_new();
    if (simulate_camera)
        camera.handle = SIMULATED;

    if (disable_pixel_binning)
        camera.binsize = 1;

	camera.on_frame_available = pn_frame_downloaded_cb;

	pthread_create(&camera_thread, NULL, pn_camera_thread, (void *)&camera);
    camera_thread_initialized = TRUE;


    //
	// Main program loop is run by the ui code
    //
    pn_ui_run(&gps, &camera);


    //
	// Shutdown hardware and cleanup
    //

    // Tell the GPS and Camera threads to terminate themselves
    pthread_mutex_lock(&camera.read_mutex);
    camera.desired_mode = SHUTDOWN;
    pthread_mutex_unlock(&camera.read_mutex);
    gps.shutdown = TRUE;

    // Wait for the GPS and Camera threads to terminate
	void **retval = NULL;
    if (camera_thread_initialized)
	    pthread_join(camera_thread, retval);
    if (gps_thread_initialized)
        pthread_join(gps_thread, retval);

    // Final cleanup
    pn_gps_free(&gps);
    pn_camera_free(&camera);
    pn_free_preferences();
    fclose(logFile);

    return 0;
}

void pn_log(const char * format, ...)
{
	va_list args;
	va_start(args, format);

    // Log time
    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm* ptm = gmtime(&tv.tv_sec);
    char timebuf[9];
    strftime(timebuf, 9, "%H:%M:%S", ptm);

    // Construct log line
    char *msgbuf, *linebuf;
    vasprintf(&msgbuf, format, args);
    asprintf(&linebuf, "[%s.%03ld] %s", timebuf, tv.tv_usec / 1000, msgbuf);
    free(msgbuf);

    // Log to file
    fprintf(logFile, "%s", linebuf);
	fprintf(logFile, "\n");

    // Add to gui
    add_log_line(linebuf);

    free(linebuf);
    va_end(args);
}
