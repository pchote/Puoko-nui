/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ncurses.h>

#include "ui.h"
#include "gps.h"
#include "camera.h"
#include "preferences.h"

WINDOW *time_panel;
WINDOW *camera_panel;
WINDOW *acquisition_panel;
WINDOW *command_panel;
WINDOW *metadata_panel;
WINDOW *log_panel;

// A circular buffer for storing log messages
static char *log_messages[256];
static unsigned char log_position;

static int exposing = FALSE;
static int saving = FALSE;
static int should_quit = FALSE;

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
    int h = 5;
    
    WINDOW *win = newwin(h,w,y,x);
    box(win, 0, 0);
    
    char *title = " Acquisition ";
    mvwprintw(win, 0, (w-strlen(title))/2, title);
    mvwprintw(win, 1, 2, "Exposure:");
    mvwprintw(win, 2, 2, "    Type:");
    mvwprintw(win, 3, 2, "    Save:");
    
    return win;
}

static void update_acquisition_panel(PNPreferences *prefs)
{
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
	mvwprintw(acquisition_panel, 3, 12, saving ? "On " : "Off");
    
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

static WINDOW *create_log_panel()
{
    int row,col;
    getmaxyx(stdscr,row,col);

    int x = 34;
    int y = 0;
    int w = col - 34;
    int h = row - 3;
    return newwin(h,w,y,x);
}

static void update_log_panel()
{
    int height, width;
    getmaxyx(log_panel, height, width);

    // First erase the screen
    wclear(log_panel);

    // Redraw messages
    for (int i = 0; i < height && i < 256; i++)
    {
        unsigned char j = (unsigned char)(log_position - i);
        if (log_messages[j] != NULL)
            mvwprintw(log_panel, height - i - 1, 0, log_messages[j]);
    }

    wrefresh(log_panel);
}

void init_log_gui()
{
    log_position = 0;
    for (int i = 0; i < 256; i++)
        log_messages[i] = NULL;
}

void add_log_line(char *msg)
{
    log_position++;
    if (log_messages[log_position] != NULL)
        free(log_messages[log_position]);

    log_messages[log_position] = strdup(msg);
}


static WINDOW *create_command_panel()
{
    int row, col;
    getmaxyx(stdscr, row, col);

    int x = 0;
    int y = row-2;
    int w = col;
    int h = 2;
    return newwin(h,w,y,x);
}

static void print_command_option(WINDOW *w, int indent, char *hotkey, char *format, ...)
{
    if (indent)
        waddstr(w, "  ");

    wattron(w, A_STANDOUT);
    waddstr(w, hotkey);
    wattroff(w, A_STANDOUT);
    waddstr(w, " ");

    va_list args;
	va_start(args, format);
    vwprintw(w, format, args);
	va_end(args);
}

static void update_command_panel()
{
    int row,col;
    getmaxyx(stdscr,row,col);
    wclear(command_panel);
    mvwhline(command_panel, 0, 0, 0, col);

    // Acquire toggle
    wmove(command_panel, 1, 0);
    print_command_option(command_panel, FALSE, "^A", "Acquire", exposing ? "Stop " : "");

    // Save toggle
    print_command_option(command_panel, TRUE, "^S", "%s Saving", saving ? "Stop" : "Start");

    // Display parameter panel
    if (!saving)
        print_command_option(command_panel, TRUE, "^P", "Edit Parameters");

    // Display exposure panel
    if (!exposing)
        print_command_option(command_panel, TRUE, "^E", "Set Exposure");

    print_command_option(command_panel, TRUE, "^C", "Quit");
    wrefresh(command_panel);
}

void quit_handler()
{
    should_quit = TRUE;
}

void pn_ui_run(PNGPS *gps, PNCamera *camera, PNPreferences *prefs)
{
    signal(SIGINT, quit_handler);
    initscr();
    noecho();

    time_panel = create_time_panel();
    camera_panel = create_camera_panel();
    acquisition_panel = create_acquisition_panel();
    command_panel = create_command_panel();
    metadata_panel = create_metadata_panel();
    log_panel = create_log_panel();

    update_log_panel();
    update_command_panel();
    for (;;)
    {
        if (should_quit)
            break;

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
    delwin(acquisition_panel);
    delwin(command_panel);
    delwin(metadata_panel);
    delwin(log_panel);

    for (int i = 0; i < 256; i++)
        free(log_messages[i]);

    endwin();
}