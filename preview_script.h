/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef PREVIEW_SCRIPT_H
#define PREVIEW_SCRIPT_H

#include "main.h"

typedef struct PreviewScript PreviewScript;

PreviewScript *preview_script_new();
void preview_script_free(PreviewScript *preview);
void preview_script_spawn_thread(PreviewScript *preview, const Modules *modules);
void preview_script_join_thread(PreviewScript *preview);
void preview_script_notify_shutdown(PreviewScript *preview);
bool preview_script_thread_alive(PreviewScript *preview);
void preview_script_run(PreviewScript *preview);

#endif
