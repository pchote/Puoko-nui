#!/usr/bin/env python

# Copyright 2007-2010 The Authors (see AUTHORS)
# This file is part of Rangahau, which is free software. It is made available
# to you under the terms of version 3 of the GNU General Public License, as
# published by the Free Software Foundation. For more information, see LICENSE.


"""Test plot script"""
import os
import time
import calendar
import fnmatch
import copy
import sys
import pyfits
import numpy
import scipy.signal
import ds9
import types
import math
import matplotlib.mlab as mlab
import matplotlib.pyplot as plt
    
def main():
    target = []
    comparison = []
    obstime = []
    firstt = -1
    f = open("output_target.dat", "r")
    for line in f.readlines():
        if line[0] is '#':
            continue
        
        filename, date, starttime, exptime, star, sky = line.split()
        
        t = time.strptime("{0} {1}".format(date,starttime), "%Y-%m-%d %H:%M:%S")
        if (firstt == -1):
            firstt = t
            
        target.append(float(star))
        obstime.append(calendar.timegm(t) - calendar.timegm(firstt))

    f = open("output_comp.dat", "r")
    for line in f.readlines():
        if line[0] is '#':
            continue
        filename, date, starttime, exptime, star, sky = line.split()
        comparison.append(float(star))
    
    display = []
    for i in range(0,len(target),1):
        display.append(20000*target[i]/comparison[i])
    
    plt.plot(obstime,display, 'bx')    
    plt.plot(obstime,target,'gx')
    plt.plot(obstime,comparison,'rx')
    plt.show()

if __name__ == '__main__':
    main()
