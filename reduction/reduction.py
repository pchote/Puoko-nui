#!/usr/bin/env python

# Copyright 2007-2010 The Authors (see AUTHORS)
# This file is part of Rangahau, which is free software. It is made available
# to you under the terms of version 3 of the GNU General Public License, as
# published by the Free Software Foundation. For more information, see LICENSE.


"""Online reduction script for rangahau. Processes photometric
intensity of a selection of stars """

import sys
import pyfits
import numpy
import scipy.signal
import ds9
import types
import math
import matplotlib.mlab as mlab
import matplotlib.pyplot as plt

# rect: x y width height for the target region
# data: total image data
def display_region(rect, data):
    # Copy the region specified by `rectangle` into `out`
    out = numpy.empty([rect[3],rect[2]])
    for j in range(0,rect[3],1):
      for i in range(0,rect[2],1):
        out[j,i] = data[j+rect[1],i+rect[0]]

    # low-pass filter the image with a 2d binomial filter
    #filt = scipy.signal.convolve2d(out, [[0.0625,0.125,0.0625],[0.125,0.25,0.125],[0.0625,0.125,0.0625]])

    # Calculate the background level as the mode of the histogram
    # which is estimated by 3*mean - 2*median
    filt = out
    bg = 3*numpy.mean(filt) - 2*numpy.median(filt)

    # d = ds9.ds9('rangahau')
    #    d.set_np2arr(filt)
    #    d.set('crosshair %f %f' % (xc,yc))
    
    # Draw a histogram
    # flattened = filt.reshape(-1)  
    # n, bins, patches = plt.hist(flattened, 50, normed=1, facecolor='green', alpha=0.75)  
    # plt.xlabel('Counts')
    # plt.ylabel('Probability')
    # plt.title(r'$\mathrm{Histogram\ of\ region:}$')
    # plt.grid(True)
    # plt.show()
    
    # Calculate the standard deviation of the data from the mode (use numpy.std to give it from the mean)
    std = math.sqrt(numpy.mean(abs(out - bg)**2))
    print std

    # Threshold the image to eliminate background
    # Todo use 3sigma above the background
    thresh = numpy.empty([rect[3],rect[2]])
    t = bg
    for j in range(0,rect[3],1):
        for i in range(0,rect[2],1):
            if filt[j,i] < bg + 3*std:
                thresh[j,i] = 0
            else:
                thresh[j,i] = filt[j,i] - bg

    # Calculate the center points
    num = 0
    denom = 0
    for i in range(0,rect[2],1):
        m = 0
        for j in range(0,rect[3],1):
            m += thresh[j,i]
        num += i*m
        denom += m
  
    print "xc = %f/%f = %f " % (num, denom, num/denom)
    xc = num/denom

    num = 0
    denom = 0
    for j in range(0,rect[3],1):
        m = 0
        for i in range(0,rect[2],1):
            m += thresh[j,i]
        num += j*m
        denom += m
    yc = num/denom

    print "yc = %f/%f = %f " % (num, denom, num/denom)

    # Display the region in ds9
    d = ds9.ds9('rangahau')
    d.set_np2arr(thresh)
    d.set('crosshair %f %f' % (xc,yc))

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
        
        x = int(args[0])
        y = int(args[1])
        r1 = int(args[2])
        r2 = int(args[3])
        
        d.set("regions deleteall")
        d.set("regions command {circle %d %d %d}" % (x,y,r1))
        d.set("regions command {circle %d %d %d #background}" % (x,y,r2))
        
        msg = "Enter x,y,r1,r2 or press enter to confirm: "
        first = False
        print x,y,r1,r2
    
    return x, y, r1, r2

def calculate_background(x, y, r1, r2, imagedata):
    mask = numpy.ones(imagedata.shape)
    print x,y,r1,r2
    for j in range(y-r2,y+r2,1):
        for i in range(x-r2,x+r2,1):
            d2 = (x-i)**2 + (y-j)**2
            if (d2 > r1*r1 and d2 < r2*r2):
                mask[j,i] = 0
    
    masked = numpy.ma.masked_array(imagedata, mask=mask)
    bg = 3*numpy.mean(masked) - 2*numpy.ma.extras.median(masked)
    std = math.sqrt(numpy.mean(abs(masked - bg)**2))
    return bg, std
    
def main():
    # First argument gives the name of the fits file to open 
    if len(sys.argv) >= 2:
        name = sys.argv[1]
        hdulist = pyfits.open(name)
        hdulist.info()

        imagedata = hdulist[0].data
        # Copy imagedata into a format ds9 can display (this is an ugly hack!)
        out = numpy.empty(imagedata.shape)
        for j in range(0,imagedata.shape[0],1):
          for i in range(0,imagedata.shape[1],1):
            out[j,i] = imagedata[j,i]
            
        d = ds9.ds9('rangahau')
        d.set_np2arr(out)
        
        # Prompt the user for x,y,r1,r2
        x, y, r1, r2 = prompt_region(d)

        bg, std = calculate_background(x, y, r1, r2, imagedata)
        print "Background level: %f" % bg
        print "Standard Deviation: %f" % std
        
        thresh = numpy.empty(imagedata.shape)
        for j in range(0,imagedata.shape[0],1):
            for i in range(0,imagedata.shape[1],1):
                if imagedata[j,i] < bg + 3*std:
                    thresh[j,i] = 0
                else:
                    thresh[j,i] = imagedata[j,i] - bg
                    
        d.set_np2arr(thresh)
                  
        #display_region([x - r2,460,150,150], imagedata)
        hdulist.close()
    else:
        print 'No filename specified'

if __name__ == '__main__':
    main()
