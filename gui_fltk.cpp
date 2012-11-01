/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui_fltk.h"

FLTKGui *gui;

#pragma mark C API entrypoints
void pn_ui_new(Camera *camera, TimerUnit *timer)
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

static void populate_int_preference(Fl_Int_Input *input, PNPreferenceType key)
{
    char buf[32];
    snprintf(buf, 32, "%d", pn_preference_int(key));
    input->value(buf);
}

#pragma mark C++ Implementation

void FLTKGui::addLogLine(const char *msg)
{
    m_logDisplay->add(msg);
    m_logDisplay->bottomline(m_logDisplay->size());
}

bool FLTKGui::update()
{
    PNCameraMode mode = camera_mode(m_cameraRef);
    double temperature = camera_temperature(m_cameraRef);
    double readout = camera_readout_time(m_cameraRef);
    bool camera_downloading = timer_camera_downloading(m_timerRef);
    unsigned char exposure_time = pn_preference_char(EXPOSURE_TIME);

    if (cached_camera_mode != mode ||
        cached_camera_downloading != camera_downloading)
    {
        cached_camera_mode = mode;
        cached_camera_downloading = camera_downloading;
        updateButtonGroup();
        updateCameraGroup();
    }

    if (cached_camera_temperature != temperature || cached_camera_readout != readout)
    {
        cached_camera_temperature = temperature;
        cached_camera_readout = readout;
        updateCameraGroup();
    }

    int remaining_frames = pn_preference_int(CALIBRATION_COUNTDOWN);
    int run_number = pn_preference_int(RUN_NUMBER);
    if (remaining_frames != cached_calibration_framecount ||
        run_number != cached_run_number ||
        exposure_time != cached_exposure_time)
    {
        cached_calibration_framecount = remaining_frames;
        cached_run_number = run_number;
        cached_exposure_time = exposure_time;
        updateAcquisitionGroup();
        updateButtonGroup();
    }

    updateTimerGroup();

    Fl::redraw();
    return Fl::check() == 0;
}

Fl_Group *FLTKGui::createGroupBox(int y, int h, const char *label)
{
    int b[4] = {10, y, 250, h};
    Fl_Group *output = new Fl_Group(b[0], b[1], b[2], b[3], label);
    output->box(FL_ENGRAVED_BOX);
    output->align(FL_ALIGN_INSIDE|FL_ALIGN_TOP);
    output->labelsize(14);
    output->labelfont(FL_BOLD);
    return output;
}

Fl_Output *FLTKGui::createOutputLabel(int y, const char *label)
{
    int x = 100, w = 150, h = 14;
    Fl_Output *output = new Fl_Output(x, y, w, h, label);
    output->box(FL_NO_BOX);
    output->labelfont(FL_HELVETICA);
    output->labelsize(14);
    output->textfont(FL_HELVETICA_BOLD);
    output->textsize(13);
    return output;
}

void FLTKGui::createTimerGroup()
{
    int y = 10, margin = 20;
    m_timerGroup = createGroupBox(y, 105, "Timer Information"); y += 25;
    m_timerPCTimeOutput = createOutputLabel(y, "PC Time:"); y += margin;
    m_timerUTCTimeOutput = createOutputLabel(y, "UTC Time:"); y += margin;
    m_timerUTCDateOutput = createOutputLabel(y, "UTC Date:"); y += margin;
    m_timerExposureOutput = createOutputLabel(y, "Exposure:");
    m_timerGroup->end();
}

