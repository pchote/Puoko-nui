/*
 * Copyright 2013 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include "serial.h"
#include "main.h"

struct serial_port
{
    int fd;
    struct termios initial_tio;
};

struct serial_port *serial_port_open(const char *path, uint32_t baud, ssize_t *error)
{
    struct serial_port *port = calloc(1, sizeof(struct serial_port));
    if (!port)
        return NULL;

    // Open port read/write; don't wait for hardware CARRIER line
    port->fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (port->fd == -1)
    {
        *error = -errno;
        return NULL;
    }

    // Require exclusive access to the port
    if (ioctl(port->fd, TIOCEXCL) == -1)
    {
        *error = -errno;
        goto configuration_error;
    }

    // Clear O_NONBLOCK - non-blocking reads are set via non-canonical
    // mode VMIN=VTIME=0, which gives nicer semantics than O_NONBLOCK.
    if (fcntl(port->fd, F_SETFL, 0) == -1)
    {
        *error = -errno;
        goto configuration_error;
    }

    // Get the current options to restore when we are finished.
    if (tcgetattr(port->fd, &port->initial_tio) == -1)
    {
        *error = -errno;
        goto configuration_error;
    }

    // Read current port configuration
    struct termios tio;
    memcpy(&tio, &port->initial_tio, sizeof(struct termios));

    // Set baud rate
    cfsetispeed(&tio, baud);
    cfsetospeed(&tio, baud);

    // Enable input with 8N1 frame, disabling flow control and status lines
    tio.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);
    tio.c_cflag |= CREAD | CLOCAL | CS8;
    tio.c_iflag &= ~(IXON | IXOFF | IXANY);

    // Disable output processing
    tio.c_oflag &= ~OPOST;

    // Set non-canonical mode, disable echo and special character handling
    tio.c_lflag &= ~(ICANON | ISIG | ECHO | ECHOE);

    // Reads timeout immediately if there is no data available
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    // Apply attributes
    if (tcsetattr(port->fd, TCSANOW, &tio) == -1)
    {
        *error = -errno;
        goto configuration_error;
    }

    return port;
configuration_error:
    close(port->fd);
    return NULL;
}

void serial_port_close(struct serial_port *port)
{
    if (port->fd == -1)
        return;

    // Attempt to restore original configuration
    tcsetattr(port->fd, TCSANOW, &port->initial_tio);
    close(port->fd);
}

void serial_port_set_dtr(struct serial_port *port, bool enabled)
{
    int val = enabled ? TIOCM_DTR : 0;
    ioctl(port->fd, TIOCMSET, &val);
}

ssize_t serial_port_read(struct serial_port *port, uint8_t *buf, size_t length)
{
    ssize_t ret = read(port->fd, buf, length);
    return (ret == -1) ? -errno : ret;
}

ssize_t serial_port_write(struct serial_port *port, const uint8_t *buf, size_t length)
{
    ssize_t ret = write(port->fd, buf, length);
    return (ret == -1) ? -errno : ret;
}

const char *serial_port_error_string(ssize_t code)
{
    return strerror(-code);
}