#!/bin/bash

# Load configuration overrides if defined
if [ -f config.sh ]; then source ./config.sh; fi

# Open a new copy of ds9 if it is not available, otherwise preview
if [ "$(xpaaccess -n Online_Preview)" = 0 ]; then
	./startup.sh
fi

tsreduce preview $(pwd)/preview.fits.gz Online_Preview