void FLTKGui::updateTimerGroup()
{
    // PC time
    char strtime[20];
    time_t t = time(NULL);
    strftime(strtime, 20, "%H:%M:%S", gmtime(&t));
    m_timerPCTimeOutput->value(strtime);

    // GPS time
    TimerTimestamp ts = timer_current_timestamp(m_timerRef);

    if (ts.year > 0)
    {
        snprintf(strtime, 20, "%04d-%02d-%02d", ts.year, ts.month, ts.day);
        m_timerUTCDateOutput->value(strtime);
        snprintf(strtime, 20, "%02d:%02d:%02d (%s)",
                 ts.hours, ts.minutes, ts.seconds,
                 (ts.locked ? "Locked" : "Unlocked"));
        m_timerUTCTimeOutput->value(strtime);
    }
    else
    {
        m_timerUTCTimeOutput->value("NA");
        m_timerUTCDateOutput->value("NA");
    }

    char buf[20];
    if (cached_subsecond_mode)
        snprintf(buf, 20, "%s / %d ms", (ts.remaining_exposure > 0 && ts.year > 0) ? "Active" : "Disabled", 10*cached_exposure_time);
    else if (ts.remaining_exposure > 0 && ts.year > 0)
        snprintf(buf, 20, "%d / %d sec", cached_exposure_time - ts.remaining_exposure, cached_exposure_time);
    else
        snprintf(buf, 20, "Disabled / %d sec", cached_exposure_time);

    m_timerExposureOutput->value(buf);
}

void FLTKGui::createCameraGroup()
{
    // x,y,w,h
    int y = 125, margin = 20;
    m_cameraGroup = createGroupBox(y, 85, "Camera Information"); y += 25;
    m_cameraStatusOutput = createOutputLabel(y, "Status:"); y += margin;
    m_cameraTemperatureOutput = createOutputLabel(y, "Temp:"); y += margin;
    m_cameraReadoutOutput = createOutputLabel(y, "Readout:");
    m_cameraGroup->end();
}

void FLTKGui::updateCameraGroup()
{
    // Camera status
    switch (cached_camera_mode)
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
            m_cameraStatusOutput->value("Active");
            break;
        case SHUTDOWN:
            m_cameraStatusOutput->value("Closing");
            break;
    }

    char buf[11];
    if (cached_camera_mode == ACQUIRING || cached_camera_mode == IDLE)
    {
        snprintf(buf, 11, "%0.02f \u00B0C", cached_camera_temperature);
        m_cameraTemperatureOutput->value(buf);
        snprintf(buf, 11, "%0.02f sec", cached_camera_readout);
        m_cameraReadoutOutput->value(buf);
    }
    else
    {
        m_cameraTemperatureOutput->value("Unavailable");
        m_cameraReadoutOutput->value("Unavailable");
    }
}

void FLTKGui::createAcquisitionGroup()
{
    int y = 220, margin = 20;
    m_acquisitionGroup = createGroupBox(y, 85, "Acquisition"); y += 25;
    m_acquisitionTypeOutput = createOutputLabel(y, "Type:"); y += margin;
    m_acquisitionTargetOutput = createOutputLabel(y, "Target:"); m_acquisitionTargetOutput->hide();
    m_acquisitionRemainingOutput = createOutputLabel(y, "To Save:"); y += margin;
    m_acquisitionFilenameOutput = createOutputLabel(y, "File:");
    m_acquisitionGroup->end();
}

void FLTKGui::updateAcquisitionGroup()
{
    PNFrameType type = (PNFrameType)pn_preference_char(OBJECT_TYPE);
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
    m_logDisplay = new Fl_Multi_Browser(270, 10, 380, 295);
}

void FLTKGui::buttonMetadataPressed(Fl_Widget* o, void *userdata)
{
    FLTKGui* gui = (FLTKGui *)userdata;
    gui->showMetadataWindow();
}

void FLTKGui::buttonCameraPressed(Fl_Widget* o, void *userdata)
{
    FLTKGui* gui = (FLTKGui *)userdata;
    gui->showCameraWindow();
}

