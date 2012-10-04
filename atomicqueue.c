/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <stdlib.h>
#include <pthread.h>
#include "atomicqueue.h"

struct atomicqueueitem
{
    void *object;
    struct atomicqueueitem *next;
};

struct atomicqueue
{
    struct atomicqueueitem *head;
    pthread_mutex_t mutex;
};

struct atomicqueue *atomicqueue_create()
{
    struct atomicqueue *queue = malloc(sizeof(struct atomicqueue));
    if (!queue)
        return NULL;

    pthread_mutex_init(&queue->mutex, NULL);
    queue->head = NULL;
    return queue;
}

void atomicqueue_destroy(struct atomicqueue *queue)
{
    pthread_mutex_lock(&queue->mutex);
    while (queue->head != NULL)
    {
        struct atomicqueueitem *next = queue->head->next;
        free(queue->head->object);
        free(queue->head);
        queue->head = next;
    }
    pthread_mutex_unlock(&queue->mutex);
    pthread_mutex_destroy(&queue->mutex);
    free(queue);
}

bool atomicqueue_push(struct atomicqueue *queue, void *object)
{
    struct atomicqueueitem *tail = malloc(sizeof(struct atomicqueueitem));
    if (!tail)
        return false;
    
    // Add to frame queue
    tail->object = object;
    tail->next = NULL;

    pthread_mutex_lock(&queue->mutex);
    // Empty queue
    if (queue->head == NULL)
        queue->head = tail;
    else
    {
        // Find tail of queue - queue is assumed to be short
        struct atomicqueueitem *item = queue->head;
        while (item->next != NULL)
            item = item->next;
        item->next = tail;
    }
    pthread_mutex_unlock(&queue->mutex);

    return true;
}

void *atomicqueue_pop(struct atomicqueue *queue)
{
    pthread_mutex_lock(&queue->mutex);
    if (queue->head == NULL)
    {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }

    struct atomicqueueitem *head = queue->head;
    queue->head = queue->head->next;
    pthread_mutex_unlock(&queue->mutex);
    void *object = head->object;
    free(head);

    return object;
}