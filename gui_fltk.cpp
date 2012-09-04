/*
 * Copyright 2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include "gui_fltk.h"

FLTKGui *gui;

void init_log_gui()
{
    
}

void add_log_line(char *msg)
{
    
}

void pn_ui_new()
{
    gui = new FLTKGui();
}

bool pn_ui_update()
{
    return Fl::check() == 0;
}

void pn_ui_free()
{
    delete gui;
}

Fl_Group *FLTKGui::CreateGroupBox(int y, int h, const char *label)
{
    int b[4] = {10, y, 230, h};
    Fl_Group *output = new Fl_Group(b[0], b[1], b[2], b[3], label);
    output->box(FL_ENGRAVED_BOX);
    output->align(FL_ALIGN_INSIDE|FL_ALIGN_TOP);
    output->labelsize(14);
    output->labelfont(FL_BOLD);
    return output;
}

Fl_Output *FLTKGui::CreateOutputLabel(int y, const char *label)
{
    int b[5] = {105, y, 130, 14};
    Fl_Output *output = new Fl_Output(b[0], b[1], b[2], b[3], label);
    output->box(FL_NO_BOX);
    output->labelsize(14);
    output->textsize(13);
    output->textfont(FL_BOLD);
    output->value("2011-07-30 18:06:49sdfsdfsddssdsdfdsfdssd");
    return output;
}


void FLTKGui::CreateTimerGroup()
{
    int y = 10, margin = 20;
    m_timerGroup = CreateGroupBox(y, 105, "Timer Information"); y += 25;
    m_timerStatusOutput = CreateOutputLabel(y, "Status:"); y += margin;
    m_timerPCTimeOutput = CreateOutputLabel(y, "PC Time:"); y += margin;
    m_timerUTCTimeOutput = CreateOutputLabel(y, "UTC Time:"); y += margin;
    m_timerCountdownOutput = CreateOutputLabel(y, "Countdown:");
    m_timerGroup->end();
}

void FLTKGui::CreateCameraGroup()
{
    // x,y,w,h
    int y = 125, margin = 20;
    m_cameraGroup = CreateGroupBox(y, 65, "Camera Information"); y += 25;
    m_cameraStatusOutput = CreateOutputLabel(y, "Status:"); y += margin;
    m_cameraTemperatureOutput = CreateOutputLabel(y, "Temperature:");
    m_cameraGroup->end();
}

void FLTKGui::CreateAcquisitionGroup()
{
    int y = 200, margin = 20;
    m_acquisitionGroup = CreateGroupBox(y, 145, "Acquisition"); y += 25;
    m_acquisitionObserversOutput = CreateOutputLabel(y, "Observers:"); y += margin;
    m_acquisitionTypeOutput = CreateOutputLabel(y, "Type:"); y += margin;
    m_acquisitionTargetOutput = CreateOutputLabel(y, "Target:"); y += margin;
    m_acquisitionRemainingOutput = CreateOutputLabel(y, "Remaining:"); y += margin;
    m_acquisitionExposureOutput = CreateOutputLabel(y, "Exposure:"); y += margin;
    m_acquisitionFilenameOutput = CreateOutputLabel(y, "Filename:");
    m_acquisitionGroup->end();
}

FLTKGui::FLTKGui()
{
	// Create the main window
	m_mainWindow = new Fl_Window(680, 400, "Acquisition Control");
    m_mainWindow->user_data((void*)(this));

    CreateTimerGroup();
    CreateCameraGroup();
    CreateAcquisitionGroup();

	m_mainWindow->end();
	m_mainWindow->show();
}

FLTKGui::~FLTKGui()
{

}
