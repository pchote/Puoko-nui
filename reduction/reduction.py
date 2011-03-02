#!/usr/bin/env python

# Copyright 2010-2011 Paul Chote
# This file is part of Rangahau, which is free software. It is made available
# to you under the terms of version 3 of the GNU General Public License, as
# published by the Free Software Foundation. For more information, see LICENSE.


"""Utility class for reducing photometric data"""
import os
import fnmatch
import sys
import pyfits
import numpy
import types
import math
import time
import calendar

def print_timing(func):
    def wrapper(*arg):
        t1 = time.time()
        res = func(*arg)
        t2 = time.time()
#        print '%s took %0.3f ms' % (func.func_name, (t2-t1)*1000.0)
        return res
    return wrapper

# Find the center of the star within the inner circle
#   Takes the search annulus and imagedata
#   Returns x,y coordinates for the star center
@print_timing
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
@print_timing
def calculate_background(x, y, r1, r2, imagedata):
    mask = numpy.ones([2*r2, 2*r2])
    data = numpy.empty([2*r2, 2*r2])
    for j in range(y-r2,y+r2,1):
        for i in range(x-r2,x+r2,1):
            data[j-y+r2,i-x+r2] = imagedata[j,i]
            d2 = (x-i)**2 + (y-j)**2
            if (d2 > r1*r1 and d2 < r2*r2):
                mask[j-y+r2,i-x+r2] = 0

    masked = numpy.ma.masked_array(data, mask=mask)

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
def line_circle_intersection(x,y,r, p0, p1):
    # Line from p1 to p2
    dp = (p1[0] - p0[0], p1[1] - p0[1])
    # Line from c to p1
    dc = (p0[0] - x, p0[1] - y)

    # Polynomial coefficients
    a = dp[0]**2 + dp[1]**2
    b = 2*(dc[0]*dp[0] + dc[1]*dp[1])
    c = dc[0]**2 + dc[1]**2 - r**2

    # Solve for line parameter x.
    x1,x2 = quadratic(a,b,c)

    # The solution we want will be 0<=x<=1
    x = x1 if (x1 >= 0 and x1 <= 1) else x2
    return (p0[0]+x*dp[0], p0[1] + x*dp[1])

# Calculate the area inside a chord, defined by p1,p2 on the edge of a circle radius r
def chord_area(p1, p2, r):
    # b is 0.5*the length of the chord defined by p1 and p2
    b = math.sqrt((p2[0]-p1[0])**2 + (p2[1]-p1[1])**2)/2
    return r*r*math.asin(b/r) - b*math.sqrt(r*r-b*b)

# Calculate the area of a polygon defined by a list of points
#   Returns the area
def polygon_area(*args):
    a = 0
    n = len(args)
    for i in range(0,n,1):
        a += args[(i-1)%n][0]*args[i][1] - args[i][0]*args[(i-1)%n][1]
    return abs(a/2)


# Convenience function to get a corner, with indices that go outside the indexed range
corners = [(0,0),(0,1),(1,1),(1,0)]
def c(k):
    return corners[k%4]

# Calculate the intesection between the unit pixel (with TL corner at the origin)
# and the aperture defined by x,y,r.
#   Returns a number between 0 and 1 specifying the intersecting area
def pixel_aperture_intesection(x,y,r):
    # Test each corner of the pixel for intersection - record indices
    hit = [k for k in range(0,4,1) if (corners[k][0] - x)**2 + (corners[k][1] - y)**2 <= r**2]
    
    count = len(hit)
    if count == 0:
        return 0

    elif count == 4:
        return 1

    elif count == 1:
        # Intersection points
        x1 = line_circle_intersection(x,y,r, c(hit[0] - 1), c(hit[0]))
        x2 = line_circle_intersection(x,y,r, c(hit[0]), c(hit[0] + 1))
    
        # Area is triangle + chord
        return polygon_area(x1, c(hit[0]), x2) + chord_area(x1, x2, r)
                
    elif count == 2:
        # Find the first inside the aperture
        first = hit[1] if hit[1] - hit[0] == 3 else hit[0]

        # Intersection points
        x1 = line_circle_intersection(x,y,r, c(first-1), c(first))
        x2 = line_circle_intersection(x,y,r, c(first+1), c(first+2))

        # Area is a quadralateral + chord
        return polygon_area(x1, c(first), c(first+1), x2) + chord_area(x1, x2, r)

    elif count == 3:
        for k in range(0,4,1):
            if k not in hit:
                outside = k
                break
    
        # Intersection points
        x1 = line_circle_intersection(x,y,r, c(outside-1), c(outside))
        x2 = line_circle_intersection(x,y,r, c(outside), c(outside+1))
    
        # Area is square - triangle + chord
        return 1 - polygon_area(x1, c(outside), x2) + chord_area(x1, x2, r)

