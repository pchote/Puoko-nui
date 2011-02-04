#!/usr/bin/env python

# Copyright 2007-2010 The Authors (see AUTHORS)
# This file is part of Rangahau, which is free software. It is made available
# to you under the terms of version 3 of the GNU General Public License, as
# published by the Free Software Foundation. For more information, see LICENSE.


"""Online reduction script for rangahau. Processes photometric
intensity of a selection of stars """
import os
import fnmatch
import sys
import pyfits
import numpy
import scipy.signal
import ds9
import types
import math
import matplotlib.mlab as mlab
import matplotlib.pyplot as plt

# Prompts the user for a region to search
#   Takes a ds9 object to preview the region in
#   Returns the x,y,r1,r2 coordinates of the region
def prompt_region(d):
    x = y = r1 = r2 = -1
    done = False
    first = True
    msg = "Enter x,y,r1,r2: "
    while done == False:
        args = raw_input(msg).split(',')            
        if len(args) != 4 and (first or args != ['']):
            msg = "Enter x,y,r1,r2: "
            continue;
        
        if args == ['']:
            break;
        
        x = int(args[0])-1
        y = int(args[1])-1
        r1 = int(args[2])
        r2 = int(args[3])
        
        d.set("regions deleteall")
        d.set("regions command {circle %d %d %d}" % (x+1,y+1,r1))
        d.set("regions command {circle %d %d %d #background}" % (x+1,y+1,r2))
        
        msg = "Enter x,y,r1,r2 or press enter to confirm: "
        first = False
        print x,y,r1,r2
    
    return x, y, r1, r2

# Find the center of the star within the inner circle
#   Takes the search annulus and imagedata
#   Returns x,y coordinates for the star center
def center_aperture(x, y, r1, bg, std, imagedata):
    # Copy the data within the inside (circular) apeture
    # and threshold / subtract the background level
    data = numpy.empty([2*r1,2*r1])
    for j in range(0,2*r1,1):
        for i in range(0,2*r1,1):
            xx = x - r1 + i
            yy = y - r1 + j
            
            d2 = (x-xx)**2 + (y-yy)**2
            if d2 < r1*r1 and imagedata[yy,xx] > bg + 3*std:
                data[j,i] = imagedata[yy,xx] - bg
            else:
                data[j,i] = 0

    # Calculate the x and y marginals
    # (collapse the image into a 1d line in each axis)
    xm = data.sum(axis=0)
    ym = data.sum(axis=1)

    # Total counts to normalize by
    tot = xm.sum()

    # Calculate x and y moments
    xc,yc = 0,0
    for i in range(0,2*r1,1):
        xc += i*xm[i]/tot
        yc += i*ym[i]/tot
    
    # Convert back to image coords
    xc += x - r1
    yc += y - r1
    return xc, yc

# Takes the search annulus and imagedata
# Returns the background intensity plus standard deviation
def calculate_background(x, y, r1, r2, imagedata):
    mask = numpy.ones(imagedata.shape)
    for j in range(y-r2,y+r2,1):
        for i in range(x-r2,x+r2,1):
            d2 = (x-i)**2 + (y-j)**2
            if (d2 > r1*r1 and d2 < r2*r2):
                mask[j,i] = 0
    
    masked = numpy.ma.masked_array(imagedata, mask=mask)
    # Calculate the mode of the background
    bg = 3*numpy.mean(masked) - 2*numpy.ma.extras.median(masked)
    std = math.sqrt(numpy.mean(abs(masked - bg)**2))
    return bg, std

# Integrates the flux within the specified aperture, 
# accounting for partially covered pixels.
#   Takes the x,y and radius coordinates of the center of the aperture
#      and the image data
#   Returns the contained flux (including background)
def integrate_aperture(x,y,r1, imagedata):
    # First attempt - only count pixels that are totally inside the region
    x = math.floor(x)
    y = math.floor(y)
    r1 = math.floor(r1)
    
    mask = numpy.ones(imagedata.shape)
    for j in range(y-r1,y+r1,1):
        for i in range(x-r1,x+r1,1):
            d2 = (x-i)**2 + (y-j)**2
            if d2 <= r1*r1:
                mask[j,i] = 0
    
    masked = numpy.ma.masked_array(imagedata, mask=mask)
    return numpy.sum(masked)


def process_frame(imagedata, region, d):
    x,y,r1,r2 = region

    bg, std = calculate_background(x, y, r1, r2, imagedata)
    print "Background level: %f" % bg
    print "Standard Deviation: %f" % std
    
    # Compute improved x,y
    x,y = center_aperture(x, y, r1, bg, std, imagedata)
    d.set('crosshair %f %f' % (x+1,y+1))
    print x,y
    d.set("regions deleteall")
    d.set("regions command {circle %d %d %d}" % (x+1,y+1,r1))

    # Evaluate star and sky intensities
    star_intensity = integrate_aperture(x,y,r1, imagedata)
    sky_intensity = bg*math.pi*r1*r1
    
    print star_intensity, sky_intensity, star_intensity - sky_intensity
    
    
def main():

    # First argument gives the dir containing images, second the regex of the files to process 
    if len(sys.argv) >= 2:
        
        os.chdir(sys.argv[1])
        files = os.listdir('.')
        first = True
        region = [-1,-1,-1,-1]
        filtered = fnmatch.filter(files, sys.argv[2])
        print "Found %d files" % len(filtered)
        
        # Todo open an output file and write header, pass to process_frame
        
        for filename in filtered:
            print filename
            hdulist = pyfits.open(filename)
            imagedata = hdulist[0].data
            
            d = ds9.ds9('rangahau')
            d.set_np2arr(imagedata,dtype=numpy.int32)
            
            # Promt the user for a region
            if first:
                region = prompt_region(d)
                first = False
            
            process_frame(imagedata, region, d)
        
            hdulist.close()
    else:
        print 'No filename specified'

if __name__ == '__main__':
    main()
