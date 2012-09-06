/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#ifndef GUI_NCURSES_H
#define GUI_NCURSES_H

#include <stdbool.h>

// Input parsing modes
typedef enum
{
    INPUT_MAIN,
    INPUT_EXPOSURE,
    INPUT_PARAMETERS,
    INPUT_RUN_PREFIX,
    INPUT_FRAME_DIR,
    INPUT_FRAME_NUMBER,
    INPUT_OBJECT_NAME,
    INPUT_FRAME_TYPE,
    INPUT_COUNTDOWN_NUMBER
} PNUIInputType;

#endif