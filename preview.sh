#!/bin/bash
# Open a new copy of ds9 if it is not available, otherwise preview
if [ "$(xpaaccess -n Online_Preview)" = 0 ]; then
	echo "Restarting ds9. Frame will not be previewed"
    ds9 -title Online_Preview &> /dev/null &
else
    xpaset -p Online_Preview file $(pwd)/preview.fits.gz
fi