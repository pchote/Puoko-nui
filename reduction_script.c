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
#include "reduction_script.h"
#include "atomicqueue.h"
#include "main.h"
#include "preferences.h"
#include "platform.h"

struct ReductionScript
{
    pthread_t reduction_thread;
    bool thread_alive;
    bool shutdown;

    struct atomicqueue *new_frames;
};

ReductionScript *reduction_script_new()
{
    ReductionScript *reduction = calloc(1, sizeof(struct ReductionScript));
    if (!reduction)
        return NULL;

    reduction->new_frames = atomicqueue_create();
    if (!reduction->new_frames)
    {
        free(reduction);
        return NULL;
    }

    return reduction;
}

void reduction_script_free(ReductionScript *reduction)
{
    atomicqueue_destroy(reduction->new_frames);
}

void *reduction_thread(void *_reduction)
{
    ReductionScript *reduction = _reduction;

    // Loop until shutdown, parsing incoming data
    while (true)
    {
        millisleep(100);
        if (reduction->shutdown)
            break;

        // Check for new frames to reduce
        if (atomicqueue_length(reduction->new_frames))
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
            while ((frame = atomicqueue_pop(reduction->new_frames)) != NULL)
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

    reduction->thread_alive = false;
    return NULL;
}

void reduction_script_spawn_thread(ReductionScript *reduction, ThreadCreationArgs *args)
{
    reduction->thread_alive = true;
    if (pthread_create(&reduction->reduction_thread, NULL, reduction_thread, (void *)reduction))
    {
        pn_log("Failed to create reduction thread");
        reduction->thread_alive = false;
    }
}

void reduction_script_join_thread(ReductionScript *reduction)
{
    void **retval = NULL;
    if (reduction->thread_alive)
        pthread_join(reduction->reduction_thread, retval);
}

void reduction_script_notify_shutdown(ReductionScript *reduction)
{
    reduction->shutdown = true;
}

bool reduction_script_thread_alive(ReductionScript *reduction)
{
    return reduction->thread_alive;
}

void reduction_push_frame(ReductionScript *reduction, const char *filepath)
{
    char *copy = strdup(filepath);
    if (!copy)
    {
        pn_log("Failed to duplicate filepath. Skipping reduction notification");
        return;
    }

    if (!atomicqueue_push(reduction->new_frames, copy))
        pn_log("Failed to push filepath. Reduction notification has been ignored");
}
