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
#include <pthread.h>

#include "main.h"
#include "camera.h"
#include "preferences.h"
#include "platform.h"

struct internal
{
    PicamHandle device_handle;
    PicamHandle model_handle;
    pibyte *image_buffer;
    piint readout_stride;

    uint16_t frame_width;
    uint16_t frame_height;
    size_t frame_bytes;

    uint64_t timestamp_resolution;
    uint64_t start_timestamp;
    bool first_frame;

    // String descriptions to store in frame headers
    char *current_port_desc;
    char *current_speed_desc;
    char *current_gain_desc;

    bool current_port_is_em;
    double current_em_gain;

    uint16_t exposure_shortcut_ms;
    double vertical_shift_us;

    double readout_time;
};

static void log_picam_error(PicamError error)
{
    if (error == PicamError_None)
        return;

    const pichar* string;
    Picam_GetEnumerationString(PicamEnumeratedType_Error, error, &string);
    pn_log("PICAM error: %d = %s.", error, string);
    Picam_DestroyString(string);
}

static PicamError set_integer_param(PicamHandle model_handle, PicamParameter parameter, piint value)
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
    return error;
}

static PicamError set_float_param(PicamHandle model_handle, PicamParameter parameter, piflt value)
{
    PicamError error = Picam_SetParameterFloatingPointValue(model_handle, parameter, value);
    if (error != PicamError_None)
    {
        const pichar *name, *err;
        Picam_GetEnumerationString(PicamEnumeratedType_Parameter, parameter, &name);
        Picam_GetEnumerationString(PicamEnumeratedType_Error, error, &err);
        pn_log("Failed to set `%s': %s.", name, err);
        Picam_DestroyString(err);
        Picam_DestroyString(name);
    }
    return error;
}

static int commit_camera_params(struct internal *internal)
{
    pibln all_params_committed;
    Picam_AreParametersCommitted(internal->model_handle, &all_params_committed);

    if (!all_params_committed)
    {
        const PicamParameter *failed_params = NULL;
        piint failed_param_count = 0;
        PicamError error = Picam_CommitParameters(internal->model_handle, &failed_params, &failed_param_count);

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
            pn_log("Parameter commit failed.");
            return CAMERA_ERROR;
        }
        Picam_DestroyParameters(failed_params);

        if (error != PicamError_None)
        {
            pn_log("Picam_CommitParameters failed.", error);
            log_picam_error(error);
            return CAMERA_ERROR;
        }

        error = PicamAdvanced_CommitParametersToCameraDevice(internal->model_handle);
        if (error != PicamError_None)
        {
            pn_log("Advanced parameter commit failed.");
            log_picam_error(error);
            return CAMERA_ERROR;
        }
    }

    return CAMERA_OK;
}

static int read_temperature(PicamHandle model_handle, double *out_temperature)
{
    piflt temperature;
    PicamError error = Picam_ReadParameterFloatingPointValue(model_handle, PicamParameter_SensorTemperatureReading, &temperature);
    if (error != PicamError_None)
    {
        pn_log("Temperature Read failed");
        log_picam_error(error);
        return CAMERA_ERROR;
    }

    // TODO: Can query PicamEnumeratedType_SensorTemperatureStatus to get locked/unlocked status
    *out_temperature = temperature;
    return CAMERA_OK;
}

