/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <picam.h>
#include <picam_advanced.h>

#include "main.h"
#include "camera.h"
#include "preferences.h"
#include "platform.h"

struct internal
{
    PicamHandle device_handle;
    PicamHandle model_handle;
    pibyte *image_buffer;

    uint16_t frame_width;
    uint16_t frame_height;
    size_t frame_bytes;

    uint64_t timestamp_resolution;
    uint64_t start_timestamp;
    bool first_frame;
};

static void fatal_error(struct internal *internal, char *format, ...)
{
    va_list args;
    va_start(args, format);
    int len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    char *err = malloc((len + 1)*sizeof(char));
    if (err)
    {
        va_start(args, format);
        vsnprintf(err, len + 1, format, args);
        va_end(args);
    }
    else
        pn_log("Failed to allocate memory for fatal error.");

    trigger_fatal_error(err);

    // Attempt to die cleanly
    Picam_StopAcquisition(internal->model_handle);
    PicamAdvanced_CloseCameraDevice(internal->device_handle);
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

static void set_integer_param(PicamHandle model_handle, PicamParameter parameter, piint value)
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

static void commit_camera_params(struct internal *internal)
{
    pibln all_params_committed;
    Picam_AreParametersCommitted(internal->model_handle, &all_params_committed);

    if (!all_params_committed)
    {
        const PicamParameter *failed_params = NULL;
        piint failed_param_count = 0;
        PicamError error = Picam_CommitParameters(internal->model_handle, &failed_params, &failed_param_count);
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
            fatal_error(internal, "Parameter commit failed.");
        }
        Picam_DestroyParameters(failed_params);

        if (PicamAdvanced_CommitParametersToCameraDevice(internal->model_handle) != PicamError_None)
            pn_log("Advanced parameter commit failed.");
    }
}

