/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <ncurses.h>
#include <string.h>
#include <unistd.h>


#include "ui.h"
#include "gps.h"
#include "camera.h"
#include "preferences.h"

WINDOW *time_panel;
WINDOW *camera_panel;
WINDOW *acquisition_panel;
WINDOW *command_panel;
WINDOW *metadata_panel;

static WINDOW *create_time_panel()
{
    int x = 0;
    int y = 0;
    int w = 33;
    int h = 6;
    
    WINDOW *win = newwin(h,w,y,x);
    box(win, 0, 0);
    
    char *title = " Timer Information ";
    mvwprintw(win, 0, (w-strlen(title))/2, title);
    mvwprintw(win, 1, 2, "  Status:");
    mvwprintw(win, 2, 2, " PC Time:");    
    mvwprintw(win, 3, 2, "GPS Time:");
    mvwprintw(win, 4, 2, "Exposure:");
    
    return win;
}

static void update_time_panel(PNGPS *gps)
{
	/* PC time */	
	char strtime[30];
	time_t t = time(NULL);
	strftime(strtime, 30, "%Y-%m-%d %H:%M:%S", gmtime(&t));
	mvwprintw(time_panel, 2, 12, "%s", strtime);

	/* GPS time */
	char *gpsstring = "Unavailable";
	char gpsbuf[30];
	
    char *expstring = "Unavailable";
    char expbuf[30];

    pthread_mutex_lock(&gps->currenttime_mutex);
    PNGPSTimestamp ts = gps->current_timestamp;
    pthread_mutex_unlock(&gps->currenttime_mutex);
	
	if (ts.valid)
	{
		sprintf(gpsbuf, "%04d-%02d-%02d %02d:%02d:%02d", ts.year, ts.month, ts.day, ts.hours, ts.minutes, ts.seconds);
		gpsstring = gpsbuf;
        printf(expbuf, "%03d        ", ts.remaining_exposure);
        expstring = expbuf;
	}
	
	mvwprintw(time_panel, 3, 12, gpsstring);
    mvwprintw(time_panel, 4, 12, expstring);
    mvwprintw(time_panel, 1, 12, ts.locked ? "Locked  " : "Unlocked");
	
    wrefresh(time_panel);
}

static WINDOW *create_camera_panel()
{
    int x = 0;
    int y = 6;
    int w = 33;
    int h = 4;
    
    WINDOW *win = newwin(h,w,y,x);
    box(win, 0, 0);
    
    char *title = " Camera Information ";
    mvwprintw(win, 0, (w-strlen(title))/2, title);
    mvwprintw(win, 1, 2, "     Status:");
    mvwprintw(win, 2, 2, "Temperature:");
    
    return win;
}

static void update_camera_panel(PNCamera *camera)
{
	/* Camera status */
	char *label;
	switch(camera->mode)
	{
		default:		
		case INITIALISING:
		case ACQUIRE_START:
		case ACQUIRE_STOP:
			label = "Initialising";
		break;
		case IDLE:
			label = "Idle        ";
		break;
		case ACQUIRING:
			label = "Acquiring   ";
        break;
        case DOWNLOADING:
            label = "Downloading ";
        break;
		case SHUTDOWN:
			label = "Closing     ";
		break;
	}
	mvwprintw(camera_panel, 1, 15, label);

	/* Camera temperature */
	char *tempstring = "Unavailable";
    char tempbuf[30];
    
	if (camera->mode == ACQUIRING || camera->mode == IDLE)
	{
	    sprintf(tempbuf, "%0.02f °C    ",(float)camera->temperature/100);
        tempstring = tempbuf;
	}
	mvwprintw(camera_panel, 2, 15, "%s", tempstring);

	wrefresh(camera_panel);
}

static WINDOW *create_acquisition_panel()
{
    int x = 0;
    int y = 16;
    int w = 33;
    int h = 6;
    
    WINDOW *win = newwin(h,w,y,x);
    box(win, 0, 0);
    
    char *title = " Acquisition ";
    mvwprintw(win, 0, (w-strlen(title))/2, title);
    mvwprintw(win, 1, 2, "Exposure:");
    mvwprintw(win, 2, 2, "    Type:");
    mvwprintw(win, 3, 2, " Preview:");
    mvwprintw(win, 4, 2, "    Save:");
    
    return win;
}

static void update_acquisition_panel(PNPreferences *prefs)
{
    int saving = 0;
    int preview = 1;
    
    char *type;
    switch (prefs->object_type)
    {
        case OBJECT_DARK:
            type = "Dark";
            break;
        case OBJECT_FLAT:
            type = "Flat";
            break;
        case OBJECT_TARGET:
            type = "Object";
            break;
    }
	mvwprintw(acquisition_panel, 1, 12, "%d seconds     ", prefs->exposure_time);
	mvwprintw(acquisition_panel, 2, 12, type);
	mvwprintw(acquisition_panel, 3, 12, preview ? "On " : "Off");
	mvwprintw(acquisition_panel, 4, 12, saving ? "On " : "Off");
    
	wrefresh(acquisition_panel);
}

static WINDOW *create_metadata_panel()
{
    int x = 0;
    int y = 10;
    int w = 33;
    int h = 6;
    
    WINDOW *win = newwin(h,w,y,x);
    box(win, 0, 0);
    
    char *title = " Metadata ";
    mvwprintw(win, 0, (w-strlen(title))/2, title);
    mvwprintw(win, 1, 2, "Observatory:");
    mvwprintw(win, 2, 2, "  Observers:");
    mvwprintw(win, 3, 2, "  Telescope:");
    mvwprintw(win, 4, 2, "     Object:");
    wrefresh(win);

    return win;
}

static void update_metadata_panel(PNPreferences *prefs)
{
	mvwprintw(metadata_panel, 1, 15, prefs->observatory);
	mvwprintw(metadata_panel, 2, 15, prefs->observers);
	mvwprintw(metadata_panel, 3, 15, prefs->telescope);
    
    char *object = prefs->object_name;
    switch (prefs->object_type)
    {
        case OBJECT_DARK:
            object = "Dark";
        break;
        case OBJECT_FLAT:
            object = "Flat";
        break;
    }
    
	mvwprintw(metadata_panel, 4, 15, object);
    
	wrefresh(metadata_panel);
}

static WINDOW *create_command_panel()
{
    int row,col;
    getmaxyx(stdscr,row,col);
    
    int x = 0;
    int y = row-2;
    int w = col;
    int h = 2;
    
    WINDOW *win = newwin(h,w,y,x);
    mvwhline(win, 0, 0, 0, col);
    mvwprintw(win, 1, 1, "F1 Do Something");
    wrefresh(win);

    return win;
}

void pn_ui_run(PNGPS *gps, PNCamera *camera, PNPreferences *prefs)
{
    initscr();
    noecho();

    time_panel = create_time_panel();
    camera_panel = create_camera_panel();
    acquisition_panel = create_acquisition_panel();
    command_panel = create_command_panel();
    metadata_panel = create_metadata_panel();
    for (;;)
    {
        update_time_panel(gps);
        update_camera_panel(camera);
        update_metadata_panel(prefs);
        update_acquisition_panel(prefs);
        
        sleep(1);
    }
}


void pn_ui_shutdown()
{
    delwin(time_panel);
    delwin(camera_panel);
    endwin();
}