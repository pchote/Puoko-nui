/*
* Copyright 2010, 2011, 2012 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <picam.h>

#include "common.h"
#include "camera.h"
#include "gps.h"
#include "preferences.h"

#pragma mark Camera Routines (Called from camera thread)
extern PNCamera *camera;
extern PNGPS *gps;
static PicamHandle handle = NULL;

static void fatal_error(const char *msg, int line)
{
    asprintf(&camera->fatal_error, "FATAL: %s:%d -- %s\n", __FILE__, line, msg);

    // Attempt to die cleanly
    Picam_StopAcquisition(handle);
    Picam_CloseCamera(handle);
    Picam_UninitializeLibrary();
    pthread_exit(NULL);
}

// Sample the camera temperature to be read by the other threads in a threadsafe manner
static void read_temperature()
{    
    piflt temperature;
    Picam_ReadParameterFloatingPointValue(handle, PicamParameter_SensorTemperatureReading, &temperature);
    pthread_mutex_lock(&camera->read_mutex);
    camera->temperature = temperature;
    pthread_mutex_unlock(&camera->read_mutex);
}

static void commit_camera_params()
{
    pibln all_params_committed;
    Picam_AreParametersCommitted(handle, &all_params_committed);

    if (!all_params_committed)
    {
        const PicamParameter *failed_params = NULL;
        piint failed_param_count = 0;
        Picam_CommitParameters(handle, &failed_params, &failed_param_count);

        if (failed_param_count > 0)
        {
            pn_log("%d parameters failed to commit:", failed_param_count);
            for (piint i = 0; i < failed_param_count; i++)
            {
                const pichar *name;
                Picam_GetEnumerationString(PicamEnumeratedType_Parameter, failed_params[i], &name);
                pn_log("   %s", name);
                Picam_DestroyString(name);
            }
            Picam_DestroyParameters(failed_params);
            fatal_error("Parameter commit failed", __LINE__);
        }
    }
}

// Initialize PICAM and the camera hardware
static void initialize_camera()
{
    set_mode(INITIALISING);

    pthread_mutex_lock(&camera->read_mutex);
    PNCameraMode desired_mode = camera->desired_mode;
    pthread_mutex_unlock(&camera->read_mutex);

    // Wait for a camera to be available
    while (desired_mode != SHUTDOWN)
    {
        Picam_InitializeLibrary();
        if (Picam_OpenFirstCamera(&handle) == PicamError_None)
            break; // Camera found

        pn_log("Camera unavailable. Retrying...");

        pthread_mutex_lock(&camera->read_mutex);
        desired_mode = camera->desired_mode;
        pthread_mutex_unlock(&camera->read_mutex);

        // TODO: Won't detect the camera when it becomes available
        // unless we close and reopen the library
        Picam_UninitializeLibrary();
    }

    // User has given up waiting for a camera
    if (desired_mode == SHUTDOWN)
    {
        set_mode(UNINITIALIZED);
        return;
    }

    pn_log("Camera available. Initializing...");
    PicamError error;
    PicamCameraID id;
    Picam_GetCameraID(handle, &id);

    // Query camera model info
    const pichar *string;
    Picam_GetEnumerationString(PicamEnumeratedType_Model, id.model, &string);
    pn_log("%s (SN:%s) [%s]\r\n", string, id.serial_number, id.sensor_name);
    Picam_DestroyString(string);

    // Set temperature
    error = Picam_SetParameterFloatingPointValue(handle, PicamParameter_SensorTemperatureSetPoint, pn_preference_int(CAMERA_TEMPERATURE)/100.0f);
    if (error != PicamError_None)
        pn_log("PicamParameter_SensorTemperatureSetPoint failed. Errorcode: %d", error);

    // Enable frame transfer mode
    error = Picam_SetParameterIntegerValue(handle, PicamParameter_ReadoutControlMode, PicamReadoutControlMode_FrameTransfer);
    if (error != PicamError_None)
        pn_log("PicamParameter_ReadoutControlMode failed. Errorcode: %d", error);

    // Enable external trigger, negative edge, one frame per pulse, DC coupling
    error = Picam_SetParameterIntegerValue(handle, PicamParameter_TriggerResponse, PicamTriggerResponse_ReadoutPerTrigger);
    if (error != PicamError_None)
        pn_log("PicamParameter_TriggerResponse failed. Errorcode: %d", error);
    error = Picam_SetParameterIntegerValue(handle, PicamParameter_TriggerDetermination, PicamTriggerDetermination_FallingEdge);
    if (error != PicamError_None)
        pn_log("PicamParameter_TriggerDetermination failed. Errorcode: %d", error);

    // Set output low while the camera is reading out a frame
    // Maybe use PicamOutputSignal_Busy instead?
    error = Picam_SetParameterIntegerValue(handle, PicamParameter_OutputSignal, PicamOutputSignal_ReadingOut);
    if (error != PicamError_None)
        pn_log("PicamParameter_OutputSignal failed. Errorcode: %d", error);
    error = Picam_SetParameterIntegerValue(handle, PicamParameter_InvertOutputSignal, 1);
    if (error != PicamError_None)
        pn_log("PicamParameter_InvertOutputSignal failed. Errorcode: %d", error);

    // Keep the shutter closed until we start a sequence
    error = Picam_SetParameterIntegerValue(handle, PicamParameter_ShutterTimingMode, PicamShutterTimingMode_AlwaysClosed);
    if (error != PicamError_None)
        pn_log("PicamParameter_ShutterTimingMode failed. Errorcode: %d", error);

    // TODO: PicamParameter_AdcSpeed: Digitization rate in MHz

    // Commit parameter changes to hardware
    commit_camera_params();

    pn_log("Camera initialized");
    set_mode(IDLE);
}

// Start an acquisition sequence
static void start_acquiring()
{
    set_mode(ACQUIRE_START);
    pn_log("Starting acquisition run...");

    camera->frame_width = 1024;
    camera->frame_height = 1024;
/*
    // Get chip dimensions
    const PicamRoisConstraint  *constraint;
    if (Picam_GetParameterRoisConstraint(handle, PicamParameter_Rois, PicamConstraintCategory_Required, &constraint) != PicamError_None)
        fatal_error("Error determining ROIs Constraint", __LINE__);

    camera->frame_width = constraint->width_constraint.maximum - constraint->width_constraint.minimum;
    camera->frame_height = constraint->height_constraint.maximum - constraint->height_constraint.minimum;

    // TODO: FIXME: default region gives: ROI Area: [0:1023] x [0:2]
    pn_log("ROI Area: [%d:%d] x [%d:%d]",
    constraint->width_constraint.minimum, constraint->width_constraint.maximum,
    constraint->height_constraint.minimum, constraint->height_constraint.maximum);

    // Define ROI as entire chip, with requested binning
    unsigned char superpixel_size = pn_preference_char(SUPERPIXEL_SIZE);
    pn_log("Superpixel size: %d", superpixel_size);

    // Get region definition
    const PicamRois *region;
    if (Picam_GetParameterRoisValue(handle, PicamParameter_Rois, &region) != PicamError_None)
    {
        Picam_DestroyRoisConstraints(constraint);
        Picam_DestroyRois(region);
        fatal_error("Error determining current ROI", __LINE__);
    }

    if (region->roi_count != 1)
    {
        pn_log("region has %d ROIs", region->roi_count);
        Picam_DestroyRoisConstraints(constraint);
        Picam_DestroyRois(region);
        fatal_error("Unsure how to proceed", __LINE__);
    }

    PicamRoi *roi = &region->roi_array[0];
    roi->x = constraint->width_constraint.minimum;
    roi->y = constraint->width_constraint.maximum;
    roi->width = camera->frame_width;
    roi->height = camera->frame_height;
    roi->x_binning = superpixel_size;
    roi->y_binning = superpixel_size;
    Picam_DestroyRoisConstraints(constraint);

    camera->frame_height /= superpixel_size;
    camera->frame_width /= superpixel_size;

    if (Picam_SetParameterRoisValue(handle, PicamParameter_Rois, region) != PicamError_None)
    {
        Picam_DestroyRois(region);
        fatal_error("Error setting ROI", __LINE__);
    }
    Picam_DestroyRois(region);
*/

    // Continue exposing until explicitly stopped or error
    // TODO: This should be set to 0 to allow unlimited frames, but this causes a segfault
    Picam_SetParameterLargeIntegerValue(handle, PicamParameter_ReadoutCount, 1E3);

    // Set exposure to 0. Actual exposure is controlled by trigger interval, so this value isn't relevant
    // TODO: This actually is relevant... needs working around
    // Set exposure time to GPS exposure - 100ms
    Picam_SetParameterFloatingPointValue(handle, PicamParameter_ExposureTime, 1000*pn_preference_char(EXPOSURE_TIME) - 100);

    // Keep the shutter open during the sequence
    Picam_SetParameterIntegerValue(handle, PicamParameter_ShutterTimingMode, PicamShutterTimingMode_AlwaysOpen);

    pn_log("About to commit");
    commit_camera_params();
    pn_log("Committed");

    Picam_StartAcquisition(handle);

    pn_log("Acquisition run started");

    // Sample initial temperature
    read_temperature();

    set_mode(ACQUIRING);
}