void FLTKGui::buttonAcquirePressed(Fl_Widget* o, void *userdata)
{
    FLTKGui* gui = (FLTKGui *)userdata;
    PNCameraMode mode = camera_mode(gui->m_cameraRef);

    if (mode == IDLE)
    {
        clear_queued_data();
        camera_start_exposure(gui->m_cameraRef);
        bool use_monitor = !camera_is_simulated(gui->m_cameraRef) && pn_preference_char(TIMER_MONITOR_LOGIC_OUT);
        timer_start_exposure(gui->m_timerRef, pn_preference_char(EXPOSURE_TIME), use_monitor);
    }
    else if (mode == ACQUIRING)
    {
        camera_stop_exposure(gui->m_cameraRef);
        timer_stop_exposure(gui->m_timerRef);
    }
}

void FLTKGui::buttonSavePressed(Fl_Widget* o, void *userdata)
{
    FLTKGui* gui = (FLTKGui *)userdata;

    // Can't enable saving for calibration frames after the target count has been reached
    if (!pn_preference_allow_save())
    {
        pn_log("Failed to toggle save: countdown is zero.");
        return;
    }

    unsigned char save = pn_preference_toggle_save();
    gui->updateButtonGroup();
    pn_log("%s saving.", save ? "Enabled" : "Disabled");
}

void FLTKGui::buttonQuitPressed(Fl_Widget* o, void *userdata)
{
    FLTKGui* gui = (FLTKGui *)userdata;
    closeMainWindowCallback(gui->m_mainWindow, NULL);
}

void FLTKGui::createButtonGroup()
{
    int y = 315;
    m_buttonMetadata = new Fl_Button(10, y, 120, 30, "Set Metadata");
    m_buttonMetadata->user_data((void*)(this));
    m_buttonMetadata->callback(buttonMetadataPressed);

    m_buttonCamera = new Fl_Button(140, y, 120, 30, "Set Camera");
    m_buttonCamera->user_data((void*)(this));
    m_buttonCamera->callback(buttonCameraPressed);

    m_buttonAcquire = new Fl_Toggle_Button(270, y, 120, 30, "Acquire");
    m_buttonAcquire->user_data((void*)(this));
    m_buttonAcquire->callback(buttonAcquirePressed);

    m_buttonSave = new Fl_Toggle_Button(400, y, 120, 30, "Save");
    m_buttonSave->user_data((void*)(this));
    m_buttonSave->callback(buttonSavePressed);

    m_buttonQuit = new Fl_Button(530, y, 120, 30, "Quit");
    m_buttonQuit->user_data((void*)(this));
    m_buttonQuit->callback(buttonQuitPressed);
}

#define set_string_from_input(a,b) _set_string_from_input(a, # a, b)
void _set_string_from_input(PNPreferenceType key, const char *name, Fl_Input *input)
{
    char *oldval = pn_preference_string(key);
    const char *newval = input->value();
    if (strcmp(oldval, newval))
    {
        pn_preference_set_string(key, newval);
        pn_log("%s set to `%s'.", name, newval);
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
        pn_log("%s set to `%d'.", name, newval);
    }
}

#define set_char(a,b) _set_char(a, # a, b)
void _set_char(PNPreferenceType key, const char *name, int newval)
{
    char oldval = pn_preference_char(key);
    if (oldval != newval)
    {
        pn_preference_set_char(key, newval);
        pn_log("%s set to `%d'.", name, newval);
    }
}

