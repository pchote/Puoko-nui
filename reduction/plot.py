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
import numpy
from ppgplot import *
from Numeric import *

def main():
    if len(sys.argv) >= 1:
        os.chdir(sys.argv[1])
        files = os.listdir('.')
        x = []
        reltime = []
        data = []

        files = fnmatch.filter(files, "data-*.dat")
        
        first = True
        for filename in files:
            f = open(filename, "r")
            temp = []
            for line in f.readlines():
                if line[0] is '#':
                    continue
                filename, date, starttime, exptime, star, sky = line.split()
                temp.append(float(star))
                if first is True:
                    t = time.strptime("{0} {1}".format(date,starttime), "%Y-%m-%d %H:%M:%S.%f")
                    reltime.append(float(calendar.timegm(t))/60)
                    x.append(calendar.timegm(t))
            
            data.append(temp)
            first = False
        
        for i in range(0,len(x),1):
            reltime[i] = reltime[-1] - reltime[i]


        pgbeg("9/XSERVE",1,1)


        # Plot a truncated data set
        maxy = 0
        for y in data:
            maxy = max(maxy,max(y))

        # Plot the target divided by the average of the comparison
        display = numpy.empty([len(x)])
        for i in range(0,len(data[0]),1):
            div = 0
            for j in range(1,len(data),1):
                div += data[j][i]
            div /= len(data)-1
            display[i] = 0 if div < 1 else data[0][i] / div


        mean = numpy.mean(display)
        std = numpy.std(display)

        # Show the last 15 minutes of data
        pgsvp(0.075,0.95,0.5,0.9)
        pgswin(0,15,0,maxy*1.1)
        pgbox("bc",0,0, "bctn",0,0)
        pglab("", "Absolute Counts", "Last 15 minutes")
        for y in range(0,len(data),1):
            pgsci(y+2)
            pgpt(array(reltime), array(data[y]), 17)

        mean = numpy.mean(display)
        std = numpy.std(display)

        pgsci(1)
        pgsvp(0.075,0.95,0.1,0.5)
        pgswin(0,15,mean-3*std,mean+3*std)
        pgbox("bcstn",0,0, "bctn",0,0)
        pglab("Time passed (min)", "Target / Comparisons", "")
        pgpt(array(reltime), array(display), 17)

        pgend()

if __name__ == '__main__':
    main()
