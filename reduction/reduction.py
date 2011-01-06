#!/usr/local/bin/python -tt
# Copyright 2007-2010 The Authors (see AUTHORS)
# This file is part of Rangahau, which is free software. It is made available
# to you under the terms of version 3 of the GNU General Public License, as
# published by the Free Software Foundation. For more information, see LICENSE.


"""Online reduction script for rangahau. Processes photometric
intensity of a selection of stars """

import sys
import pyfits
import numpy
import ds9
import astropysics.phot
import types

# rect: x y width height for the target region
# data: total image data
def display_region(rect, data):
  # Copy the region specified by `rectangle` into `out`
  out = numpy.empty([rect[2],rect[3]])
  for j in range(0,rect[3],1):
    for i in range(0,rect[2],1):
      out[j,i] = data[j+rect[1],i+rect[0]]    

  # Display the region in ds9
  d = ds9.ds9('rangahau')
  d.set_np2arr(out)
  d.set("crosshair 20 20 physical")
  
  psf = astropysics.phot.GaussianPointSpreadFunction()
  print psf.fit(out)
  

def main():  
  # First argument gives the name of the fits file to open 
  if len(sys.argv) >= 2:
    name = sys.argv[1]
    hdulist = pyfits.open(name)
    hdulist.info()
    
    imagedata = hdulist[0].data
    display_region([609,543,30,30], imagedata)
    hdulist.close()
  else:
    print 'No filename specified'

if __name__ == '__main__':
  main()
