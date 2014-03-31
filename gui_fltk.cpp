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
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_File_Icon.H>
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

void pn_ui_show_fatal_error()
{
    gui->showErrorPanel();
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
    // Restrict log preview to the last 1000 lines
    if (m_logEntries >= 1000)
        m_logDisplay->remove(m_logDisplay->topline());
    else
        m_logEntries++;

    m_logDisplay->add(msg);
    m_logDisplay->bottomline(m_logDisplay->size());
}

bool FLTKGui::update()
{
    PNCameraMode mode = camera_mode(m_cameraRef);
    double temperature = camera_temperature(m_cameraRef);
    double readout = camera_readout_time(m_cameraRef);
    TimerMode timermode = timer_mode(m_timerRef);
    uint16_t exposure_time = pn_preference_int(EXPOSURE_TIME);

    if (cached_camera_mode != mode ||
        cached_timer_mode != timermode)
    {
        cached_camera_mode = mode;
        cached_timer_mode = timermode;
        updateButtonGroup();
        updateCameraGroup();
    }

    if (cached_camera_temperature != temperature || cached_camera_readout != readout)
    {
        cached_camera_temperature = temperature;
        cached_camera_readout = readout;
        updateCameraGroup();
    }

	bool burst_enabled = pn_preference_char(BURST_ENABLED);
    int burst_countdown = pn_preference_int(BURST_COUNTDOWN);
    int run_number = pn_preference_int(RUN_NUMBER);
    if (burst_enabled != cached_burst_enabled ||
		burst_countdown != cached_burst_countdown ||
        run_number != cached_run_number ||
        exposure_time != cached_exposure_time)
    {
        cached_burst_countdown = burst_countdown;
		cached_burst_enabled = burst_enabled;
        cached_run_number = run_number;
        cached_exposure_time = exposure_time;
        updateAcquisitionGroup();
        updateButtonGroup();
    }

    updateTimerGroup();

    Fl::redraw();
    Fl::check();
    return shutdown_requested;
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
    char buf[32];
    time_t t = time(NULL);
    strftime(buf, 32, "%H:%M:%S", gmtime(&t));
    m_timerPCTimeOutput->value(buf);

    // GPS time
    uint16_t progress = 0;
    if (timer_gps_status(m_timerRef) == GPS_ACTIVE)
    {
        TimerTimestamp ts = timer_current_timestamp(m_timerRef);
        snprintf(buf, 32, "%04d-%02d-%02d", ts.year, ts.month, ts.day);
        m_timerUTCDateOutput->value(buf);
        snprintf(buf, 32, "%02d:%02d:%02d (%s)",
                 ts.hours, ts.minutes, ts.seconds,
                 (ts.locked ? "Locked" : "Unlocked"));
        m_timerUTCTimeOutput->value(buf);
        progress = ts.exposure_progress;
    }
    else
    {
        // No valid timestamp
        m_timerUTCTimeOutput->value("N/A");
        m_timerUTCDateOutput->value("N/A");
    }

    if (cached_trigger_mode == TRIGGER_BIAS)
    {
        m_timerExposureOutput->value("N/A");
        return;
    }

    const char *message = "";
    bool display_progress = false;
    switch (cached_timer_mode)
    {
        case TIMER_READOUT:
        case TIMER_EXPOSING:
        {
            bool short_exposure = (cached_trigger_mode == TRIGGER_MILLISECONDS && cached_exposure_time < 5000) || cached_exposure_time < 5;

            if (short_exposure)
                message = "(Active)";
            else if (cached_readout_display && cached_timer_mode == TIMER_READOUT)
                message = "(Read)";

            if (!short_exposure)
                display_progress = true;
            break;
        }
        break;
        case TIMER_WAITING:
            message = "(Waiting)";
        break;
        case TIMER_ALIGN:
            message = "(Align)";
        break;
        case TIMER_IDLE:
            message = "(Disabled)";
        break;
        default:
        break;
    }

    // Construct "Exposure:" string
    buf[0] = '\0';
    uint16_t total = cached_exposure_time;
    uint16_t total_ms = 0;

    if (cached_trigger_mode != TRIGGER_SECONDS)
    {
        progress /= 1000;
        total /= 1000;
        total_ms = cached_exposure_time % 1000;
    }

    if (display_progress)
        strncatf(buf, 32, "%u / ", progress);

    strncatf(buf, 32, "%u", total);
    if (total_ms)
        strncatf(buf, 32, ".%03d", total_ms);

    strncatf(buf, 32, " s %s", message);
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
        snprintf(buf, 11, "%0.03f sec", cached_camera_readout);
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
    m_acquisitionTargetOutput = createOutputLabel(y, "Target:"); y += margin;
    m_acquisitionBurstOutput = createOutputLabel(y, "Acquisition:"); y += margin;
    m_acquisitionFilenameOutput = createOutputLabel(y, "File:");
    m_acquisitionGroup->end();
}

