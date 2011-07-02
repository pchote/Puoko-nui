#!/usr/bin/env python
# Copyright 2010-2011 Paul Chote
# This file is part of Puoko-nui, which is free software. It is made available
# to you under the terms of version 3 of the GNU General Public License, as
# published by the Free Software Foundation. For more information, see LICENSE.


"""Online reduction configuration script for Puoko-nui.
 Prompts the user for configuration details for a reduction session"""
import os
import fnmatch
import sys
import pyfits
import numpy
import ds9
import reduction

def main():
    # configure.py <data filename> <frame dir> <frame pattern> [<dark frame> <flat frame>]
    if len(sys.argv) >= 4:
        os.chdir(sys.argv[2])
        pattern = sys.argv[3]

        files = os.listdir('.')
        files.sort()
        first = True
        region = [-1,-1,-1,-1]
        filtered = fnmatch.filter(files, pattern)
        print "Found %d files" % len(filtered)
        if filtered > 0:
            output = open(sys.argv[1], 'w')
            output.write('# Puoko-nui Online reduction output\n')
            output.write('# FrameDir: {0}\n'.format(sys.argv[1]))
            output.write('# FramePattern: {0}\n'.format(pattern))
            if len(sys.argv) >= 5:
                output.write('# DarkTemplate: {0}\n'.format(sys.argv[4]))
            if len(sys.argv) >= 6:
                output.write('# FlatTemplate: {0}\n'.format(sys.argv[5]))
            hdulist = pyfits.open(filtered[0])
            imagedata = hdulist[0].data
        
            d = ds9.ds9('puoko-nui')
            d.set_np2arr(imagedata,dtype=numpy.int32)
        
            regions = []
        
            # Promt the user for regions
            while True:
                region = reduction.prompt_region(d, regions)
                if region is -1:
                    break;
                regions.append(region)
            
            # Write regions to config file
            for r in regions:
                output.write('# Target: {0}\n'.format(r))
                
            if hdulist[0].header.has_key('UTC-BEG'):
                datestart = hdulist[0].header['UTC-DATE'] + ' ' + hdulist[0].header['UTC-BEG']
            elif hdulist[0].header.has_key('GPSTIME'):
                datestart = hdulist[0].header['GPSTIME']
            elif hdulist[0].header.has_key('UTC'):
                datestart = hdulist[0].header['UTC'][:19] 
            else:
                raise Exception('No valid time header found')
            
            output.write('# ReferenceTime: {0}\n'.format(datestart))
            output.close()
            hdulist.close()
    else:
        print 'No filename specified'

if __name__ == '__main__':
    main()
