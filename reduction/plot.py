#!/usr/bin/env python

# Copyright 2010-2011 Paul Chote
# This file is part of Rangahau, which is free software. It is made available
# to you under the terms of version 3 of the GNU General Public License, as
# published by the Free Software Foundation. For more information, see LICENSE.


"""Test plot script"""
import os
import time
import calendar
import fnmatch
import sys
import numpy
from ppgplot import *
from Numeric import *

def main():
    if len(sys.argv) >= 1:
        os.chdir(sys.argv[1])
        # Convert the filename column to zeros
        data = numpy.loadtxt("data.dat", converters={-1: lambda s: 0.0})
       
        # first col is time, last col is filename.
        numstars = (numpy.size(data, 1) - 2) / 2


        # Plot the entire data set

        # Extract star data
        stardata = data[:,1:2*numstars+1:2]
        time = data[:,0]

        device = sys.argv[2] if len(sys.argv) >= 3 else "9/XSERVE"
       
        
        xrange = (numpy.min(time), numpy.max(time))
        yrange = (numpy.min(stardata), numpy.max(stardata))
        
        # Plot all data
        pgbeg(device,1,1)

        pgsvp(0.075,0.95,0.55,0.9)
        pgswin(xrange[0], xrange[1], yrange[0], yrange[1])
        pgbox("bctn",0,0, "bctn",0,0)
        pglab("Time (s)", "Absolute Counts (ADU / s)", "Total data")
        for i in range(0,numpy.size(stardata, 1)):
            pgsci(i+1)
            pgpt(array(time), array(stardata[:,i]), 17)


        ratio = stardata[:,0]/stardata[:,2];
	#print ratio
        # Scale to the center of the range
        ratio *= 2500
	ratio -= 600
	
	#print ratio
        pgsci(7)
        pgpt(array(time), array(ratio), 17)
        
        # Select the data in the last 20 min
        timelimit = 20
        time2 = [(xrange[1] - t)/60 for t in time]
        # Used only for filtering max/min
        stardata2 = stardata[[i for i,t in enumerate(time2) if t < timelimit],:]
        xrange2 = (timelimit, 0)
        yrange2 = (numpy.min(stardata2), numpy.max(stardata2))

        pgsci(1)
        pgsvp(0.075,0.95,0.1,0.45)
        pgswin(xrange2[0], xrange2[1], yrange2[0], yrange2[1])
        pgbox("bctn",0,0, "bctn",0,0)
        pglab("Time passed (min)", "Absolute Counts (ADU / s)", "Total data")
        for i in range(0,numpy.size(stardata, 1)):
            pgsci(i+1)
            pgpt(array(time2), array(stardata[:,i]), 17)

        pgend()
        #print numstars
if __name__ == '__main__':
    main()

