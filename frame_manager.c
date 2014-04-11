/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <string.h>
#include <fitsio.h>
#include <pthread.h>
#include <math.h>
#include "atomicqueue.h"
#include "camera.h"
#include "reduction_script.h"
#include "preview_script.h"
#include "timer.h"
#include "preferences.h"
#include "version.h"
#include "frame_manager.h"
#include "platform.h"
#include "main.h"

struct FrameManager
{
    pthread_t frame_thread;
    pthread_mutex_t frame_mutex;
    pthread_cond_t signal_condition;
    pthread_mutex_t signal_mutex;

    struct atomicqueue *frame_queue;
    struct atomicqueue *trigger_queue;
    bool first_frame;

    bool thread_alive;
    bool shutdown;
};

FrameManager *frame_manager_new()
{
    FrameManager *frame = calloc(1, sizeof(struct FrameManager));
    if (!frame)
        return NULL;

    frame->first_frame = true;
    frame->frame_queue = atomicqueue_create();
    frame->trigger_queue = atomicqueue_create();
    if (!frame->frame_queue || !frame->trigger_queue)
    {
        atomicqueue_destroy(frame->frame_queue);
        atomicqueue_destroy(frame->trigger_queue);
        free(frame);
        return NULL;
    }

    pthread_mutex_init(&frame->frame_mutex, NULL);
    pthread_cond_init(&frame->signal_condition, NULL);
    pthread_mutex_init(&frame->signal_mutex, NULL);
    return frame;
}

void frame_manager_free(FrameManager *frame)
{
    clear_queued_data(true);

    atomicqueue_destroy(frame->trigger_queue);
    atomicqueue_destroy(frame->frame_queue);
    pthread_mutex_destroy(&frame->frame_mutex);
    pthread_mutex_destroy(&frame->signal_mutex);
    pthread_cond_destroy(&frame->signal_condition);
    free(frame);
}

// Transform the frame data in a CameraFrame with the
// flip/transpose operations specified in the preferences.
void frame_process_transforms(CameraFrame *frame)
{
    if (pn_preference_char(FRAME_FLIP_X))
    {
        for (uint16_t j = 0; j < frame->height; j++)
            for (uint16_t i = 0; i < frame->width/2; i++)
            {
                uint16_t temp = frame->data[j*frame->width + i];
                frame->data[j*frame->width + i] = frame->data[j*frame->width + (frame->width - i - 1)];
                frame->data[j*frame->width + (frame->width - i - 1)] = temp;
            }
        
        if (frame->has_image_region)
        {
            uint16_t temp = frame->width - frame->image_region[0];
            frame->image_region[0] = frame->width - frame->image_region[1];
            frame->image_region[1] = temp;
        }
        
        if (frame->has_bias_region)
        {
            uint16_t temp = frame->width - frame->bias_region[0];
            frame->bias_region[0] = frame->width - frame->bias_region[1];
            frame->bias_region[1] = temp;
        }
    }
    
    if (pn_preference_char(FRAME_FLIP_Y))
    {
        for (uint16_t j = 0; j < frame->height/2; j++)
            for (uint16_t i = 0; i < frame->width; i++)
            {
                uint16_t temp = frame->data[j*frame->width + i];
                frame->data[j*frame->width + i] = frame->data[(frame->height - j - 1)*frame->width + i];
                frame->data[(frame->height - j - 1)*frame->width + i] = temp;
            }
        
        if (frame->has_image_region)
        {
            uint16_t temp = frame->height - frame->image_region[2];
            frame->image_region[2] = frame->height - frame->image_region[3];
            frame->image_region[3] = temp;
        }
        
        if (frame->has_bias_region)
        {
            uint16_t temp = frame->height - frame->bias_region[2];
            frame->bias_region[2] = frame->height - frame->bias_region[3];
            frame->bias_region[3] = temp;
        }
    }
    
    if (pn_preference_char(FRAME_TRANSPOSE))
    {
        // Create a copy of the frame to simplify transpose when width != height
        size_t s = frame->width*frame->height*sizeof(uint16_t);
        uint16_t *data = malloc(s);
        if (!data)
        {
            pn_log("Failed to allocate memory. Discarding frame");
            return;
        }
        memcpy(data, frame->data, s);
        
        for (uint16_t j = 0; j < frame->height; j++)
            for (uint16_t i = 0; i < frame->width; i++)
                frame->data[i*frame->height + j] = data[j*frame->width + i];
        free(data);
        
        if (frame->has_image_region)
            for (uint8_t i = 0; i < 2; i++)
            {
                uint16_t temp = frame->image_region[i];
                frame->image_region[i] = frame->image_region[i+2];
                frame->image_region[i+2] = temp;
            }
        
        if (frame->has_bias_region)
            for (uint8_t i = 0; i < 2; i++)
            {
                uint16_t temp = frame->bias_region[i];
                frame->bias_region[i] = frame->bias_region[i+2];
                frame->bias_region[i+2] = temp;
            }
        
        uint16_t temp = frame->height;
        frame->height = frame->width;
        frame->width = temp;
    }
}

