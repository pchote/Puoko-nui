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
import types
import math
import time
import calendar


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
def chord_area(c, p1, p2):
    r = c[2]
    # b is 0.5*the length of the chord defined by p1 and p2
    b = math.sqrt((p2[0]-p1[0])**2 + (p2[1]-p1[1])**2)/2
    return r*r*math.asin(b/r) - b*math.sqrt(r*r-b*b)

# Calculate the area of a polygon defined by a list of points
#   Returns the area
def polygon_area(p):
    a = 0
    n = len(p)
    for i in range(0,n,1):
        a += p[(i-1)%n][0]*p[i][1] - p[i][0]*p[(i-1)%n][1]
    return abs(a/2)

# Integrates the flux within the specified aperture, 
# accounting for partially covered pixels.
#   Takes the aperture (x,y,r) and the image data (2d numpy array)
#   Returns the contained flux (including background)
def integrate_aperture(aperture, imagedata):
    boxx = int(aperture[0])
    boxy = int(aperture[1])
    boxr = int(aperture[2]) + 1
    x = aperture[0]
    y = aperture[1]
    r1 = aperture[2]
    total = 0
    
    for j in range(boxy-boxr,boxy+boxr,1):
        for i in range(boxx-boxr,boxx+boxr,1):
            # Test 4 corners to see how much of pixel is contained
            #           TL    BL    BR    TR
            corners = [[0,0],[0,1],[1,1],[1,0]]
            hit = [False,False,False,False]
            hit_count = 0
            
            for c in range(0,4,1):
                ii = i + corners[c][0]
                jj = j + corners[c][1]
                if (ii-x)**2 + (jj-y)**2 <= r1*r1:
                    hit[c] = True
                    hit_count += 1
            
            if hit_count is 0:
                continue
            
            if hit_count is 1:
                # Find the vertex that is inside
                inside = 0
                for c in range(0,4,1):
                    if hit[c] is True:
                        inside = c
                        break
                
                # Corner points
                pbef = [i + corners[(inside-1)%4][0], j + corners[(inside-1)%4][1]]
                pin = [i + corners[inside][0], j + corners[inside][1]]
                paft = [i + corners[(inside+1)%4][0], j + corners[(inside+1)%4][1]]
                
                # Intersection points
                x1 = line_circle_intersection([x,y,r1], pbef, pin)
                x2 = line_circle_intersection([x,y,r1], pin, paft)
                
                # Area
                total += (polygon_area([x1, pin, x2]) + chord_area(aperture, x1, x2))*imagedata[j,i]
                            
            elif hit_count is 2:
                # Find the vertex that is inside
                first = 0
                for c in range(0,4,1):
                    if hit[c] is True and hit[(c+1)%4] is True:
                        first = c
                        break

                # Corner points
                pbef = [i + corners[(first-1)%4][0], j + corners[(first-1)%4][1]]
                p1 = [i + corners[first][0], j + corners[first][1]]
                p2 = [i + corners[(first+1)%4][0], j + corners[(first+1)%4][1]]
                paft = [i + corners[(first+2)%4][0], j + corners[(first+2)%4][1]]

                # Intersection points
                x1 = line_circle_intersection([x,y,r1], pbef, p1)
                x2 = line_circle_intersection([x,y,r1], p2, paft)

                # Area
                total += (polygon_area([x1, p1, p2, x2]) + chord_area(aperture, x1, x2))*imagedata[j,i]
            
            elif hit_count is 3:
                # Find the vertex that is outside
                outside = 0
                for c in range(0,4,1):
                    if hit[c] is False:
                        outside = c
                        break

                # Corner points
                pbef = [i + corners[(outside-1)%4][0], j + corners[(outside-1)%4][1]]
                pout = [i + corners[outside][0], j + corners[outside][1]]
                paft = [i + corners[(outside+1)%4][0], j + corners[(outside+1)%4][1]]

                # Intersection points
                x1 = line_circle_intersection([x,y,r1], pbef, pout)
                x2 = line_circle_intersection([x,y,r1], pout, paft)

                # Area
                total += (1 - polygon_area([x1, pout, x2]) + chord_area(aperture, x1, x2))*imagedata[j,i]
                        
            elif hit_count is 4:
                total += imagedata[j,i]
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
    
def main():
    # First argument gives the dir containing images, second the regex of the files to process 
    if len(sys.argv) >= 1:
        os.chdir(sys.argv[1])
        regions = []
        processed = []
        pattern = ""
        dark = []
        try:
            data = open('data.dat', 'r+')
            for line in data.readlines():
                if line[:10] == "# Pattern:":
                    pattern = line[11:-1]
                elif line[:9] == "# Region:":
                    regions.append(eval(line[10:]))
                elif line[:12] == "# Startdate:":
                    refdate = calendar.timegm(time.strptime(line[13:-1], "%Y-%m-%d %H:%M:%S"))
                elif line[:15] == "# DarkTemplate:":
                    darkhdu = pyfits.open(line[16:-1])
                    dark = darkhdu[0].data
                    darkhdu.close()
                elif line[0] is '#':
                    continue
                else:
                    # Assume that any other lines are reduced data files
                    # Last element of the line is the filename
                    processed.append(line.split()[-1])
        except IOError as e:
            print e
            return 1
        
        files = os.listdir('.')
        files.sort()
        first = True
        print "searching pattern: {0}".format(pattern)
        filtered = fnmatch.filter(files, pattern)
        print "Found %d files" % len(filtered)
        
        current_file = 0
        total_files = len(filtered)
                    
        for filename in filtered:
            current_file += 1
            
            # Check if file has been reduced
            if filename in processed:
                continue
            
            print "{1} / {2}: {0}".format(filename, current_file, total_files)
            
            hdulist = pyfits.open(filename)
            imagedata = hdulist[0].data
            if hdulist[0].header.has_key('UTC-BEG'):
                datestart = hdulist[0].header['UTC-DATE'] + ' ' + hdulist[0].header['UTC-BEG']
            elif hdulist[0].header.has_key('GPSTIME'):
                datestart = hdulist[0].header['GPSTIME'] 
            elif hdulist[0].header.has_key('UTC'):
                datestart = hdulist[0].header['UTC'][:23] 
            else:
                raise Exception('No valid time header found')
            
            startdate = calendar.timegm(time.strptime(datestart, "%Y-%m-%d %H:%M:%S.%f"))
            
            exptime = int(hdulist[0].header['EXPTIME'])
            data.write('{0} '.format(startdate - refdate))
            
            if dark is not []:
                imagedata -= dark

            for region in regions:
                try:
                    star, sky = process_frame(filename, datestart, exptime, imagedata, region)
                    data.write('{0:10f} {1:10f} '.format(star, sky))
                except Exception as e:
                    print "Error processing frame: {0}\n".format(e)
                    data.write('0 0 ')
                
            data.write('{0}\n'.format(filename))
            data.flush()
            hdulist.close()
        data.close()
    else:
        print 'No filename specified'

if __name__ == '__main__':
    main()
