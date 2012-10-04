#!/bin/bash

# Load configuration overrides if defined
if [ -f config.sh ]; then source ./config.sh; fi

# Default platescale for puoko-nui camera at 2x2 binning = 0.66 arcsec / px
if [ -z "${CAMERA_PLATESCALE}" ]; then CAMERA_PLATESCALE=0.66; fi

# Open a new copy of ds9 if it is not available, otherwise preview
if [ "$(xpaaccess -n Online_Preview)" = 0 ]; then
	./startup.sh
fi

tsreduce preview $(pwd)/preview.fits.gz Online_Preview ${CAMERA_PLATESCALE}