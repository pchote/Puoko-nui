#!/bin/bash

# Called at program startup to initialize ds9
if [ "$1" = "startup" ]; then
	if [ $(xpaaccess -n Puoko-nui) = 0 ]; then
	    ds9 -title Puoko-nui&
	fi
    exit	
fi

# Open a new copy of ds9 if it is not available, otherwise preview
if [ $(xpaaccess -n Puoko-nui) = 0 ]; then
    ds9 -title Puoko-nui&
else
    xpaset -p Puoko-nui file $(pwd)/preview.fits.gz
fi