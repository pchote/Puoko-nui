/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

// GIT_SHA1 is defined by the makefile.
// We include this in a separate file that recompiles quickly on each build
const char *program_version()
{
    return GIT_SHA1;
}