/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef IMAGEHANDLER_H
#define IMAGEHANDLER_H

#include <stdint.h>
#include "timer.h"
#include "camera.h"

void pn_run_startup_script();
void pn_run_preview_script(const char *filepath);
void pn_run_saved_script(const char *filepath);

char *pn_save_frame(PNFrame *frame, TimerTimestamp timestamp, PNCamera *camera);
void pn_save_preview(PNFrame *frame, TimerTimestamp timestamp);

#endif
