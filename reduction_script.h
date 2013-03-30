/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef REDUCTION_SCRIPT_H
#define REDUCTION_SCRIPT_H

#include "main.h"

typedef struct ReductionScript ReductionScript;

ReductionScript *reduction_script_new();
void reduction_script_free(ReductionScript *reduction);
void reduction_script_spawn_thread(ReductionScript *reduction, ThreadCreationArgs *args);
void reduction_script_join_thread(ReductionScript *reduction);
void reduction_script_notify_shutdown(ReductionScript *reduction);
bool reduction_script_thread_alive(ReductionScript *reduction);
void reduction_push_frame(ReductionScript *reduction, const char *filepath);

#endif
