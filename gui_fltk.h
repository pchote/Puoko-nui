/*
 * Copyright 2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>

extern "C" {
    #include "gps.h"
    #include "camera.h"
    #include "preferences.h"
    #include "common.h"
}

#ifndef GUI_FLTK_H
#define GUI_FLTK_H

extern "C"
{
    void init_log_gui();
    void add_log_line(char *msg);
    void pn_ui_new();
    bool pn_ui_update();
    void pn_ui_free();
}



class FLTKGui
{
public:
	FLTKGui();
	~FLTKGui();
    void addLogLine(const char *msg);
    void updateTimerGroup();
    void updateCameraGroup(PNCameraMode mode, int camera_downloading, float temperature);

private:
    static Fl_Group *createGroupBox(int y, int h, const char *label);
    static Fl_Output *createOutputLabel(int y, const char *label);
    void createTimerGroup();
    void createCameraGroup();
    void createAcquisitionGroup();
    void createLogGroup();

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
};

#endif
