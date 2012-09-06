/*
 * Copyright 2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <math.h>

#include "gui_fltk.h"

FLTKGui *gui;

#pragma mark C API entrypoints
void pn_ui_new(PNCamera *camera, TimerUnit *timer)
{
    gui = new FLTKGui(camera, timer);
}

void pn_ui_log_line(char *message)
{
    gui->addLogLine(message);
}

void pn_ui_show_fatal_error(char *message)
{
    fl_alert("A fatal error has occurred and the program will now close:\n\n%s", message);
}

bool pn_ui_update()
{
    return gui->update();
}

void pn_ui_free()
{
    delete gui;
}

#pragma mark C++ Implementation

void FLTKGui::addLogLine(const char *msg)
{
    m_logBuffer->append(msg);
    m_logBuffer->append("\n");
    // TODO: Scroll display if necessary
}

bool FLTKGui::update()
{
    pthread_mutex_lock(&m_cameraRef->read_mutex);
    PNCameraMode camera_mode = m_cameraRef->mode;
    float camera_temperature = m_cameraRef->temperature;
    float camera_readout_time = m_cameraRef->readout_time;
    pthread_mutex_unlock(&m_cameraRef->read_mutex);

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
            updateAcquisitionGroup();
        }
        last_camera_readout_time = camera_readout_time;
    }

    bool camera_downloading = timer_camera_downloading(m_timerRef);
    updateTimerGroup();

    if (last_camera_mode != camera_mode ||
        last_camera_downloading != camera_downloading)
    {
        updateButtonGroup(camera_mode);
        updateCameraGroup(camera_mode, camera_downloading, camera_temperature);
        last_camera_mode = camera_mode;
        last_camera_downloading = camera_downloading;
    }

    if (last_camera_temperature != camera_temperature)
    {
        updateCameraGroup(camera_mode, camera_downloading, camera_temperature);
        last_camera_temperature = camera_temperature;
    }

    int remaining_frames = pn_preference_int(CALIBRATION_COUNTDOWN);
    int run_number = pn_preference_int(RUN_NUMBER);
    if (remaining_frames != last_calibration_framecount ||
        run_number != last_run_number)
    {
        updateAcquisitionGroup();
        updateButtonGroup(camera_mode);
        last_calibration_framecount = remaining_frames;
        last_run_number = run_number;
    }

    Fl::redraw();
    return Fl::check() == 0;
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
    TimerTimestamp ts = timer_current_timestamp(m_timerRef);
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

void FLTKGui::updateCameraGroup(PNCameraMode mode, int camera_downloading, float temperature)
{
    // Camera status
    switch(mode)
    {
        default:
        case INITIALISING:
        case ACQUIRE_START:
        case ACQUIRE_STOP:
            m_cameraStatusOutput->value("Initialising");
            break;
        case IDLE:
            m_cameraStatusOutput->value("Idle");
            break;
        case ACQUIRING:
            if (camera_downloading)
                m_cameraStatusOutput->value("Downloading");
            else
                m_cameraStatusOutput->value("Acquiring");
            break;
        case DOWNLOADING:
            m_cameraStatusOutput->value("Downloading");
            break;
        case SHUTDOWN:
            m_cameraStatusOutput->value("Closing");
            break;
    }

    // Camera temperature
    const char *tempstring = "Unavailable";
    char tempbuf[30];

    if (mode == ACQUIRING || mode == IDLE)
    {
        sprintf(tempbuf, "%0.02f C", temperature);
        tempstring = tempbuf;
    }
    m_cameraTemperatureOutput->value(tempstring);
}

void FLTKGui::createAcquisitionGroup()
{
    int y = 200, margin = 20;
    m_acquisitionGroup = createGroupBox(y, 105, "Acquisition"); y += 25;
    m_acquisitionTypeOutput = createOutputLabel(y, "Type:"); y += margin;
    m_acquisitionTargetOutput = createOutputLabel(y, "Target:"); m_acquisitionTargetOutput->hide();
    m_acquisitionRemainingOutput = createOutputLabel(y, "Remaining:"); y += margin;
    m_acquisitionExposureOutput = createOutputLabel(y, "Exposure:"); y += margin;
    m_acquisitionFilenameOutput = createOutputLabel(y, "Filename:");
    m_acquisitionGroup->end();
}

void FLTKGui::updateAcquisitionGroup()
{
    PNFrameType type = (PNFrameType)pn_preference_char(OBJECT_TYPE);
    unsigned char exptime = pn_preference_char(EXPOSURE_TIME);
    int remaining_frames = pn_preference_int(CALIBRATION_COUNTDOWN);
    char *run_prefix = pn_preference_string(RUN_PREFIX);
    int run_number = pn_preference_int(RUN_NUMBER);
    char *object = pn_preference_string(OBJECT_NAME);

    switch (type)
    {
        case OBJECT_DARK:
            m_acquisitionTypeOutput->value("Dark");
            break;
        case OBJECT_FLAT:
            m_acquisitionTypeOutput->value("Flat");
            break;
        case OBJECT_TARGET:
            m_acquisitionTypeOutput->value("Target");
            break;
    }

    char buf[100];
    snprintf(buf, 100, "%d seconds", exptime);
    m_acquisitionExposureOutput->value(buf);

    if (type == OBJECT_TARGET)
    {
        m_acquisitionTargetOutput->show();
        m_acquisitionTargetOutput->value(object);
        m_acquisitionRemainingOutput->hide();
    }
    else
    {
        m_acquisitionTargetOutput->hide();
        m_acquisitionRemainingOutput->show();
        snprintf(buf, 100, "%d", remaining_frames);
        m_acquisitionRemainingOutput->value(buf);
    }

    snprintf(buf, 100, "%s-%04d.fits.gz", run_prefix, run_number);
    m_acquisitionFilenameOutput->value(buf);
    free(object);
    free(run_prefix);
}

void FLTKGui::createLogGroup()
{
    m_logBuffer = new Fl_Text_Buffer();
    m_logDisplay = new Fl_Text_Display(250, 10, 400, 295, NULL);
    m_logDisplay->buffer(m_logBuffer);
}

void FLTKGui::buttonMetadataPressed(Fl_Widget* o, void *userdata)
{
    pn_log("Metadata pressed");
}

void FLTKGui::buttonExposurePressed(Fl_Widget* o, void *userdata)
{
    pn_log("Exposure pressed");
}

void FLTKGui::buttonAcquirePressed(Fl_Widget* o, void *userdata)
{
    FLTKGui* gui = (FLTKGui *)userdata;

    pthread_mutex_lock(&gui->m_cameraRef->read_mutex);
    PNCameraMode camera_mode = gui->m_cameraRef->mode;
    pthread_mutex_unlock(&gui->m_cameraRef->read_mutex);

    if (camera_mode == IDLE)
    {
        pn_camera_request_mode(ACQUIRING);
        timer_start_exposure(gui->m_timerRef, pn_preference_char(EXPOSURE_TIME));
    }
    else if (camera_mode == ACQUIRING)
    {
        pn_camera_request_mode(IDLE);
        timer_stop_exposure(gui->m_timerRef);
    }
}

void FLTKGui::buttonSavePressed(Fl_Widget* o, void *userdata)
{
    FLTKGui* gui = (FLTKGui *)userdata;

    pthread_mutex_lock(&gui->m_cameraRef->read_mutex);
    PNCameraMode camera_mode = gui->m_cameraRef->mode;
    pthread_mutex_unlock(&gui->m_cameraRef->read_mutex);

    // Can't enable saving for calibration frames after the target count has been reached
    if (!pn_preference_allow_save())
    {
        pn_log("Unable to toggle save: countdown is zero");
        return;
    }

    unsigned char save = pn_preference_toggle_save();
    gui->updateButtonGroup(camera_mode);
    pn_log("%s saving", save ? "Enabled" : "Disabled");
}

void FLTKGui::buttonQuitPressed(Fl_Widget* o, void *userdata)
{
    pn_log("Quit pressed");
}

void FLTKGui::createButtonGroup()
{
    m_buttonMetadata = new Fl_Button(10, 315, 120, 30, "Set Metadata");
    m_buttonMetadata->user_data((void*)(this));
    m_buttonMetadata->callback(buttonMetadataPressed);

    m_buttonExposure = new Fl_Button(140, 315, 120, 30, "Set Exposure");
    m_buttonExposure->user_data((void*)(this));
    m_buttonExposure->callback(buttonExposurePressed);

    m_buttonAcquire = new Fl_Toggle_Button(270, 315, 120, 30, "Acquire");
    m_buttonAcquire->user_data((void*)(this));
    m_buttonAcquire->callback(buttonAcquirePressed);

    m_buttonSave = new Fl_Toggle_Button(400, 315, 120, 30, "Save");
    m_buttonSave->user_data((void*)(this));
    m_buttonSave->callback(buttonSavePressed);

    m_buttonQuit = new Fl_Button(530, 315, 120, 30, "Quit");
    m_buttonQuit->user_data((void*)(this));
    m_buttonQuit->callback(buttonQuitPressed);
}

void FLTKGui::updateButtonGroup(PNCameraMode camera_mode)
{
    bool save_pressed = pn_preference_char(SAVE_FRAMES) && pn_preference_allow_save();
    bool acquire_pressed = camera_mode != IDLE;
    bool acquire_disabled = camera_mode != ACQUIRING && camera_mode != IDLE;

    m_buttonAcquire->value(acquire_pressed);
    if (acquire_disabled)
        m_buttonAcquire->deactivate();
    else
        m_buttonAcquire->activate();

    m_buttonSave->value(save_pressed);
}

FLTKGui::FLTKGui(PNCamera *camera, TimerUnit *timer)
    : m_cameraRef(camera), m_timerRef(timer)
{
	// Create the main window
	m_mainWindow = new Fl_Window(660, 355, "Acquisition Control");
    m_mainWindow->user_data((void*)(this));

    createTimerGroup();
    createCameraGroup();
    createAcquisitionGroup();
    createLogGroup();
    createButtonGroup();

    pthread_mutex_lock(&camera->read_mutex);
    PNCameraMode camera_mode = camera->mode;
    float camera_temperature = camera->temperature;
    pthread_mutex_unlock(&camera->read_mutex);

    bool camera_downloading = timer_camera_downloading(timer);
    updateTimerGroup();
    updateCameraGroup(camera_mode, camera_downloading, camera_temperature);
    updateAcquisitionGroup();
    updateButtonGroup(camera_mode);

	m_mainWindow->end();
	m_mainWindow->show();
}

FLTKGui::~FLTKGui()
{

}
