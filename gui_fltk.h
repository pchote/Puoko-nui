/*
 * Copyright 2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef GUI_FLTK_H
#define GUI_FLTK_H

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Toggle_Button.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Value_Input.H>
#include <FL/fl_ask.H>

extern "C" {
    #include "timer.h"
    #include "camera.h"
    #include "preferences.h"
    #include "common.h"
    #include "gui.h"
}

class FLTKGui
{
public:
	FLTKGui(PNCamera *camera, TimerUnit *timer);
	~FLTKGui();
    void addLogLine(const char *msg);
    bool update();
    void updateTimerGroup();
    void updateCameraGroup(PNCameraMode mode, int camera_downloading, float temperature);
    void updateAcquisitionGroup();
    void updateButtonGroup(PNCameraMode mode);

private:
    static Fl_Group *createGroupBox(int y, int h, const char *label);
    static Fl_Output *createOutputLabel(int y, const char *label);
    void createTimerGroup();
    void createCameraGroup();
    void createAcquisitionGroup();
    void createLogGroup();
    void createButtonGroup();

    void createExposureWindow();
    void showExposureWindow();

    static void buttonMetadataPressed(Fl_Widget* o, void *v);
    static void buttonExposurePressed(Fl_Widget* o, void *v);
    static void buttonAcquirePressed(Fl_Widget* o, void *v);
    static void buttonSavePressed(Fl_Widget* o, void *v);
    static void buttonQuitPressed(Fl_Widget* o, void *v);

    static void buttonExposureConfirmPressed(Fl_Widget* o, void *v);
    static void closeMainWindowCallback(Fl_Widget *window, void *v);

    PNCamera *m_cameraRef;
    TimerUnit *m_timerRef;

    Fl_Window *m_mainWindow;

    // Timer info
    Fl_Group *m_timerGroup;
    Fl_Output *m_timerStatusOutput;
    Fl_Output *m_timerPCTimeOutput;
    Fl_Output *m_timerUTCTimeOutput;
    Fl_Output *m_timerCountdownOutput;

    // Camera info
    Fl_Group *m_cameraGroup;
    Fl_Output *m_cameraStatusOutput;
    Fl_Output *m_cameraTemperatureOutput;

    // Acquisition info
    Fl_Group *m_acquisitionGroup;
    Fl_Output *m_acquisitionObserversOutput;
    Fl_Output *m_acquisitionTypeOutput;
    Fl_Output *m_acquisitionTargetOutput;
    Fl_Output *m_acquisitionRemainingOutput;
    Fl_Output *m_acquisitionExposureOutput;
    Fl_Output *m_acquisitionFilenameOutput;
    
    // Log panel
    Fl_Text_Buffer *m_logBuffer;
    Fl_Text_Display *m_logDisplay;

    // Action buttons
    Fl_Button *m_buttonMetadata;
    Fl_Button *m_buttonExposure;
    Fl_Toggle_Button *m_buttonAcquire;
    Fl_Toggle_Button *m_buttonSave;
    Fl_Button *m_buttonQuit;

    // Temporary state comparables
    PNCameraMode last_camera_mode;
    float last_camera_temperature;
    float last_camera_readout_time;
    int last_calibration_framecount;
    int last_run_number;
    int last_camera_downloading;

    // Exposure window
    Fl_Window *m_exposureWindow;
    Fl_Value_Input *m_exposureInput;
    Fl_Button *m_exposureButtonConfirm;

 };

#endif