static void acquired_frame(struct internal *internal, uint8_t *frame_data, uint64_t timestamp)
{
    // Copy frame data and pass ownership to main thread
    CameraFrame *frame = malloc(sizeof(CameraFrame));
    if (frame)
    {
        frame->data = malloc(internal->frame_bytes);
        if (frame->data)
        {
            if (internal->first_frame)
            {
                internal->start_timestamp = timestamp;
                timestamp = 0;
                internal->first_frame = false;
            }
            else
                timestamp -= internal->start_timestamp;

            memcpy(frame->data, frame_data, internal->frame_bytes);
            frame->width = internal->frame_width;
            frame->height = internal->frame_height;
            read_temperature(internal->model_handle, &frame->temperature);

            frame->has_timestamp = true;
            frame->timestamp = timestamp*1.0/internal->timestamp_resolution;
            frame->has_image_region = false;
            frame->has_bias_region = false;
            frame->readout_time = internal->readout_time;
            frame->vertical_shift_us = internal->vertical_shift_us;

            frame->port_desc = strdup(internal->current_port_desc);
            frame->speed_desc = strdup(internal->current_speed_desc);
            frame->gain_desc = strdup(internal->current_gain_desc);

            frame->has_em_gain = internal->current_port_is_em;
            frame->em_gain = internal->current_em_gain;

            frame->has_exposure_shortcut = true;
            frame->exposure_shortcut_ms = internal->exposure_shortcut_ms;

            queue_framedata(frame);
        }
        else
            pn_log("Failed to allocate CameraFrame->data. Discarding frame.");
    }
    else
        pn_log("Failed to allocate CameraFrame. Discarding frame.");
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
PicamError PIL_CALL acquisitionUpdatedCallback(PicamHandle handle, const PicamAvailableData *data, const PicamAcquisitionStatus* status)
{
    if (status->errors != PicamAcquisitionErrorsMask_None)
    {
        // Print errors
        if (status->errors & PicamAcquisitionErrorsMask_DataLost)
            pn_log("Camera error: Frame data lost. Continuing...");

        if (status->errors & PicamAcquisitionErrorsMask_ConnectionLost)
            pn_log("Camera error: Connection lost. Continuing...");
    }
    else if (data)
    {
        for (pi64s i = 0; i < data->readout_count; i++)
        {
            uint8_t *frame_data = (uint8_t *)data->initial_readout + i*callback_internal_ref->readout_stride;
            uint64_t timestamp = *(uint64_t *)(frame_data + callback_internal_ref->frame_bytes);
            acquired_frame(callback_internal_ref, frame_data, timestamp);
        }
    }

    // Check for buffer overrun. Should never happen in practice, but we log this
    // to aid future debugging if it does crop up in the field.
    pibln overran;
    PicamError error = PicamAdvanced_HasAcquisitionBufferOverrun(handle, &overran);
    if (error != PicamError_None)
    {
        pn_log("Failed to check for acquisition overflow. Continuing.");
        log_picam_error(error);
    }
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
            pn_log("PicamAdvanced_OpenCameraDevice failed.");
            log_picam_error(error);
            continue;
        }

        error = PicamAdvanced_GetCameraModel(internal->device_handle, &internal->model_handle);
        if (error != PicamError_None)
        {
            pn_log("Failed to query camera model.");
            log_picam_error(error);
            continue;
        }

        return;
    }
}

