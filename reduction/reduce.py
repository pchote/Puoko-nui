#!/usr/bin/env python

# Copyright 2010-2011 Paul Chote
# This file is part of Rangahau, which is free software. It is made available
# to you under the terms of version 3 of the GNU General Public License, as
# published by the Free Software Foundation. For more information, see LICENSE.

"""Reduce a set of frames"""
import os
import fnmatch
import sys
import pyfits
import numpy
import time
import calendar
import reduction

def main():
    test2()
    die
    # First argument gives the dir containing images, second the regex of the files to process 
    if len(sys.argv) >= 1:
        os.chdir(sys.argv[1])
        regions = []
        processed = []
        pattern = ""
        dark = -1
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
            
            try:
                startdate = calendar.timegm(time.strptime(datestart, "%Y-%m-%d %H:%M:%S.%f"))
            except ValueError as e:
                startdate = calendar.timegm(time.strptime(datestart, "%Y-%m-%d %H:%M:%S"))
            
            exptime = int(hdulist[0].header['EXPTIME'])
            data.write('{0} '.format(startdate - refdate))
            
            if dark is not -1:
                imagedata -= dark

            for region in regions:
                try:
                    star, sky = reduction.process_frame(filename, datestart, exptime, imagedata, region)
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

def test2():
    hdulist = pyfits.open("EC20058_0001.fit.gz")
    print reduction.integrate_aperture([359.14096264375621, 748.35848584951259, 20], hdulist[0].data);


def test_integration():
    bg = numpy.ones([50,50])
    
    print 'area should be {0}'.format(math.pi*10*10)
    print integrate_aperture([20,20,10], bg)
    
    for i in range(20,50,1):
        for j in range(0,50,1):
            bg[j,i] = 0
    
    print 'area should be {0}'.format(math.pi*10*10/2)
    print integrate_aperture([20,20,10], bg)
    
    for j in range(20,50,1):
        for i in range(0,50,1):
            bg[j,i] = 0
    
    print 'area should be {0}'.format(math.pi*10*10/4)
    print integrate_aperture([20,20,10], bg)
if __name__ == '__main__':
    main()
