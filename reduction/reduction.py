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

# Calculates the two solutions of a quadratic equation
#   Takes the coefficients in the form ax^2 + bx + c = 0
def quadratic(a, b, c):
    d = math.sqrt(b**2-4*a*c)
    x1 = (-b + d)/(2*a)
    x2 = (-b - d)/(2*a)
    return x1,x2

# Finds the intersection point of the line defined by p1 and p2 (both x,y)
# with the circle c (x,y,r).
#   Assumes that there is only one intersection (one point inside, one outside)
#   Returns (x,y) of the intersection
# See logbook 07/02/11 for calculation workthrough
def line_circle_intersection(c, p1, p2):
    # Line from p1 to p2
    dp = p2[0] - p1[0], p2[1] - p1[1]
    # Line from c to p1
    dc = p1[0] - c[0], p1[1] - c[1]
    
    # Polynomial coefficients
    a = dp[0]**2 + dp[1]**2
    b = 2*(dc[0]*dp[0] + dc[1]*dp[1])
    c = dc[0]**2 + dc[1]**2 - c[2]**2
    
    # Solve for line parameter x.
    x1,x2 = quadratic(a,b,c)
    # The solution we want will be 0<=x<=1
    x = x1 if (x1 >= 0 and x1 <= 1) else x2
    
    return [p1[0]+x*dp[0], p1[1] + x*dp[1]]

# Calculate the area enclosed between a chord defined by p1 and p2 (both x,y)
# and the edge of a circle c (x,y,r)
#   Returns the area
def chord_area(p1, p2, c):
    r = c[2]
    # b is 0.5*the length of the chord defined by p1 and p2
    b = math.sqrt((p2[0]-p1[0])**2 + (p2[1]-p1[1])**2)/2
    return r*r*math.asin(b/r) - b*math.sqrt(r*r-b*b)

# Integrates the flux within the specified aperture, 
# accounting for partially covered pixels.
#   Takes the x,y and radius coordinates of the center of the aperture
#      and the image data
#   Returns the contained flux (including background)
def integrate_aperture(x,y,r1, imagedata):
    total = 0
    for j in range(y-r1,y+r1,1):
        for i in range(x-r1,x+r1,1):
            # Test 4 corners to see how much of pixel is contained
            corners = [[0,0],[0,1],[1,1],[1,0]]
            hit = 0
            for c in corners:
                ii = x + c[0]
                jj = y + c[1]
                if (ii-i)**2 + (jj-j)**2 <= r1*r1:
                    hit += 1
            
            # Second attempt: Weight by the number of corners contained
            total += (imagedata[j,i] * hit / 4)
            #362,752,30,60
    return total


def process_frame(filename, datestart, exptime, imagedata, region, output, d):
    x,y,r1,r2 = region

    bg, std = calculate_background(x, y, r1, r2, imagedata)
    print "Background level: %f" % bg
    print "Standard Deviation: %f" % std
    
    # Compute improved x,y
    x,y = center_aperture(x, y, r1, bg, std, imagedata)
    d.set('crosshair %f %f' % (x+1,y+1))
    print x,y
    d.set("regions deleteall")
    d.set("regions command {circle %f %f %d}" % (x+1,y+1,r1))

    # Evaluate star and sky intensities, normalised to / 1s
    star_intensity = integrate_aperture(x,y,r1, imagedata) / exptime
    sky_intensity = bg*math.pi*r1*r1 / exptime
    
    output.write('{0} {1} {2} {3:10f} {4:10f}\n'.format(filename, datestart, exptime, star_intensity - sky_intensity, sky_intensity))
    output.flush()
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
        output = open('output.dat', 'w')
        output.write('#Filenamem, Datestart, Exptime, Star - Sky (normalised), Sky (normalised)\n')
        
        for filename in filtered:
            print filename
            hdulist = pyfits.open(filename)
            imagedata = hdulist[0].data

            datestart = hdulist[0].header['GPSTIME']
            exptime = int(hdulist[0].header['EXPTIME'])
            
            d = ds9.ds9('rangahau')
            d.set_np2arr(imagedata,dtype=numpy.int32)
            
            # Promt the user for a region
            if first:
                region = prompt_region(d)
                first = False
            
            process_frame(filename, datestart, exptime, imagedata, region, output, d)
        
            hdulist.close()
    else:
        print 'No filename specified'

if __name__ == '__main__':
    main()
