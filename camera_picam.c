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
#include <picam_advanced.h>

#include "common.h"
#include "camera.h"
#include "gps.h"
#include "preferences.h"

#pragma mark Camera Routines (Called from camera thread)
extern PNCamera *camera;
extern PNGPS *gps;
static PicamHandle handle = NULL;
static pibyte *image_buffer = NULL;

static void fatal_error(const char *msg, int line)
{
    asprintf(&camera->fatal_error, "FATAL: %s:%d -- %s\n", __FILE__, line, msg);

    // Attempt to die cleanly
    Picam_StopAcquisition(handle);
    Picam_CloseCamera(handle);
    Picam_UninitializeLibrary();
    pthread_exit(NULL);
}

static void print_error(const char *msg, PicamError error)
{
    if (error == PicamError_None)
        return;

    const pichar* string;
    Picam_GetEnumerationString(PicamEnumeratedType_Error, error, &string);
    pn_log("%s: %s", msg, string);
    Picam_DestroyString(string);
}

// Sample the camera temperature to be read by the other threads in a threadsafe manner
static void read_temperature()
{
    piflt temperature;
    PicamError error = Picam_ReadParameterFloatingPointValue(handle, PicamParameter_SensorTemperatureReading, &temperature);
    if (error != PicamError_None)
        print_error("Temperature Read failed", error);

    // TODO: Can query PicamEnumeratedType_SensorTemperatureStatus to get locked/unlocked status

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
        PicamError error = Picam_CommitParameters(handle, &failed_params, &failed_param_count);
        if (error != PicamError_None)
            print_error("Picam_CommitParameters failed", error);

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
    PicamError error;
    set_mode(INITIALISING);

    pthread_mutex_lock(&camera->read_mutex);
    PNCameraMode desired_mode = camera->desired_mode;
    pthread_mutex_unlock(&camera->read_mutex);

    // Loop until a camera is available
    while (desired_mode != SHUTDOWN)
    {
        const PicamCameraID *cameras = NULL;
        piint camera_count = 0;

        Picam_InitializeLibrary();
        error = Picam_GetAvailableCameraIDs(&cameras, &camera_count);

        if (error != PicamError_None || camera_count == 0)
        {
            pn_log("Camera unavailable. Retrying...");

            // Camera detection fails unless we close and initialize the library
            Picam_UninitializeLibrary();

            // Detect and handle shutdown requests
            pthread_mutex_lock(&camera->read_mutex);
            desired_mode = camera->desired_mode;
            pthread_mutex_unlock(&camera->read_mutex);

            millisleep(500);
            continue;
        }

        // Camera found - continue
        error = PicamAdvanced_OpenCameraDevice(&cameras[0], &handle);
        if (error != PicamError_None)
        {
            print_error("PicamAdvanced_OpenCameraDevice failed", error);
            continue;
        }
        break;
    }

    // User has given up waiting for a camera
    if (desired_mode == SHUTDOWN)
    {
        set_mode(UNINITIALIZED);
        return;
    }

    pn_log("Camera available. Initializing...");
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
        print_error("PicamParameter_SensorTemperatureSetPoint failed", error);

    // Enable frame transfer mode
    error = Picam_SetParameterIntegerValue(handle, PicamParameter_ReadoutControlMode, PicamReadoutControlMode_FrameTransfer);
    if (error != PicamError_None)
        print_error("PicamParameter_ReadoutControlMode failed", error);

    // Enable external trigger, negative edge, one frame per pulse, DC coupling
    error = Picam_SetParameterIntegerValue(handle, PicamParameter_TriggerResponse, PicamTriggerResponse_ReadoutPerTrigger);
    if (error != PicamError_None)
        print_error("PicamParameter_TriggerResponse failed", error);

    error = Picam_SetParameterIntegerValue(handle, PicamParameter_TriggerDetermination, PicamTriggerDetermination_FallingEdge);
    if (error != PicamError_None)
        print_error("PicamParameter_TriggerDetermination failed", error);

    // Set output high when the camera is able to respond to a readout trigger
    error = Picam_SetParameterIntegerValue(handle, PicamParameter_OutputSignal, PicamOutputSignal_WaitingForTrigger);
    if (error != PicamError_None)
        print_error("PicamParameter_OutputSignal failed", error);

    // Keep the shutter closed until we start a sequence
    error = Picam_SetParameterIntegerValue(handle, PicamParameter_ShutterTimingMode, PicamShutterTimingMode_AlwaysClosed);
    if (error != PicamError_None)
        pn_log("PicamParameter_ShutterTimingMode failed", error);

    // Use the low noise digitization port
    error = Picam_SetParameterIntegerValue(handle, PicamParameter_AdcQuality, PicamAdcQuality_LowNoise);
    if (error != PicamError_None)
        print_error("PicamParameter_AdcQuality failed", error);

    // Set the requested digitization rate in MHz
    piflt readout_rate = 1; // Default 1MHz
    switch (pn_preference_char(CAMERA_READOUT_MODE))
    {
        case 0: // 100kHz
            readout_rate = 0.1;
            break;
        case 1: // 1MHz
            readout_rate = 1;
            break;
        case 2: // 5MHz
            readout_rate = 5;
            break;
    }

    error = Picam_SetParameterFloatingPointValue(handle, PicamParameter_AdcSpeed, readout_rate);
    if (error != PicamError_None)
        print_error("PicamParameter_AdcSpeed failed", error);

    // Commit parameter changes to hardware
    commit_camera_params();

    pn_log("Camera initialized");
    set_mode(IDLE);
}

