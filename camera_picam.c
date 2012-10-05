/*
 * Copyright 2010-2012 Paul Chote
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

#include "main.h"
#include "camera.h"
#include "preferences.h"
#include "platform.h"

#pragma mark Camera Routines (Called from camera thread)
extern PNCamera *camera;
static PicamHandle device_handle = NULL;
static PicamHandle model_handle = NULL;
static pibyte *image_buffer = NULL;

static void fatal_error(const char *msg)
{
    trigger_fatal_error(strdup(msg));

    // Attempt to die cleanly
    Picam_StopAcquisition(model_handle);
    PicamAdvanced_CloseCameraDevice(device_handle);
    Picam_UninitializeLibrary();
    pthread_exit(NULL);
}

static void print_error(const char *msg, PicamError error)
{
    if (error == PicamError_None)
        return;

    const pichar* string;
    Picam_GetEnumerationString(PicamEnumeratedType_Error, error, &string);
    pn_log("%s: %s.", msg, string);
    Picam_DestroyString(string);
}


static void set_integer_param(PicamParameter parameter, piint value)
{
    PicamError error = Picam_SetParameterIntegerValue(model_handle, parameter, value);
    if (error != PicamError_None)
    {
        const pichar *name, *err;
        Picam_GetEnumerationString(PicamEnumeratedType_Parameter, parameter, &name);
        Picam_GetEnumerationString(PicamEnumeratedType_Error, error, &err);
        pn_log("Failed to set `%s': %s.", name, err);
        Picam_DestroyString(err);
        Picam_DestroyString(name);
    }
}

// Sample the camera temperature to be read by the other threads in a threadsafe manner
static void read_temperature()
{
    piflt temperature;
    PicamError error = Picam_ReadParameterFloatingPointValue(model_handle, PicamParameter_SensorTemperatureReading, &temperature);
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
    Picam_AreParametersCommitted(model_handle, &all_params_committed);

    if (!all_params_committed)
    {
        const PicamParameter *failed_params = NULL;
        piint failed_param_count = 0;
        PicamError error = Picam_CommitParameters(model_handle, &failed_params, &failed_param_count);
        if (error != PicamError_None)
            print_error("Picam_CommitParameters failed.", error);

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
            fatal_error("Parameter commit failed.");
        }
        Picam_DestroyParameters(failed_params);

        if (PicamAdvanced_CommitParametersToCameraDevice(model_handle) != PicamError_None)
            pn_log("Advanced parameter commit failed.");
    }
}

// Frame status change callback
// - called when any of the following occur during acquisition:
//   - a new readout arrives
//   - acquisition completes due to total readouts acquired
//   - acquisition completes due to a stop request
//   - acquisition completes due to an acquisition error
//   - an acquisition error occurs
// - called on another thread
// - all update callbacks are serialized
PicamError PIL_CALL acquisitionUpdatedCallback(PicamHandle handle, const PicamAvailableData* data,
                                                      const PicamAcquisitionStatus* status)
{
    if (data && data->readout_count && status->errors == PicamAcquisitionErrorsMask_None)
    {
        // Copy frame data and pass ownership to main thread
        CameraFrame *frame = malloc(sizeof(CameraFrame));
        if (frame)
        {
            size_t frame_bytes = camera->frame_width*camera->frame_height*sizeof(uint16_t);
            frame->data = malloc(frame_bytes);
            if (frame->data)
            {
                memcpy(frame->data, data->initial_readout, frame_bytes);
                frame->width = camera->frame_width;
                frame->height = camera->frame_height;
                frame->temperature = camera->temperature;
                queue_framedata(frame);
            }
            else
                pn_log("Failed to allocate CameraFrame->data. Discarding frame.");
        }
        else
            pn_log("Failed to allocate CameraFrame. Discarding frame.");
    }

    // Error
    else if (status->errors != PicamAcquisitionErrorsMask_None)
    {
        // Print errors
        if (status->errors & PicamAcquisitionErrorsMask_DataLost)
            pn_log("Frame data lost. Continuing.");

        if (status->errors & PicamAcquisitionErrorsMask_ConnectionLost)
            pn_log("Camera connection lost. Continuing.");
    }

    // Check for buffer overrun. Should never happen in practice, but we log this
    // to aid future debugging if it does crop up in the field.
    pibln overran;

    PicamError error = PicamAdvanced_HasAcquisitionBufferOverrun(handle, &overran);
    if (error != PicamError_None)
        print_error("Failed to check for acquisition overflow. Continuing.", error);
    else if (overran)
        pn_log("Acquisition buffer overflow! Continuing.");

    return PicamError_None;
}

// Connect to the first available camera
// Expects PICAM to be initialized
static void connect_camera()
{
    pthread_mutex_lock(&camera->read_mutex);
    PNCameraMode desired_mode = camera->desired_mode;
    pthread_mutex_unlock(&camera->read_mutex);

    // Loop until a camera is available or the user gives up
    while (desired_mode != SHUTDOWN)
    {
        const PicamCameraID *cameras = NULL;
        piint camera_count = 0;

        PicamError error = Picam_GetAvailableCameraIDs(&cameras, &camera_count);
        if (error != PicamError_None || camera_count == 0)
        {
            pn_log("Waiting for camera...");

            // Camera detection fails unless we close and initialize the library
            Picam_UninitializeLibrary();
            Picam_InitializeLibrary();

            // Wait a bit longer so we don't spin and eat CPU
            millisleep(500);

            // Detect and handle shutdown requests
            pthread_mutex_lock(&camera->read_mutex);
            desired_mode = camera->desired_mode;
            pthread_mutex_unlock(&camera->read_mutex);
            continue;
        }

        error = PicamAdvanced_OpenCameraDevice(&cameras[0], &device_handle);
        if (error != PicamError_None)
        {
            print_error("PicamAdvanced_OpenCameraDevice failed.", error);
            continue;
        }

        error = PicamAdvanced_GetCameraModel(device_handle, &model_handle);
        if (error != PicamError_None)
        {
            print_error("Failed to query camera model.", error);
            continue;
        }

        return;
    }
}

static void set_readout_port()
{
    // Set readout port by constraint index
    const PicamCollectionConstraint *adc_port_constraint;
    if (Picam_GetParameterCollectionConstraint(model_handle, PicamParameter_AdcQuality, PicamConstraintCategory_Required, &adc_port_constraint) != PicamError_None)
        fatal_error("Failed to query AdcSpeed Constraints.");

    pn_log("Available Readout Ports:");
    for (size_t i = 0; i < adc_port_constraint->values_count; i++)
    {
        const pichar *value;
        Picam_GetEnumerationString(PicamEnumeratedType_AdcQuality, adc_port_constraint->values_array[i], &value);
        pn_log("   %d = %s", i, value);
        Picam_DestroyString(value);
    }

    const size_t readport_id = pn_preference_char(CAMERA_READPORT_MODE);
    if (readport_id < adc_port_constraint->values_count)
    {
        piflt readout_port = adc_port_constraint->values_array[readport_id];
        PicamError error = Picam_SetParameterIntegerValue(model_handle, PicamParameter_AdcQuality, (piint)(readout_port));
        if (error != PicamError_None)
            print_error("Failed to set Readout Port.", error);
    	else
        {
            const pichar *value;
            Picam_GetEnumerationString(PicamEnumeratedType_AdcQuality, readout_port, &value);
            pn_log("Readout Port set to %s.", value);
            Picam_DestroyString(value);
        }
    }
    else
        pn_log("Invalid Readout Port setting: %d. Ignoring value.", readport_id);
}

static void set_readout_speed()
{
    const PicamCollectionConstraint *adc_speed_constraint;
    if (Picam_GetParameterCollectionConstraint(model_handle, PicamParameter_AdcSpeed, PicamConstraintCategory_Required, &adc_speed_constraint) != PicamError_None)
        fatal_error("Failed to query AdcSpeed Constraints.");

    pn_log("Available Readout Speeds:");
    for (size_t i = 0; i < adc_speed_constraint->values_count; i++)
        pn_log("   %d = %0.1f MHz", i, adc_speed_constraint->values_array[i]);

    // Set readout rate by constraint index
    const size_t readspeed_id = pn_preference_char(CAMERA_READSPEED_MODE);
    if (readspeed_id < adc_speed_constraint->values_count)
    {
        piflt readout_rate = adc_speed_constraint->values_array[readspeed_id];
        PicamError error = Picam_SetParameterFloatingPointValue(model_handle, PicamParameter_AdcSpeed, readout_rate);
        if (error != PicamError_None)
            print_error("Failed to set Readout Speed.", error);
    	else
            pn_log("Readout Speed set to %0.1f MHz.", readout_rate);
    }
    else
        pn_log("Invalid Readout Speed setting: %d. Ignoring value.", readspeed_id);
}

static void set_readout_gain()
{
    // Set readout port by constraint index
    const PicamCollectionConstraint *adc_gain_constraint;
    if (Picam_GetParameterCollectionConstraint(model_handle, PicamParameter_AdcAnalogGain, PicamConstraintCategory_Required, &adc_gain_constraint) != PicamError_None)
        fatal_error("Failed to query AdcGain Constraints.");

    pn_log("Available Gain Settings:");
    for (size_t i = 0; i < adc_gain_constraint->values_count; i++)
    {
        const pichar *value;
        Picam_GetEnumerationString(PicamEnumeratedType_AdcAnalogGain, adc_gain_constraint->values_array[i], &value);
        pn_log("   %d = %s", i, value);
        Picam_DestroyString(value);
    }

    const size_t readgain_id = pn_preference_char(CAMERA_GAIN_MODE);
    if (readgain_id < adc_gain_constraint->values_count)
    {
        piflt readout_gain = adc_gain_constraint->values_array[readgain_id];
        PicamError error = Picam_SetParameterIntegerValue(model_handle, PicamParameter_AdcAnalogGain, (piint)(readout_gain));
        if (error != PicamError_None)
            print_error("Failed to set Readout Gain.", error);
    	else
        {
            const pichar *value;
            Picam_GetEnumerationString(PicamEnumeratedType_AdcAnalogGain, readout_gain, &value);
            pn_log("Readout Gain set to %s.", value);
            Picam_DestroyString(value);
        }
    }
    else
        pn_log("Invalid Readout Gain setting: %d. Ignoring value.", readgain_id);
}

static void calculate_readout_time()
{
    // Query readout time - this determines the minimum allowable exposure time
    piflt readout_time;
    PicamError error = Picam_GetParameterFloatingPointValue(model_handle, PicamParameter_ReadoutTimeCalculation, &readout_time);
    if (error != PicamError_None)
        print_error("Failed to query temperature.", error);

    // Convert to seconds
    readout_time /= 1000;
    pthread_mutex_lock(&camera->read_mutex);
    camera->readout_time = readout_time;
    pn_log("Frame Readout: %.2f seconds.", readout_time);
    pthread_mutex_unlock(&camera->read_mutex);
}

// Initialize PICAM and the camera hardware
static void initialize_camera()
{
    set_mode(INITIALISING);
    Picam_InitializeLibrary();

    connect_camera();

    pthread_mutex_lock(&camera->read_mutex);
    PNCameraMode desired_mode = camera->desired_mode;
    pthread_mutex_unlock(&camera->read_mutex);

    // User has given up waiting for a camera
    if (desired_mode == SHUTDOWN)
    {
        set_mode(UNINITIALIZED);
        return;
    }

    pn_log("Initializing camera...");
    PicamCameraID id;
    Picam_GetCameraID(device_handle, &id);

    // Query camera model info
    const pichar *string;
    Picam_GetEnumerationString(PicamEnumeratedType_Model, id.model, &string);
    pn_log("Camera ID: %s (SN:%s) [%s].", string, id.serial_number, id.sensor_name);
    Picam_DestroyString(string);
    Picam_DestroyCameraIDs(&id);

    // Set temperature
    PicamError error = Picam_SetParameterFloatingPointValue(model_handle, PicamParameter_SensorTemperatureSetPoint, pn_preference_int(CAMERA_TEMPERATURE)/100.0f);
    if (error != PicamError_None)
        print_error("Failed to set `PicamParameter_SensorTemperatureSetPoint'.", error);

    // Enable frame transfer mode
    set_integer_param(PicamParameter_ReadoutControlMode, PicamReadoutControlMode_FrameTransfer);

    // Enable external trigger
    set_integer_param(PicamParameter_TriggerResponse, PicamTriggerResponse_ReadoutPerTrigger);

    // Set falling edge trigger (actually low level trigger)
    set_integer_param(PicamParameter_TriggerDetermination, PicamTriggerDetermination_FallingEdge);

    // Set output high when the camera is able to respond to a readout trigger
    set_integer_param(PicamParameter_OutputSignal, PicamOutputSignal_WaitingForTrigger);

    // Keep the shutter closed until we start a sequence
    set_integer_param(PicamParameter_ShutterTimingMode, PicamShutterTimingMode_AlwaysClosed);

    set_readout_port();
    set_readout_speed();
    set_readout_gain();

    unsigned char superpixel_size = pn_preference_char(CAMERA_PIXEL_SIZE);

    // Get chip dimensions
    const PicamRoisConstraint  *constraint;
    if (Picam_GetParameterRoisConstraint(model_handle, PicamParameter_Rois, PicamConstraintCategory_Required, &constraint) != PicamError_None)
        fatal_error("Failed to query ROIs Constraint.");

    // Get region definition
    const PicamRois *region;
    if (Picam_GetParameterRoisValue(model_handle, PicamParameter_Rois, &region) != PicamError_None)
    {
        Picam_DestroyRoisConstraints(constraint);
        Picam_DestroyRois(region);
        fatal_error("Failed to query current ROI.");
    }

    // Set ROI to full chip, with requested binning
    PicamRoi *roi = &region->roi_array[0];
    roi->x = constraint->x_constraint.minimum;
    roi->y = constraint->y_constraint.minimum;
    roi->width = constraint->width_constraint.maximum;
    roi->height = constraint->height_constraint.maximum;
    roi->x_binning = superpixel_size;
    roi->y_binning = superpixel_size;

    pn_log("Pixel size set to %dx%d.", superpixel_size, superpixel_size);

    camera->frame_width  = (uint16_t)(constraint->width_constraint.maximum) / superpixel_size;
    camera->frame_height = (uint16_t)(constraint->height_constraint.maximum) / superpixel_size;

    if (Picam_SetParameterRoisValue(model_handle, PicamParameter_Rois, region) != PicamError_None)
    {
    	Picam_DestroyRoisConstraints(constraint);
        Picam_DestroyRois(region);
        fatal_error("Failed to set ROI");
    }

    Picam_DestroyRoisConstraints(constraint);
    Picam_DestroyRois(region);

    calculate_readout_time();

    // Continue exposing until explicitly stopped or error
    // Requires a user specified image buffer to be provided - the interal
    // routines appears to use this parameter to determine the size of the
    // internal buffer to use.
    error = Picam_SetParameterLargeIntegerValue(model_handle, PicamParameter_ReadoutCount, 0);
    if (error != PicamError_None)
        print_error("Failed to set PicamParameter_ReadoutCount.", error);

    // Create a buffer large enough to hold 5 frames. In normal operation,
    // only one should be required, but we include some overhead to be safe.
    size_t buffer_size = 5;
    piint frame_stride = 0;
    error = Picam_GetParameterIntegerValue(model_handle, PicamParameter_ReadoutStride, &frame_stride);
    if (error != PicamError_None)
        print_error("Failed to set PicamParameter_ReadoutStride.", error);

    image_buffer = (pibyte *)malloc(buffer_size*frame_stride*sizeof(pibyte));
    if (image_buffer == NULL)
        fatal_error("Failed to allocate frame buffer.");

    PicamAcquisitionBuffer buffer =
    {
        .memory = image_buffer,
        .memory_size = buffer_size*frame_stride
    };

    error = PicamAdvanced_SetAcquisitionBuffer(device_handle, &buffer);
    if (error != PicamError_None)
    {
        print_error("PicamAdvanced_SetAcquisitionBuffer failed.", error);
        fatal_error("Acquisition setup failed.");
    }

    // Commit parameter changes to hardware
    commit_camera_params();

    // Register callback for acquisition status change / frame available
    error = PicamAdvanced_RegisterForAcquisitionUpdated(device_handle, acquisitionUpdatedCallback);
    if (error != PicamError_None)
    {
        print_error("PicamAdvanced_RegisterForAcquisitionUpdated failed.", error);
        fatal_error("Acquisition setup failed.");
    }

    pn_log("Camera is now idle.");
    set_mode(IDLE);
}

// Start an acquisition sequence
static void start_acquiring()
{
    PicamError error;

    set_mode(ACQUIRE_START);
    pn_log("Camera is preparing for acquisition.");

    // The ProEM camera cannot be operated in trigger = download mode
    // Instead, we set an exposure period 20ms shorter than the trigger period,
    // giving the camera a short window to be responsive for the next trigger
    piflt exptime = 1000*pn_preference_char(EXPOSURE_TIME) - 20;
    error = Picam_SetParameterFloatingPointValue(model_handle, PicamParameter_ExposureTime, exptime);
    if (error != PicamError_None)
        print_error("PicamParameter_ExposureTime failed.", error);

    // Keep the shutter open during the sequence
    set_integer_param(PicamParameter_ShutterTimingMode, PicamShutterTimingMode_AlwaysOpen);

    commit_camera_params();

    error = Picam_StartAcquisition(model_handle);
    if (error != PicamError_None)
    {
        print_error("Picam_StartAcquisition failed.", error);
        fatal_error("Aquisition initialization failed.");
    }

    camera->safe_to_stop_acquiring = false;
    pn_log("Camera is now acquiring.");
    set_mode(ACQUIRING);
}

// Stop an acquisition sequence
static void stop_acquiring()
{
    set_mode(ACQUIRE_STOP);

    pibln running;
    PicamError error = Picam_IsAcquisitionRunning(model_handle, &running);
    if (error != PicamError_None)
        print_error("Picam_IsAcquisitionRunning failed.", error);

    if (running)
    {
        error = Picam_StopAcquisition(model_handle);
        if (error != PicamError_None)
            print_error("Picam_StopAcquisition failed.", error);
    }

    error = Picam_IsAcquisitionRunning(model_handle, &running);
    if (error != PicamError_None)
        print_error("Picam_IsAcquisitionRunning failed.", error);

    if (running)
        fatal_error("Failed to stop acquisition");
    else
        pn_log("Camera is now idle.");

    // Close the shutter until the next exposure sequence
    set_integer_param(PicamParameter_ShutterTimingMode, PicamShutterTimingMode_AlwaysClosed);
    commit_camera_params();
    
    set_mode(IDLE);
}

// Main camera thread loop
void *pn_picam_camera_thread(void *_args)
{
    ThreadCreationArgs *args = (ThreadCreationArgs *)_args;
    PNCamera *camera = args->camera;

    // Initialize the camera
    initialize_camera();

    // Loop and respond to user commands
    int temp_ticks = 0;

    pthread_mutex_lock(&camera->read_mutex);
    PNCameraMode desired_mode = camera->desired_mode;
    bool safe_to_stop_acquiring = camera->safe_to_stop_acquiring;
    pthread_mutex_unlock(&camera->read_mutex);

    while (desired_mode != SHUTDOWN)
    {
        // Start/stop acquisition
        if (desired_mode == ACQUIRING && camera->mode == IDLE)
            start_acquiring();

        if (desired_mode == IDLE && camera->mode == ACQUIRING)
            camera->mode = IDLE_WHEN_SAFE;

        // Stop acquisition
        if (camera->mode == IDLE_WHEN_SAFE && safe_to_stop_acquiring)
            stop_acquiring();

        // Check temperature every second
        if (++temp_ticks >= 10)
        {
            temp_ticks = 0;
            read_temperature();
        }

        millisleep(100);
        pthread_mutex_lock(&camera->read_mutex);
        desired_mode = camera->desired_mode;
        safe_to_stop_acquiring = camera->safe_to_stop_acquiring;
        pthread_mutex_unlock(&camera->read_mutex);
    }

    // Stop aquisition sequence if necessary
    if (camera->mode == ACQUIRING || camera->mode == IDLE_WHEN_SAFE)
        stop_acquiring();

    // Shutdown camera and PICAM
    PicamAcquisitionBuffer buffer = {.memory = NULL, .memory_size = 0};
    PicamError error = PicamAdvanced_SetAcquisitionBuffer(device_handle, &buffer);
    if (error != PicamError_None)
        print_error("PicamAdvanced_SetAcquisitionBuffer failed.", error);
    free(image_buffer);

    PicamAdvanced_UnregisterForAcquisitionUpdated(device_handle, acquisitionUpdatedCallback);
    PicamAdvanced_CloseCameraDevice(device_handle);
    Picam_UninitializeLibrary();

    pn_log("Camera uninitialized.");
    return NULL;
}

