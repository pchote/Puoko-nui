/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <gtk/gtk.h>
#include "camera.h"
#include "gps.h"
#include "preferences.h"

#ifndef VIEW_H
#define VIEW_H

typedef struct
{
	/* Pointers to hardware for gui updates */
	RangahauCamera *camera;
	RangahauGPS *gps;
	RangahauPreferences *prefs;

	GtkWidget *window;
	/* Acquire panel */
	GtkWidget *startstop_btn;
	GtkWidget *save_checkbox;
	GtkWidget *display_checkbox;

	/* Hardware panel */
	GtkWidget *gpstime_label;
	GtkWidget *pctime_label;
	GtkWidget *camerastatus_label;

	/* Settings panel */
	GtkWidget *observers_entry;
	GtkWidget *observatory_entry;
	GtkWidget *telescope_entry;
	GtkWidget *target_combobox;
	GtkWidget *target_entry;
	GtkWidget *binsize_entry;
	GtkWidget *exptime_entry;

	/* Destination panel */
	GtkWidget *destination_btn;
	GtkWidget *run_entry;
	GtkWidget *frame_entry;
} RangahauView;

void rangahau_init_gui(RangahauView *view, void (starstop_pressed_cb)(GtkWidget *, void *));
void rangahau_set_camera_editable(RangahauView *view, gboolean editable);

void rangahau_update_camera_preferences(RangahauView *view);
void rangahau_update_output_preferences(RangahauView *view);
#endif

