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
import sys
from ppgplot import *
from Numeric import *

def main():
    if len(sys.argv) >= 1:
        os.chdir(sys.argv[1])
        files = os.listdir('.')
        x = []
        data = []
        
        first = True
        for filename in fnmatch.filter(files, "data-*.dat"):
            f = open(filename, "r")
            temp = []
            for line in f.readlines():
                if line[0] is '#':
                    continue
                filename, date, starttime, exptime, star, sky = line.split()
                temp.append(float(star))
                if first is True:
                    t = time.strptime("{0} {1}".format(date,starttime), "%Y-%m-%d %H:%M:%S.%f")
                    x.append(calendar.timegm(t))
            
            data.append(temp)
            first = False
        
        for i in range(0,len(x),1):
            x[i] = x[-1] - x[i]


        pgbeg("9/XSERVE",1,1)
        pgsvp(0.075,0.95,0.5,0.9)

        # Plot a truncated data set
        maxy = 0
        for y in data:
            maxy = max(maxy,max(y))

        # Show the last 15 minutes of data
        pgswin(0,900,0,maxy*1.1)
        pgbox("bc",0,0, "bcstn",0,0)
        pglab("", "Absolute Counts", "Observed intensities")
        for y in range(0,len(data),1):
            pgsci(y+2)
            pgpt(array(x), array(data[y]), 17)

        # Plot the target divided by the average of the comparison
        # Dirty hack
        display = []
        dmax = -1000000
        dmin = 1000000
        for i in range(0,len(data[0]),1):
            div = 0
            for j in range(1,len(data),1):
                div += data[j][i]
            div /= len(data)-1
            dd = 0 if div < 1 else data[0][i] / div
            display.append(dd)
            dmax = max(dmax,dd)
            dmin = min(dmin,dd)

        pgsci(1)
        pgsvp(0.075,0.95,0.1,0.5)
        pgswin(0,900,dmin,dmax)
        pgbox("bcstn",0,0, "bcstn",0,0)
        pglab("Time passed (s)", "Target / Comparisons", "")
        pgpt(array(x), array(display), 17)

        pgend()

if __name__ == '__main__':
    main()
