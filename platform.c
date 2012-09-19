/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

// Platform-specific shims so we can expect the same functions everywhere

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/stat.h>
#include "common.h"

#if (defined _WIN32 || defined _WIN64)
    #include <windows.h>
#endif

#include "platform.h"

void normalize_tm(struct tm *t)
{
#ifdef _WIN32
    time_t b = mktime(t);
    *t = *localtime(&b);
#elif defined _WIN64
    time_t b = _mkgmtime(t);
    gmtime_s(&b, t);
#else
    time_t b = timegm(t);
    gmtime_r(&b, t);
#endif
}

// Sleep for ms milliseconds
void millisleep(int ms)
{
    #if (defined _WIN32 || defined _WIN64)
        Sleep(ms);
    #else
        nanosleep(&(struct timespec){0, ms*1e6}, NULL);
    #endif
}

#if (defined _WIN32 || defined _WIN64)
int vasprintf(char **bufptr, const char *fmt, va_list args)
{
    // Get length
    int len = vsnprintf(NULL, 0, fmt, args);
    if (len < 0 || (*bufptr = malloc(len + 1)) == NULL)
        return -1;

    return vsprintf(*bufptr, fmt, args);
}

int asprintf(char **bufptr, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vasprintf(bufptr, fmt, args);
    va_end(args);
    return ret;
}

char *strndup(const char *s, size_t max)
{
#ifdef _WIN64
    size_t len = strnlen(s, max);
#else
    size_t len = strlen(s);
    if (len > max)
        len = max;
#endif
    char *ret = malloc(len + 1);
    if (ret == NULL)
        return NULL;

    memcpy(ret, s, len);
    ret[len] = '\0';
    return ret;
}

#endif

// Cross platform equivalent of realpath()
char *canonicalize_path(const char *path)
{
#if (defined _WIN32)
    char pathBuf[MAX_PATH], *ptr;
    GetFullPathName(path, MAX_PATH, pathBuf, &ptr);
#else
    char pathBuf[PATH_MAX];
    realpath(path, pathBuf);
#endif
    return strdup(pathBuf);
}

// Run a script in the background, without
// showing a command window on Windows.
// Simply wraps system() on other platforms
void run_command_async(const char *cmd)
{
#if (defined _WIN32 || defined _WIN64)
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );

    if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        pn_log("Failed to spawn script with errorcode: %d", GetLastError());
        return;
    }
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
#else
    system(cmd);
#endif
}

// Run a command synchronously, logging output with a given prefix
int run_command(const char *cmd, char *log_prefix)
{
    FILE *process = popen(cmd, "r");
    if (!process)
    {
        pn_log("%sError invoking read process: %s", log_prefix, cmd);
        return 1;
    }

    char buffer[1024];
    while (!feof(process))
    {
        if (fgets(buffer, 1024, process) != NULL)
        {
            // Split log messages on newlines
            char *str = buffer, *end;
            while ((end = strstr(str, "\n")) != NULL)
            {
                char *next = end + 1;
                end = '\0';
                if (strlen(str) > 0)
                    pn_log("%s%s", log_prefix, str);
                str = next;
            }

            if (strlen(str) > 0)
                pn_log("%s%s", log_prefix, str);
        }
    }

    return pclose(process);
}
	