/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <master.h>
#include <pvcam.h>

#ifndef ACQUISITION_H
#define ACQUISITION_H

typedef struct
{
	RangahauCamera *camera;
	uns16 binsize;
	boolean cancelled;
	boolean active;
	void (*on_frame_available)(RangahauFrame frame);
} RangahauAcquisitionThreadInfo;

void *rangahau_acquisition_thread(void *info);

#endif