// Stop an acquisition sequence
static void stop_acquiring()
{
    set_mode(ACQUIRE_STOP);
    Picam_StopAcquisition(handle);

    // Picam_WaitForAcquisitionUpdate must be called until status.running is false;
    // this is true regardless of acquisition errors or calling Picam_StopAcquisition.
    pibln running = true;
    PicamAvailableData data;
    PicamAcquisitionStatus status;
    while (running)
    {
        Picam_WaitForAcquisitionUpdate(handle, 100, &data, &status);
        running = status.running;
    }

    // Keep the shutter closed until we start a sequence
    Picam_SetParameterIntegerValue(handle, PicamParameter_ShutterTimingMode, PicamShutterTimingMode_AlwaysClosed);
    commit_camera_params();

    camera->first_frame = true;

    pn_log("Acquisition sequence uninitialized");
    set_mode(IDLE);
}

// Main camera thread loop
void *pn_picam_camera_thread(void *_unused)
{
    // Initialize the camera
    initialize_camera();

    // Loop and respond to user commands
    int temp_ticks = 0;

    pthread_mutex_lock(&camera->read_mutex);
    PNCameraMode desired_mode = camera->desired_mode;
    pthread_mutex_unlock(&camera->read_mutex);

    PicamAvailableData data;
    PicamAcquisitionStatus status;

    while (desired_mode != SHUTDOWN)
    {
        // Start/stop acquisition
        if (desired_mode == ACQUIRING && camera->mode == IDLE)
            start_acquiring();

        if (desired_mode == ACQUIRE_WAIT && camera->mode == ACQUIRING)
            camera->mode = ACQUIRE_WAIT;

        if (desired_mode == IDLE && camera->mode == ACQUIRE_WAIT)
            stop_acquiring();

        if (camera->mode == ACQUIRING)
        {
            // Wait up to 100ms for a new frame, otherwise timeout and continue
            PicamError ret = Picam_WaitForAcquisitionUpdate(handle, 100, &data, &status);

            // New frame
            if (ret == PicamError_None && status.errors == PicamAcquisitionErrorsMask_None &&
                data.readout_count)
            {
                pn_log("Frame available @ %d", (int)time(NULL));

                // Do something with the frame data
                PNFrame frame;            
                frame.width = camera->frame_width;
                frame.height = camera->frame_height;
                frame.data = data.initial_readout;
                frame_downloaded(&frame);
            }
            // Error
            else if (status.errors != PicamAcquisitionErrorsMask_None)
            {
                // Print errors
                if (status.errors &= PicamAcquisitionErrorsMask_DataLost)
                    pn_log("Error: Data lost");
                
                if (status.errors &= PicamAcquisitionErrorsMask_ConnectionLost)
                    pn_log("Error: Camera connection lost");
            }
        }

        // Check temperature
        if (++temp_ticks >= 50)
        {
            temp_ticks = 0;
            read_temperature();
        }

        pthread_mutex_lock(&camera->read_mutex);
        desired_mode = camera->desired_mode;
        pthread_mutex_unlock(&camera->read_mutex);
    }

    // Shutdown camera
    if (camera->mode == ACQUIRING || camera->mode == ACQUIRE_WAIT)
    {
        stop_acquiring();
    }
    
    // Close the PICAM lib (which in turn closes the camera)
    if (camera->mode == IDLE)
    {
        Picam_CloseCamera( handle );
        Picam_UninitializeLibrary();
        pn_log("PICAM uninitialized");
    }

    pthread_exit(NULL);
}

