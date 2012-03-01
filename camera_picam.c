/*
* Copyright 2010, 2011 Paul Chote
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
    Picam_ReadParameterFloatingPointValue(handle,
                                          PicamParameter_SensorTemperatureReading,
                                          &temperature );
    pthread_mutex_lock(&camera->read_mutex);
    camera->temperature = temperature;
    pthread_mutex_unlock(&camera->read_mutex);
}

static void commit_camera_params()
{
    pibln committed;
	Picam_AreParametersCommitted(handle, &committed);
	if( !committed )
	{
		const PicamParameter* failed_parameter_array = NULL;
		piint failed_parameter_count = 0;
        
		Picam_CommitParameters(handle, &failed_parameter_array, &failed_parameter_count);
		if(failed_parameter_count)
        {
            pn_log("%d parameters failed to commit", failed_parameter_count);
			Picam_DestroyParameters(failed_parameter_array);
        }
    }
}

// Initialize PVCAM and the camera hardware
static void initialize_camera()
{
    set_mode(INITIALISING);
    Picam_InitializeLibrary();

    // Open the first available camera
	PicamCameraID id;
    if(Picam_OpenFirstCamera( &handle ) == PicamError_None)
        Picam_GetCameraID( handle, &id );
    else
	{
		camera->fatal_error = strdup("Camera not found");
        pthread_exit(NULL);
    }

    // Query camera model info
    const pichar* string;
    Picam_GetEnumerationString(PicamEnumeratedType_Model, id.model, &string);
    pn_log("%s (SN:%s) [%s]\r\n", string, id.serial_number, id.sensor_name);
    Picam_DestroyString(string);

    // Set temperature
    Picam_SetParameterFloatingPointValue(handle, PicamParameter_SensorTemperatureSetPoint, pn_preference_int(CAMERA_TEMPERATURE)/100.0f);

    // Enable frame transfer mode
    Picam_SetParameterIntegerValue(handle, PicamParameter_ReadoutControlMode, PicamReadoutControlMode_FrameTransfer);

    // Enable external trigger, negative edge, one frame per pulse, DC coupling
    Picam_SetParameterIntegerValue(handle, PicamParameter_TriggerSource, PicamTriggerSource_External);
    Picam_SetParameterIntegerValue(handle, PicamParameter_TriggerDetermination, PicamTriggerDetermination_FallingEdge);
    Picam_SetParameterIntegerValue(handle, PicamParameter_TriggerResponse, PicamTriggerResponse_ReadoutPerTrigger);
    Picam_SetParameterIntegerValue(handle, PicamParameter_TriggerCoupling, PicamTriggerCoupling_DC);
    // TODO?: PicamParameter_TriggerTermination sets termination: 50ohm, or high impedance

    // Set output low while the camera is reading out a frame
    Picam_SetParameterIntegerValue(handle, PicamParameter_AuxOutput, PicamOutputSignal_NotReadingOut);

    // Keep the shutter closed until we start a sequence
    Picam_SetParameterIntegerValue(handle, PicamParameter_ShutterTimingMode, PicamShutterTimingMode_AlwaysClosed);

    // TODO: PicamParameter_AdcSpeed: Digitization rate in MHz

	// Commit parameter changes
    commit_camera_params();

    pn_log("Camera initialized");
    set_mode(IDLE);
}

// Start an acquisition sequence
static void start_acquiring()
{
    set_mode(ACQUIRE_START);
    pn_log("Starting acquisition run...");

	// Get chip dimensions
	const PicamRoisConstraint  *constraint;
	if (Picam_GetParameterRoisConstraint(handle, PicamParameter_Rois, PicamConstraintCategory_Required, &constraint) != PicamError_None)
		fatal_error("Error determining ROIs Constraint", __LINE__);

	camera->frame_width = constraint->width_constraint.maximum - constraint->width_constraint.minimum;
	camera->frame_height = constraint->height_constraint.maximum - constraint->height_constraint.minimum;
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

    // Continue exposing until explicitly stopped or error
    Picam_SetParameterIntegerValue(handle, PicamParameter_ReadoutCount, 0);

    // Set exposure to 0. Actual exposure is controlled by trigger interval, so this value isn't relevant
    Picam_SetParameterIntegerValue(handle, PicamParameter_ExposureTime, 0);

    // Keep the shutter open during the sequence
    Picam_SetParameterIntegerValue(handle, PicamParameter_ShutterTimingMode, PicamShutterTimingMode_AlwaysOpen);
    commit_camera_params();

    Picam_StartAcquisition(handle);
    pn_log("Acquisition run started");

    // Sample initial temperature
    read_temperature();

    camera->first_frame = true;
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
    
    // Close the PVCAM lib (which in turn closes the camera)
    if (camera->mode == IDLE)
    {
        Picam_CloseCamera( handle );
        Picam_UninitializeLibrary();
        pn_log("PVCAM uninitialized");
    }

    pthread_exit(NULL);
}

