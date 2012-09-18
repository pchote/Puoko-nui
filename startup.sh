#!/bin/bash

# Called at program startup to initialize ds9
if [ "$(xpaaccess -n Online_Preview)" = 0 ]; then
	echo "Opening ds9"
    ds9 -title Online_Preview &> /dev/null &
fi