// Save a frame and trigger to disk
// Returns true on success or false on failure
bool frame_save(CameraFrame *frame, TimerTimestamp *timestamp, char *filepath)
{
    fitsfile *fptr;
    int status = 0;
    char fitserr[128];
    
    // Create a new fits file
    if (fits_create_file(&fptr, filepath, &status))
    {
        pn_log("Failed to save file. fitsio error %d.", status);
        while (fits_read_errmsg(fitserr))
            pn_log(fitserr);
        
        return false;
    }
    
    // Create the primary array image (16-bit short integer pixels
    long size[2] = { frame->width, frame->height };
    fits_create_img(fptr, USHORT_IMG, 2, size, &status);

    // Write header keys
    uint8_t trigger_mode = pn_preference_char(TIMER_TRIGGER_MODE);
    long exposure_time = pn_preference_int(EXPOSURE_TIME);

    if (trigger_mode == TRIGGER_BIAS)
    {
        fits_update_key(fptr, TSTRING, "OBJECT", "Bias", "Object name", &status);
    }
    else
    {
        switch (pn_preference_char(OBJECT_TYPE))
        {
            case OBJECT_DARK:
                fits_update_key(fptr, TSTRING, "OBJECT", "Dark", "Object name", &status);
                break;
            case OBJECT_FLAT:
                fits_update_key(fptr, TSTRING, "OBJECT", "Flat Field", "Object name", &status);
                break;
            case OBJECT_FOCUS:
                fits_update_key(fptr, TSTRING, "OBJECT", "Focus", "Object name", &status);
                break;
            case OBJECT_TARGET:
            default:
            {
                char *object_name = pn_preference_string(OBJECT_NAME);
                fits_update_key(fptr, TSTRING, "OBJECT", (void *)object_name, "Object name", &status);
                free(object_name);
                break;
            }
        }
    
        if (trigger_mode == TRIGGER_MILLISECONDS)
        {
            double exptime = exposure_time / 1000.0;
            fits_update_key(fptr, TDOUBLE, "EXPTIME", &exptime, "Actual integration time (sec)", &status);
        }
        else
            fits_update_key(fptr, TLONG, "EXPTIME", &exposure_time, "Actual integration time (sec)", &status);
    }

    char *observers = pn_preference_string(OBSERVERS);
    fits_update_key(fptr, TSTRING, "OBSERVER", (void *)observers, "Observers", &status);
    free(observers);
    
    char *observatory = pn_preference_string(OBSERVATORY);
    fits_update_key(fptr, TSTRING, "OBSERVAT", (void *)observatory, "Observatory", &status);
    free(observatory);
    
    char *telescope = pn_preference_string(TELESCOPE);
    fits_update_key(fptr, TSTRING, "TELESCOP", (void *)telescope, "Telescope name", &status);
    free(telescope);
    
    char *instrument = pn_preference_string(INSTRUMENT);
    fits_update_key(fptr, TSTRING, "INSTRUME", (void *)instrument, "Instrument name", &status);
    free(instrument);
    
    char *filter = pn_preference_string(FILTER);
    fits_update_key(fptr, TSTRING, "FILTER", (void *)filter, "Filter type", &status);
    free(filter);
    
    fits_update_key(fptr, TSTRING, "PROG-VER", (void *)program_version() , "Acquisition program version reported by git", &status);
    
    // Trigger timestamp defines the *start* of the frame
    if (trigger_mode != TRIGGER_BIAS)
    {
        TimerTimestamp start = *timestamp;
        TimerTimestamp end = start;
        if (trigger_mode == TRIGGER_MILLISECONDS)
            end.milliseconds += exposure_time;
        else
            end.seconds += exposure_time;
        timestamp_normalize(&end);
    
        char datebuf[15], gpstimebuf[15];
        snprintf(datebuf, 15, "%04d-%02d-%02d", start.year, start.month, start.day);
    
        if (trigger_mode == TRIGGER_MILLISECONDS)
            snprintf(gpstimebuf, 15, "%02d:%02d:%02d.%03d", start.hours, start.minutes, start.seconds, start.milliseconds);
        else
            snprintf(gpstimebuf, 15, "%02d:%02d:%02d", start.hours, start.minutes, start.seconds);
    
        // Used by ImageJ and other programs
        fits_update_key(fptr, TSTRING, "UT_DATE", datebuf, "Exposure start date (GPS)", &status);
        fits_update_key(fptr, TSTRING, "UT_TIME", gpstimebuf, "Exposure start time (GPS)", &status);
    
        // Used by tsreduce
        fits_update_key(fptr, TSTRING, "UTC-DATE", datebuf, "Exposure start date (GPS)", &status);
        fits_update_key(fptr, TSTRING, "UTC-BEG", gpstimebuf, "Exposure start time (GPS)", &status);
    
        if (trigger_mode == TRIGGER_MILLISECONDS)
            snprintf(gpstimebuf, 15, "%02d:%02d:%02d.%03d", end.hours, end.minutes, end.seconds, end.milliseconds);
        else
            snprintf(gpstimebuf, 15, "%02d:%02d:%02d", end.hours, end.minutes, end.seconds);
    
        fits_update_key(fptr, TSTRING, "UTC-END", gpstimebuf, "Exposure end time (GPS)", &status);
        fits_update_key(fptr, TLOGICAL, "UTC-LOCK", &(int){start.locked}, "UTC time has GPS lock", &status);
    }

    time_t pctime = time(NULL);
    
    char timebuf[15];
    strftime(timebuf, 15, "%Y-%m-%d", gmtime(&pctime));
    fits_update_key(fptr, TSTRING, "PC-DATE", (void *)timebuf, "PC Date when frame was saved to disk", &status);
    
    strftime(timebuf, 15, "%H:%M:%S", gmtime(&pctime));
    fits_update_key(fptr, TSTRING, "PC-TIME", (void *)timebuf, "PC Time when frame was saved to disk", &status);

    if (frame->has_timestamp)
        fits_update_key(fptr, TDOUBLE, "CCD-TIME", &frame->timestamp, "CCD time relative to first exposure in seconds", &status);
    
    // Camera temperature
    char tempbuf[10];
    snprintf(tempbuf, 10, "%0.02f", frame->temperature);
    fits_update_key(fptr, TSTRING, "CCD-TEMP", (void *)tempbuf, "CCD temperature at end of exposure (deg C)", &status);
    fits_update_key(fptr, TSTRING, "CCD-PORT", (void *)frame->port_desc, "CCD readout port description", &status);
    fits_update_key(fptr, TSTRING, "CCD-RATE", (void *)frame->speed_desc, "CCD readout rate description", &status);
    fits_update_key(fptr, TSTRING, "CCD-GAIN", (void *)frame->gain_desc, "CCD readout gain description", &status);
    fits_update_key(fptr, TLONG,   "CCD-BIN",  &(long){pn_preference_char(CAMERA_BINNING)},  "CCD pixel binning", &status);
    fits_update_key(fptr, TDOUBLE, "CCD-ROUT",  &frame->readout_time,  "CCD readout time (s)", &status);
    fits_update_key(fptr, TDOUBLE, "CCD-SHFT",  &frame->vertical_shift_us,  "CCD vertical shift time (us)", &status);

    if (frame->has_em_gain)
        fits_update_key(fptr, TDOUBLE,   "CCD-EMGN",  &frame->em_gain,  "CCD electron multiplication gain", &status);

    if (frame->has_exposure_shortcut)
        fits_update_key(fptr, TUSHORT, "CCD-SCUT", &frame->exposure_shortcut_ms, "ProEM exposure shortcut (ms)", &status);

    char *trigger_mode_str;
    switch (trigger_mode)
    {
        case TRIGGER_MILLISECONDS: trigger_mode_str = "High Resolution"; break;
        case TRIGGER_SECONDS: trigger_mode_str = "Low Resolution"; break;
        case TRIGGER_BIAS: trigger_mode_str = "Bias (no triggers)"; break;
            break;
    }
    fits_update_key(fptr, TSTRING, "TRG-MODE", (void *)trigger_mode_str, "Instrument trigger mode", &status);

    if (trigger_mode != TRIGGER_BIAS)
        fits_update_key(fptr, TLOGICAL, "TRG-ALGN", &(int){pn_preference_char(TIMER_ALIGN_FIRST_EXPOSURE)}, "Initial trigger aligned to a full minute", &status);

    char *pscale = pn_preference_string(CAMERA_PLATESCALE);
    fits_update_key(fptr, TDOUBLE, "IM-SCALE",  &(double){pn_preference_char(CAMERA_BINNING)*atof(pscale)},  "Image scale (arcsec/px)", &status);
    free(pscale);
    
    if (frame->has_image_region)
    {
        char buf[25];
        snprintf(buf, 25, "[%d, %d, %d, %d]",
                 frame->image_region[0], frame->image_region[1],
                 frame->image_region[2], frame->image_region[3]);
        fits_update_key(fptr, TSTRING, "IMAG-RGN", buf, "Frame image subregion", &status);
    }
    
    if (frame->has_bias_region)
    {
        char buf[25];
        snprintf(buf, 25, "[%d, %d, %d, %d]",
                 frame->bias_region[0], frame->bias_region[1],
                 frame->bias_region[2], frame->bias_region[3]);
        fits_update_key(fptr, TSTRING, "BIAS-RGN", buf, "Frame bias subregion", &status);
    }
    
    // Write the frame data to the image and close the file
    fits_write_img(fptr, TUSHORT, 1, frame->width*frame->height, frame->data, &status);
    fits_close_file(fptr, &status);
    
    // Log any error messages
    while (fits_read_errmsg(fitserr))
        pn_log("cfitsio error: %s.", fitserr);
    
    return true;
}

