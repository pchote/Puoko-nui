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

def main():
    # Convert the filename column to zeros
    data1 = numpy.loadtxt(sys.argv[1]+'.dat', converters={-1: lambda s: 0.0})
    mean = numpy.mean(data1[:,-2])
    
    data = open(sys.argv[1]+'.mmod', 'w')
    for i in range(0,len(data1),1):
        data.write('{0} {1:10f}\n'.format(data1[i,0], 1000*(data1[i,-2]-mean)/mean))
    
if __name__ == '__main__':
    main()