int camera_picam_initialize(Camera *camera, void **out_internal)
{
    struct internal *internal = calloc(1, sizeof(struct internal));
    if (!internal)
        return CAMERA_ALLOCATION_FAILED;

    Picam_InitializeLibrary();

    connect_camera(camera, internal);

    // User has given up waiting for a camera
    if (camera_desired_mode(camera) == SHUTDOWN)
        return CAMERA_INITIALIZATION_ABORTED;

    PicamCameraID id;
    Picam_GetCameraID(internal->device_handle, &id);

    // Query camera model info
    const pichar *string;
    Picam_GetEnumerationString(PicamEnumeratedType_Model, id.model, &string);
    pn_log("Camera ID: %s (SN:%s) [%s].", string, id.serial_number, id.sensor_name);
    Picam_DestroyString(string);
    Picam_DestroyCameraIDs(&id);

    // Set initial temperature
    set_float_param(internal->model_handle, PicamParameter_SensorTemperatureSetPoint, pn_preference_int(CAMERA_TEMPERATURE)/100.0f);

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

    // Set Electron-Multiplication gain.
    internal->current_em_gain = pn_preference_int(PROEM_EM_GAIN);
    set_integer_param(internal->model_handle, PicamParameter_AdcEMGain, internal->current_em_gain);
    pn_log("Set EM Gain to %u", pn_preference_int(PROEM_EM_GAIN));

    // Validate and set vertical shift rate
    const PicamCollectionConstraint *shift_constraint;
    PicamError error = Picam_GetParameterCollectionConstraint(internal->model_handle, PicamParameter_VerticalShiftRate,
                                                   PicamConstraintCategory_Required, &shift_constraint);
    if (error != PicamError_None)
    {
        pn_log("Failed to query Vertical Shift Rate Constraints.");
        log_picam_error(error);
        return CAMERA_ERROR;
    }

    uint8_t shift_id = pn_preference_char(PROEM_SHIFT_MODE);
    if (shift_id >= shift_constraint->values_count)
    {
        pn_log("Invalid shift index: %d. Reset to %d.", shift_id, 0);
        pn_preference_set_char(PROEM_SHIFT_MODE, 0);
        shift_id = 0;
    }
    set_float_param(internal->model_handle, PicamParameter_VerticalShiftRate,
                    shift_constraint->values_array[shift_id]);

    pn_log("Set vertical shift rate to %gus", shift_constraint->values_array[shift_id]);
    internal->vertical_shift_us = shift_constraint->values_array[shift_id];
    Picam_DestroyCollectionConstraints(shift_constraint);

    pi64s timestamp_resolution;
    error = Picam_GetParameterLargeIntegerValue(internal->model_handle, PicamParameter_TimeStampResolution, &timestamp_resolution);
    if (error != PicamError_None)
    {
        pn_log("Failed to get PicamParameter_TimeStampResolution.");
        log_picam_error(error);
        return CAMERA_ERROR;
    }
    internal->timestamp_resolution = timestamp_resolution;

    // Continue exposing until explicitly stopped or error
    // Requires a user specified image buffer to be provided - the interal
    // routines appears to use this parameter to determine the size of the
    // internal buffer to use.
    error = Picam_SetParameterLargeIntegerValue(internal->model_handle, PicamParameter_ReadoutCount, 0);
    if (error != PicamError_None)
    {
        pn_log("Failed to set PicamParameter_ReadoutCount.");
        log_picam_error(error);
        return CAMERA_ERROR;
    }

    // Commit parameter changes to hardware
    int status = commit_camera_params(internal);
    if (status != CAMERA_OK)
        return status;

    // Register callback for acquisition status change / frame available
    callback_internal_ref = internal;
    error = PicamAdvanced_RegisterForAcquisitionUpdated(internal->device_handle, acquisitionUpdatedCallback);
    if (error != PicamError_None)
    {
        pn_log("PicamAdvanced_RegisterForAcquisitionUpdated failed.");
        log_picam_error(error);
        return CAMERA_ERROR;
    }

    *out_internal = internal;
    return CAMERA_OK;
}

