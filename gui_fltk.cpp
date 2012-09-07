/*
 * Copyright 2010-2012 Paul Chote
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

static void populate_string_preference(Fl_Input *input, PNPreferenceType key)
{
    char *val = pn_preference_string(key);
    input->value(val);
    free(val);
}

static void populate_char_preference(Fl_Int_Input *input, PNPreferenceType key)
{
    char *buf;
    asprintf(&buf, "%d", pn_preference_char(key));
    input->value(buf);
    free(buf);
}

static void populate_int_preference(Fl_Int_Input *input, PNPreferenceType key)
{
    char *buf;
    asprintf(&buf, "%d", pn_preference_int(key));
    input->value(buf);
    free(buf);
}

#pragma mark C++ Implementation

void FLTKGui::addLogLine(const char *msg)
{
    if (m_logBuffer->length())
        m_logBuffer->append("\n");
    m_logBuffer->append(msg);

    // Scroll to bottom by moving the insert cursor
    // then scrolling to show the cursor position
    while (m_logDisplay->move_down());
    m_logDisplay->show_insert_position();
}

bool FLTKGui::update()
{
    PNCameraMode camera_mode = pn_camera_mode();
    float camera_temperature = pn_camera_temperature();
    float camera_readout_time = pn_camera_readout_time();

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
    m_logDisplay->hide_cursor();
}

void FLTKGui::buttonMetadataPressed(Fl_Widget* o, void *userdata)
{
    FLTKGui* gui = (FLTKGui *)userdata;
    gui->showMetadataWindow();
}

void FLTKGui::buttonExposurePressed(Fl_Widget* o, void *userdata)
{
    FLTKGui* gui = (FLTKGui *)userdata;
    gui->showExposureWindow();
}

void FLTKGui::buttonAcquirePressed(Fl_Widget* o, void *userdata)
{
    FLTKGui* gui = (FLTKGui *)userdata;

    pthread_mutex_lock(&gui->m_cameraRef->read_mutex);
    PNCameraMode camera_mode = gui->m_cameraRef->mode;
    pthread_mutex_unlock(&gui->m_cameraRef->read_mutex);

    if (camera_mode == IDLE)
    {
        clear_queued_data();
        pn_camera_start_exposure();
        timer_start_exposure(gui->m_timerRef, pn_preference_char(EXPOSURE_TIME));
    }
    else if (camera_mode == ACQUIRING)
    {
        pn_camera_stop_exposure();
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
    FLTKGui* gui = (FLTKGui *)userdata;
    closeMainWindowCallback(gui->m_mainWindow, NULL);
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

void FLTKGui::buttonExposureConfirmPressed(Fl_Widget* o, void *userdata)
{
    FLTKGui* gui = (FLTKGui *)userdata;
    uint8_t oldexp = pn_preference_char(EXPOSURE_TIME);
    int new_exposure = atoi(gui->m_exposureInput->value());

    pthread_mutex_lock(&gui->m_cameraRef->read_mutex);
    PNCameraMode camera_mode = gui->m_cameraRef->mode;
    float camera_readout_time = gui->m_cameraRef->readout_time;
    pthread_mutex_unlock(&gui->m_cameraRef->read_mutex);

    if (camera_mode != IDLE)
    {
        pn_log("Cannot change exposure time while acquiring");
        gui->m_exposureWindow->hide();
        return;
    }

    if (new_exposure < camera_readout_time || new_exposure > 255)
    {
        // Invalid entry
        if (new_exposure < camera_readout_time)
            pn_log("Minimum exposure: %.2f seconds", camera_readout_time);
        else
            pn_log("Maximum exposure: 255 seconds");

        return;
    }
    else
    {
        if (oldexp != new_exposure)
        {
            // Update preferences
            pn_preference_set_char(EXPOSURE_TIME, new_exposure);
            gui->updateAcquisitionGroup();
            pn_log("Exposure set to %d seconds", new_exposure);
        }
    }
    gui->m_exposureWindow->hide();
}

void FLTKGui::createExposureWindow()
{
    m_exposureWindow = new Fl_Window(150, 40, "Set Exposure");
    m_exposureWindow->user_data((void*)(this));

    m_exposureInput = new Fl_Int_Input(10, 10, 70, 20);

    m_exposureButtonConfirm = new Fl_Button(90, 10, 50, 20, "Set");
    m_exposureButtonConfirm->user_data((void*)(this));
    m_exposureButtonConfirm->callback(buttonExposureConfirmPressed);

    m_exposureWindow->hide();
}

void FLTKGui::showExposureWindow()
{
    // Update and show the "Set Exposure" dialog
    populate_char_preference(m_exposureInput, EXPOSURE_TIME);
    m_exposureWindow->show();
}

void FLTKGui::metadataFrameTypeChangedCallback(Fl_Widget *input, void *userdata)
{
    FLTKGui* gui = (FLTKGui *)userdata;

    if (((Fl_Choice *)input)->value() == 2)
    {
        gui->m_metadataTargetInput->show();
        gui->m_metadataCountdownInput->hide();
    }
    else
    {
        gui->m_metadataTargetInput->hide();
        gui->m_metadataCountdownInput->show();
    }
}

void FLTKGui::createMetadataWindow()
{
    m_metadataWindow = new Fl_Window(420, 155, "Set Metadata");
    m_metadataWindow->user_data((void*)(this));

    // File output
    Fl_Group *outputGroup = new Fl_Group(10, 10, 400, 55, "Output Filename");
    outputGroup->box(FL_ENGRAVED_BOX);
    outputGroup->align(FL_ALIGN_INSIDE|FL_ALIGN_TOP);
    outputGroup->labelsize(14);
    outputGroup->labelfont(FL_BOLD);

    int x = 20, y = 35, h = 20, margin = 25;
    m_metadataOutputDir = new Fl_Input(x, y, 170, h); x+= 180;
    m_metadataRunPrefix = new Fl_Input(x, y, 100, h, "/"); x+= 110;
    m_metadataRunNumber = new Fl_Int_Input(x, y, 50, h, "-"); x+= 95;

    // Dirty hack... FLTK doesn't have a standalone label widget!?!
    new Fl_Input(x, y, 0, h, ".fits.gz");

    outputGroup->end();

    x = 90;
    y = 75;
    int w = 110;

    m_metadataFrameTypeInput = new Fl_Choice(x, y, w, h, "Type:"); y += margin;
    m_metadataFrameTypeInput->add("Dark");
    m_metadataFrameTypeInput->add("Flat");
    m_metadataFrameTypeInput->add("Target");
    m_metadataFrameTypeInput->callback(metadataFrameTypeChangedCallback);
    m_metadataFrameTypeInput->user_data((void*)(this));

    m_metadataTargetInput = new Fl_Input(x, y, w, h, "Target:");
    m_metadataCountdownInput = new Fl_Int_Input(x, y, w, h, "Countdown:"); y += margin;
    m_metadataCountdownInput->hide();
    m_metadataObserversInput = new Fl_Input(x, y, w, h, "Observers:");

    x = 300;
    y = 75;
    m_metadataObservatoryInput = new Fl_Input(x, y, w, h, "Observatory:"); y += margin;
    m_metadataTelecopeInput = new Fl_Input(x, y, w, h, "Telescope:"); y += margin;

    m_metadataButtonConfirm = new Fl_Button(x, y, w, h, "Save");
    m_metadataButtonConfirm->user_data((void*)(this));
    m_metadataButtonConfirm->callback(buttonMetadataConfirmPressed);

    m_metadataWindow->hide();
}

void FLTKGui::showMetadataWindow()
{
    // Update windows with preferences
    populate_string_preference(m_metadataOutputDir, OUTPUT_DIR);
    populate_string_preference(m_metadataRunPrefix, RUN_PREFIX);
    populate_int_preference(m_metadataRunNumber, RUN_NUMBER);

    uint8_t frame_type = pn_preference_char(OBJECT_TYPE);
    m_metadataFrameTypeInput->value(frame_type);
    if (frame_type == 2)
    {
        m_metadataTargetInput->show();
        m_metadataCountdownInput->hide();
    }
    else
    {
        m_metadataTargetInput->hide();
        m_metadataCountdownInput->show();
    }

    populate_string_preference(m_metadataTargetInput, OBJECT_NAME);
    populate_int_preference(m_metadataCountdownInput, CALIBRATION_COUNTDOWN);
    populate_string_preference(m_metadataObserversInput, OBSERVERS);
    populate_string_preference(m_metadataObservatoryInput, OBSERVATORY);
    populate_string_preference(m_metadataTelecopeInput, TELESCOPE);
    m_metadataWindow->show();
}

#define set_string_from_input(a,b) _set_string_from_input(a, # a, b)
void _set_string_from_input(PNPreferenceType key, const char *name, Fl_Input *input)
{
    char *oldval = pn_preference_string(key);
    const char *newval = input->value();
    if (strcmp(oldval, newval))
    {
        pn_preference_set_string(key, newval);
        pn_log("%s set to `%s'", name, newval);
    }
    free(oldval);
}

#define set_int(a,b) _set_int(a, # a, b)
void _set_int(PNPreferenceType key, const char *name, int newval)
{
    int oldval = pn_preference_int(key);
    if (oldval != newval)
    {
        pn_preference_set_int(key, newval);
        pn_log("%s set to `%d'", name, newval);
    }
}

void FLTKGui::buttonMetadataConfirmPressed(Fl_Widget* o, void *userdata)
{
    FLTKGui* gui = (FLTKGui *)userdata;

    // Validate parameters
    int run_number = atoi(gui->m_metadataRunNumber->value());
    int calibration_countdown = atoi(gui->m_metadataCountdownInput->value());
    if (run_number < 0)
    {
        pn_log("RUN_NUMBER number must be positive");
        return;
    }

    if (calibration_countdown < 0)
    {
        pn_log("CALIBRATION_COUNTDOWN must be positive");
        return;
    }

    // Update preferences from fields
    set_string_from_input(OUTPUT_DIR, gui->m_metadataOutputDir);
    set_string_from_input(RUN_PREFIX, gui->m_metadataRunPrefix);
    set_int(RUN_NUMBER, run_number);

    pn_preference_set_char(OBJECT_TYPE, gui->m_metadataFrameTypeInput->value());
    set_string_from_input(OBJECT_NAME, gui->m_metadataTargetInput);
    set_string_from_input(OBSERVERS, gui->m_metadataObserversInput);
    set_string_from_input(OBSERVATORY, gui->m_metadataObservatoryInput);
    set_string_from_input(TELESCOPE, gui->m_metadataTelecopeInput);
    set_int(CALIBRATION_COUNTDOWN, calibration_countdown);

    gui->updateAcquisitionGroup();
    gui->m_metadataWindow->hide();
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

    if (acquire_pressed)
    {
        m_buttonExposure->deactivate();
        m_exposureWindow->hide();
    }
    else
        m_buttonExposure->activate();

    m_buttonSave->value(save_pressed);
    if (save_pressed)
    {
        m_buttonMetadata->deactivate();
        m_metadataWindow->hide();
    }
    else
        m_buttonMetadata->activate();
}

void FLTKGui::closeMainWindowCallback(Fl_Widget *window, void *v)
{
    FLTKGui* gui = (FLTKGui *)window->user_data();

    // User pressed escape
    if (Fl::event() == FL_SHORTCUT && Fl::event_key() == FL_Escape)
        return;

    // Hide all windows.
    // The next Fl::check() call in pn_update_ui() will
    // return false, causing the program to shutdown.
    gui->m_exposureWindow->hide();
    gui->m_metadataWindow->hide();
    window->hide();
}

FLTKGui::FLTKGui(PNCamera *camera, TimerUnit *timer)
    : m_cameraRef(camera), m_timerRef(timer)
{
	// Create the main window
	m_mainWindow = new Fl_Window(660, 355, "Acquisition Control");
    m_mainWindow->user_data((void*)(this));
    m_mainWindow->callback(closeMainWindowCallback);

    createTimerGroup();
    createCameraGroup();
    createAcquisitionGroup();
    createLogGroup();
    createButtonGroup();

    createExposureWindow();
    createMetadataWindow();

    // Set initial state
    last_camera_mode = pn_camera_mode();
    last_camera_temperature = pn_camera_temperature();
    last_camera_readout_time = pn_camera_readout_time();
    last_calibration_framecount = pn_preference_int(CALIBRATION_COUNTDOWN);
    last_run_number = pn_preference_int(RUN_NUMBER);
    last_camera_downloading = timer_camera_downloading(timer);

    updateTimerGroup();
    updateCameraGroup(last_camera_mode, last_camera_downloading, last_camera_temperature);
    updateAcquisitionGroup();
    updateButtonGroup(last_camera_mode);

	m_mainWindow->end();
	m_mainWindow->show();
}

FLTKGui::~FLTKGui()
{

}
