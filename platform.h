/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

// Platform-specific shims so we can expect the same functions everywhere
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdarg.h>
#include <stdbool.h>
#include <time.h>

time_t struct_tm_to_time_t(struct tm *t);
void normalize_tm(struct tm *t);
void millisleep(int ms);
char *canonicalize_path(const char *path);
bool rename_atomically(const char *src, const char *dest, bool overwrite);
bool delete_file(const char *path);
char *last_path_component(char *path);
int run_command(const char *cmd, char *log_prefix);

#endif