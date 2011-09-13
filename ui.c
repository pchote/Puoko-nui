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
#include <panel.h>
#include <ctype.h>

#include "ui.h"
#include "gps.h"
#include "camera.h"
#include "preferences.h"
#include "common.h"

WINDOW  *time_window, *camera_window, *acquisition_window,
        *command_window, *metadata_window, *log_window,
        *status_window, *exposure_window;

PANEL   *time_panel, *camera_panel, *acquisition_panel,
        *command_panel, *metadata_panel, *log_panel,
        *status_panel, *exposure_panel;

// A circular buffer for storing log messages
static char *log_messages[256];
static unsigned char log_position;
static unsigned char last_log_position;
static int should_quit = FALSE;

static WINDOW *create_time_window()
{
    int x = 0;
    int y = 0;
    int w = 34;
    int h = 6;
    
    WINDOW *win = newwin(h, w, y, x);
    box(win, 0, 0);
    
    char *title = " Timer Information ";
    mvwaddstr(win, 0, (w-strlen(title))/2, title);
    mvwaddstr(win, 1, 2, "   Status:");
    mvwaddstr(win, 2, 2, "  PC Time:");
    mvwaddstr(win, 3, 2, " GPS Time:");
    mvwaddstr(win, 4, 2, "Countdown:");
    
    return win;
}

static void update_time_window(PNGPS *gps)
{
	/* PC time */	
	char strtime[30];
	time_t t = time(NULL);
	strftime(strtime, 30, "%Y-%m-%d %H:%M:%S", gmtime(&t));
	mvwaddstr(time_window, 2, 13, strtime);

	/* GPS time */
    pthread_mutex_lock(&gps->read_mutex);
    PNGPSTimestamp ts = gps->current_timestamp;
    pthread_mutex_unlock(&gps->read_mutex);
	
    mvwaddstr(time_window, 1, 13, (ts.locked ? "Locked  " : "Unlocked"));

	if (ts.valid)
	{
        mvwprintw(time_window, 3, 13, "%04d-%02d-%02d %02d:%02d:%02d", ts.year, ts.month, ts.day, ts.hours, ts.minutes, ts.seconds);

        if (ts.remaining_exposure > 0)
            mvwprintw(time_window, 4, 13, "%03d        ", ts.remaining_exposure);
        else
            mvwaddstr(time_window, 4, 13, "Disabled    ");
	}
    else
    {
        mvwaddstr(time_window, 3, 13, "Unavailable");
        mvwaddstr(time_window, 4, 13, "Unavailable");
    }
}

static WINDOW *create_camera_window()
{
    int x = 0;
    int y = 6;
    int w = 34;
    int h = 4;
    
    WINDOW *win = newwin(h, w, y, x);
    box(win, 0, 0);
    
    char *title = " Camera Information ";
    mvwaddstr(win, 0, (w-strlen(title))/2, title);
    mvwaddstr(win, 1, 2, "     Status:");
    mvwaddstr(win, 2, 2, "Temperature:");
    
    return win;
}

static void update_camera_window(PNCameraMode mode, int camera_downloading, float temp)
{
	/* Camera status */
	char *label;
	switch(mode)
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
            if (camera_downloading)
                label = "Downloading ";
            else
                label = "Acquiring   ";
        break;
        case DOWNLOADING:
            label = "Downloading ";
        break;
		case SHUTDOWN:
			label = "Closing     ";
		break;
	}
	mvwaddstr(camera_window, 1, 15, label);

	/* Camera temperature */
	char *tempstring = "Unavailable";
    char tempbuf[30];
    
	if (mode == ACQUIRING || mode == IDLE)
	{
	    sprintf(tempbuf, "%0.02f C     ", temp);
        tempstring = tempbuf;
	}
	mvwaddstr(camera_window, 2, 15, tempstring);
}

static WINDOW *create_acquisition_window()
{
    int x = 0;
    int y = 16;
    int w = 34;
    int h = 6;
    
    WINDOW *win = newwin(h, w, y, x);
    box(win, 0, 0);
    
    char *title = " Acquisition ";
    mvwaddstr(win, 0, (w-strlen(title))/2, title);
    mvwaddstr(win, 1, 2, " Exposure:");
    mvwaddstr(win, 2, 2, "     Type:");
    mvwaddstr(win, 3, 2, "Remaining:");
    mvwaddstr(win, 4, 2, " Filename:");
    
    return win;
}

