/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#ifndef GUI_H
#define GUI_H

void pn_ui_log_line(char *message);
void pn_ui_new();
bool pn_ui_update();
void pn_ui_free();
void pn_ui_show_fatal_error(char *message);

#endif