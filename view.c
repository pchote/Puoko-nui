/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <time.h>
#include <pthread.h>
#include <fitsio.h>

#include <sys/select.h>
#include <xpa.h>
#include "view.h"

/* target type changed: hide or display the target name field */
static void targettype_changed(GtkWidget *widget, gpointer data)
{
	RangahauView *view = (RangahauView *)data;	
	gboolean visible = (gtk_combo_box_get_active(GTK_COMBO_BOX(view->target_combobox)) == OBJECT_TARGET);
	gtk_widget_set_visible(view->target_entry, visible);
}

/* Called when the startstop button is pressed to enable or disable the settings fields */
void rangahau_set_fields_editable(RangahauView *view, gboolean editable)
{
	/* Settings fields */
	gtk_editable_set_editable(GTK_EDITABLE(view->observers_entry), editable);
	gtk_widget_set_sensitive(view->observers_entry, editable);
	gtk_editable_set_editable(GTK_EDITABLE(view->observatory_entry), editable);
	gtk_widget_set_sensitive(view->observatory_entry, editable);
	gtk_editable_set_editable(GTK_EDITABLE(view->telescope_entry), editable);
	gtk_widget_set_sensitive(view->telescope_entry, editable);
	gtk_editable_set_editable(GTK_EDITABLE(view->exptime_entry), editable);
	gtk_widget_set_sensitive(view->exptime_entry, editable);
	gtk_editable_set_editable(GTK_EDITABLE(view->binsize_entry), editable);
	gtk_widget_set_sensitive(view->binsize_entry, editable);
	gtk_widget_set_sensitive(view->target_combobox, editable);
	gtk_editable_set_editable(GTK_EDITABLE(view->target_entry), editable);
	gtk_widget_set_sensitive(view->target_entry, editable);

	/* Destination fields */
	gtk_widget_set_sensitive(view->destination_btn, editable);
	gtk_editable_set_editable(GTK_EDITABLE(view->run_entry), editable);
	gtk_widget_set_sensitive(view->run_entry, editable);
	gtk_editable_set_editable(GTK_EDITABLE(view->frame_entry), editable);
	gtk_widget_set_sensitive(view->frame_entry, editable);
}