static void update_acquisition_window()
{
    PNFrameType type = pn_preference_char(OBJECT_TYPE);
    unsigned char exptime = pn_preference_char(EXPOSURE_TIME);
    int remaining_frames = pn_preference_int(CALIBRATION_REMAINING_FRAMECOUNT);
    char *run_prefix = pn_preference_string(RUN_PREFIX);
    int run_number = pn_preference_int(RUN_NUMBER);

    char *typename;
    switch (type)
    {
        case OBJECT_DARK:
            typename = "Dark  ";
            break;
        case OBJECT_FLAT:
            typename = "Flat  ";
            break;
        case OBJECT_TARGET:
            typename = "Object";
            break;
    }

	mvwprintw(acquisition_window, 1, 13, "%d seconds   ", exptime);
	mvwaddstr(acquisition_window, 2, 13, typename);

    if (type == OBJECT_TARGET)
        mvwaddstr(acquisition_window, 3, 13, "N/A        ");
    else
        mvwprintw(acquisition_window, 3, 13, "%d   ", remaining_frames);

    mvwaddstr(acquisition_window, 4, 13, "                    ");
    mvwprintw(acquisition_window, 4, 13, "%s-%04d.fits.gz", run_prefix, run_number);
    free(run_prefix);
}

static WINDOW *create_metadata_window()
{
    int x = 0;
    int y = 10;
    int w = 34;
    int h = 6;
    
    WINDOW *win = newwin(h, w, y, x);
    box(win, 0, 0);
    
    char *title = " Metadata ";
    mvwaddstr(win, 0, (w-strlen(title))/2, title);
    mvwaddstr(win, 1, 2, "Observatory:");
    mvwaddstr(win, 2, 2, "  Observers:");
    mvwaddstr(win, 3, 2, "  Telescope:");
    mvwaddstr(win, 4, 2, "     Object:");

    return win;
}

static void update_metadata_window()
{
    char *observatory = pn_preference_string(OBSERVATORY);
	mvwaddstr(metadata_window, 1, 15, observatory);
    free(observatory);

    char *observers = pn_preference_string(OBSERVERS);
    mvwaddstr(metadata_window, 2, 15, observers);
    free(observers);

    char *telescope = pn_preference_string(TELESCOPE);
	mvwaddstr(metadata_window, 3, 15, telescope);
    free(telescope);

    char *object = pn_preference_string(OBJECT_NAME);
	mvwaddstr(metadata_window, 4, 15, object);
    free(object);
}

static WINDOW *create_log_window()
{
    int row,col;
    getmaxyx(stdscr,row,col);

    int x = 35;
    int y = 0;
    int w = col - 34;
    int h = row - 4;
    return newwin(h, w, y, x);
}