void FLTKGui::buttonCameraConfirmPressed(Fl_Widget* o, void *userdata)
{
    FLTKGui* gui = (FLTKGui *)userdata;

    if (camera_mode(gui->m_cameraRef) != IDLE)
    {
        pn_log("Cannot change camera parameters while acquiring.");
        gui->m_cameraWindow->hide();
        return;
    }

    // Validated by the camera implementation
    set_char(CAMERA_READPORT_MODE, (uint8_t)(gui->m_cameraPortInput->value()));
    set_char(CAMERA_READSPEED_MODE, (uint8_t)(gui->m_cameraSpeedInput->value()));
    set_char(CAMERA_GAIN_MODE, (uint8_t)(gui->m_cameraGainInput->value()));
    set_int(CAMERA_TEMPERATURE, (int)(atof(gui->m_cameraTemperatureInput->value())*100));
    set_int(CAMERA_WINDOW_X, (int)(gui->m_cameraWindowX->value()));
    set_int(CAMERA_WINDOW_Y, (int)(gui->m_cameraWindowY->value()));
    set_int(CAMERA_WINDOW_WIDTH, (int)(gui->m_cameraWindowWidth->value()));
    set_int(CAMERA_WINDOW_HEIGHT, (int)(gui->m_cameraWindowHeight->value()));

    set_char(CAMERA_BINNING, (uint8_t)(gui->m_cameraBinningSpinner->value()));
    set_char(EXPOSURE_TIME, (uint8_t)(gui->m_cameraExposureSpinner->value()));

    camera_update_settings(gui->m_cameraRef);
    gui->updateAcquisitionGroup();
    gui->m_cameraWindow->hide();
}

void FLTKGui::cameraRebuildPortTree(uint8_t port_id, uint8_t speed_id, uint8_t gain_id)
{
    struct camera_port_option *port;
    uint8_t port_count = camera_port_options(m_cameraRef, &port);

    // Validate requested settings
    if (port_id >= port_count)
        port_id = 0;
    if (speed_id >= port[port_id].speed_count)
        speed_id = 0;
    if (gain_id >= port[port_id].speed[speed_id].gain_count)
        gain_id = 0;

    m_cameraPortInput->clear();
    for (uint8_t i = 0; i < port_count; i++)
        m_cameraPortInput->add(port[i].name);
    m_cameraPortInput->value(port_id);

    m_cameraSpeedInput->clear();
    for (uint8_t i = 0; i < port[port_id].speed_count; i++)
        m_cameraSpeedInput->add(port[port_id].speed[i].name);
    m_cameraSpeedInput->value(speed_id);

    m_cameraGainInput->clear();
    for (uint8_t i = 0; i < port[port_id].speed[speed_id].gain_count; i++)
        m_cameraGainInput->add(port[port_id].speed[speed_id].gain[i].name);
    m_cameraGainInput->value(gain_id);
}

void FLTKGui::cameraPortSpeedGainChangedCallback(Fl_Widget *input, void *userdata)
{
    FLTKGui* gui = (FLTKGui *)userdata;
    uint8_t port_id = gui->m_cameraPortInput->value();
    uint8_t speed_id = gui->m_cameraSpeedInput->value();
    uint8_t gain_id = gui->m_cameraGainInput->value();
    gui->cameraRebuildPortTree(port_id, speed_id, gain_id);
}

