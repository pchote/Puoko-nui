/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef SCRIPTING_H
#define SCRIPTING_H

#include "common.h"

typedef struct ScriptingInterface ScriptingInterface;

ScriptingInterface *scripting_new();
void scripting_free(ScriptingInterface *timer);
void scripting_spawn_thread(ScriptingInterface *scripting, ThreadCreationArgs *args);
void scripting_shutdown(ScriptingInterface *timer);

void scripting_update_preview(ScriptingInterface *scripting);
void scripting_notify_frame(ScriptingInterface *scripting, const char *filepath);

#endif
