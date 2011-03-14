/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <fitsio.h>

#ifndef FRAMEDATA_H
#define FRAMEDATA_H

void error(const char *msg);

typedef struct
{
    fitsfile *_fptr;
    int rows;
    int cols;
    int *data;
} framedata;


framedata framedata_new(const char *filename);
int framedata_get_header_int(framedata *this, const char *key);
int framedata_has_header_string(framedata *this, const char *key);
int framedata_has_header_string(framedata *this, const char *key);
void framedata_get_header_string(framedata *this, const char *key, char *ret);
void framedata_subtract(framedata *this, framedata *other);
void framedata_add(framedata *this, framedata *other);
void framedata_free(framedata this);

#endif