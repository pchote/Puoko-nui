/*
 * Copyright 2013 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "serial.h"
#include "main.h"

#ifdef _WIN32
#   include <windows.h>
#else
#   include <errno.h>
#   include <fcntl.h>
#   include <poll.h>
#   include <sys/ioctl.h>
#   include <termios.h>
#endif
struct serial_port
{
#ifdef _WIN32
    HANDLE handle;
#else
    int fd;
#endif
};


#ifdef _WIN32
// Static buffer for error messages
TCHAR error_buf[1024];
#endif

struct serial_port *serial_new(const char *path, uint32_t baud, ssize_t *error)
{
    struct serial_port *port = calloc(1, sizeof(struct serial_port));

#ifdef _WIN32
    if (!port)
    {
        *error = -ERROR_NOT_ENOUGH_MEMORY;
        return NULL;
    }

    port->handle = CreateFile(path, GENERIC_READ | GENERIC_WRITE,
                              0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (port->handle == INVALID_HANDLE_VALUE)
    {
        *error = -GetLastError();
        goto open_error;
    }

    DCB dcb;
    memset(&dcb, 0, sizeof(DCB));
    dcb.DCBlength = sizeof(DCB);

    // Set baud and 8N1 frame
    dcb.BaudRate = baud;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;

    // Set control lines and flow control
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;

    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fAbortOnError = FALSE;

    if (!SetCommState(port->handle, &dcb))
    {
        *error = -GetLastError();
        goto configuration_error;
    }

    COMMTIMEOUTS ct;
    memset(&ct, 0, sizeof(COMMTIMEOUTS));

    // Set non-blocking reads
    ct.ReadIntervalTimeout = MAXDWORD;
    ct.ReadTotalTimeoutMultiplier = 0;
    ct.ReadTotalTimeoutConstant = 0;
    ct.WriteTotalTimeoutMultiplier = 0;
    ct.WriteTotalTimeoutConstant = 0;

    if (!SetCommTimeouts(port->handle, &ct))
    {
        *error = -GetLastError();
        goto configuration_error;
    }

    return port;

configuration_error:
    CloseHandle(port->handle);
open_error:
    free(port);
    return NULL;

#else
    if (!port)
    {
        *error = -ENOMEM;
        return NULL;
    }

    // Open port read/write; don't wait for hardware CARRIER line
    port->fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (port->fd == -1)
    {
        *error = -errno;
        goto open_error;
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

    // Get the current options
    struct termios tio;
    if (tcgetattr(port->fd, &tio) == -1)
    {
        *error = -errno;
        goto configuration_error;
    }

    // Set baud rate
    cfsetispeed(&tio, baud);
    cfsetospeed(&tio, baud);

    // Enable input with 8N1 frame, disabling flow control and status lines
    tio.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);
    tio.c_cflag |= CREAD | CLOCAL | CS8;
    tio.c_iflag &= ~(IXON | IXOFF | IXANY);

    // Disable input processing
    tio.c_iflag &= ~(INLCR | ICRNL | IGNCR);

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
open_error:
    free(port);
    return NULL;
#endif
}

void serial_free(struct serial_port *port)
{
#ifdef _WIN32
    if (port->handle != 0)
        CloseHandle(port->handle);
#else
    if (port->fd != -1)
        close(port->fd);
#endif
    free(port);
}

void serial_set_dtr(struct serial_port *port, bool enabled)
{
#ifdef _WIN32
    EscapeCommFunction(port->handle, enabled ? SETDTR : CLRDTR);
#else
    int val = enabled ? TIOCM_DTR : 0;
    ioctl(port->fd, TIOCMSET, &val);
#endif
}

ssize_t serial_read(struct serial_port *port, uint8_t *buf, size_t length)
{
#ifdef _WIN32
    DWORD read;
    if (!ReadFile(port->handle, buf, length, &read, NULL))
        return -GetLastError();
    return (ssize_t)read;
#else
    // Note: On some systems (e.g. Ubuntu 12.04) a non-blocking (VTIME = VMIN = 0)
    // read() returns 0 instead of -1 / ENXIO when a device has been unplugged.
    // This makes it difficult to distinguish between no-data and error.
    // Instead, use poll() to check for the no-data case, so that any read() == 0
    // indicates that the device has been unplugged.
    int ready = poll(&(struct pollfd) {.fd = port->fd, .events = POLLIN}, 1, 0);
    if (ready == 0)
        return 0;
    else if (ready == -1)
        return -errno;

    ssize_t ret = read(port->fd, buf, length);
    if (ret == 0)
        return -ENXIO;
    return (ret == -1) ? -errno : ret;
#endif
}

ssize_t serial_write(struct serial_port *port, const uint8_t *buf, size_t length)
{
#ifdef _WIN32
    DWORD written;
    if (!WriteFile(port->handle, buf, length, &written, NULL))
        return -GetLastError();
    return written;
#else
    ssize_t ret = write(port->fd, buf, length);
    return (ret == -1) ? -errno : ret;
#endif
}

const char *serial_error_string(ssize_t code)
{
#ifdef _WIN32
    DWORD ret = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                              NULL, (DWORD)(-code), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                              (LPTSTR)error_buf, 1024, NULL);
    if (!ret)
        return "Error constructing error string";
    return error_buf;
#else
    return strerror(-code);
#endif
}