static void update_log_window()
{
    int height, width;
    getmaxyx(log_window, height, width);

    // First erase the screen
    wclear(log_window);

    // Redraw messages
    for (int i = 0; i < height && i < 256; i++)
    {
        unsigned char j = (unsigned char)(log_position - height + i + 1);
        if (log_messages[j] != NULL)
            mvwaddstr(log_window, i, 0, log_messages[j]);
    }
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


static WINDOW *create_command_window()
{
    int row, col;
    getmaxyx(stdscr, row, col);

    int x = 0;
    int y = row-2;
    int w = col;
    int h = 2;
    return newwin(h, w, y, x);
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

static void update_command_window(PNCameraMode camera_mode)
{
    int row,col;
    getmaxyx(stdscr,row,col);
    wclear(command_window);
    mvwhline(command_window, 0, 0, 0, col);

    if (should_quit)
        return;

    PNFrameType type = pn_preference_char(OBJECT_TYPE);
    unsigned char save = pn_preference_char(SAVE_FRAMES);
    int remaining_frames = pn_preference_int(CALIBRATION_REMAINING_FRAMECOUNT);

    // Acquire toggle
    wmove(command_window, 1, 0);
    if (camera_mode == IDLE || camera_mode == ACQUIRING)
        print_command_option(command_window, FALSE, "^A", "Acquire", camera_mode == ACQUIRING ? "Stop " : "");

    // Save toggle
    if (type == OBJECT_TARGET || remaining_frames > 0)
        print_command_option(command_window, TRUE, "^S", "%s Saving", save ? "Stop" : "Start");

    // Display parameter panel
    if (!save)
        print_command_option(command_window, TRUE, "^P", "Edit Parameters");

    // Display exposure panel
    if (camera_mode == IDLE)
        print_command_option(command_window, TRUE, "^E", "Set Exposure");

    print_command_option(command_window, TRUE, "^C", "Quit");
}


static WINDOW *create_status_window()
{
    int row, col;
    getmaxyx(stdscr, row, col);

    int x = 35;
    int y = row-3;
    int w = col-35;
    int h = 1;

    WINDOW *win = newwin(h, w, y, x);
    mvwaddstr(win, 0, 5, "Acquiring:");
    mvwaddstr(win, 0, 27, "Saving:");
    return win;
}

static void update_status_window(PNCameraMode camera_mode)
{
    unsigned char save = pn_preference_char(SAVE_FRAMES);
    wattron(status_window, A_STANDOUT);
    mvwaddstr(status_window, 0, 16, (camera_mode == ACQUIRING ? " YES " : " NO "));
    wattroff(status_window, A_STANDOUT);
    waddstr(status_window, " ");
    wattron(status_window, A_STANDOUT);
    mvwaddstr(status_window, 0, 35, (save ? " YES " : " NO "));
    wattroff(status_window, A_STANDOUT);
    waddstr(status_window, " ");
}

static WINDOW *create_exposure_window()
{
    int row, col;
    getmaxyx(stdscr, row, col);

    int x = 0;
    int y = row-2;
    int w = col;
    int h = 2;
    WINDOW *win = newwin(h, w, y, x);
    mvwhline(win, 0, 0, 0, col);
    mvwaddstr(win, 1, 1, "Enter an exposure time: ");

    return win;
}

char exp_entry_buf[1024];
int exp_entry_length = 0;
static void update_exposure_window()
{
    mvwaddnstr(exposure_window, 1, 25, exp_entry_buf, exp_entry_length);
    wclrtoeol(exposure_window);
}

PNCameraMode last_camera_mode;
float last_camera_temperature;
int last_calibration_framecount;
int last_run_number;
int last_camera_downloading;
PNUIInputType input_type = INPUT_MAIN;
void pn_ui_run(PNGPS *gps, PNCamera *camera)
{
    initscr();
    noecho();
    raw();

    // Create windows
    time_window = create_time_window();
    camera_window = create_camera_window();
    acquisition_window = create_acquisition_window();
    command_window = create_command_window();
    metadata_window = create_metadata_window();
    log_window = create_log_window();
    status_window = create_status_window();
    exposure_window = create_exposure_window();

    // Create panels
    time_panel = new_panel(time_window);
    camera_panel = new_panel(camera_window);
    acquisition_panel = new_panel(acquisition_window);
    metadata_panel = new_panel(metadata_window);
    log_panel = new_panel(log_window);
    status_panel = new_panel(status_window);
    command_panel = new_panel(command_window);
    exposure_panel = new_panel(exposure_window);

    // Set initial state
    last_camera_mode = camera->mode;
    last_camera_temperature = camera->temperature;

    update_log_window();
    update_status_window(last_camera_mode);
    update_command_window(last_camera_mode);
    update_metadata_window();

    pthread_mutex_lock(&gps->read_mutex);
    last_camera_downloading = gps->camera_downloading;
    pthread_mutex_unlock(&gps->read_mutex);

    update_acquisition_window();
    update_time_window(gps);
    update_camera_window(last_camera_mode, last_camera_downloading, last_camera_temperature);
    hide_panel(exposure_panel);

    // Only wait for 100ms for input so we can keep the ui up to date
    // *and* respond timely to input
    timeout(100);
    for (;;)
    {
        if (camera->fatal_error != NULL)
        {
            pn_log("Fatal camera error: %s", camera->fatal_error);
            timeout(-1);
            getch();
            break;
        }

        // Read once at the start of the loop so values remain consistent
        pthread_mutex_lock(&camera->read_mutex);
        PNCameraMode camera_mode = camera->mode;
        float camera_temperature = camera->temperature;
        pthread_mutex_unlock(&camera->read_mutex);

        pthread_mutex_lock(&gps->read_mutex);
        int camera_downloading = gps->camera_downloading;
        pthread_mutex_unlock(&gps->read_mutex);

        int ch;
        while ((ch = getch()) != ERR)
        {
            char buf[128];
            switch (input_type)
            {
                case INPUT_MAIN:
                    switch (ch)
                    {
                        case 0x01: // ^A - Toggle Acquire
                            if (camera_mode == IDLE)
                            {
                                pthread_mutex_lock(&camera->read_mutex);
                                camera->desired_mode = ACQUIRING;
                                pthread_mutex_unlock(&camera->read_mutex);

                                unsigned char exptime = pn_preference_char(EXPOSURE_TIME);
                                pn_gps_start_exposure(gps, exptime);
                            }
                            else if (camera_mode == ACQUIRING)
                            {
                                pthread_mutex_lock(&camera->read_mutex);
                                camera->desired_mode = ACQUIRE_WAIT;
                                pthread_mutex_unlock(&camera->read_mutex);

                                pn_gps_stop_exposure(gps);
                            }
                            break;
                        case 0x05: // ^E - Set Exposure
                            if (camera_mode != IDLE)
                                break;

                            input_type = INPUT_EXPOSURE;

                            exp_entry_length = sprintf(exp_entry_buf, "%d", pn_preference_char(EXPOSURE_TIME));
                            update_exposure_window();

                            hide_panel(command_panel);
                            show_panel(exposure_panel);
                        break;
                        case 0x13: // ^S - Toggle Save
                            // Can't enable saving for calibration frames after the target count has been reached
                            if (!pn_preference_allow_save())
                            {
                                add_log_line("fail");
                                break;
                            }

                            unsigned char save = pn_preference_toggle_save();
                            update_status_window(camera_mode);
                            update_command_window(camera_mode);
                            pn_log("%s saving", save ? "Enabled" : "Disabled");
                        break;
                        case 0x03: // ^C - Quit
                            should_quit = TRUE;
                            pn_log("Shutting down...");
                            update_command_window(camera_mode);
                        break;
                        default:
                            sprintf(buf, "Pressed %c 0x%02x", ch, ch);
                            add_log_line(buf);
                        break;
                    }
                break;
                case INPUT_EXPOSURE:
                    if (ch == '\n')
                    {
                        exp_entry_buf[exp_entry_length] = '\0';
                        int newexp = atoi(exp_entry_buf);
                        unsigned char oldexp = pn_preference_char(EXPOSURE_TIME);
                        if (newexp < 3 || newexp > 255)
                        {
                            // Invalid entry
                            exp_entry_length = sprintf(exp_entry_buf, "%d", oldexp);
                        }
                        else
                        {
                            input_type = INPUT_MAIN;
                            hide_panel(exposure_panel);
                            show_panel(command_panel);

                            if (oldexp != newexp)
                            {
                                // Update preferences
                                pn_preference_set_char(EXPOSURE_TIME, newexp);
                                update_acquisition_window();
                                pn_log("Exposure set to %d seconds", newexp);
                            }
                        }
                    }
                    else if (ch == 0x7f && exp_entry_length > 0) // Backspace
                        --exp_entry_length;
                    else if (isdigit(ch) && exp_entry_length < 1024 - 1)
                        exp_entry_buf[exp_entry_length++] = ch;

                    update_exposure_window();
                break;
            }
        }
        update_time_window(gps);

        if (log_position != last_log_position)
        {
            update_log_window();
            last_log_position = log_position;
        }

        if (last_camera_mode != camera_mode ||
            last_camera_downloading != camera_downloading)
        {
            update_command_window(camera_mode);
            update_status_window(camera_mode);
            update_camera_window(camera_mode, camera_downloading, camera_temperature);
            last_camera_mode = camera_mode;
            last_camera_downloading = camera_downloading;
        }

        if (last_camera_temperature != camera_temperature)
        {
            update_camera_window(camera_mode, camera_downloading, camera_temperature);
            last_camera_temperature = camera_temperature;
        }

        int remaining_frames = pn_preference_int(CALIBRATION_REMAINING_FRAMECOUNT);
        int run_number = pn_preference_int(RUN_NUMBER);
        if (remaining_frames != last_calibration_framecount ||
            run_number != last_run_number)
        {
            update_acquisition_window();
            last_calibration_framecount = remaining_frames;
            last_run_number = run_number;
        }

        update_panels();
        doupdate();

        if (input_type == INPUT_MAIN)
        {
            // curs_set(0) doesn't work properly when running via ssh from osx
            // so put the cursor in the corner where it can't cause trouble
            int row,col;
            getmaxyx(stdscr, row, col);
            move(row-1, col-1);
        }

        if (should_quit)
            break;
    }

    // Destroy ui
    del_panel(time_panel);
    del_panel(camera_panel);
    del_panel(acquisition_panel);
    del_panel(metadata_panel);
    del_panel(log_panel);
    del_panel(command_panel);
    del_panel(exposure_panel);

    delwin(time_window);
    delwin(camera_window);
    delwin(acquisition_window);
    delwin(metadata_window);
    delwin(log_window);
    delwin(command_window);
    delwin(exposure_window);

    for (int i = 0; i < 256; i++)
        free(log_messages[i]);

    endwin();
}
