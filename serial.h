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

struct serial_port;
struct serial_port *serial_port_open(const char *path, uint32_t baud, ssize_t *error);
void serial_port_close(struct serial_port *port);
ssize_t serial_port_read(struct serial_port *port, uint8_t *buf, size_t length);
ssize_t serial_port_write(struct serial_port *port, const uint8_t *buf, size_t length);
const char *serial_port_error_string(ssize_t code);

#endif