void FLTKGui::updateAcquisitionGroup()
{
    PNFrameType type = (PNFrameType)pn_preference_char(OBJECT_TYPE);
    char *run_prefix = pn_preference_string(RUN_PREFIX);
    char *object = pn_preference_string(OBJECT_NAME);

    switch (type)
    {
		// bias is handled separately, so treat it as a dark here.
		case OBJECT_BIAS:
        case OBJECT_DARK:
            m_acquisitionTargetOutput->value("Dark");
            break;
        case OBJECT_FLAT:
            m_acquisitionTargetOutput->value("Flat");
            break;
        case OBJECT_FOCUS:
            m_acquisitionTargetOutput->value("Focus");
            break;
        case OBJECT_TARGET:
			m_acquisitionTargetOutput->value(object);
            break;
    }

	if (cached_trigger_mode == TRIGGER_BIAS)
        m_acquisitionTargetOutput->value("Bias");

    char buf[100];
	if (cached_burst_enabled)
   		snprintf(buf, 100, "%d Remaining", cached_burst_countdown);
	else
		strcpy(buf, "Continuous");
    m_acquisitionBurstOutput->value(buf);

    snprintf(buf, 100, "%s-%04d.fits.gz", run_prefix, cached_run_number);
    m_acquisitionFilenameOutput->value(buf);
    free(object);
    free(run_prefix);
}

void FLTKGui::createLogGroup()
{
    m_logDisplay = new Fl_Multi_Browser(270, 10, 430, 295);
    m_logEntries = 0;
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
        clear_queued_data(true);
        camera_start_exposure(gui->m_cameraRef, !pn_preference_char(CAMERA_DISABLE_SHUTTER));

		if (gui->cached_trigger_mode != TRIGGER_BIAS)
		{
        	bool use_monitor = !camera_is_simulated(gui->m_cameraRef) && pn_preference_char(TIMER_MONITOR_LOGIC_OUT);
        	timer_start_exposure(gui->m_timerRef, pn_preference_int(EXPOSURE_TIME), use_monitor);
		}
    }
    else if (mode == ACQUIRING)
    {
        camera_stop_exposure(gui->m_cameraRef);
        if (gui->cached_trigger_mode != TRIGGER_BIAS)
            timer_stop_exposure(gui->m_timerRef);
        else
            camera_notify_safe_to_stop(gui->m_cameraRef);
    }

    gui->updateButtonGroup();
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

void FLTKGui::buttonReductionPressed(Fl_Widget* o, void *userdata)
{
    bool reduce = pn_preference_char(REDUCE_FRAMES);
    pn_preference_set_char(REDUCE_FRAMES, !reduce);
    char *prefix = pn_preference_string(RUN_PREFIX);
    pn_log("%s reduction of %s.dat.", !reduce ? "Enabled" : "Disabled", prefix);
    free(prefix);
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

    m_buttonAcquire = new Fl_Toggle_Button(270, y, 100, 30, "Acquire");
    m_buttonAcquire->user_data((void*)(this));
    m_buttonAcquire->callback(buttonAcquirePressed);

    m_buttonSave = new Fl_Toggle_Button(380, y, 100, 30, "Save");
    m_buttonSave->user_data((void*)(this));
    m_buttonSave->callback(buttonSavePressed);

    m_buttonReduction = new Fl_Toggle_Button(490, y, 100, 30, "Reduction");
    m_buttonReduction->user_data((void*)(this));
    m_buttonReduction->callback(buttonReductionPressed);

    m_buttonQuit = new Fl_Button(600, y, 100, 30, "Quit");
    m_buttonQuit->user_data((void*)(this));
    m_buttonQuit->callback(buttonQuitPressed);
}

