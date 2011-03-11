#!/usr/bin/env python

# Copyright 2010-2011 Paul Chote
# This file is part of Puoko-nui, which is free software. It is made available
# to you under the terms of version 3 of the GNU General Public License, as
# published by the Free Software Foundation. For more information, see LICENSE.

"""Numerically calculates the profile of a star specified in a frame"""

import os
import fnmatch
import sys
import pyfits
import numpy
import time
import calendar
import reduction
import math
#from ppgplot import *
from Numeric import *
from pylab import *
def main():
    #ec20058
    file = "EC20058_0001.fit.gz"
    x,y,r1,r2 = 364,750,20,40
    
    #ec04207
    file = "ec04207-0487.fits.gz"
    x,y,r1,r2 = 412,351,10,20
    hdulist = pyfits.open(file)
    imagedata = hdulist[0].data
    
    bg, std = reduction.calculate_background(x, y, r1, r2, imagedata)

    # Compute improved x,y
    x,y = reduction.center_aperture(x, y, r1, bg, std, imagedata)

    print x,y
    vals = numpy.empty([30])
    # Evaluate star and sky intensities, normalised to / 1s
    for i in range(0,30,1):
        vals[i] = reduction.integrate_aperture([x,y,i], imagedata)

    out = numpy.empty([29])
    for i in range(0,29,1):
        area = math.pi*(2*i + 1)
        out[i] = (vals[i+1]-vals[i])/area
    
    plot(array(range(1,30,1)), array(out))
    show()
    # Plot all data
    #pgbeg(device,1,1)

    #pgsvp(0.075,0.95,0.55,0.9)
    #pgswin(0,20,0,800)
    #pgbox("bctn",0,0, "bctn",0,0)
    #pglab("Time (s)", "Absolute Counts (ADU / s)", "Total data")
    #pgpt(array(range(1,30,1)), array(out), 17)



    print out
if __name__ == '__main__':
    main()

