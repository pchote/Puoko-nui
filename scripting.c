/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "scripting.h"
#include "atomicqueue.h"
#include "main.h"
#include "preferences.h"
#include "platform.h"

struct ScriptingInterface
{
    pthread_t reduction_thread;
    pthread_t preview_thread;
    bool reduction_thread_initialized;
    bool preview_thread_initialized;
    bool shutdown;

    struct atomicqueue *new_frames;
    bool preview_available;
};

static int run_script(char *script, char *log_prefix)
{
#if (defined _WIN32)
    char *msys_bash_path = pn_preference_string(MSYS_BASH_PATH);
    char *cwd = getcwd(NULL, 0);
    char *path = canonicalize_path(cwd);
    free(cwd);

    size_t cmd_len = strlen(msys_bash_path) + strlen(path) + strlen(script) + 26;
    char *cmd = malloc(cmd_len*sizeof(char));
    if (!cmd)
    {
        pn_log("Failed to allocate command string. Skipping script.");
        return 1;
    }

    snprintf(cmd, cmd_len, "%s --login -c \"cd \\\"%s\\\" && %s", msys_bash_path, path, script);
    free(path);
    free(msys_bash_path);

    int ret = run_command(cmd, log_prefix);
    free(cmd);
    return ret;
#else
    return run_command(script, log_prefix);
#endif
}

void *reduction_thread(void *args);
void *preview_thread(void *args);

ScriptingInterface *scripting_new()
{
    ScriptingInterface *scripting = calloc(1, sizeof(struct ScriptingInterface));
    if (!scripting)
        return NULL;

    scripting->new_frames = atomicqueue_create();
    if (!scripting->new_frames)
    {
        free(scripting);
        return NULL;
    }

    return scripting;
}

void scripting_free(ScriptingInterface *scripting)
{
    atomicqueue_destroy(scripting->new_frames);
}

void scripting_spawn_thread(ScriptingInterface *scripting, ThreadCreationArgs *args)
{
    pthread_create(&scripting->reduction_thread, NULL, reduction_thread, (void *)scripting);
    scripting->reduction_thread_initialized = true;
    pthread_create(&scripting->preview_thread, NULL, preview_thread, (void *)scripting);
    scripting->preview_thread_initialized = true;
}

void *reduction_thread(void *_scripting)
{
    ScriptingInterface *scripting = (ScriptingInterface *)_scripting;

    // Loop until shutdown, parsing incoming data
    while (true)
    {
        millisleep(100);
        if (scripting->shutdown)
            break;

        // Check for new frames to reduce
        if (atomicqueue_length(scripting->new_frames))
        {
            size_t command_size = 256;
            char *command = calloc(command_size, sizeof(char));
            if (!command)
            {
                pn_log("Failed to allocate reduction string. Skipping reduction");
                break;
            }

            char *reduce_string = pn_preference_char(REDUCE_FRAMES) ? "true" : "false";
            size_t command_len = strncatf(command, command_size, "./reduction.sh %s ", reduce_string);

            char *frame;
            while ((frame = atomicqueue_pop(scripting->new_frames)) != NULL)
            {
                size_t frame_len = strlen(frame);

                if (command_len + frame_len >= command_size)
                {
                    do
                    {
                        command_size += 256;
                    } while (command_len + frame_len >= command_size);

                    command = realloc(command, command_size);
                    if (!command)
                    {
                        pn_log("Failed to allocate reduction string. Skipping reduction");
                        break;
                    }
                }

                command_len = strncatf(command, command_size, "\"%s\" ", frame);
            }

            if (!command)
                continue;

            if (command_len + 5 >= command_size)
            {
                command_size += 5;
                command = realloc(command, command_size);
                if (!command)
                {
                    pn_log("Failed to allocate reduction string. Skipping reduction");
                    continue;
                }
            }
            strncatf(command, command_size, "2>&1");

            if (command)
            {
                run_script(command, "Reduction: ");
                free(command);
            }
        }
    }

    return NULL;
}

void *preview_thread(void *_scripting)
{
    ScriptingInterface *scripting = (ScriptingInterface *)_scripting;

    // Run startup script
    run_script("./startup.sh 2>&1", "Startup: ");

    // Loop until shutdown, parsing incoming data
    while (true)
    {
        millisleep(100);
        if (scripting->shutdown)
            break;
        
        // Check for new preview to call
        bool preview_available = scripting->preview_available;
        scripting->preview_available = false;

        if (preview_available)
        {
            pn_log("Updating preview.");
            run_script("./preview.sh 2>&1", "Preview: ");
        }
    }

    return NULL;
}

void scripting_update_preview(ScriptingInterface *scripting)
{
    // TODO: Use signals so the preview thread can sleep
    scripting->preview_available = true;
}

void scripting_notify_frame(ScriptingInterface *scripting, const char *filepath)
{
    char *copy = strdup(filepath);
    if (!copy)
    {
        pn_log("Failed to duplicate filepath. Skipping reduction notification");
        return;
    }

    if (!atomicqueue_push(scripting->new_frames, copy))
        pn_log("Failed to push filepath. Reduction notification has been ignored");
}

void scripting_shutdown(ScriptingInterface *scripting)
{
    scripting->shutdown = true;
    void **retval = NULL;
    if (scripting->reduction_thread_initialized)
        pthread_join(scripting->reduction_thread, retval);
    scripting->reduction_thread_initialized = false;
    if (scripting->preview_thread_initialized)
        pthread_join(scripting->preview_thread, retval);
    scripting->preview_thread_initialized = false;

}