/* Return a GtkWidget containing the settings panel */
GtkWidget *rangahau_settings_panel(RangahauView *view)
{
	/* Frame around the panel */
	GtkWidget *frame = gtk_frame_new("Settings");
	
	/* Table */
	GtkWidget *table = gtk_table_new(5,5,FALSE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_container_add(GTK_CONTAINER(frame), table);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	GtkWidget *field;
	
	/* Telescope */
	field = gtk_label_new("Telescope:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,2,3);
	view->telescope_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(view->telescope_entry), "MJUO 1-meter");
	gtk_entry_set_width_chars(GTK_ENTRY(view->telescope_entry), 12);
	gtk_table_attach_defaults(GTK_TABLE(table), view->telescope_entry, 1,2,2,3);

	/* Observatory */
	field = gtk_label_new("Observatory:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,1,2);
	view->observatory_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(view->observatory_entry), "MJUO");
	gtk_entry_set_width_chars(GTK_ENTRY(view->observatory_entry), 12);
	gtk_table_attach_defaults(GTK_TABLE(table), view->observatory_entry, 1,2,1,2);

	/* Observers */
	field = gtk_label_new("Observers:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,0,1);
	view->observers_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(view->observers_entry), "DJS");
	gtk_entry_set_width_chars(GTK_ENTRY(view->observers_entry), 12);
	gtk_table_attach_defaults(GTK_TABLE(table), view->observers_entry, 1,2,0,1);

	/* Observation type (dark, flat, target) */
	field = gtk_label_new("Type:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 2,3,0,1);

	view->target_combobox = gtk_combo_box_new_text();
	gtk_table_attach_defaults(GTK_TABLE(table), view->target_combobox, 3,4,0,1);
	gtk_combo_box_insert_text(GTK_COMBO_BOX(view->target_combobox), OBJECT_DARK, "Dark");
	gtk_combo_box_insert_text(GTK_COMBO_BOX(view->target_combobox), OBJECT_FLAT, "Flat");
	gtk_combo_box_insert_text(GTK_COMBO_BOX(view->target_combobox), OBJECT_TARGET, "Target");
	gtk_combo_box_set_active(GTK_COMBO_BOX(view->target_combobox), OBJECT_TARGET);
	g_signal_connect(view->target_combobox, "changed", G_CALLBACK (targettype_changed), view);

	view->target_entry = gtk_entry_new();
	gtk_table_attach_defaults(GTK_TABLE(table), view->target_entry, 4,5,0,1);
	gtk_entry_set_text(GTK_ENTRY(view->target_entry), "ec20058");
	gtk_entry_set_width_chars(GTK_ENTRY(view->target_entry), 12);

	/* Bin size */
	field = gtk_label_new("Bin Size:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 2,3,1,2);

	view->binsize_entry = gtk_spin_button_new((GtkAdjustment *)gtk_adjustment_new(1, 1, 1024, 1, 1, 0), 0, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), view->binsize_entry, 3,4,1,2);
	gtk_entry_set_width_chars(GTK_ENTRY(view->binsize_entry), 3);
	field = gtk_label_new("pixel(s)");
	gtk_table_attach_defaults(GTK_TABLE(table), field, 4,5,1,2);
	gtk_misc_set_alignment(GTK_MISC(field), 0, 0.5);

	/* Exposure time */
	field = gtk_label_new("Exp. Time:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 2,3,2,3);

	view->exptime_entry = gtk_spin_button_new((GtkAdjustment *)gtk_adjustment_new(5, 2, 10000, 1, 10, 0), 0, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), view->exptime_entry, 3,4,2,3);
	gtk_entry_set_width_chars(GTK_ENTRY(view->exptime_entry), 3);

	/* Hack: pad the label with extra spaces to the panel 
	 * doesn't reflow then the target entry is hidden */
	field = gtk_label_new("seconds              ");
	gtk_table_attach_defaults(GTK_TABLE(table), field, 4,5,2,3);
	gtk_misc_set_alignment(GTK_MISC(field), 0, 0.5);


	return frame;
}

/* Return a GtkWidget containing the hardware panel */
GtkWidget *rangahau_hardware_panel(RangahauView *view)
{
	GtkWidget *frame = gtk_frame_new("Hardware");
	GtkWidget *table = gtk_table_new(4,2,FALSE);
	gtk_container_add(GTK_CONTAINER(frame), table);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	GtkWidget *field = gtk_label_new("Camera:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,0,1);
	view->camerastatus_label = gtk_label_new("Idle");
	gtk_misc_set_alignment(GTK_MISC(view->camerastatus_label), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), view->camerastatus_label, 1,2,0,1);

	field = gtk_label_new("GPS:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,1,2);
	field = gtk_label_new("TODO");
	gtk_misc_set_alignment(GTK_MISC(field), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 1,2,1,2);


	field = gtk_label_new("GPS Time:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,2,3);
	view->gpstime_label = gtk_label_new("2010-12-10 04:47:57");
	gtk_misc_set_alignment(GTK_MISC(view->gpstime_label), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), view->gpstime_label, 1,2,2,3);

	field = gtk_label_new("PC Time:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,3,4);
	view->pctime_label = gtk_label_new("2010-12-10 04:47:57");
	gtk_misc_set_alignment(GTK_MISC(view->pctime_label), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), view->pctime_label, 1,2,3,4);

	return frame;
}

/* Return a GtkWidget containing the save settings panel */
GtkWidget *rangahau_save_panel(RangahauView *view)
{
	GtkWidget *frame = gtk_frame_new("Output");
	GtkWidget *box = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), box);
	gtk_container_set_border_width(GTK_CONTAINER(box), 0);

	GtkWidget *align = gtk_alignment_new(0,0.5,0,0);
	gtk_box_pack_start(GTK_BOX(box), align, FALSE, FALSE, 5);
	GtkWidget *path = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(align), path);

	GtkWidget *item = gtk_label_new("Directory:");
	gtk_box_pack_start(GTK_BOX(path), item, FALSE, FALSE, 5);

	view->destination_btn = gtk_file_chooser_button_new("Select output directory",
                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(view->destination_btn), "/home/sullivan/Desktop");
	gtk_file_chooser_button_set_width_chars(GTK_FILE_CHOOSER_BUTTON(view->destination_btn), 30);	
	gtk_box_pack_start(GTK_BOX(path), view->destination_btn, FALSE, FALSE, 0);

	item = gtk_label_new("File:");
	gtk_box_pack_start(GTK_BOX(path), item, FALSE, FALSE, 5);

	view->run_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(view->run_entry), "run");
	gtk_entry_set_width_chars(GTK_ENTRY(view->run_entry), 7);
	gtk_box_pack_start(GTK_BOX(path), view->run_entry, FALSE, FALSE, 0);

	item = gtk_label_new("-");
	gtk_box_pack_start(GTK_BOX(path), item, FALSE, FALSE, 0);

	view->frame_entry = gtk_spin_button_new((GtkAdjustment *)gtk_adjustment_new(0, 0, 100000, 1, 10, 0), 0, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(view->frame_entry), TRUE);
	gtk_entry_set_width_chars(GTK_ENTRY(view->frame_entry), 4);
	gtk_box_pack_start(GTK_BOX(path), view->frame_entry, FALSE, FALSE, 0);

	item = gtk_label_new(".fits.gz");
	gtk_box_pack_start(GTK_BOX(path), item, FALSE, FALSE, 0);

	/* Display and view checkboxes */
	GtkWidget *box2 = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), box2, FALSE, FALSE, 0);

	view->save_checkbox = gtk_check_button_new_with_label ("Save");
	gtk_box_pack_start(GTK_BOX(box2), view->save_checkbox, TRUE, TRUE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(view->save_checkbox), FALSE);

	view->display_checkbox = gtk_check_button_new_with_label("Preview");
	gtk_box_pack_start(GTK_BOX(box2), view->display_checkbox, TRUE, TRUE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(view->display_checkbox), TRUE);
    
	return frame;
}

