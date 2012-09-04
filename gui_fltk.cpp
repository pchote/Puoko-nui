/*
 * Copyright 2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <math.h>

#include "gui_fltk.h"

extern "C" {
    #include "gps.h"
    #include "camera.h"
    #include "preferences.h"
    #include "common.h"
}

extern PNGPS *gps;
extern PNCamera *camera;
FLTKGui *gui;

PNCameraMode last_camera_mode;
float last_camera_temperature;
float last_camera_readout_time;
int last_calibration_framecount;
int last_run_number;
int last_camera_downloading;

// HACK: This should be in pn_ui_new, but we need to enable the log earlier
void init_log_gui()
{
    gui = new FLTKGui();
}

void add_log_line(char *msg)
{
    gui->addLogLine(msg);
}

void pn_ui_new()
{

}

bool pn_ui_update()
{
    pthread_mutex_lock(&camera->read_mutex);
    PNCameraMode camera_mode = camera->mode;
    float camera_temperature = camera->temperature;
    float camera_readout_time = camera->readout_time;
    pthread_mutex_unlock(&camera->read_mutex);

    // Check that the exposure time is greater than
    // the camera readout, and change if necessary
    if (camera_readout_time != last_camera_readout_time)
    {
        unsigned char exposure_time = pn_preference_char(EXPOSURE_TIME);
        if (exposure_time < camera_readout_time)
        {
            unsigned char new_exposure = (unsigned char)(ceil(camera_readout_time));
            pn_preference_set_char(EXPOSURE_TIME, new_exposure);
            pn_log("Increasing exposure time to %d seconds", new_exposure);
            //update_acquisition_window();
        }
        last_camera_readout_time = camera_readout_time;
    }

    pthread_mutex_lock(&gps->read_mutex);
    int camera_downloading = gps->camera_downloading;
    pthread_mutex_unlock(&gps->read_mutex);

    gui->updateTimerGroup();

    if (last_camera_mode != camera_mode ||
        last_camera_downloading != camera_downloading)
    {
        //update_command_window(camera_mode);
        //update_status_window(camera_mode);
        //update_camera_window(camera_mode, camera_downloading, camera_temperature);
        last_camera_mode = camera_mode;
        last_camera_downloading = camera_downloading;
    }

    if (last_camera_temperature != camera_temperature)
    {
        //update_camera_window(camera_mode, camera_downloading, camera_temperature);
        last_camera_temperature = camera_temperature;
    }

    int remaining_frames = pn_preference_int(CALIBRATION_COUNTDOWN);
    int run_number = pn_preference_int(RUN_NUMBER);
    if (remaining_frames != last_calibration_framecount ||
        run_number != last_run_number)
    {
        //update_acquisition_window();
        //update_status_window(camera_mode);
        last_calibration_framecount = remaining_frames;
        last_run_number = run_number;
    }

    Fl::redraw();
    return Fl::check() == 0;
}

void pn_ui_free()
{
    delete gui;
}

void FLTKGui::addLogLine(const char *msg)
{
    m_logBuffer->append(msg);
    m_logBuffer->append("\n");
    // TODO: Scroll display if necessary
}

Fl_Group *FLTKGui::createGroupBox(int y, int h, const char *label)
{
    int b[4] = {10, y, 230, h};
    Fl_Group *output = new Fl_Group(b[0], b[1], b[2], b[3], label);
    output->box(FL_ENGRAVED_BOX);
    output->align(FL_ALIGN_INSIDE|FL_ALIGN_TOP);
    output->labelsize(14);
    output->labelfont(FL_BOLD);
    return output;
}

Fl_Output *FLTKGui::createOutputLabel(int y, const char *label)
{
    int b[5] = {105, y, 130, 14};
    Fl_Output *output = new Fl_Output(b[0], b[1], b[2], b[3], label);
    output->box(FL_NO_BOX);
    output->labelsize(14);
    output->textsize(13);
    output->textfont(FL_BOLD);
    return output;
}


void FLTKGui::createTimerGroup()
{
    int y = 10, margin = 20;
    m_timerGroup = createGroupBox(y, 105, "Timer Information"); y += 25;
    m_timerStatusOutput = createOutputLabel(y, "Status:"); y += margin;
    m_timerPCTimeOutput = createOutputLabel(y, "PC Time:"); y += margin;
    m_timerUTCTimeOutput = createOutputLabel(y, "UTC Time:"); y += margin;
    m_timerCountdownOutput = createOutputLabel(y, "Countdown:");
    m_timerGroup->end();
}

void FLTKGui::updateTimerGroup()
{
    // PC time
    char strtime[30];
    time_t t = time(NULL);
    strftime(strtime, 30, "%Y-%m-%d %H:%M:%S", gmtime(&t));
    m_timerPCTimeOutput->value(strtime);

    // GPS time
    pthread_mutex_lock(&gps->read_mutex);
    PNGPSTimestamp ts = gps->current_timestamp;
    pthread_mutex_unlock(&gps->read_mutex);

    m_timerStatusOutput->value((ts.locked ? "Locked  " : "Unlocked"));

    if (ts.valid)
    {
        snprintf(strtime, 30, "%04d-%02d-%02d %02d:%02d:%02d", ts.year, ts.month, ts.day, ts.hours, ts.minutes, ts.seconds);
        m_timerUTCTimeOutput->value(strtime);

        if (ts.remaining_exposure > 0)
        {
            char buf[30];
            sprintf(buf, "%03d", ts.remaining_exposure);
            m_timerCountdownOutput->value(buf);
        }
        else
            m_timerCountdownOutput->value("Disabled");
    }
    else
    {
        m_timerStatusOutput->value("Unavailable");
        m_timerUTCTimeOutput->value("Unavailable");
    }
}

void FLTKGui::createCameraGroup()
{
    // x,y,w,h
    int y = 125, margin = 20;
    m_cameraGroup = createGroupBox(y, 65, "Camera Information"); y += 25;
    m_cameraStatusOutput = createOutputLabel(y, "Status:"); y += margin;
    m_cameraTemperatureOutput = createOutputLabel(y, "Temperature:");
    m_cameraGroup->end();
}

void FLTKGui::createAcquisitionGroup()
{
    int y = 200, margin = 20;
    m_acquisitionGroup = createGroupBox(y, 145, "Acquisition"); y += 25;
    m_acquisitionObserversOutput = createOutputLabel(y, "Observers:"); y += margin;
    m_acquisitionTypeOutput = createOutputLabel(y, "Type:"); y += margin;
    m_acquisitionTargetOutput = createOutputLabel(y, "Target:"); y += margin;
    m_acquisitionRemainingOutput = createOutputLabel(y, "Remaining:"); y += margin;
    m_acquisitionExposureOutput = createOutputLabel(y, "Exposure:"); y += margin;
    m_acquisitionFilenameOutput = createOutputLabel(y, "Filename:");
    m_acquisitionGroup->end();
}

void FLTKGui::createLogGroup()
{
    m_logBuffer = new Fl_Text_Buffer();
    m_logDisplay = new Fl_Text_Display(250, 10, 420, 335, NULL);
    m_logDisplay->buffer(m_logBuffer);
}

FLTKGui::FLTKGui()
{
	// Create the main window
	m_mainWindow = new Fl_Window(680, 400, "Acquisition Control");
    m_mainWindow->user_data((void*)(this));

    createTimerGroup();
    createCameraGroup();
    createAcquisitionGroup();
    createLogGroup();

	m_mainWindow->end();
	m_mainWindow->show();
}

FLTKGui::~FLTKGui()
{

}
