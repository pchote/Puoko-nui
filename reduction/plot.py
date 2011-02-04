#!/usr/bin/env python

# Copyright 2007-2010 The Authors (see AUTHORS)
# This file is part of Rangahau, which is free software. It is made available
# to you under the terms of version 3 of the GNU General Public License, as
# published by the Free Software Foundation. For more information, see LICENSE.


"""Test plot script"""
import os
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
    f = open("output_target.dat", "r")
    for line in f.readlines():
        if line[0] is '#':
            continue
        target.append(float(line.split()[0]))

    f = open("output_comp.dat", "r")
    for line in f.readlines():
        if line[0] is '#':
            continue
        comparison.append(float(line.split()[0]))
    
    display = []
    for i in range(0,len(target),1):
        display.append(200000*target[i]/comparison[i])
    
    plt.plot(display)    
    plt.plot(target)
    plt.plot(comparison)
    plt.show()

if __name__ == '__main__':
    main()
