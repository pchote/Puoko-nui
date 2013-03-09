/*
 * Copyright 2013 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef SERIAL_H
#define SERIAL_H

#include <stdbool.h>
#include <stdint.h>

#if (defined _WIN32 && !defined _WIN64)
typedef int ssize_t;
#endif

struct serial_port;

struct serial_port *serial_new(const char *path, uint32_t baud, ssize_t *error);
void serial_free(struct serial_port *port);
void serial_set_dtr(struct serial_port *port, bool enabled);
ssize_t serial_read(struct serial_port *port, uint8_t *buf, size_t length);
ssize_t serial_write(struct serial_port *port, const uint8_t *buf, size_t length);
const char *serial_error_string(ssize_t code);

#endif
