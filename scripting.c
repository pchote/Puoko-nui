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
#include "common.h"
#include "preferences.h"
#include "platform.h"

// Private struct implementation
struct FilepathQueue
{
    char *filepath;
    struct FilepathQueue *next;
};

struct ScriptingInterface
{
    pthread_t reduction_thread;
    pthread_t preview_thread;
    bool reduction_thread_initialized;
    bool preview_thread_initialized;
    bool shutdown;

    struct FilepathQueue *new_frames;
    bool preview_available;
    pthread_mutex_t read_mutex;
};

static int run_script(char *script, char *log_prefix)
{
#if (defined _WIN32)
    char *msys_bash_path = pn_preference_string(MSYS_BASH_PATH);
    char *cwd = getcwd(NULL, 0);
    char *path = canonicalize_path(cwd);
    free(cwd);
    char *cmd;
    asprintf(&cmd, "%s --login -c \"cd \\\"%s\\\" && %s", msys_bash_path, path, script);
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
    ScriptingInterface *scripting = malloc(sizeof(struct ScriptingInterface));
    if (!scripting)
        trigger_fatal_error("Malloc failed while allocating scripting");

    scripting->reduction_thread_initialized = false;
    scripting->preview_thread_initialized = false;
    scripting->shutdown = false;
    scripting->preview_available = false;
    scripting->new_frames = false;

    pthread_mutex_init(&scripting->read_mutex, NULL);
    return scripting;
}

void scripting_free(ScriptingInterface *scripting)
{
    pthread_mutex_destroy(&scripting->read_mutex);
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

        // Check for new frames to notify
        pthread_mutex_lock(&scripting->read_mutex);
        struct FilepathQueue *new_frames = scripting->new_frames;
        scripting->new_frames = NULL;
        pthread_mutex_unlock(&scripting->read_mutex);

        if (new_frames)
        {
            // Create command string to send to the script
            // This consists of a base command, and a list of arguments (one per frame)
            char *base = "./frame_available.sh";
            size_t command_length = strlen(base) + 1;
            struct FilepathQueue *next = new_frames;
            do
            {
                command_length += strlen(next->filepath) + 3;
                next = next->next;
            } while (next != NULL);

            // Construct string
            char *command = malloc(command_length*sizeof(char));
            strcpy(command, base);

            next = new_frames;
            do
            {
                struct FilepathQueue *cur = next;
                strcat(command, " \"");
                strcat(command, cur->filepath);
                strcat(command, "\"");

                next = cur->next;
                free (cur->filepath);
                free (cur);
            } while (next != NULL);

            pn_log("Scripting: Running reduction script");
            run_script(command, "Reduction Script: ");
            pn_log("Scripting: Reduction script complete");

            free(command);
        }
    }

    return NULL;
}

void *preview_thread(void *_scripting)
{
    ScriptingInterface *scripting = (ScriptingInterface *)_scripting;

    // Run startup script
    pn_log("Scripting: Running startup script");
    run_script("./startup.sh", "Startup Script: ");
    pn_log("Scripting: Startup script complete");

    // Loop until shutdown, parsing incoming data
    while (true)
    {
        millisleep(100);
        if (scripting->shutdown)
            break;
        
        // Check for new preview to call
        pthread_mutex_lock(&scripting->read_mutex);
        bool preview_available = scripting->preview_available;
        scripting->preview_available = false;
        pthread_mutex_unlock(&scripting->read_mutex);

        if (preview_available)
        {
            pn_log("Scripting: Running preview script");
            run_script("./preview.sh", "Preview script: ");
            pn_log("Scripting: Preview script complete");
        }
    }

    return NULL;
}

void scripting_update_preview(ScriptingInterface *scripting)
{
    // TODO: Use signals so the preview thread can sleep
    pthread_mutex_lock(&scripting->read_mutex);
    scripting->preview_available = true;
    pthread_mutex_unlock(&scripting->read_mutex);
}

void scripting_notify_frame(ScriptingInterface *scripting, const char *filepath)
{
    struct FilepathQueue *tail = malloc(sizeof(struct FilepathQueue));
    if (tail == NULL)
    {
        pn_log("Error allocating memory for scripting notification");
        return;
    }
    tail->filepath = strdup(filepath);
    tail->next = NULL;

    pthread_mutex_lock(&scripting->read_mutex);
    // Empty queue
    if (scripting->new_frames == NULL)
        scripting->new_frames = tail;
    else
    {
        // Find tail of queue - queue is assumed to be short
        struct FilepathQueue *item = scripting->new_frames;
        while (item->next != NULL)
            item = item->next;

        item->next = tail;
    }
    pthread_mutex_unlock(&scripting->read_mutex);
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