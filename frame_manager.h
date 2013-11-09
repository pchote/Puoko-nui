/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef FRAME_MANAGER_H
#define FRAME_MANAGER_H

#include "main.h"

typedef struct FrameManager FrameManager;

FrameManager *frame_manager_new();
void frame_manager_free(FrameManager *frame);
void frame_manager_spawn_thread(FrameManager *frame, Modules *modules);
void frame_manager_join_thread(FrameManager *frame);
void frame_manager_notify_shutdown(FrameManager *frame);
bool frame_manager_thread_alive(FrameManager *frame);
void frame_manager_run(FrameManager *frame);

void frame_manager_purge_queues(FrameManager *frame, bool reset_first_frame);
void frame_manager_queue_frame(FrameManager *frame, CameraFrame *f);
void frame_manager_queue_trigger(FrameManager *frame, TimerTimestamp *t);

#endif
