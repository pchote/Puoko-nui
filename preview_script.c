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
#include "preview_script.h"
#include "atomicqueue.h"
#include "main.h"
#include "preferences.h"
#include "platform.h"

struct PreviewScript
{
    pthread_t preview_thread;
    bool thread_alive;
    bool shutdown;
    bool preview_available;
};

PreviewScript *preview_script_new()
{
    PreviewScript *preview = calloc(1, sizeof(struct PreviewScript));
    if (!preview)
        return NULL;

    return preview;
}

void preview_script_free(PreviewScript *preview)
{

}

void *preview_thread(void *_preview)
{
    PreviewScript *preview = _preview;

    // Run startup script
    run_script("./startup.sh 2>&1", "Startup: ");

    // Loop until shutdown, parsing incoming data
    while (true)
    {
        millisleep(100);
        if (preview->shutdown)
            break;
        
        // Check for new preview to call
        bool preview_available = preview->preview_available;
        preview->preview_available = false;

        if (preview_available)
        {
            pn_log("Updating preview.");
            run_script("./preview.sh 2>&1", "Preview: ");
        }
    }

    preview->thread_alive = false;
    return NULL;
}

void preview_script_spawn_thread(PreviewScript *preview, ThreadCreationArgs *args)
{
    preview->thread_alive = true;
    if (pthread_create(&preview->preview_thread, NULL, preview_thread, (void *)preview))
    {
        pn_log("Failed to create preview thread");
        preview->thread_alive = false;
    }
}

void preview_script_join_thread(PreviewScript *preview)
{
    void **retval = NULL;
    if (preview->thread_alive)
        pthread_join(preview->preview_thread, retval);
}

void preview_script_notify_shutdown(PreviewScript *preview)
{
    preview->shutdown = true;
}

bool preview_script_thread_alive(PreviewScript *preview)
{
    return preview->thread_alive;
}

void preview_script_run(PreviewScript *preview)
{
    // TODO: Use signals so the preview thread can sleep
    preview->preview_available = true;
}
