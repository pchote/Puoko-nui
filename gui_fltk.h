/*
 * Copyright 2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

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

#endif
