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
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include "main.h"

#if (defined _WIN32 || defined _WIN64)
    #include <windows.h>
#endif

#include "platform.h"

TimerTimestamp system_time()
{
#ifdef _WIN32
    SYSTEMTIME st;
    GetSystemTime(&st);

    return (TimerTimestamp) {
        .year = st.wYear,
        .month = st.wMonth,
        .day = st.wDay,
        .hours = st.wHour,
        .minutes = st.wMinute,
        .seconds = st.wSecond,
        .milliseconds = st.wMilliseconds,
        .locked = true,
        .exposure_progress = 0
    };
#else
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);

    struct tm *st = gmtime(&tv.tv_sec);
    return (TimerTimestamp) {
        .year = st->tm_year + 1900,
        .month = st->tm_mon,
        .day = st->tm_wday,
        .hours = st->tm_hour,
        .minutes = st->tm_min,
        .seconds = st->tm_sec,
        .milliseconds = tv.tv_usec / 1000,
        .locked = true,
        .exposure_progress = 0
    };
#endif
}

time_t struct_tm_to_time_t(struct tm *t)
{
#ifdef _WIN32
    return mktime(t);
#elif defined _WIN64
    return _mkgmtime(t);
#else
    return timegm(t);
#endif
}

void normalize_tm(struct tm *t)
{
    time_t b = struct_tm_to_time_t(t);
#ifdef _WIN32
    *t = *localtime(&b);
#elif defined _WIN64
    gmtime_s(&b, t);
#else
    gmtime_r(&b, t);
#endif
}

// Sleep for ms milliseconds
void millisleep(int ms)
{
    #if (defined _WIN32 || defined _WIN64)
        Sleep(ms);
    #else
        nanosleep(&(struct timespec){ms / 1000, (ms % 1000)*1e6}, NULL);
    #endif
}

// Cross platform equivalent of realpath()
char *canonicalize_path(const char *path)
{
#if (defined _WIN32)
    char path_buf[MAX_PATH], *ptr;
    GetFullPathName(path, MAX_PATH, path_buf, &ptr);

    // Replace all '\' in path cd ../tsreducewith '/'
    char *i;
    while ((i = strstr(path_buf, "\\")))
        i[0] = '/';
#else
    char path_buf[PATH_MAX];
    realpath(path, path_buf);
#endif
    return strdup(path_buf);
}

bool rename_atomically(const char *src, const char *dest, bool overwrite)
{
#ifdef _WIN32
    return MoveFileEx(src, dest, overwrite ? MOVEFILE_REPLACE_EXISTING : 0);
#else
    // File exists
    if (!overwrite && access(dest, F_OK) == 0)
        return false;

    return rename(src, dest) == 0;
#endif
}

bool delete_file(const char *path)
{
#ifdef _WIN32
    return DeleteFile(path);
#else
    return unlink(path) == 0;
#endif
}

// Cross platform equivalent of basename() that doesn't modify the string.
// Assumes '/' as path separator, so only use after canonicalize_path()
char *last_path_component(char *path)
{
    char *str = path;
    char *last = path;
    while (*str != '\0')
    {
        if (*str == '/')
            last = str+1;
        str++;
    }
    return last;
}

// Run a command synchronously, logging output with a given prefix
int run_command(const char *cmd, char *log_prefix)
{
#if (defined _WIN32 || defined _WIN64)
    // Create pipe for stdout/stderr
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;
    HANDLE stdout_read, stdout_write, stdin_read, stdin_write;
    if (!CreatePipe(&stdout_read, &stdout_write, &saAttr, 0))
    {
        pn_log("Failed to create stdout pipe");
        return 1;
    }

    if (!SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0))
    {
        pn_log("Failed to create stdout pipe");
        return 1;
    }

    if (!CreatePipe(&stdin_read, &stdin_write, &saAttr, 0))
    {
        pn_log("Failed to create stdout pipe");
        return 1;
    }

    if (!SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0))
    {
        pn_log("Failed to create stdout pipe");
        return 1;
    }

    // Create process
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.hStdError = stdout_write;
    si.hStdOutput = stdout_write;
    si.hStdInput = stdin_read;
    si.dwFlags |= STARTF_USESTDHANDLES;
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcess(NULL, (char *)cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        pn_log("Failed to spawn script with errorcode: %d", GetLastError());
        return 1;
    }

    CloseHandle(pi.hThread);
    CloseHandle(stdin_write);

    // Read output until process terminates
    CloseHandle(stdout_write);
    CHAR buffer[1024];
    DWORD bytes_read;

    for (;;)
    {
        if (!ReadFile(stdout_read, buffer, 1023, &bytes_read, NULL))
            break;

        buffer[bytes_read] = '\0';

        // Split log messages on newlines
        char *str = buffer, *end;
        while ((end = strstr(str, "\n")) != NULL)
        {
            char *next = end + 1;
            *end = '\0';
            if (strlen(str) > 0)
                pn_log("%s%s", log_prefix, str);
            str = next;
        }

        if (strlen(str) > 0)
            pn_log("%s%s", log_prefix, str);
    }

    CloseHandle(stdout_read);
    CloseHandle(stdin_read);
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);

    return (int)exit_code;
#else
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
#endif
}
	