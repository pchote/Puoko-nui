/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef GUI_FLTK_H
#define GUI_FLTK_H

// FLTK headers are full of (void *)0 instead of NULLs
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"

#pragma GCC diagnostic ignored "-Wlong-long"
#include <FL/Fl.H>
#pragma GCC diagnostic warning "-Wlong-long"

#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Float_Input.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Int_Input.H>
#include <FL/Fl_Multi_Browser.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Spinner.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Toggle_Button.H>

#pragma GCC diagnostic warning "-Wint-to-pointer-cast"

extern "C" {
    #include "timer.h"
    #include "camera.h"
    #include "preferences.h"
    #include "main.h"
    #include "platform.h"
    #include "gui.h"
}

class FLTKGui
{
public:
	FLTKGui(Camera *camera, TimerUnit *timer);
	~FLTKGui();
    void addLogLine(const char *msg);
    bool update();
    void updateTimerGroup();
    void updateCameraGroup();
    void updateAcquisitionGroup();
    void updateButtonGroup();
    void showErrorPanel();

private:
    static Fl_Group *createGroupBox(int y, int h, const char *label);
    static Fl_Output *createOutputLabel(int y, const char *label);
    void createTimerGroup();
    void createCameraGroup();
    void createAcquisitionGroup();
    void createLogGroup();
    void createButtonGroup();

    void cameraRebuildPortTree(uint8_t port_id, uint8_t speed_id, uint8_t gain_id);
    void createCameraWindow();
    void showCameraWindow();
    void createMetadataWindow();
    void showMetadataWindow();
    void createErrorPanel();

    static void cameraPortSpeedGainChangedCallback(Fl_Widget *input, void *userdata);
    static uint16_t findDesiredExposure(FLTKGui *gui, uint8_t mode);
    static void cameraTimingModeChangedCallback(Fl_Widget *input, void *userdata);
    static void buttonMetadataPressed(Fl_Widget* o, void *v);
    static void buttonCameraPressed(Fl_Widget* o, void *v);
    static void buttonAcquirePressed(Fl_Widget* o, void *v);
    static void buttonSavePressed(Fl_Widget* o, void *v);
    static void buttonReductionPressed(Fl_Widget* o, void *v);
    static void buttonQuitPressed(Fl_Widget* o, void *v);

    static void buttonCameraConfirmPressed(Fl_Widget* o, void *v);
    static void buttonMetadataConfirmPressed(Fl_Widget* o, void *v);
    static void buttonErrorConfirmPressed(Fl_Widget* o, void *v);

    static void buttonMetadataOutputDirPressed(Fl_Widget *input, void *v);
    static void metadataAcquisitionTypeChangedCallback(Fl_Widget *input, void *v);
    static void metadataFrameTypeChangedCallback(Fl_Widget *input, void *v);

    static void closeMainWindowCallback(Fl_Widget *window, void *v);

    Camera *m_cameraRef;
    TimerUnit *m_timerRef;

    Fl_Double_Window *m_mainWindow;
    bool shutdown_requested;

    // Timer info
    Fl_Group *m_timerGroup;
    Fl_Output *m_timerPCTimeOutput;
    Fl_Output *m_timerUTCTimeOutput;
    Fl_Output *m_timerUTCDateOutput;
    Fl_Output *m_timerExposureOutput;

    // Camera info
    Fl_Group *m_cameraGroup;
    Fl_Output *m_cameraStatusOutput;
    Fl_Output *m_cameraTemperatureOutput;
    Fl_Output *m_cameraReadoutOutput;

    // Acquisition info
    Fl_Group *m_acquisitionGroup;
    Fl_Output *m_acquisitionObserversOutput;
    Fl_Output *m_acquisitionTargetOutput;
    Fl_Output *m_acquisitionBurstOutput;
    Fl_Output *m_acquisitionFilenameOutput;
    
    // Log panel
    Fl_Multi_Browser *m_logDisplay;
    size_t m_logEntries;

    // Action buttons
    Fl_Button *m_buttonMetadata;
    Fl_Button *m_buttonCamera;
    Fl_Toggle_Button *m_buttonAcquire;
    Fl_Toggle_Button *m_buttonSave;
    Fl_Toggle_Button *m_buttonReduction;
    Fl_Button *m_buttonQuit;

    // Temporary state comparables
    PNCameraMode cached_camera_mode;
    double cached_camera_temperature;
    double cached_camera_readout;
    bool cached_burst_enabled;
    int cached_burst_countdown;
    int cached_run_number;
    uint16_t cached_exposure_time;
    TimerMode cached_timer_mode;
    uint8_t cached_trigger_mode;
    bool cached_readout_display;

    // Camera window
    Fl_Double_Window *m_cameraWindow;
    Fl_Choice *m_cameraPortInput;
    Fl_Choice *m_cameraSpeedInput;
    Fl_Choice *m_cameraGainInput;
    Fl_Choice *m_cameraTimingModeInput;
    Fl_Choice *m_cameraShutterInput;
    Fl_Check_Button *m_timerAlignFirstExposureCheckbox;
    uint16_t m_cameraCachedPreBiasExposure;
    uint8_t m_cameraCachedPreBiasType;
    uint8_t m_cameraCachedTimingMode;

    Fl_Spinner *m_cameraExposureSpinner;
    Fl_Float_Input *m_cameraTemperatureInput;
    Fl_Spinner *m_cameraBinningSpinner;
    Fl_Button *m_cameraButtonConfirm;

    Fl_Spinner *m_cameraWindowX;
    Fl_Spinner *m_cameraWindowY;
    Fl_Spinner *m_cameraWindowWidth;
    Fl_Spinner *m_cameraWindowHeight;

    // Metadata window
    Fl_Double_Window *m_metadataWindow;
    Fl_Button *m_metadataButtonConfirm;

    Fl_Button *m_metadataOutputDir;
    Fl_Input *m_metadataRunPrefix;
    Fl_Int_Input *m_metadataRunNumber;

    Fl_Choice *m_metadataAcquistionInput;
    Fl_Int_Input *m_metadataBurstInput;

    Fl_Choice *m_metadataFrameTypeInput;
    Fl_Input *m_metadataTargetInput;
    Fl_Input *m_metadataObserversInput;
    Fl_Input *m_metadataObservatoryInput;
    Fl_Input *m_metadataTelecopeInput;
    Fl_Input *m_metadataFilterInput;

    // Error panel
    Fl_Group *m_errorPanel;
 };

#endif