# Integrates the flux within the specified aperture, 
# accounting for partially covered pixels.
#   Takes the aperture (x,y,r) and the image data (2d numpy array)
#   Returns the contained flux (including background)
@print_timing
def integrate_aperture(aperture, imagedata):
    boxx = int(aperture[0])
    boxy = int(aperture[1])
    boxr = int(aperture[2]) + 1
    x = aperture[0]
    y = aperture[1]
    r = aperture[2]
    total = sum([pixel_aperture_intesection(x-i, y-j, r)*imagedata[j,i] for i in range(boxx-boxr,boxx+boxr,1) for j in range(boxy-boxr,boxy+boxr,1)])
    return total


def process_frame(filename, datestart, exptime, imagedata, region):
    x,y,r1,r2 = region

    bg, std = calculate_background(x, y, r1, r2, imagedata)

    # Compute improved x,y
    x,y = center_aperture(x, y, r1, bg, std, imagedata)

    # Evaluate star and sky intensities, normalised to / 1s
    star_intensity = integrate_aperture([x,y,r1], imagedata) / exptime
    sky_intensity = bg*math.pi*r1*r1 / exptime

    return star_intensity - sky_intensity, sky_intensity

# Prompts the user for a region to search
#   Takes a ds9 object to preview the region in
#   Returns the x,y,r1,r2 coordinates of the region
def prompt_region(d, regions):
    x = y = r1 = r2 = -1
    done = False
    first = True
    msg = "Enter aperture coordinates x,y,r1,r2 or ctrl-d to finish: "
    while done == False:
        try:
            args = raw_input(msg).split(',')            
            if len(args) != 4 and (first or args != ['']):
                msg = "Enter aperture coordinates x,y,r1,r2: "
                continue;

            if args == ['']:
                break;

            x = int(args[0])-1
            y = int(args[1])-1
            r1 = int(args[2])
            r2 = int(args[3])

            d.set("regions deleteall")
            # Existing region
            for r in regions:
                d.set("regions command {circle %d %d %d #color=blue}" % (r[0]+1,r[1]+1,r[2]))
                d.set("regions command {circle %d %d %d #background color=blue}" % (r[0]+1,r[1]+1,r[3]))

            # New region
            d.set("regions command {circle %d %d %d}" % (x+1,y+1,r1))
            d.set("regions command {circle %d %d %d #background}" % (x+1,y+1,r2))

            msg = "Modify coordinates, or enter to confirm, or ctrl-d to finish: "
            first = False
        except (EOFError, KeyboardInterrupt):
            # Print a newline
            print
            return -1
    return x, y, r1, r2


# Calculate an average dark frame in adu/s
def create_dark(filepattern):
    files = os.listdir('.')
    files.sort()
    if os.path.exists('master-dark.fits'):
        return

    print "Calculating master dark frame"

    frames = fnmatch.filter(files, filepattern)
    hdulist = pyfits.open(frames[0])
    total = hdulist[0].data
    hdulist.close()
    for i in range(1,len(frames)):
        hdulist = pyfits.open(frames[i])
        total += hdulist[0].data
        hdulist.close()
    total /= len(frames)

    hdu = pyfits.PrimaryHDU(total)
    hdu.writeto('master-dark.fits')