// Helper function for determining the
// filepath of the next frame
static char *next_filepath()
{
    // Construct the output filepath from the output dir, run prefix, and run number.
    int run_number = pn_preference_int(RUN_NUMBER);
    char *output_dir = pn_preference_string(OUTPUT_DIR);
    char *run_prefix = pn_preference_string(RUN_PREFIX);

    size_t filepath_len = snprintf(NULL, 0, "%s/%s-%04d.fits.gz", output_dir, run_prefix, run_number) + 1;
    char *filepath = malloc(filepath_len*sizeof(char));

    if (filepath)
        snprintf(filepath, filepath_len, "%s/%s-%04d.fits.gz", output_dir, run_prefix, run_number);

    free(run_prefix);
    free(output_dir);

    return filepath;
}

static char *temporary_filepath(const char *prefix, size_t length)
{
    char *path = malloc((length + 14)*sizeof(char));

    if (path)
    {
        strcpy(path, prefix);

        size_t n = 0;
        do
        {
            // Give up after 1000 failed attempts
            if (n++ > 1000)
            {
                free(path);
                return NULL;
            }

            // Windows will only return numbers in the range 0-0x7FFF
            // but this still gives 32k potential files
            uint32_t test = rand() & 0xFFFF;
            snprintf(path + length, 14, ".%04x.fits.gz", test);
        }
        while (file_exists(path));
    }

    return path;
}

