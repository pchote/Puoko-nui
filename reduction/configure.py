#!/usr/bin/env python
# Copyright 2011 Paul Chote
# This file is part of Rangahau, which is free software. It is made available
# to you under the terms of version 3 of the GNU General Public License, as
# published by the Free Software Foundation. For more information, see LICENSE.


"""Online reduction configuration script for rangahau.
 Prompts the user for configuration details for a reduction session"""
import os
import fnmatch
import sys
import pyfits
import numpy
import ds9

import matplotlib.mlab as mlab
import matplotlib.pyplot as plt

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

def main():
    # First argument gives the dir containing images, second the regex of the files to process 
    if len(sys.argv) >= 2:
        os.chdir(sys.argv[1])
        pattern = sys.argv[2]


        # Create a master dark frame
        darkpattern = sys.argv[3]
        create_dark(darkpattern)
        
        files = os.listdir('.')
        files.sort()
        first = True
        region = [-1,-1,-1,-1]
        filtered = fnmatch.filter(files, pattern)
        print "Found %d files" % len(filtered)
        if filtered > 0:
            output = open('data.dat', 'w')
            output.write('# Rangahau Online reduction output\n')
            output.write('# Pattern: {0}\n'.format(pattern))
            output.write('# DarkTemplate: master-dark.fits\n')

            hdulist = pyfits.open(filtered[0])
            imagedata = hdulist[0].data
        
            d = ds9.ds9('rangahau')
            d.set_np2arr(imagedata,dtype=numpy.int32)
        
            regions = []
        
            # Promt the user for regions
            while True:
                region = prompt_region(d, regions)
                if region is -1:
                    break;
                regions.append(region)
            
            # Write regions to config file
            for r in regions:
                output.write('# Region: {0}\n'.format(r))
                
            if hdulist[0].header.has_key('UTC-BEG'):
                datestart = hdulist[0].header['UTC-DATE'] + ' ' + hdulist[0].header['UTC-BEG']
            elif hdulist[0].header.has_key('GPSTIME'):
                datestart = hdulist[0].header['GPSTIME']
            elif hdulist[0].header.has_key('UTC'):
                datestart = hdulist[0].header['UTC'][:23] 
            else:
                raise Exception('No valid time header found')
            
            output.write('# Startdate: {0}\n'.format(datestart))
            output.close()
            hdulist.close()
    else:
        print 'No filename specified'

if __name__ == '__main__':
    main()
