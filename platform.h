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
#include <time.h>

void normalize_tm(struct tm *t);
void millisleep(int ms);

#if (defined _WIN32 || defined _WIN64)
char *realpath(const char *path, char resolved_path[]);
int vasprintf (char **resultp, const char *format, va_list args);
int asprintf(char **resultp, const char *format, ...);
char *strndup(const char *s, size_t max);
#endif
#endif