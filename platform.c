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

/*
 * realpath() Win32 implementation, supports non standard glibc extension
 * This file has no copyright assigned and is placed in the Public Domain.
 * Written by Nach M. S. September 8, 2005
*/

char *realpath(const char *path, char resolved_path[PATH_MAX])
{
    char *return_path = 0;

    if (path) //Else EINVAL
    {
        if (resolved_path)
        {
            return_path = resolved_path;
        }
        else
        {
            //Non standard extension that glibc uses
            return_path = malloc(PATH_MAX);
        }

        if (return_path) //Else EINVAL
        {
            //This is a Win32 API function similar to what realpath() is supposed to do
            size_t size = GetFullPathNameA(path, PATH_MAX, return_path, 0);

            //GetFullPathNameA() returns a size larger than buffer if buffer is too small
            if (size > PATH_MAX)
            {
                if (return_path != resolved_path) //Malloc'd buffer - Unstandard extension retry
                {
                    size_t new_size;

                    free(return_path);
                    return_path = malloc(size);

                    if (return_path)
                    {
                        new_size = GetFullPathNameA(path, size, return_path, 0); //Try again

                        if (new_size > size) //If it's still too large, we have a problem, don't try again
                        {
                            free(return_path);
                            return_path = 0;
                            errno = ENAMETOOLONG;
                        }
                        else
                        {
                            size = new_size;
                        }
                    }
                    else
                    {
                        //I wasn't sure what to return here, but the standard does say to return EINVAL
                        //if resolved_path is null, and in this case we couldn't malloc large enough buffer
                        errno = EINVAL;
                    }  
                }
                else //resolved_path buffer isn't big enough
                {
                    return_path = 0;
                    errno = ENAMETOOLONG;
                }
            }

            //GetFullPathNameA() returns 0 if some path resolve problem occured
            if (!size) 
            {
                if (return_path != resolved_path) //Malloc'd buffer
                {
                    free(return_path);
                }

                return_path = 0;

                //Convert MS errors into standard errors
                switch (GetLastError())
                {
                    case ERROR_FILE_NOT_FOUND:
                        errno = ENOENT;
                    break;

                    case ERROR_PATH_NOT_FOUND: case ERROR_INVALID_DRIVE:
                        errno = ENOTDIR;
                    break;

                    case ERROR_ACCESS_DENIED:
                        errno = EACCES;
                    break;

                    default: //Unknown Error
                        errno = EIO;
                    break;
                }
            }

            //If we get to here with a valid return_path, we're still doing good
            if (return_path)
            {
                struct stat stat_buffer;

                //Make sure path exists, stat() returns 0 on success
                if (stat(return_path, &stat_buffer)) 
                {
                    if (return_path != resolved_path)
                    {
                        free(return_path);
                    }

                    return_path = 0;
                    //stat() will set the correct errno for us
                }
                //else we succeeded!
            }
        }
        else
        {
            errno = EINVAL;
        }
    }
    else
    {
        errno = EINVAL;
    }

    return return_path;
}
#endif


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
	