// Save a matched frame and trigger timestamp to disk.
static void save_frame(CameraFrame *frame, TimerTimestamp *timestamp, Modules *modules)
{
    char *filepath = next_filepath();
    if (!filepath)
    {
        pn_log("Failed to determine next file path. Discarding frame");
        return;
    }

    char *temppath = temporary_filepath(filepath, strlen(filepath) - 8);
    if (!temppath)
    {
        pn_log("Failed to create unique temporary filename. Discarding frame");
        return;
    }

    if (!frame_save(frame, timestamp, temppath))
    {
        free(filepath);
        free(temppath);
        pn_log("Failed to save temporary file. Discarding frame.");
        return;
    }

    // Don't overwrite existing files
    if (!rename_atomically(temppath, filepath, false))
        pn_log("Failed to save `%s' (already exists?). Saved instead as `%s' ",
               last_path_component(filepath), last_path_component(temppath));
    else
    {
        reduction_push_frame(modules->reduction, filepath);
        pn_log("Saved `%s'.", last_path_component(filepath));
    }

    pn_preference_increment_framecount();
    free(filepath);
    free(temppath);
}

static void preview_frame(CameraFrame *frame, TimerTimestamp *timestamp, Modules *modules)
{
    // Update frame preview atomically
    char *temp_preview = temporary_filepath("./preview", 9);
    if (!temp_preview)
    {
        pn_log("Error creating temporary filepath. Skipping preview");
        return;
    }

    frame_save(frame, timestamp, temp_preview);
    if (!rename_atomically(temp_preview, "preview.fits.gz", true))
    {
        pn_log("Failed to overwrite preview frame.");
        delete_file(temp_preview);
    }
    else
        preview_script_run(modules->preview);

    free(temp_preview);
}

