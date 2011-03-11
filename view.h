/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
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
	PNCamera *camera;
	PNGPS *gps;
	PNPreferences *prefs;

	GtkWidget *window;
	/* Acquire panel */
	GtkWidget *startstop_btn;
	GtkWidget *save_checkbox;
	GtkWidget *display_checkbox;

	/* Hardware panel */
	GtkWidget *gpsstatus_label;
	GtkWidget *gpstime_label;
	GtkWidget *pctime_label;
	GtkWidget *camerastatus_label;
	GtkWidget *cameratemp_label;

	/* Settings panel */
	GtkWidget *observers_entry;
	GtkWidget *observatory_entry;
	GtkWidget *telescope_entry;
	GtkWidget *target_combobox;
	GtkWidget *target_entry;
	GtkWidget *exptime_entry;

	/* Destination panel */
	GtkWidget *destination_btn;
	GtkWidget *run_entry;
	GtkWidget *frame_entry;
} PNView;

void pn_init_gui(PNView *view, void (starstop_pressed_cb)(GtkWidget *, void *));
void pn_set_camera_editable(PNView *view, gboolean editable);

void pn_update_camera_preferences(PNView *view);
void pn_update_output_preferences(PNView *view);
#endif