static double read_temperature(PicamHandle model_handle)
{
    piflt temperature;
    PicamError error = Picam_ReadParameterFloatingPointValue(model_handle, PicamParameter_SensorTemperatureReading, &temperature);
    if (error != PicamError_None)
        print_error("Temperature Read failed", error);

    // TODO: Can query PicamEnumeratedType_SensorTemperatureStatus to get locked/unlocked status
    return temperature;
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
static struct internal *callback_internal_ref;
PicamError PIL_CALL acquisitionUpdatedCallback(PicamHandle handle, const PicamAvailableData* data,
                                                      const PicamAcquisitionStatus* status)
{
    if (data && data->readout_count && status->errors == PicamAcquisitionErrorsMask_None)
    {
        // Copy frame data and pass ownership to main thread
        CameraFrame *frame = malloc(sizeof(CameraFrame));
        if (frame)
        {
            frame->data = malloc(callback_internal_ref->frame_bytes);
            if (frame->data)
            {
                // Calculate camera timestamp
                uint64_t timestamp = *(uint64_t *)(data->initial_readout + callback_internal_ref->frame_bytes);

                if (callback_internal_ref->first_frame)
                {
                    callback_internal_ref->start_timestamp = timestamp;
                    timestamp = 0;
                    callback_internal_ref->first_frame = false;
                }
                else
                    timestamp -= callback_internal_ref->start_timestamp;

                memcpy(frame->data, data->initial_readout, callback_internal_ref->frame_bytes);
                frame->width = callback_internal_ref->frame_width;
                frame->height = callback_internal_ref->frame_height;
                frame->temperature = read_temperature(callback_internal_ref->model_handle);

                frame->has_timestamp = true;
                frame->timestamp = timestamp*1.0/callback_internal_ref->timestamp_resolution;

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
static void connect_camera(Camera *camera, struct internal *internal)
{
    // Loop until a camera is available or the user gives up
    while (camera_desired_mode(camera) != SHUTDOWN)
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

            continue;
        }

        error = PicamAdvanced_OpenCameraDevice(&cameras[0], &internal->device_handle);
        if (error != PicamError_None)
        {
            print_error("PicamAdvanced_OpenCameraDevice failed.", error);
            continue;
        }

        error = PicamAdvanced_GetCameraModel(internal->device_handle, &internal->model_handle);
        if (error != PicamError_None)
        {
            print_error("Failed to query camera model.", error);
            continue;
        }

        return;
    }
}

void *camera_picam_initialize(Camera *camera, ThreadCreationArgs *args)
{
    struct internal *internal = calloc(1, sizeof(struct internal));
    if (!internal)
        return NULL;

    Picam_InitializeLibrary();

    connect_camera(camera, internal);

    // User has given up waiting for a camera
    if (camera_desired_mode(camera) == SHUTDOWN)
        return NULL;

    PicamCameraID id;
    Picam_GetCameraID(internal->device_handle, &id);

    // Query camera model info
    const pichar *string;
    Picam_GetEnumerationString(PicamEnumeratedType_Model, id.model, &string);
    pn_log("Camera ID: %s (SN:%s) [%s].", string, id.serial_number, id.sensor_name);
    Picam_DestroyString(string);
    Picam_DestroyCameraIDs(&id);

    // Enable frame transfer mode
    set_integer_param(internal->model_handle, PicamParameter_ReadoutControlMode, PicamReadoutControlMode_FrameTransfer);

    // Enable external trigger
    set_integer_param(internal->model_handle, PicamParameter_TriggerResponse, PicamTriggerResponse_ReadoutPerTrigger);

    // Set falling edge trigger (actually low level trigger)
    set_integer_param(internal->model_handle, PicamParameter_TriggerDetermination, PicamTriggerDetermination_FallingEdge);

    // Set output high when the camera is able to respond to a readout trigger
    set_integer_param(internal->model_handle, PicamParameter_OutputSignal, PicamOutputSignal_WaitingForTrigger);

    // Disable CCD cleaning between exposures
    set_integer_param(internal->model_handle, PicamParameter_CleanUntilTrigger, 0);
    set_integer_param(internal->model_handle, PicamParameter_CleanBeforeExposure, 0);

    // Keep the shutter closed until we start a sequence
    set_integer_param(internal->model_handle, PicamParameter_ShutterTimingMode, PicamShutterTimingMode_AlwaysClosed);

    // Tag each frame with start time from the camera
    set_integer_param(internal->model_handle, PicamParameter_TimeStamps, PicamTimeStampsMask_ExposureStarted);

    pi64s timestamp_resolution;
    PicamError error = Picam_GetParameterLargeIntegerValue(internal->model_handle, PicamParameter_TimeStampResolution, &timestamp_resolution);
    if (error != PicamError_None)
        print_error("Failed to set PicamParameter_TimeStampResolution.", error);
    internal->timestamp_resolution = timestamp_resolution;

    // Continue exposing until explicitly stopped or error
    // Requires a user specified image buffer to be provided - the interal
    // routines appears to use this parameter to determine the size of the
    // internal buffer to use.
    error = Picam_SetParameterLargeIntegerValue(internal->model_handle, PicamParameter_ReadoutCount, 0);
    if (error != PicamError_None)
        print_error("Failed to set PicamParameter_ReadoutCount.", error);

    // Commit parameter changes to hardware
    commit_camera_params(internal);

    // Register callback for acquisition status change / frame available
    callback_internal_ref = internal;
    error = PicamAdvanced_RegisterForAcquisitionUpdated(internal->device_handle, acquisitionUpdatedCallback);
    if (error != PicamError_None)
    {
        print_error("PicamAdvanced_RegisterForAcquisitionUpdated failed.", error);
        fatal_error(internal, "Acquisition setup failed.");
    }

    return internal;
}

double camera_picam_update_camera_settings(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;
    PicamError error;

    // Validate and set readout port
    const PicamCollectionConstraint *port_constraint;
    if (Picam_GetParameterCollectionConstraint(internal->model_handle, PicamParameter_AdcQuality,
                                               PicamConstraintCategory_Required, &port_constraint) != PicamError_None)
        fatal_error(internal, "Failed to query AdcSpeed Constraints.");

    uint8_t port_id = pn_preference_char(CAMERA_READPORT_MODE);
    if (port_id >= port_constraint->values_count)
    {
        pn_log("Invalid port index: %d. Reset to %d.", port_id, 0);
        pn_preference_set_char(CAMERA_READPORT_MODE, 0);
        port_id = 0;
    }
    set_integer_param(internal->model_handle, PicamParameter_AdcQuality,
                      (piint)(port_constraint->values_array[port_id]));
    Picam_DestroyCollectionConstraints(port_constraint);

    // Validate and set readout speed
    const PicamCollectionConstraint *speed_constraint;
    if (Picam_GetParameterCollectionConstraint(internal->model_handle, PicamParameter_AdcSpeed,
                                               PicamConstraintCategory_Required, &speed_constraint) != PicamError_None)
        fatal_error(internal, "Failed to query AdcSpeed Constraints.");

    uint8_t speed_id = pn_preference_char(CAMERA_READSPEED_MODE);
    if (speed_id >= speed_constraint->values_count)
    {
        pn_log("Invalid speed index: %d. Reset to %d.", speed_id, 0);
        pn_preference_set_char(CAMERA_READSPEED_MODE, 0);
        speed_id = 0;
    }
    error = Picam_SetParameterFloatingPointValue(internal->model_handle, PicamParameter_AdcSpeed,
                                                            speed_constraint->values_array[speed_id]);
    if (error != PicamError_None)
        print_error("Failed to set Readout Speed.", error);
    Picam_DestroyCollectionConstraints(speed_constraint);

    // Validate and set readout gain
    const PicamCollectionConstraint *gain_constraint;
    if (Picam_GetParameterCollectionConstraint(internal->model_handle, PicamParameter_AdcAnalogGain,
                                               PicamConstraintCategory_Required, &gain_constraint) != PicamError_None)
        fatal_error(internal, "Failed to query AdcGain Constraints.");

    uint8_t gain_id = pn_preference_char(CAMERA_GAIN_MODE);
    if (speed_id >= gain_constraint->values_count)
    {
        pn_log("Invalid gain index: %d. Reset to %d.", speed_id, 0);
        pn_preference_set_char(CAMERA_READSPEED_MODE, 0);
        gain_id = 0;
    }
    set_integer_param(internal->model_handle, PicamParameter_AdcAnalogGain,
                      (piint)(gain_constraint->values_array[gain_id]));
    Picam_DestroyCollectionConstraints(gain_constraint);

    // Set temperature
    error = Picam_SetParameterFloatingPointValue(internal->model_handle, PicamParameter_SensorTemperatureSetPoint,
                                                            pn_preference_int(CAMERA_TEMPERATURE)/100.0f);
    if (error != PicamError_None)
        print_error("Failed to set `PicamParameter_SensorTemperatureSetPoint'.", error);

    // Get chip dimensions
    const PicamRoisConstraint  *roi_constraint;
    if (Picam_GetParameterRoisConstraint(internal->model_handle, PicamParameter_Rois, PicamConstraintCategory_Required, &roi_constraint) != PicamError_None)
        fatal_error(internal, "Failed to query ROIs Constraint.");

    // Set readout area
    uint16_t ww = pn_preference_int(CAMERA_WINDOW_WIDTH);
    if (ww < roi_constraint->width_constraint.minimum || ww > roi_constraint->width_constraint.maximum)
    {
        pn_log("Invalid window width: %d. Reset to %d.", ww, roi_constraint->width_constraint.minimum);
        ww = roi_constraint->width_constraint.minimum;
        pn_preference_set_int(CAMERA_WINDOW_WIDTH, ww);
    }

    uint16_t wh = pn_preference_int(CAMERA_WINDOW_HEIGHT);
    if (wh < roi_constraint->height_constraint.minimum || wh > roi_constraint->height_constraint.maximum)
    {
        pn_log("Invalid window height: %d. Reset to %d.", wh, roi_constraint->height_constraint.minimum);
        wh = roi_constraint->height_constraint.minimum;
        pn_preference_set_int(CAMERA_WINDOW_HEIGHT, wh);
    }

    uint8_t bin = pn_preference_char(CAMERA_BINNING);
    if (bin < 1 || bin > ww || bin > wh)
    {
        pn_log("Invalid binning: %d. Reset to %d.", bin, 1);
        bin = 1;
        pn_preference_set_char(CAMERA_BINNING, bin);
    }

    uint16_t wx = pn_preference_int(CAMERA_WINDOW_X);
    if (wx + ww > roi_constraint->width_constraint.maximum)
    {
        pn_log("Invalid window x: %d. Reset to %d.", wx, 0);
        wx = 0;
        pn_preference_set_int(CAMERA_WINDOW_X, wx);
    }

    uint16_t wy = pn_preference_int(CAMERA_WINDOW_Y);
    if (wy + wh > roi_constraint->height_constraint.maximum)
    {
        pn_log("Invalid window y: %d. Reset to %d.", wy, 0);
        wy = 0;
        pn_preference_set_int(CAMERA_WINDOW_Y, wy);
    }

    // Get region definition
    const PicamRois *region;
    if (Picam_GetParameterRoisValue(internal->model_handle, PicamParameter_Rois, &region) != PicamError_None)
    {
        Picam_DestroyRoisConstraints(roi_constraint);
        Picam_DestroyRois(region);
        fatal_error(internal, "Failed to query current ROI.");
    }

    // Set ROI
    PicamRoi *roi = &region->roi_array[0];
    roi->x = wx;
    roi->y = wy;
    roi->width = ww;
    roi->height = wh;
    roi->x_binning = bin;
    roi->y_binning = bin;

    internal->frame_width = ww / bin;
    internal->frame_height = wh / bin;

    if (Picam_SetParameterRoisValue(internal->model_handle, PicamParameter_Rois, region) != PicamError_None)
    {
    	Picam_DestroyRoisConstraints(roi_constraint);
        Picam_DestroyRois(region);
        fatal_error(internal, "Failed to set ROI");
    }

    Picam_DestroyRoisConstraints(roi_constraint);
    Picam_DestroyRois(region);

    // Query readout time
    piflt readout_time;
    error = Picam_GetParameterFloatingPointValue(internal->model_handle, PicamParameter_ReadoutTimeCalculation, &readout_time);
    if (error != PicamError_None)
        print_error("Failed to query readout time.", error);

    double exposure_time = pn_preference_int(EXPOSURE_TIME);

    // Convert readout time from to the base exposure unit (s or ms) for comparison
    bool ms_mode = pn_preference_char(TIMER_MILLISECOND_MODE);
    if (!ms_mode)
        readout_time /= 1000;

    if (exposure_time < readout_time)
    {
        uint16_t new_exposure = (uint16_t)(ceil(readout_time));
        pn_preference_set_int(EXPOSURE_TIME, new_exposure);
        pn_log("Increasing EXPOSURE_TIME to %d.", new_exposure);
    }

    return ms_mode ? readout_time / 1000 : readout_time;
}

uint8_t camera_picam_port_table(Camera *camera, void *_internal, struct camera_port_option **out_ports)
{
    struct internal *internal = _internal;

    const PicamCollectionConstraint *port_constraint;
    if (Picam_GetParameterCollectionConstraint(internal->model_handle, PicamParameter_AdcQuality, PicamConstraintCategory_Required, &port_constraint) != PicamError_None)
        fatal_error(internal, "Failed to query AdcSpeed Constraints.");

    uint8_t port_count = port_constraint->values_count;
    struct camera_port_option *ports = calloc(port_count, sizeof(struct camera_port_option));
    if (!ports)
        fatal_error(internal, "Failed to allocate memory for %d readout ports.", port_count);

    const pichar *value;
    char str[100];
    for (uint8_t i = 0; i < port_count; i++)
    {
        struct camera_port_option *port = &ports[i];

        Picam_GetEnumerationString(PicamEnumeratedType_AdcQuality, port_constraint->values_array[i], &value);
        port->name = strdup(value);
        Picam_DestroyString(value);

        PicamError error = Picam_SetParameterIntegerValue(internal->model_handle, PicamParameter_AdcQuality, (piint)(port_constraint->values_array[i]));
        if (error != PicamError_None)
            fatal_error(internal, "Failed to set Readout Port.", error);

        const PicamCollectionConstraint *speed_constraint;
        if (Picam_GetParameterCollectionConstraint(internal->model_handle, PicamParameter_AdcSpeed, PicamConstraintCategory_Required, &speed_constraint) != PicamError_None)
            fatal_error(internal, "Failed to query AdcSpeed Constraints.");

        port->speed_count = speed_constraint->values_count;
        port->speed = calloc(port->speed_count, sizeof(struct camera_speed_option));
        if (!port->speed)
            fatal_error(internal, "Failed to allocate memory for %d readout speeds.", port->speed_count);

        for (uint8_t j = 0; j < port->speed_count; j++)
        {
            struct camera_speed_option *speed = &port->speed[j];
            snprintf(str, 100, "%0.1f MHz", speed_constraint->values_array[j]);
            speed->name = strdup(str);

            PicamError error = Picam_SetParameterFloatingPointValue(internal->model_handle, PicamParameter_AdcSpeed, speed_constraint->values_array[j]);
            if (error != PicamError_None)
                fatal_error(internal, "Failed to set Readout Speed.", error);

            const PicamCollectionConstraint *gain_constraint;
            if (Picam_GetParameterCollectionConstraint(internal->model_handle, PicamParameter_AdcAnalogGain, PicamConstraintCategory_Required, &gain_constraint) != PicamError_None)
                fatal_error(internal, "Failed to query AdcGain Constraints.");

            speed->gain_count = gain_constraint->values_count;
            speed->gain = calloc(speed->gain_count, sizeof(struct camera_gain_option));
            if (!speed->gain)
                fatal_error(internal, "Failed to allocate memory for readout gains.");

            for (uint8_t k = 0; k < speed->gain_count; k++)
            {
                Picam_GetEnumerationString(PicamEnumeratedType_AdcAnalogGain, gain_constraint->values_array[k], &value);
                speed->gain[k].name = strdup(value);
                Picam_DestroyString(value);
            }
            Picam_DestroyCollectionConstraints(gain_constraint);
        }
        Picam_DestroyCollectionConstraints(speed_constraint);
    }
    Picam_DestroyCollectionConstraints(port_constraint);

    *out_ports = ports;
    return port_count;
}

void camera_picam_uninitialize(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;

    // Shutdown camera and PICAM
    PicamAdvanced_UnregisterForAcquisitionUpdated(internal->device_handle, acquisitionUpdatedCallback);
    PicamAdvanced_CloseCameraDevice(internal->device_handle);
    Picam_UninitializeLibrary();
}

void camera_picam_start_acquiring(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;
    PicamError error;

    internal->first_frame = true;

    // Create a buffer large enough for PICAM to hold multiple frames.
    size_t buffer_size = pn_preference_int(CAMERA_FRAME_BUFFER_SIZE);

    piint readout_stride = 0;
    error = Picam_GetParameterIntegerValue(internal->model_handle, PicamParameter_ReadoutStride, &readout_stride);
    if (error != PicamError_None)
        print_error("Failed to set PicamParameter_ReadoutStride.", error);

    internal->image_buffer = (pibyte *)malloc(buffer_size*readout_stride*sizeof(pibyte));
    if (!internal->image_buffer)
        fatal_error(internal, "Failed to allocate frame buffer.");

    PicamAcquisitionBuffer buffer =
    {
        .memory = internal->image_buffer,
        .memory_size = buffer_size*readout_stride
    };

    error = PicamAdvanced_SetAcquisitionBuffer(internal->device_handle, &buffer);
    if (error != PicamError_None)
    {
        print_error("PicamAdvanced_SetAcquisitionBuffer failed.", error);
        fatal_error(internal, "Acquisition setup failed.");
    }

    piint frame_size = 0;
    error = Picam_GetParameterIntegerValue(internal->model_handle, PicamParameter_FrameSize, &frame_size);
    if (error != PicamError_None)
        print_error("Failed to set PicamParameter_FrameSize.", error);
    internal->frame_bytes = frame_size;

    // Convert from base exposure units (s or ms) to ms
    piflt exptime = pn_preference_int(EXPOSURE_TIME);
    if (!pn_preference_char(TIMER_MILLISECOND_MODE))
        exptime *= 1000;

    // Set exposure period shorter than the trigger period, allowing
    // the camera to complete the frame transfer and be ready for the next trigger
    exptime -= pn_preference_int(PROEM_EXPOSURE_SHORTCUT);

    error = Picam_SetParameterFloatingPointValue(internal->model_handle, PicamParameter_ExposureTime, exptime);
    if (error != PicamError_None)
        print_error("PicamParameter_ExposureTime failed.", error);

    // Keep the shutter open during the sequence
    set_integer_param(internal->model_handle, PicamParameter_ShutterTimingMode, PicamShutterTimingMode_AlwaysOpen);

    commit_camera_params(internal);

    error = Picam_StartAcquisition(internal->model_handle);
    if (error != PicamError_None)
    {
        print_error("Picam_StartAcquisition failed.", error);
        fatal_error(internal, "Aquisition initialization failed.");
    }
}

// Stop an acquisition sequence
void camera_picam_stop_acquiring(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;

    pibln running;
    PicamError error = Picam_IsAcquisitionRunning(internal->model_handle, &running);
    if (error != PicamError_None)
        print_error("Picam_IsAcquisitionRunning failed.", error);

    if (running)
    {
        error = Picam_StopAcquisition(internal->model_handle);
        if (error != PicamError_None)
            print_error("Picam_StopAcquisition failed.", error);
    }

    error = Picam_IsAcquisitionRunning(internal->model_handle, &running);
    if (error != PicamError_None)
        print_error("Picam_IsAcquisitionRunning failed.", error);

    if (running)
        fatal_error(internal, "Failed to stop acquisition");

    // Close the shutter until the next exposure sequence
    set_integer_param(internal->model_handle, PicamParameter_ShutterTimingMode, PicamShutterTimingMode_AlwaysClosed);
    commit_camera_params(internal);

    PicamAcquisitionBuffer buffer = {.memory = NULL, .memory_size = 0};
    error = PicamAdvanced_SetAcquisitionBuffer(internal->device_handle, &buffer);
    if (error != PicamError_None)
        print_error("PicamAdvanced_SetAcquisitionBuffer failed.", error);
    free(internal->image_buffer);
}

// New frames are notified by callback, so we don't need to do anything here
void camera_picam_tick(Camera *camera, void *internal, PNCameraMode current_mode, double current_temperature) {}

double camera_picam_read_temperature(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;
    return read_temperature(internal->model_handle);
}

void camera_picam_query_ccd_region(Camera *camera, void *_internal, uint16_t region[4])
{
    struct internal *internal = _internal;
    const PicamRoisConstraint  *roi_constraint;
    if (Picam_GetParameterRoisConstraint(internal->model_handle, PicamParameter_Rois, PicamConstraintCategory_Required, &roi_constraint) != PicamError_None)
        fatal_error(internal, "Failed to query ROIs Constraint.");

    region[0] = roi_constraint->x_constraint.minimum;
    region[1] = roi_constraint->x_constraint.maximum;
    region[2] = roi_constraint->y_constraint.minimum;
    region[3] = roi_constraint->y_constraint.maximum;

    Picam_DestroyRoisConstraints(roi_constraint);
}