#define set_string(a,b) _set_string(a, # a, b)
void _set_string(PNPreferenceType key, const char *name, const char *str)
{
    char *oldval = pn_preference_string(key);
    if (strcmp(oldval, str))
    {
        pn_preference_set_string(key, str);
        pn_log("%s set to `%s'.", name, str);
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

	if (camera_supports_shutter_disabling(gui->m_cameraRef))
    	set_char(CAMERA_DISABLE_SHUTTER, (uint8_t)(gui->m_cameraShutterInput->value()));

    set_char(CAMERA_BINNING, (uint8_t)(gui->m_cameraBinningSpinner->value()));

    set_char(TIMER_TRIGGER_MODE, (uint8_t)(gui->m_cameraTimingModeInput->value()));
    gui->cached_trigger_mode = pn_preference_char(TIMER_TRIGGER_MODE);

	if (gui->cached_trigger_mode != TRIGGER_BIAS)
    	set_int(EXPOSURE_TIME, (uint16_t)(gui->m_cameraExposureSpinner->value()));

	set_char(TIMER_ALIGN_FIRST_EXPOSURE, (uint8_t)(gui->m_timerAlignFirstExposureCheckbox->value()));

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


uint16_t FLTKGui::findDesiredExposure(FLTKGui *gui, uint8_t new_mode)
{
	uint16_t old_exp = gui->m_cameraExposureSpinner->value();
	uint8_t old_mode = gui->m_cameraCachedTimingMode;

	if (old_mode == TRIGGER_BIAS)
	{
		// Restore the stashed values
		old_exp = gui->m_cameraCachedPreBiasExposure;
		old_mode = gui->m_cameraCachedPreBiasType;
	}

	if (old_mode == new_mode)
		return old_exp;

	if (old_mode == TRIGGER_MILLISECONDS)
	{
		// ms -> s: Round up to the next second
		return (uint16_t)((uint32_t)(old_exp + 999) / 1000);
	}
	else if (old_mode == TRIGGER_SECONDS)
	{
		// s -> ms: Limit at max exposure length instead of overflowing
		return (uint16_t)fmin(65535, (uint32_t)(old_exp * 1000));
	}

	return old_exp;
}

void FLTKGui::cameraTimingModeChangedCallback(Fl_Widget *input, void *userdata)
{
    FLTKGui* gui = (FLTKGui *)userdata;
	uint8_t mode = gui->m_cameraTimingModeInput->value();

    const char *expstring = mode == TRIGGER_SECONDS ? "Exposure (s):" : "Exposure (ms):";
    gui->m_cameraExposureSpinner->label(expstring);

	if (mode == TRIGGER_BIAS)
	{
		gui->m_cameraCachedPreBiasExposure = gui->m_cameraExposureSpinner->value();
		gui->m_cameraCachedPreBiasType = gui->m_cameraCachedTimingMode;

		gui->m_cameraExposureSpinner->deactivate();
		gui->m_timerAlignFirstExposureCheckbox->deactivate();
		gui->m_cameraExposureSpinner->value(0);
		gui->m_metadataFrameTypeInput->value(OBJECT_BIAS);
	}
	else
	{
		gui->m_cameraExposureSpinner->activate();
		gui->m_timerAlignFirstExposureCheckbox->activate();
		gui->m_cameraExposureSpinner->value(findDesiredExposure(gui, mode));
		gui->m_metadataFrameTypeInput->value(pn_preference_char(OBJECT_TYPE));
	}

	gui->m_cameraCachedTimingMode = mode;
	metadataFrameTypeChangedCallback(gui->m_metadataFrameTypeInput, gui);
}

void FLTKGui::createCameraWindow()
{
    m_cameraWindow = new Fl_Double_Window(395, 230, "Set Camera Parameters");
    m_cameraWindow->user_data((void*)(this));

    Fl_Group *readoutGroup = new Fl_Group(10, 10, 375, 80, "Readout Geometry");
    readoutGroup->box(FL_ENGRAVED_BOX);
    readoutGroup->align(FL_ALIGN_INSIDE|FL_ALIGN_TOP);
    readoutGroup->labelsize(14);
    readoutGroup->labelfont(FL_BOLD);

    int x = 20, y = 35, h = 20, w = 65, margin = 25;
    m_cameraWindowX = new Fl_Spinner(90, y, w, 20, "x,y (px):");
    m_cameraWindowY = new Fl_Spinner(177, y, w, 20, ",  ");
    y += margin;
    m_cameraWindowWidth = new Fl_Spinner(90, y, w, 20, "Size (px):");
    m_cameraWindowHeight = new Fl_Spinner(177, y, w, 20, " x ");

    m_cameraBinningSpinner = new Fl_Spinner(325, y, 50, 20, "Bin (px):");
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

	m_cameraShutterInput = new Fl_Choice(x, y, w, h, "Shutter:"); y += margin;
    m_cameraShutterInput->user_data((void*)(this));
    m_cameraShutterInput->add("Open");
    m_cameraShutterInput->add("Closed");
	m_cameraShutterInput->add("N\\/A", 0, 0, 0, FL_MENU_INVISIBLE);

    x = 295; y = 100; w = 90;

    m_cameraTemperatureInput = new Fl_Float_Input(x, y, w, h, "Temp. (\u00B0C):"); y += margin;

	m_cameraTimingModeInput = new Fl_Choice(x, y, w, h, "Trigger Type:"); y += margin;
    m_cameraTimingModeInput->callback(cameraTimingModeChangedCallback);
    m_cameraTimingModeInput->user_data((void*)(this));
    m_cameraTimingModeInput->add("Low Res");
    m_cameraTimingModeInput->add("High Res");
	if (camera_supports_bias_acquisition(m_cameraRef))
    	m_cameraTimingModeInput->add("Bias");

    m_cameraExposureSpinner = new Fl_Spinner(x, y, w, h, ""); y += margin;
    m_cameraExposureSpinner->maximum(65535);
    m_cameraExposureSpinner->minimum(1);

	m_timerAlignFirstExposureCheckbox = new Fl_Check_Button(x - 75, y, w + 75, h, "Align first exposure"); y += margin;

    x = 265; w = 120;
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

	m_cameraCachedPreBiasExposure = pn_preference_int(EXPOSURE_TIME);
    m_cameraExposureSpinner->value(m_cameraCachedPreBiasExposure);
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

	m_cameraCachedTimingMode = m_cameraCachedPreBiasType = pn_preference_char(TIMER_TRIGGER_MODE);
    m_cameraTimingModeInput->value(gui->m_cameraCachedTimingMode);
    cameraTimingModeChangedCallback(m_cameraTimingModeInput, this);

    m_cameraShutterInput->value(pn_preference_char(CAMERA_DISABLE_SHUTTER));
	if (!camera_supports_shutter_disabling(m_cameraRef))
	{
		m_cameraShutterInput->deactivate();
		m_cameraShutterInput->value(2);
	}

    m_timerAlignFirstExposureCheckbox->value(pn_preference_char(TIMER_ALIGN_FIRST_EXPOSURE));

    m_cameraWindow->show();
}


void FLTKGui::buttonMetadataOutputDirPressed(Fl_Widget *button, void *userdata)
{
	FLTKGui *gui = (FLTKGui *)userdata;

	Fl_Native_File_Chooser fc;
	fc.title("Choose the frame output directory");
	fc.type(Fl_Native_File_Chooser::BROWSE_DIRECTORY);
	fc.options(Fl_Native_File_Chooser::NEW_FOLDER);

	char *dir = platform_path(gui->m_metadataOutputDir->label());
	fc.directory(dir);
    free(dir);

	switch (fc.show())
	{
		case -1:
			pn_log("Error querying data directory: %s\n", fc.errmsg());
		break;
		case 1:
		break;
		default:
	    	gui->m_metadataOutputDir->copy_label(canonicalize_path(fc.filename()));
		break;
	}
}

void FLTKGui::metadataAcquisitionTypeChangedCallback(Fl_Widget *input, void *userdata)
{
	FLTKGui *gui = (FLTKGui *)userdata;

	if (((Fl_Choice *)input)->value())
	{
	    populate_int_preference(gui->m_metadataBurstInput, BURST_COUNTDOWN);
		gui->m_metadataBurstInput->activate();
	}
	else
	{
		gui->m_metadataBurstInput->value("N/A");
		gui->m_metadataBurstInput->deactivate();
	}
}

void FLTKGui::metadataFrameTypeChangedCallback(Fl_Widget *input, void *userdata)
{
	FLTKGui *gui = (FLTKGui *)userdata;
	uint8_t type = ((Fl_Choice *)input)->value();

    if (type == OBJECT_TARGET)
	{
	    populate_string_preference(gui->m_metadataTargetInput, OBJECT_NAME);
		gui->m_metadataTargetInput->activate();
	}
	else
	{
		gui->m_metadataTargetInput->value("N/A");
		gui->m_metadataTargetInput->deactivate();
	}

	if (type == OBJECT_BIAS)
		gui->m_metadataFrameTypeInput->deactivate();
	else
		gui->m_metadataFrameTypeInput->activate();
}

void FLTKGui::createMetadataWindow()
{
    m_metadataWindow = new Fl_Double_Window(430, 200, "Set Metadata");
    m_metadataWindow->user_data((void*)(this));

    // File output
    Fl_Group *outputGroup = new Fl_Group(10, 10, 410, 55, "Output Filename");
    outputGroup->box(FL_ENGRAVED_BOX);
    outputGroup->align(FL_ALIGN_INSIDE|FL_ALIGN_TOP);
    outputGroup->labelsize(14);
    outputGroup->labelfont(FL_BOLD);

    int x = 20, y = 35, h = 20, margin = 25;
    m_metadataOutputDir = new Fl_Button(x, y, 170, h); x+= 180;
	m_metadataOutputDir->align(FL_ALIGN_CLIP | FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
    m_metadataOutputDir->user_data((void*)(this));
    m_metadataOutputDir->callback(buttonMetadataOutputDirPressed);

    m_metadataRunPrefix = new Fl_Input(x, y, 100, h, "/"); x+= 110;
    m_metadataRunNumber = new Fl_Int_Input(x, y, 50, h, "-"); x+= 95;

    // Dirty hack... FLTK doesn't have a standalone label widget!?!
    new Fl_Input(x, y, 0, h, ".fits.gz");

    outputGroup->end();

    x = 100;
    y = 75;
    int w = 110;

    m_metadataAcquistionInput = new Fl_Choice(x, y, w, h, "Acquisition:"); y += margin;
    m_metadataAcquistionInput->add("Continuous");
    m_metadataAcquistionInput->add("Burst");
    m_metadataAcquistionInput->callback(metadataAcquisitionTypeChangedCallback);
    m_metadataAcquistionInput->user_data((void*)(this));

    m_metadataFrameTypeInput = new Fl_Choice(x, y, w, h, "Type:"); y += margin;
    m_metadataFrameTypeInput->add("Dark");
    m_metadataFrameTypeInput->add("Flat");
    m_metadataFrameTypeInput->add("Focus");
    m_metadataFrameTypeInput->add("Target");
	m_metadataFrameTypeInput->add("Bias", 0, 0, 0, FL_MENU_INVISIBLE);
    m_metadataFrameTypeInput->callback(metadataFrameTypeChangedCallback);
    m_metadataFrameTypeInput->user_data((void*)(this));

    m_metadataObservatoryInput = new Fl_Input(x, y, w, h, "Observatory:"); y += margin;
    m_metadataObserversInput = new Fl_Input(x, y, w, h, "Observers:"); y += margin;

    x = 310;
    y = 75;
    m_metadataBurstInput = new Fl_Int_Input(x, y, w, h, "Burst Count:"); y += margin;
    m_metadataTargetInput = new Fl_Input(x, y, w, h, "Target:"); y += margin;
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
    char *dir = pn_preference_string(OUTPUT_DIR);
    m_metadataOutputDir->copy_label(dir);
    free(dir);

    populate_string_preference(m_metadataRunPrefix, RUN_PREFIX);
    populate_int_preference(m_metadataRunNumber, RUN_NUMBER);

    populate_string_preference(m_metadataObserversInput, OBSERVERS);
    populate_string_preference(m_metadataObservatoryInput, OBSERVATORY);
    populate_string_preference(m_metadataTelecopeInput, TELESCOPE);
    populate_string_preference(m_metadataFilterInput, FILTER);

    uint8_t burst = pn_preference_char(BURST_ENABLED);
	m_metadataAcquistionInput->value(burst);
	metadataAcquisitionTypeChangedCallback(m_metadataAcquistionInput, this);

    uint8_t object_type = pn_preference_char(OBJECT_TYPE);
	if (cached_trigger_mode == TRIGGER_BIAS)
		object_type = OBJECT_BIAS;

    m_metadataFrameTypeInput->value(object_type);
	metadataFrameTypeChangedCallback(m_metadataFrameTypeInput, this);

    m_metadataWindow->show();
}

void FLTKGui::buttonMetadataConfirmPressed(Fl_Widget* o, void *userdata)
{
    FLTKGui* gui = (FLTKGui *)userdata;

    // Validate parameters
    int run_number = atoi(gui->m_metadataRunNumber->value());
    int burst_countdown = atoi(gui->m_metadataBurstInput->value());
    if (run_number < 0)
    {
        pn_log("RUN_NUMBER number must be positive.");
        return;
    }

    if (burst_countdown < 0)
    {
        pn_log("BURST_COUNTDOWN must be positive.");
        return;
    }

    // Update preferences from fields
    char *output = canonicalize_path(gui->m_metadataOutputDir->label());
    set_string(OUTPUT_DIR, output);
    free(output);

    set_string(RUN_PREFIX, gui->m_metadataRunPrefix->value());
    set_int(RUN_NUMBER, run_number);

	bool burst_enabled = gui->m_metadataAcquistionInput->value();
    pn_preference_set_char(BURST_ENABLED, burst_enabled);
	if (burst_enabled)
    	set_int(BURST_COUNTDOWN, burst_countdown);

	uint8_t object_type = gui->m_metadataFrameTypeInput->value();
	if (object_type != OBJECT_BIAS)
	{
    	pn_preference_set_char(OBJECT_TYPE, object_type);
		if (object_type == OBJECT_TARGET)
    		set_string(OBJECT_NAME, gui->m_metadataTargetInput->value());
	}

    set_string(OBSERVERS, gui->m_metadataObserversInput->value());
    set_string(OBSERVATORY, gui->m_metadataObservatoryInput->value());
    set_string(TELESCOPE, gui->m_metadataTelecopeInput->value());
    set_string(FILTER, gui->m_metadataFilterInput->value());

    gui->updateAcquisitionGroup();
    gui->m_metadataWindow->hide();
    gui->updateButtonGroup();
}

void FLTKGui::createErrorPanel()
{
    int width = 360, height = 105;
    int x = (m_mainWindow->decorated_w() - width) / 2;
    int y = (m_mainWindow->decorated_h() - height) / 2;

    // FLTK doesn't render sub-windows correctly on OSX,
    // so use a group instead of a window
    m_errorPanel = new Fl_Group(x, y, width, height);
    m_errorPanel->box(FL_UP_BOX);

    Fl_Box *message = new Fl_Box(x + 105, y + 15, 255, 75);
    message->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    message->label("A problem has occurred and the\nacquisition software must close.\n\nSee the log for more details.");

    Fl_Box *icon = new Fl_Box(x + 15, y + 15, 75, 75);
    icon->box(FL_THIN_UP_BOX);
    icon->labelfont(FL_TIMES_BOLD);
    icon->labelsize(64);
    icon->color(FL_WHITE);
    icon->labelcolor(FL_RED);
    icon->label("!");

    m_errorPanel->end();
    m_errorPanel->hide();
    m_mainWindow->add(m_errorPanel);
}

void FLTKGui::showErrorPanel()
{
    // Hide any windows that may be open
    m_metadataWindow->hide();
    m_cameraWindow->hide();

    // Disable all buttons except quit
    m_buttonMetadata->deactivate();
    m_buttonCamera->deactivate();
    m_buttonAcquire->deactivate();
    m_buttonSave->deactivate();
    m_buttonReduction->deactivate();

    m_errorPanel->show();
}

void FLTKGui::updateButtonGroup()
{
    if (shutdown_requested || m_errorPanel->visible())
    {
        m_buttonMetadata->deactivate();
        m_buttonCamera->deactivate();
        m_buttonAcquire->deactivate();
        m_buttonSave->deactivate();
        m_buttonReduction->deactivate();
        if (shutdown_requested)
        {
            m_buttonQuit->value(true);
            m_buttonQuit->deactivate();
        }
        return;
    }

    bool acquire_pressed = cached_camera_mode == ACQUIRE_START || cached_camera_mode == ACQUIRING;
    bool acquire_enabled = cached_camera_mode == ACQUIRING || cached_camera_mode == IDLE;
    bool save_enabled = cached_camera_mode == ACQUIRING && pn_preference_allow_save();
    bool save_pressed = save_enabled && pn_preference_char(SAVE_FRAMES) && pn_preference_allow_save();
    bool reduction_enabled = save_pressed;
    bool camera_enabled = cached_camera_mode == IDLE;

    m_buttonAcquire->value(acquire_pressed);
    if (acquire_enabled)
        m_buttonAcquire->activate();
    else
        m_buttonAcquire->deactivate();

    if (camera_enabled)
        m_buttonCamera->activate();
    else
    {
        m_buttonCamera->deactivate();
        m_cameraWindow->hide();
    }

    if (save_enabled)
    {
        m_buttonSave->activate();
        m_buttonSave->value(save_pressed);

        if (save_pressed)
        {
            m_buttonMetadata->deactivate();
            m_metadataWindow->hide();
            m_buttonReduction->activate();
        }
        else
            m_buttonMetadata->activate();
    }
    else
    {
        m_buttonMetadata->activate();

        m_buttonSave->value(false);
        m_buttonSave->deactivate();
        pn_preference_set_char(SAVE_FRAMES, false);
    }

    if (reduction_enabled)
        m_buttonReduction->activate();
    else
    {
        m_buttonReduction->value(false);
        m_buttonReduction->deactivate();
        pn_preference_set_char(REDUCE_FRAMES, false);
    }
}

void FLTKGui::closeMainWindowCallback(Fl_Widget *window, void *v)
{
    FLTKGui* gui = (FLTKGui *)window->user_data();

    // User pressed escape
    if (Fl::event() == FL_SHORTCUT && Fl::event_key() == FL_Escape)
        return;

    // Hide all but the main window (used to display the log)
    gui->m_errorPanel->hide();
    gui->m_cameraWindow->hide();
    gui->m_metadataWindow->hide();

    // Set a flag to be passed to the main loop which
    // will close the final window when it's ready
    gui->shutdown_requested = true;
    gui->updateButtonGroup();
}

FLTKGui::FLTKGui(Camera *camera, TimerUnit *timer)
    : m_cameraRef(camera), m_timerRef(timer), shutdown_requested(false)
{
	Fl_File_Icon::load_system_icons();

	// Create the main window
    m_mainWindow = new Fl_Double_Window(710, 355, "Acquisition Control");
    m_mainWindow->user_data((void*)(this));
    m_mainWindow->callback(closeMainWindowCallback);

    createTimerGroup();
    createCameraGroup();
    createAcquisitionGroup();
    createLogGroup();
    createButtonGroup();

    createCameraWindow();
    createMetadataWindow();
    createErrorPanel();
	m_mainWindow->end();

    // Set initial state
    cached_camera_mode = camera_mode(camera);
    cached_camera_temperature = camera_temperature(camera);
	cached_burst_enabled = pn_preference_char(BURST_ENABLED);
    cached_burst_countdown = pn_preference_int(BURST_COUNTDOWN);
    cached_run_number = pn_preference_int(RUN_NUMBER);
    cached_timer_mode = timer_mode(timer);
    cached_camera_readout = camera_readout_time(m_cameraRef);
    cached_exposure_time = pn_preference_int(EXPOSURE_TIME);
    cached_trigger_mode = pn_preference_char(TIMER_TRIGGER_MODE);
    cached_readout_display = camera_supports_readout_display(m_cameraRef);

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