int camera_picam_update_camera_settings(Camera *camera, void *_internal, double *out_readout_time)
{
    struct internal *internal = _internal;
    PicamError error;

    // Enable frame transfer mode
    PicamReadoutControlMode readout_type = PicamReadoutControlMode_FrameTransfer;

    // Enable external trigger
    PicamTriggerResponse trigger_type = PicamTriggerResponse_ReadoutPerTrigger;

    // If bias mode is enabled, then we want a fullframe readout with internal triggering
    uint8_t trigger_mode = pn_preference_char(TIMER_TRIGGER_MODE);
    if (trigger_mode == TRIGGER_BIAS)
    {
        readout_type = PicamReadoutControlMode_FullFrame;
        trigger_type = PicamTriggerResponse_NoResponse;
    }

    set_integer_param(internal->model_handle, PicamParameter_ReadoutControlMode, readout_type);
    set_integer_param(internal->model_handle, PicamParameter_TriggerResponse, trigger_type);

    // Validate and set readout port
    const PicamCollectionConstraint *port_constraint;
    error = Picam_GetParameterCollectionConstraint(internal->model_handle, PicamParameter_AdcQuality,
                                                   PicamConstraintCategory_Required, &port_constraint);
    if (error != PicamError_None)
    {
        pn_log("Failed to query AdcQuality Constraints.");
        log_picam_error(error);
        return CAMERA_ERROR;
    }

    uint8_t port_id = pn_preference_char(CAMERA_READPORT_MODE);
    if (port_id >= port_constraint->values_count)
    {
        pn_log("Invalid port index: %d. Reset to %d.", port_id, 0);
        pn_preference_set_char(CAMERA_READPORT_MODE, 0);
        port_id = 0;
    }
    set_integer_param(internal->model_handle, PicamParameter_AdcQuality,
                      (piint)(port_constraint->values_array[port_id]));

    const pichar *value;
    free(internal->current_port_desc);
    Picam_GetEnumerationString(PicamEnumeratedType_AdcQuality, port_constraint->values_array[port_id], &value);
    internal->current_port_desc = strdup(value);
    internal->current_port_is_em = port_constraint->values_array[port_id] == PicamAdcQuality_ElectronMultiplied;
    Picam_DestroyString(value);

    Picam_DestroyCollectionConstraints(port_constraint);

    // Validate and set readout speed
    const PicamCollectionConstraint *speed_constraint;
    error = Picam_GetParameterCollectionConstraint(internal->model_handle, PicamParameter_AdcSpeed,
                                                   PicamConstraintCategory_Required, &speed_constraint);
    if (error != PicamError_None)
    {
        pn_log("Failed to query AdcSpeed Constraints.");
        log_picam_error(error);
        return CAMERA_ERROR;
    }

    uint8_t speed_id = pn_preference_char(CAMERA_READSPEED_MODE);
    if (speed_id >= speed_constraint->values_count)
    {
        pn_log("Invalid speed index: %d. Reset to %d.", speed_id, 0);
        pn_preference_set_char(CAMERA_READSPEED_MODE, 0);
        speed_id = 0;
    }

    error = set_float_param(internal->model_handle, PicamParameter_AdcSpeed, speed_constraint->values_array[speed_id]);
    if (error != PicamError_None)
        return CAMERA_ERROR;

    free(internal->current_speed_desc);
    char str[100];
    snprintf(str, 100, "%0.1f MHz", speed_constraint->values_array[speed_id]);
    internal->current_speed_desc = strdup(str);
    Picam_DestroyString(value);

    Picam_DestroyCollectionConstraints(speed_constraint);

    // Validate and set readout gain
    const PicamCollectionConstraint *gain_constraint;
    error = Picam_GetParameterCollectionConstraint(internal->model_handle, PicamParameter_AdcAnalogGain,
                                                   PicamConstraintCategory_Required, &gain_constraint);
    if (error != PicamError_None)
    {
        pn_log("Failed to query AdcGain Constraints.");
        log_picam_error(error);
        return CAMERA_ERROR;
    }

    uint8_t gain_id = pn_preference_char(CAMERA_GAIN_MODE);
    if (speed_id >= gain_constraint->values_count)
    {
        pn_log("Invalid gain index: %d. Reset to %d.", speed_id, 0);
        pn_preference_set_char(CAMERA_READSPEED_MODE, 0);
        gain_id = 0;
    }
    set_integer_param(internal->model_handle, PicamParameter_AdcAnalogGain,
                      (piint)(gain_constraint->values_array[gain_id]));

    free(internal->current_gain_desc);
    Picam_GetEnumerationString(PicamEnumeratedType_AdcAnalogGain, gain_constraint->values_array[gain_id], &value);
    internal->current_gain_desc = strdup(value);
    Picam_DestroyString(value);

    Picam_DestroyCollectionConstraints(gain_constraint);

    // Set temperature
    error = set_float_param(internal->model_handle, PicamParameter_SensorTemperatureSetPoint, pn_preference_int(CAMERA_TEMPERATURE)/100.0f);
    if (error != PicamError_None)
        return CAMERA_ERROR;

    // Get chip dimensions
    const PicamRoisConstraint  *roi_constraint;
    error = Picam_GetParameterRoisConstraint(internal->model_handle, PicamParameter_Rois, PicamConstraintCategory_Required, &roi_constraint);
    if (error != PicamError_None)
    {
        pn_log("Failed to query ROIs Constraints.");
        log_picam_error(error);
        return CAMERA_ERROR;
    }

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
    error = Picam_GetParameterRoisValue(internal->model_handle, PicamParameter_Rois, &region);
    if (error != PicamError_None)
    {
        Picam_DestroyRoisConstraints(roi_constraint);
        Picam_DestroyRois(region);
        pn_log("Failed to query current ROI.");
        log_picam_error(error);
        return CAMERA_ERROR;
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

    error = Picam_SetParameterRoisValue(internal->model_handle, PicamParameter_Rois, region);
    if (error != PicamError_None)
    {
    	Picam_DestroyRoisConstraints(roi_constraint);
        Picam_DestroyRois(region);
        pn_log("Failed to set ROI");
        log_picam_error(error);
        return CAMERA_ERROR;
    }

    Picam_DestroyRoisConstraints(roi_constraint);
    Picam_DestroyRois(region);

    // Commit other settings
    commit_camera_params(internal);

    // Query readout time
    piflt readout_time;
    error = Picam_GetParameterFloatingPointValue(internal->model_handle, PicamParameter_ReadoutTimeCalculation, &readout_time);
    if (error != PicamError_None)
    {
        pn_log("Failed to query readout time.");
        log_picam_error(error);
        return CAMERA_ERROR;
    }

    internal->readout_time = readout_time / 1000;
    double exposure_time = pn_preference_int(EXPOSURE_TIME);
    double shortcut = pn_preference_int(PROEM_EXPOSURE_SHORTCUT);
    internal->exposure_shortcut_ms = shortcut;

    // Convert times from to the base exposure unit (s or ms) for comparison
    if (trigger_mode == TRIGGER_SECONDS)
    {
        shortcut /= 1000;
        readout_time /= 1000;
    }

    // Make sure that the shortened exposure is physically possible
    if (trigger_mode != TRIGGER_BIAS)
    {
        exposure_time -= shortcut;
        if (exposure_time <= readout_time)
        {
            uint16_t new_exposure = (uint16_t)(ceil(readout_time + shortcut));
            pn_preference_set_int(EXPOSURE_TIME, new_exposure);
            pn_log("EXPOSURE_TIME - PROEM_EXPOSURE_SHORTCUT > camera readout.");
            pn_log("Increasing EXPOSURE_TIME to %d.", new_exposure);
        }
    }

    *out_readout_time = internal->readout_time;
    return CAMERA_OK;
}

int camera_picam_port_table(Camera *camera, void *_internal, struct camera_port_option **out_ports, uint8_t *out_port_count)
{
    struct internal *internal = _internal;
    PicamError error;

    const PicamCollectionConstraint *port_constraint;
    error = Picam_GetParameterCollectionConstraint(internal->model_handle, PicamParameter_AdcQuality,
                                                   PicamConstraintCategory_Required, &port_constraint);
    if (error != PicamError_None)
    {
        pn_log("Failed to query AdcQuality Constraints.");
        log_picam_error(error);
        return CAMERA_ERROR;
    }

    uint8_t port_count = port_constraint->values_count;
    struct camera_port_option *ports = calloc(port_count, sizeof(struct camera_port_option));
    if (!ports)
        return CAMERA_ALLOCATION_FAILED;

    const pichar *value;
    char str[100];
    for (uint8_t i = 0; i < port_count; i++)
    {
        struct camera_port_option *port = &ports[i];

        Picam_GetEnumerationString(PicamEnumeratedType_AdcQuality, port_constraint->values_array[i], &value);
        port->name = strdup(value);
        Picam_DestroyString(value);

        error = Picam_SetParameterIntegerValue(internal->model_handle, PicamParameter_AdcQuality,
                                               (piint)(port_constraint->values_array[i]));
        if (error != PicamError_None)
        {
            pn_log("Failed to set Readout Port.");
            log_picam_error(error);
            return CAMERA_ERROR;
        }

        const PicamCollectionConstraint *speed_constraint;
        error = Picam_GetParameterCollectionConstraint(internal->model_handle, PicamParameter_AdcSpeed,
                                                       PicamConstraintCategory_Required, &speed_constraint);
        if (error != PicamError_None)
        {
            pn_log("Failed to query AdcSpeed Constraints.");
            log_picam_error(error);
            return CAMERA_ERROR;
        }

        port->speed_count = speed_constraint->values_count;
        port->speed = calloc(port->speed_count, sizeof(struct camera_speed_option));
        if (!port->speed)
            return CAMERA_ALLOCATION_FAILED;

        for (uint8_t j = 0; j < port->speed_count; j++)
        {
            struct camera_speed_option *speed = &port->speed[j];
            snprintf(str, 100, "%0.1f MHz", speed_constraint->values_array[j]);
            speed->name = strdup(str);

            error = set_float_param(internal->model_handle, PicamParameter_AdcSpeed, speed_constraint->values_array[j]);
            if (error != PicamError_None)
                return CAMERA_ERROR;

            const PicamCollectionConstraint *gain_constraint;
            error = Picam_GetParameterCollectionConstraint(internal->model_handle, PicamParameter_AdcAnalogGain,
                                                           PicamConstraintCategory_Required, &gain_constraint);
            if (error != PicamError_None)
            {
                pn_log("Failed to query AdcAnalogGain Constraints.");
                log_picam_error(error);
                return CAMERA_ERROR;
            }

            speed->gain_count = gain_constraint->values_count;
            speed->gain = calloc(speed->gain_count, sizeof(struct camera_gain_option));
            if (!speed->gain)
                return CAMERA_ALLOCATION_FAILED;

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
    *out_port_count = port_count;
    return CAMERA_OK;
}

int camera_picam_uninitialize(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;

    // Shutdown camera and PICAM
    PicamAdvanced_UnregisterForAcquisitionUpdated(internal->device_handle, acquisitionUpdatedCallback);
    PicamAdvanced_CloseCameraDevice(internal->device_handle);
    Picam_UninitializeLibrary();

    return CAMERA_OK;
}

int camera_picam_start_acquiring(Camera *camera, void *_internal, bool shutter_open)
{
    struct internal *internal = _internal;
    PicamError error;

    internal->first_frame = true;

    // Create a buffer large enough for PICAM to hold multiple frames.
    size_t buffer_size = pn_preference_int(CAMERA_FRAME_BUFFER_SIZE);

    internal->readout_stride = 0;
    error = Picam_GetParameterIntegerValue(internal->model_handle, PicamParameter_ReadoutStride, &internal->readout_stride);
    if (error != PicamError_None)
    {
        pn_log("Failed to set PicamParameter_ReadoutStride.");
        log_picam_error(error);
        return CAMERA_ERROR;
    }

    internal->image_buffer = (pibyte *)malloc(buffer_size*internal->readout_stride*sizeof(pibyte));
    if (!internal->image_buffer)
        return CAMERA_ALLOCATION_FAILED;

    PicamAcquisitionBuffer buffer =
    {
        .memory = internal->image_buffer,
        .memory_size = buffer_size*internal->readout_stride
    };

    error = PicamAdvanced_SetAcquisitionBuffer(internal->device_handle, &buffer);
    if (error != PicamError_None)
    {
        pn_log("PicamAdvanced_SetAcquisitionBuffer failed.");
        log_picam_error(error);
        return CAMERA_ERROR;
    }

    piint frame_size = 0;
    error = Picam_GetParameterIntegerValue(internal->model_handle, PicamParameter_FrameSize, &frame_size);
    if (error != PicamError_None)
    {
        pn_log("Failed to set PicamParameter_FrameSize.");
        log_picam_error(error);
    }
    internal->frame_bytes = frame_size;

    // Exposure time is zero for bias frames
    piflt exptime = 0;
    if (pn_preference_char(TIMER_TRIGGER_MODE) != TRIGGER_BIAS)
    {
        // Convert from base exposure units (s or ms) to ms
        exptime = pn_preference_int(EXPOSURE_TIME);
        if (pn_preference_char(TIMER_TRIGGER_MODE) == TRIGGER_SECONDS)
            exptime *= 1000;

        // Set exposure period shorter than the trigger period, allowing
        // the camera to complete the frame transfer and be ready for the next trigger
        exptime -= pn_preference_int(PROEM_EXPOSURE_SHORTCUT);
    }

    error = set_float_param(internal->model_handle, PicamParameter_ExposureTime, exptime);
    if (error != PicamError_None)
        return CAMERA_ERROR;

    // Open shutter if required
    set_integer_param(internal->model_handle, PicamParameter_ShutterTimingMode,
        shutter_open ? PicamShutterTimingMode_AlwaysOpen : PicamShutterTimingMode_AlwaysClosed);
    pn_log("Will acquire with shutter %s.", shutter_open ? "open" : "closed");

    int status = commit_camera_params(internal);
    if (status != CAMERA_OK)
        return status;

    error = Picam_StartAcquisition(internal->model_handle);
    if (error != PicamError_None)
    {
        pn_log("Picam_StartAcquisition failed.");
        log_picam_error(error);
        return CAMERA_ERROR;
    }

    return CAMERA_OK;
}

// Stop an acquisition sequence
int camera_picam_stop_acquiring(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;

    pibln running;
    PicamError error = Picam_IsAcquisitionRunning(internal->model_handle, &running);
    if (error != PicamError_None)
    {
        pn_log("Picam_IsAcquisitionRunning failed.");
        log_picam_error(error);
        // Continue cleanup on failure
    }

    if (running)
    {
        error = Picam_StopAcquisition(internal->model_handle);
        if (error != PicamError_None)
        {
            pn_log("Picam_StopAcquisition failed.");
            log_picam_error(error);
            // Continue cleanup on failure
        }
    }

    error = Picam_IsAcquisitionRunning(internal->model_handle, &running);
    if (error != PicamError_None)
    {
        pn_log("Picam_IsAcquisitionRunning failed.");
        log_picam_error(error);
        // Continue cleanup on failure
    }

    if (running)
        pn_log("Failed to stop acquisition");

    // Close the shutter until the next exposure sequence
    set_integer_param(internal->model_handle, PicamParameter_ShutterTimingMode, PicamShutterTimingMode_AlwaysClosed);

    // Continue cleanup on failure
    commit_camera_params(internal);

    PicamAcquisitionBuffer buffer = {.memory = NULL, .memory_size = 0};
    error = PicamAdvanced_SetAcquisitionBuffer(internal->device_handle, &buffer);
    if (error != PicamError_None)
    {
        pn_log("PicamAdvanced_SetAcquisitionBuffer failed.");
        log_picam_error(error);
        // Continue cleanup on failure
    }
    free(internal->image_buffer);
    
    return CAMERA_OK;
}

// New frames are notified by callback, so we don't need to do anything here
int camera_picam_tick(Camera *camera, void *internal, PNCameraMode current_mode)
{
    return CAMERA_OK;
}

int camera_picam_read_temperature(Camera *camera, void *_internal, double *out_temperature)
{
    struct internal *internal = _internal;
    return read_temperature(internal->model_handle, out_temperature);
}

int camera_picam_query_ccd_region(Camera *camera, void *_internal, uint16_t region[4])
{
    struct internal *internal = _internal;
    const PicamRoisConstraint  *roi_constraint;
    PicamError error = Picam_GetParameterRoisConstraint(internal->model_handle, PicamParameter_Rois,
                                                        PicamConstraintCategory_Required, &roi_constraint);
    if (error != PicamError_None)
    {
        pn_log("Failed to query ROIs Constraint.");
        log_picam_error(error);
        return CAMERA_ERROR;
    }

    region[0] = roi_constraint->x_constraint.minimum;
    region[1] = roi_constraint->x_constraint.maximum;
    region[2] = roi_constraint->y_constraint.minimum;
    region[3] = roi_constraint->y_constraint.maximum;

    Picam_DestroyRoisConstraints(roi_constraint);

    return CAMERA_OK;
}

bool camera_picam_supports_readout_display(Camera *camera, void *internal)
{
    return false;
}

bool camera_picam_supports_shutter_disabling(Camera *camera, void *internal)
{
    return true;
}

bool camera_picam_supports_bias_acquisition(Camera *camera, void *internal)
{
    return true;
}

void camera_picam_normalize_trigger(Camera *camera, void *internal, TimerTimestamp *trigger)
{
    // Do nothing: ProEM triggers already represent the start of the frame
}
