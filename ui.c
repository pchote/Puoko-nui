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
static unsigned char last_log_position;

static int exposing = FALSE;
static int saving = FALSE;
static int should_quit = FALSE;

static WINDOW *create_time_panel()
{
    int x = 0;
    int y = 0;
    int w = 33;
    int h = 6;
    
    WINDOW *win = subwin(stdscr, h, w, y, x);
    box(win, 0, 0);
    
    char *title = " Timer Information ";
    mvwaddstr(win, 0, (w-strlen(title))/2, title);
    mvwaddstr(win, 1, 2, "  Status:");
    mvwaddstr(win, 2, 2, " PC Time:");
    mvwaddstr(win, 3, 2, "GPS Time:");
    mvwaddstr(win, 4, 2, "Exposure:");
    
    return win;
}

static void update_time_panel(PNGPS *gps)
{
	/* PC time */	
	char strtime[30];
	time_t t = time(NULL);
	strftime(strtime, 30, "%Y-%m-%d %H:%M:%S", gmtime(&t));
	mvwaddstr(time_panel, 2, 12, strtime);

	/* GPS time */
    pthread_mutex_lock(&gps->currenttime_mutex);
    PNGPSTimestamp ts = gps->current_timestamp;
    pthread_mutex_unlock(&gps->currenttime_mutex);
	
    mvwaddstr(time_panel, 1, 12, (ts.locked ? "Locked  " : "Unlocked"));

	if (ts.valid)
	{
        mvwprintw(time_panel, 3, 12, "%04d-%02d-%02d %02d:%02d:%02d", ts.year, ts.month, ts.day, ts.hours, ts.minutes, ts.seconds);
        mvwprintw(time_panel, 4, 12, "%03d        ", ts.remaining_exposure);
	}
    else
    {
        mvwaddstr(time_panel, 3, 12, "Unavailable");
        mvwaddstr(time_panel, 4, 12, "Unavailable");
    }

    wrefresh(time_panel);
}

static WINDOW *create_camera_panel()
{
    int x = 0;
    int y = 6;
    int w = 33;
    int h = 4;
    
    WINDOW *win = subwin(stdscr, h, w, y, x);
    box(win, 0, 0);
    
    char *title = " Camera Information ";
    mvwaddstr(win, 0, (w-strlen(title))/2, title);
    mvwaddstr(win, 1, 2, "     Status:");
    mvwaddstr(win, 2, 2, "Temperature:");
    
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
	mvwaddstr(camera_panel, 1, 15, label);

	/* Camera temperature */
	char *tempstring = "Unavailable";
    char tempbuf[30];
    
	if (camera->mode == ACQUIRING || camera->mode == IDLE)
	{
	    sprintf(tempbuf, "%0.02f °C    ",(float)camera->temperature/100);
        tempstring = tempbuf;
	}
	mvwaddstr(camera_panel, 2, 15, tempstring);

	wrefresh(camera_panel);
}

static WINDOW *create_acquisition_panel()
{
    int x = 0;
    int y = 16;
    int w = 33;
    int h = 5;
    
    WINDOW *win = subwin(stdscr, h, w, y, x);
    box(win, 0, 0);
    
    char *title = " Acquisition ";
    mvwaddstr(win, 0, (w-strlen(title))/2, title);
    mvwaddstr(win, 1, 2, "Exposure:");
    mvwaddstr(win, 2, 2, "    Type:");
    mvwaddstr(win, 3, 2, "    Save:");
    
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
	mvwaddstr(acquisition_panel, 2, 12, type);
	mvwaddstr(acquisition_panel, 3, 12, (saving ? "On " : "Off"));
    
	wrefresh(acquisition_panel);
}

static WINDOW *create_metadata_panel()
{
    int x = 0;
    int y = 10;
    int w = 33;
    int h = 6;
    
    WINDOW *win = subwin(stdscr, h, w, y, x);
    box(win, 0, 0);
    
    char *title = " Metadata ";
    mvwaddstr(win, 0, (w-strlen(title))/2, title);
    mvwaddstr(win, 1, 2, "Observatory:");
    mvwaddstr(win, 2, 2, "  Observers:");
    mvwaddstr(win, 3, 2, "  Telescope:");
    mvwaddstr(win, 4, 2, "     Object:");
    wrefresh(win);

    return win;
}

static void update_metadata_panel(PNPreferences *prefs)
{
	mvwaddstr(metadata_panel, 1, 15, prefs->observatory);
	mvwaddstr(metadata_panel, 2, 15, prefs->observers);
	mvwaddstr(metadata_panel, 3, 15, prefs->telescope);
    
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
    
	mvwaddstr(metadata_panel, 4, 15, object);
    
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
    return subwin(stdscr, h, w, y, x);
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
        unsigned char j = (unsigned char)(log_position - height + i + 1);
        if (log_messages[j] != NULL)
            mvwaddstr(log_panel, i, 0, log_messages[j]);
    }

    wrefresh(log_panel);
}

void init_log_gui()
{
    last_log_position = log_position = 0;
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
    return subwin(stdscr, h, w, y, x);
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

void pn_ui_run(PNGPS *gps, PNCamera *camera, PNPreferences *prefs)
{
    initscr();
    noecho();
    raw();

    time_panel = create_time_panel();
    camera_panel = create_camera_panel();
    acquisition_panel = create_acquisition_panel();
    command_panel = create_command_panel();
    metadata_panel = create_metadata_panel();
    log_panel = create_log_panel();

    update_log_panel();
    update_command_panel();
    update_metadata_panel(prefs);

    update_acquisition_panel(prefs);
    update_time_panel(gps);
    update_camera_panel(camera);

    // Only wait for 100ms for input so we can keep the ui up to date
    // *and* respond timely to input
    timeout(100);

    for (;;)
    {
        int ch;
        while ((ch = getch()) != ERR)
        {
            char buf[128];

            switch (ch)
            {
                case 0x03: // ^C
                    should_quit = TRUE;
                break;
                default:
                    sprintf(buf, "Pressed %c 0x%02x", ch, ch);
                    add_log_line(buf);
                break;
            }
        }

        update_time_panel(gps);
        update_camera_panel(camera);
        update_acquisition_panel(prefs);

        if (log_position != last_log_position)
        {
            update_log_panel();
            last_log_position = log_position;
        }

        // curs_set(0) doesn't work properly when running via ssh from osx
        // so put the cursor in the corner where it can't cause trouble
        int row,col;
        getmaxyx(stdscr, row, col);
        move(row-1, col-1);

        if (should_quit)
            break;
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