// Start an acquisition sequence
static void start_acquiring()
{
    PicamError error;

    set_mode(ACQUIRE_START);
    pn_log("Starting acquisition run...");

    unsigned char superpixel_size = pn_preference_char(SUPERPIXEL_SIZE);
    pn_log("Superpixel size: %d", superpixel_size);

    // Get chip dimensions
    const PicamRoisConstraint  *constraint;
    if (Picam_GetParameterRoisConstraint(handle, PicamParameter_Rois, PicamConstraintCategory_Required, &constraint) != PicamError_None)
        fatal_error("Error determining ROIs Constraint", __LINE__);

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

    // Set ROI to full chip, with requested binning
    PicamRoi *roi = &region->roi_array[0];
    roi->x = constraint->x_constraint.minimum;
    roi->y = constraint->y_constraint.minimum;
    roi->width = constraint->width_constraint.maximum;
    roi->height = constraint->height_constraint.maximum;
    roi->x_binning = superpixel_size;
    roi->y_binning = superpixel_size;

    camera->frame_width  = (uint16_t)(constraint->width_constraint.maximum) / superpixel_size;
    camera->frame_height = (uint16_t)(constraint->height_constraint.maximum) / superpixel_size;

    if (Picam_SetParameterRoisValue(handle, PicamParameter_Rois, region) != PicamError_None)
    {
    	Picam_DestroyRoisConstraints(constraint);
        Picam_DestroyRois(region);
        fatal_error("Error setting ROI", __LINE__);
    }

    Picam_DestroyRoisConstraints(constraint);
    Picam_DestroyRois(region);

    // Set exposure to 0. Actual exposure is controlled by trigger interval, so this value isn't relevant
    // TODO: This actually is relevant... needs working around
    // Set exposure time to GPS exposure - 100ms
    error = Picam_SetParameterFloatingPointValue(handle, PicamParameter_ExposureTime, 1000*pn_preference_char(EXPOSURE_TIME) - 100);
    if (error != PicamError_None)
        print_error("PicamParameter_ExposureTime failed", error);

    // Keep the shutter open during the sequence
    error = Picam_SetParameterIntegerValue(handle, PicamParameter_ShutterTimingMode, PicamShutterTimingMode_AlwaysOpen);
    if (error != PicamError_None)
        print_error("PicamParameter_ShutterTimingMode failed", error);

    // Continue exposing until explicitly stopped or error
    // Requires a user specified image buffer to be provided - the interal
    // routines appears to use this parameter to determine the size of the
    // internal buffer to use.
    error = Picam_SetParameterLargeIntegerValue(handle, PicamParameter_ReadoutCount, 0);
    if (error != PicamError_None)
        print_error("PicamParameter_ReadoutCount failed", error);

    // Create a buffer large enough to hold 5 frames. In normal operation,
    // only one should be required, but we include some overhead to be safe.
    size_t buffer_size = 5;
    piint frame_stride = 0;
    error = Picam_GetParameterIntegerValue(handle, PicamParameter_ReadoutStride, &frame_stride);
    if (error != PicamError_None)
        print_error("PicamParameter_ReadoutStride failed", error);

    image_buffer = (pibyte *)malloc(buffer_size*frame_stride*sizeof(pibyte));
    if (image_buffer == NULL)
        fatal_error("Unable to allocate frame buffer", __LINE__);

    PicamAcquisitionBuffer buffer;
    buffer.memory = image_buffer;
    buffer.memory_size = buffer_size * frame_stride;
    error = PicamAdvanced_SetAcquisitionBuffer(handle, &buffer);
    if (error != PicamError_None)
    {
        print_error("PicamAdvanced_SetAcquisitionBuffer failed", error);
        fatal_error("Acquisition setup failed", __LINE__);
    }

    commit_camera_params();

    error = Picam_StartAcquisition(handle);
    if (error != PicamError_None)
    {
        print_error("Picam_StartAcquisition failed", error);
        fatal_error("Aquisition initialization failed", __LINE__);
    }

    pn_log("Acquisition run started");

    // Sample initial temperature
    read_temperature();

    set_mode(ACQUIRING);
}

// Stop an acquisition sequence
static void stop_acquiring()
{
    set_mode(ACQUIRE_STOP);
    PicamError error = Picam_StopAcquisition(handle);
    if (error != PicamError_None)
        print_error("Picam_StopAcquisition failed", error);

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

    // Clear circular buffer
    PicamAcquisitionBuffer buffer;
    buffer.memory = NULL;
    buffer.memory_size = 0;
    error = PicamAdvanced_SetAcquisitionBuffer(handle, &buffer);
    if (error != PicamError_None)
        print_error("PicamAdvanced_SetAcquisitionBuffer failed", error);
    free(image_buffer);

    // Keep the shutter closed until we start a sequence
    error = Picam_SetParameterIntegerValue(handle, PicamParameter_ShutterTimingMode, PicamShutterTimingMode_AlwaysClosed);
    if (error != PicamError_None)
        print_error("PicamParameter_ShutterTimingMode failed", error);

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

            // Check for buffer overflow
            pibln overran;
            PicamError error = PicamAdvanced_HasAcquisitionBufferOverrun( handle, &overran );
            if (error != PicamError_None)
                print_error("Failed to check for acquisition overflow", error);
            else if (overran)
                fatal_error("Acquisition buffer overflow!", __LINE__);
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
        stop_acquiring();

    // Close the PICAM lib (which in turn closes the camera)
    if (camera->mode == IDLE)
    {
        PicamAdvanced_CloseCameraDevice(handle);
        Picam_UninitializeLibrary();
        pn_log("PICAM uninitialized");
    }

    pthread_exit(NULL);
}

