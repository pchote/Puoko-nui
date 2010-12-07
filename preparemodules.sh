#!/bin/sh

########################################################
# System clock setup.

# Synchronize the host system clock with an NTP server.
#ntpdate cantva.canterbury.ac.nz

########################################################
# Parallel port setup (used by the external timer card).

# Remove the lp (line printer) module that has access
# to the parallel port, otherwise the external timer
# card will not be able to access the parallel port.
rmmod lp

# Ensure the modules that allow access to the parallel
# port from userspace are loaded.
modprobe parport
modprobe parport_pc
modprobe ppdev

# Give the module a second to locate the parallel
# port device and create the device node.
sleep 1

# Set the owner of the parallel port device node
# to sullivan with group sullivan. This means
# we don't have to be root to access the parallel port.
chown sullivan:sullivan /dev/parport*

# Show the parallel port device(s).
echo Parallel port devices ...
ls -l /dev/parport*

########################################################
# Serial port setup (used to get time from GPS).

# Ensure the module that allows access to the serial
# port from userspace are loaded.
modprobe serio_raw

# Give the module a second to locate the serial
# port devices and create the device nodes.
sleep 1

# set the owner of the serial port device nodes to
# sullivan with group sullivan. This means we
# don't have to be root to access the serial port.
chown sullivan:sullivan /dev/ttyS*

# Show the serial port device(s).
echo Serial port devices ...
ls -l /dev/ttyS*

########################################################
# Camera module setup.

# Remove any existing camera module (if present).
rmmod rspiusb

# Insert the camera driver module. If a camera
# is found it will create the device node
#  /dev/rspiusb0
insmod rspiusb.ko

# Give the kernel module a second to locate
# the device and create the device node.
sleep 1

# Set the owner of the device node to
# sullivan with group sullivan. This means
# we don't have to be root to access the camera. 
chown sullivan:sullivan /dev/rspiusb*

# Show the camera device(s).
echo Camera devices ...
ls -l /dev/rspiusb*