bool wait_for_next_signal(FrameManager *frame, size_t *queued_frames, size_t *queued_triggers)
{
    *queued_frames = atomicqueue_length(frame->frame_queue);
    *queued_triggers = atomicqueue_length(frame->trigger_queue);

    if (pn_preference_char(TIMER_TRIGGER_MODE) == TRIGGER_BIAS)
        return *queued_frames == 0 && !frame->shutdown;

    return (*queued_frames == 0 || *queued_triggers == 0) && !frame->shutdown;
}

void *frame_thread(void *_modules)
{
    Modules *modules = _modules;
    FrameManager *frame = modules->frame;

    // Loop until shutdown, parsing incoming data
    time_t last_update = 0;
    TimerTimestamp last_preview = system_time();
    int preview_delta = pn_preference_int(PREVIEW_RATE_LIMIT);
    while (true)
    {
        // Wait for a frame to become available
        pthread_mutex_lock(&frame->signal_mutex);
        if (frame->shutdown)
        {
            // Avoid potential race if shutdown is issued early
            pthread_mutex_unlock(&frame->signal_mutex);
            break;
        }

        size_t queued_frames, queued_triggers;

        // Sleep until frame & trigger available, or shutdown.
        while (wait_for_next_signal(frame, &queued_frames, &queued_triggers))
            pthread_cond_wait(&frame->signal_condition, &frame->signal_mutex);

        pthread_mutex_unlock(&frame->signal_mutex);

        // Update status every 5s
        time_t current = time(NULL);
        if (current - last_update > 5)
        {
            pn_log("%zu frames and %zu triggers left to process.", queued_frames, queued_triggers);
            last_update = current;
        }

        if (frame->shutdown)
            break;

        // Match frame with trigger and save to disk
        CameraFrame *f = atomicqueue_pop(frame->frame_queue);
        TimerTimestamp *t = NULL;
        bool process = true;

        uint8_t trigger_mode = pn_preference_char(TIMER_TRIGGER_MODE);
        if (trigger_mode != TRIGGER_BIAS)
        {
            t = atomicqueue_pop(frame->trigger_queue);

            // Convert trigger to start of the exposure
            camera_normalize_trigger(modules->camera, t);

            double exptime = pn_preference_int(EXPOSURE_TIME);
            if (pn_preference_char(TIMER_TRIGGER_MODE) != TRIGGER_SECONDS)
                exptime /= 1000;

            // Ensure that the trigger and frame download times are consistent
            double estimated_start_time = timestamp_to_unixtime(&f->downloaded_time) - f->readout_time - exptime;
            double mismatch = estimated_start_time - timestamp_to_unixtime(t);

            // Allow at least 1 second of leeway to account for the
            // delay in recieving the GPS time and any other factors
            if (fabs(mismatch) > 1.5)
            {
                if (pn_preference_char(VALIDATE_TIMESTAMPS))
                {
                    TimerTimestamp estimate_start = f->downloaded_time;
                    estimate_start.seconds -= f->readout_time + exptime;
                    timestamp_normalize(&estimate_start);

                    pn_log("ERROR: Estimated frame start doesn't match trigger start. Mismatch: %g", mismatch);
                    pn_log("Frame recieved: %02d:%02d:%02d", f->downloaded_time.hours, f->downloaded_time.minutes, f->downloaded_time.seconds);
                    pn_log("Estimated frame start: %02d:%02d:%02d", estimate_start.hours, estimate_start.minutes, estimate_start.seconds);
                    pn_log("Trigger start: %02d:%02d:%02d", t->hours, t->minutes, t->seconds);

                    pn_log("Discarding all stored frames and triggers.");
                    clear_queued_data(false);
                    process = false;
                }
                else
                    pn_log("WARNING: Estimated frame start doesn't match trigger start. Mismatch: %g", mismatch);
            }
        }

        if (process)
        {
            // The first frame from the MicroMax corresponds to the startup
            // and alignment period, so is meaningless
            // The first frame from the ProEM has incorrect cleaning so the
            // bias is inconsistent with the other frames
            if (!frame->first_frame)
            {
                frame_process_transforms(f);
                if (pn_preference_char(SAVE_FRAMES))
                    save_frame(f, t, modules);

                TimerTimestamp cur_preview = system_time();
                double dt = 1000*(timestamp_to_unixtime(&cur_preview) - timestamp_to_unixtime(&last_preview));
                if (dt >= preview_delta)
                {
                    preview_frame(f, t, modules);
                    last_preview = cur_preview;
                }
            }
            else
            {
                pn_log("Discarding first frame.");
                frame->first_frame = false;
            }
        }

        free(t);
        free(f->data);
        free(f);
    }

    frame->thread_alive = false;
    return NULL;
}