/* update the various information fields */
gboolean update_gui_cb(gpointer data)
{
	RangahauView *view = (RangahauView *)data;
	/* PC time */	
	char strtime[30];
	time_t t = time(NULL);
	strftime(strtime, 30, "%Y-%m-%d %H:%M:%S", gmtime(&t));
	gtk_label_set_label(GTK_LABEL(view->pctime_label), strtime);

	/* GPS time */
	char *gpstime = "Unavailable";
	RangahauGPSResponse response;
	do
	{
		rangahau_gps_send_command(view->gps, GETGPSTIME);
		response = rangahau_gps_read(view->gps, 1000);
		if (response.type == GETGPSTIME && response.error == NO_ERROR)
		{
			gpstime = (char *)response.data;
			/* Don't display ms in the time label */
			gpstime[19] = 0;
		}
	} while (response.error & UTC_ACCESS_ON_UPDATE);

	gtk_label_set_label(GTK_LABEL(view->gpstime_label), gpstime);

	/* Camera status */
	char *label;
	switch(view->camera->status)
	{
		default:		
		case INITIALISING:
			label = "Initialising";
		break;
		case IDLE:
			label = "Idle";
		break;
		case ACTIVE:
			label = "Active";
		break;
	}
	gtk_label_set_label(GTK_LABEL(view->camerastatus_label), label);

	/* Can't start/stop acquisition if the camera is initialising */
	boolean initialising = (view->camera->status == INITIALISING);
	if (initialising == gtk_widget_get_sensitive(view->startstop_btn))
		gtk_widget_set_sensitive(view->startstop_btn, !initialising);

	return TRUE;
}

void rangahau_init_gui(RangahauView *view, void (starstop_pressed_cb)(GtkWidget *, void *))
{
	/* Main window */
	view->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_size_request (GTK_WIDGET(view->window), 760, 220);
	gtk_window_set_resizable(GTK_WINDOW(view->window), FALSE);
	gtk_window_set_title(GTK_WINDOW(view->window), "Rangahau Data Acquisition System");
	gtk_container_set_border_width(GTK_CONTAINER(view->window), 5);

	g_signal_connect(view->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect_swapped(view->window, "delete-event", G_CALLBACK(gtk_widget_destroy), view->window);

	/* 
	 * Top row: Hardware & Settings
	 * Bottom row: Save path
	 */
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(view->window), vbox);

	/* top row */
	GtkWidget *topbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), topbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(topbox), rangahau_hardware_panel(view), FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(topbox), rangahau_settings_panel(view), FALSE, FALSE, 5);

	/* bottom row */
	GtkWidget *bottombox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), bottombox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(bottombox), rangahau_save_panel(view), FALSE, FALSE, 5);


	/* Start / Stop button */
	view->startstop_btn = gtk_toggle_button_new_with_label("Start Acquisition");	
	gtk_box_pack_end(GTK_BOX(bottombox), view->startstop_btn, FALSE, FALSE, 10);
	g_signal_connect(view->startstop_btn, "clicked", G_CALLBACK(starstop_pressed_cb), NULL);

	/* Can't start acquisition until the camer is ready */
	gtk_widget_set_sensitive(view->startstop_btn, FALSE);


	/* Update the gui at 10Hz */
	g_timeout_add(100, update_gui_cb, view);

	/* Display and run */
	gtk_widget_show_all(view->window);
}

