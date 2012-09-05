#!/bin/bash

# Called at program startup to initialize ds9
if [ "$(xpaaccess -n Online_Preview)" = 0 ]; then
    ds9 -title Online_Preview&
fi