void frame_manager_spawn_thread(FrameManager *frame, Modules *modules)
{
    frame->thread_alive = true;
    if (pthread_create(&frame->frame_thread, NULL, frame_thread, (void *)modules))
    {
        pn_log("Failed to create frame thread");
        frame->thread_alive = false;
    }
}

void frame_manager_join_thread(FrameManager *frame)
{
    void **retval = NULL;
    if (frame->thread_alive)
        pthread_join(frame->frame_thread, retval);
}

void frame_manager_notify_shutdown(FrameManager *frame)
{
    pthread_mutex_lock(&frame->signal_mutex);
    frame->shutdown = true;
    pthread_cond_signal(&frame->signal_condition);
    pthread_mutex_unlock(&frame->signal_mutex);
}

bool frame_manager_thread_alive(FrameManager *frame)
{
    return frame->thread_alive;
}

// Called by the camera thread to pass ownership of an acquired
// frame to the main thread for processing.
void frame_manager_queue_frame(FrameManager *frame, CameraFrame *f)
{
    pthread_mutex_lock(&frame->frame_mutex);
    bool success = atomicqueue_push(frame->frame_queue, f);
    pthread_mutex_unlock(&frame->frame_mutex);

    if (!success)
    {
        pn_log("Failed to push frame. Discarding.");
        free(f);
    }

    // Wake processing thread
    pthread_mutex_lock(&frame->signal_mutex);
    pthread_cond_signal(&frame->signal_condition);
    pthread_mutex_unlock(&frame->signal_mutex);
}

// Called by the timer thread to pass ownership of a received
// trigger timestamp to the main thread for processing.
void frame_manager_queue_trigger(FrameManager *frame, TimerTimestamp *t)
{
    pthread_mutex_lock(&frame->frame_mutex);
    bool success = atomicqueue_push(frame->trigger_queue, t);
    pthread_mutex_unlock(&frame->frame_mutex);

    if (!success)
    {
        pn_log("Failed to push trigger. Discarding.");
        free(t);
    }

    // Wake processing thread
    pthread_mutex_lock(&frame->signal_mutex);
    pthread_cond_signal(&frame->signal_condition);
    pthread_mutex_unlock(&frame->signal_mutex);
}


// Called by main thread to remove all queued frames and
// triggers before starting an acquisition or if a match
// error occurs.
void frame_manager_purge_queues(FrameManager *frame, bool reset_first_frame)
{
    void *item;
    pthread_mutex_lock(&frame->frame_mutex);

    size_t discarded = 0;
    while ((item = atomicqueue_pop(frame->frame_queue)) != NULL)
    {
        discarded++;
        free(item);
    }

    if (discarded > 0)
        pn_log("Discarded %zu queued frames.", discarded);

    discarded = 0;
    while ((item = atomicqueue_pop(frame->trigger_queue)) != NULL)
    {
        discarded++;
        free(item);
    }

    if (discarded > 0)
        pn_log("Discarded %zu queued triggers.", discarded);

    if (reset_first_frame)
        frame->first_frame = true;

    pthread_mutex_unlock(&frame->frame_mutex);
}
