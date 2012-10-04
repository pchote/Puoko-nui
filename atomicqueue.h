/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */


#ifndef ATOMICQUEUE_H
#define ATOMICQUEUE_H

#include <stdbool.h>

struct atomicqueue *atomicqueue_create();
void atomicqueue_destroy(struct atomicqueue *queue);
bool atomicqueue_push(struct atomicqueue *queue, void *object);
void *atomicqueue_pop(struct atomicqueue *queue);

#endif