void FLTKGui::createCameraWindow()
{
    m_cameraWindow = new Fl_Double_Window(350, 180, "Set Camera Parameters");
    m_cameraWindow->user_data((void*)(this));

    Fl_Group *readoutGroup = new Fl_Group(10, 10, 330, 80, "Readout Geometry");
    readoutGroup->box(FL_ENGRAVED_BOX);
    readoutGroup->align(FL_ALIGN_INSIDE|FL_ALIGN_TOP);
    readoutGroup->labelsize(14);
    readoutGroup->labelfont(FL_BOLD);

    int x = 20, y = 35, h = 20, w = 55, margin = 25;
    m_cameraWindowX = new Fl_Spinner(90, y, w, 20, "x,y (px):");
    m_cameraWindowY = new Fl_Spinner(167, y, w, 20, ",  ");
    y += margin;
    m_cameraWindowWidth = new Fl_Spinner(90, y, w, 20, "Size (px):");
    m_cameraWindowHeight = new Fl_Spinner(167, y, w, 20, " x ");

    m_cameraBinningSpinner = new Fl_Spinner(300, y, 30, 20, "Bin (px):");
    m_cameraBinningSpinner->maximum(255);
    m_cameraBinningSpinner->minimum(1);

    readoutGroup->end();

    x = 70; y = 100; w = 100;
    m_cameraPortInput = new Fl_Choice(x, y, w, h, "Port:"); y += margin;
    m_cameraPortInput->callback(cameraPortSpeedGainChangedCallback);
    m_cameraPortInput->user_data((void*)(this));

    m_cameraSpeedInput = new Fl_Choice(x, y, w, h, "Speed:"); y += margin;
    m_cameraSpeedInput->callback(cameraPortSpeedGainChangedCallback);
    m_cameraSpeedInput->user_data((void*)(this));

    m_cameraGainInput = new Fl_Choice(x, y, w, h, "Gain:"); y += margin;
    m_cameraGainInput->callback(cameraPortSpeedGainChangedCallback);
    m_cameraGainInput->user_data((void*)(this));

    x = 285; y = 100; w = 55;

    m_cameraTemperatureInput = new Fl_Float_Input(x, y, w, h, "Temp. (\u00B0C):"); y += margin;

    const char *expstring = pn_preference_char(SUBSECOND_MODE) ? "Exp. (10ms):" : "Exposure (s):";
    m_cameraExposureSpinner = new Fl_Spinner(x, y, w, h, expstring); y += margin;
    m_cameraExposureSpinner->maximum(255);
    m_cameraExposureSpinner->minimum(1);

    x = 220; w = 120;
    m_cameraButtonConfirm = new Fl_Button(x, y, w, h, "Save");
    m_cameraButtonConfirm->user_data((void*)(this));
    m_cameraButtonConfirm->callback(buttonCameraConfirmPressed);
    m_cameraWindow->end();
}

void FLTKGui::showCameraWindow()
{
    uint8_t port_id = pn_preference_char(CAMERA_READPORT_MODE);
    uint8_t speed_id = pn_preference_char(CAMERA_READSPEED_MODE);
    uint8_t gain_id = pn_preference_char(CAMERA_GAIN_MODE);
    cameraRebuildPortTree(port_id, speed_id, gain_id);

    m_cameraExposureSpinner->value(pn_preference_char(EXPOSURE_TIME));
    m_cameraBinningSpinner->value(pn_preference_char(CAMERA_BINNING));

    // Useable chip region xmin,xmax, ymin,ymax
    uint16_t ccd_region[4];
    camera_ccd_region(m_cameraRef, ccd_region);
    m_cameraWindowX->minimum(ccd_region[0]);
    m_cameraWindowX->maximum(ccd_region[1]);
    m_cameraWindowX->value(pn_preference_int(CAMERA_WINDOW_X));

    m_cameraWindowY->minimum(ccd_region[2]);
    m_cameraWindowY->maximum(ccd_region[3]);
    m_cameraWindowY->value(pn_preference_int(CAMERA_WINDOW_Y));

    m_cameraWindowWidth->minimum(1);
    m_cameraWindowWidth->maximum(ccd_region[1] - ccd_region[0] + 1);
    m_cameraWindowWidth->value(pn_preference_int(CAMERA_WINDOW_WIDTH));

    m_cameraWindowHeight->minimum(1);
    m_cameraWindowHeight->maximum(ccd_region[3] - ccd_region[2] + 1);
    m_cameraWindowHeight->value(pn_preference_int(CAMERA_WINDOW_HEIGHT));

    char buf[32];
    snprintf(buf, 32, "%.2f", pn_preference_int(CAMERA_TEMPERATURE) / 100.0);
    m_cameraTemperatureInput->value(buf);

    m_cameraWindow->show();
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
    m_metadataWindow = new Fl_Double_Window(420, 180, "Set Metadata");
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
    m_metadataCountdownInput = new Fl_Int_Input(x, y, w, h, "Total:"); y += margin;
    m_metadataCountdownInput->hide();
    m_metadataObserversInput = new Fl_Input(x, y, w, h, "Observers:");

    x = 300;
    y = 75;
    m_metadataObservatoryInput = new Fl_Input(x, y, w, h, "Observatory:"); y += margin;
    m_metadataTelecopeInput = new Fl_Input(x, y, w, h, "Telescope:"); y += margin;
    m_metadataFilterInput = new Fl_Input(x, y, w, h, "Filter:"); y += margin;

    m_metadataButtonConfirm = new Fl_Button(x, y, w, h, "Save");
    m_metadataButtonConfirm->user_data((void*)(this));
    m_metadataButtonConfirm->callback(buttonMetadataConfirmPressed);
    m_metadataWindow->end();
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
    populate_string_preference(m_metadataFilterInput, FILTER);
    m_metadataWindow->show();
}

