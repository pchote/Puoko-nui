#!/bin/bash

# Load configuration overrides if defined
if [ -f config.sh ]; then source ./config.sh; fi

# platescale for puoko-nui on MJUO 1m = 0.33 arcsec / px
# tsreduce automatically corrects for binning
if [ -z "${CAMERA_PLATESCALE}" ]; then CAMERA_PLATESCALE=0.33; fi

# Open a new copy of ds9 if it is not available, otherwise preview
if [ "$(xpaaccess -n Online_Preview)" = 0 ]; then
	./startup.sh
fi

tsreduce preview $(pwd)/preview.fits.gz Online_Preview ${CAMERA_PLATESCALE}