void FLTKGui::buttonMetadataConfirmPressed(Fl_Widget* o, void *userdata)
{
    FLTKGui* gui = (FLTKGui *)userdata;

    // Validate parameters
    int run_number = atoi(gui->m_metadataRunNumber->value());
    int calibration_countdown = atoi(gui->m_metadataCountdownInput->value());
    if (run_number < 0)
    {
        pn_log("RUN_NUMBER number must be positive.");
        return;
    }

    if (calibration_countdown < 0)
    {
        pn_log("CALIBRATION_COUNTDOWN must be positive.");
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
    set_string_from_input(FILTER, gui->m_metadataFilterInput);
    set_int(CALIBRATION_COUNTDOWN, calibration_countdown);

    gui->updateAcquisitionGroup();
    gui->m_metadataWindow->hide();
}

void FLTKGui::updateButtonGroup()
{
    bool save_pressed = pn_preference_char(SAVE_FRAMES) && pn_preference_allow_save();
    bool acquire_pressed = cached_camera_mode != IDLE;
    bool acquire_disabled = cached_camera_mode != ACQUIRING && cached_camera_mode != IDLE;

    m_buttonAcquire->value(acquire_pressed);
    if (acquire_disabled)
        m_buttonAcquire->deactivate();
    else
        m_buttonAcquire->activate();

    if (acquire_pressed)
    {
        m_buttonCamera->deactivate();
        m_cameraWindow->hide();
    }
    else
        m_buttonCamera->activate();

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
    gui->m_cameraWindow->hide();
    gui->m_metadataWindow->hide();
    window->hide();
}

FLTKGui::FLTKGui(Camera *camera, TimerUnit *timer)
    : m_cameraRef(camera), m_timerRef(timer)
{
	// Create the main window
	m_mainWindow = new Fl_Double_Window(660, 355, "Acquisition Control");
    m_mainWindow->user_data((void*)(this));
    m_mainWindow->callback(closeMainWindowCallback);

    createTimerGroup();
    createCameraGroup();
    createAcquisitionGroup();
    createLogGroup();
    createButtonGroup();

    createCameraWindow();
    createMetadataWindow();
	m_mainWindow->end();

    // Set initial state
    cached_camera_mode = camera_mode(camera);
    cached_camera_temperature = camera_temperature(camera);
    cached_calibration_framecount = pn_preference_int(CALIBRATION_COUNTDOWN);
    cached_run_number = pn_preference_int(RUN_NUMBER);
    cached_camera_downloading = timer_camera_downloading(timer);
    cached_camera_readout = camera_readout_time(m_cameraRef);
    cached_exposure_time = pn_preference_char(EXPOSURE_TIME);
    cached_subsecond_mode = pn_preference_char(SUBSECOND_MODE);

    updateTimerGroup();
    updateCameraGroup();
    updateAcquisitionGroup();
    updateButtonGroup();

	m_mainWindow->show();
}

FLTKGui::~FLTKGui()
{
    // The window destructor cleans up all child widgets
    delete m_mainWindow;
    delete m_cameraWindow;
    delete m_metadataWindow;
}
