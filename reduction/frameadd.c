/* Copyright 2010-2011 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <fitsio.h>

#include "framedata.h"
#include "reduction.h"

int main( int argc, char *argv[] )
{
    // First arg gives the file to take the metadata from
    // Last arg gives the file to save to
    
    if (argc > 2)
    {
        printf("Opening file %s\n", argv[1]);
        framedata base = framedata_new(argv[1]);
        
        for (int i = 2; i < argc-1; i++)
        {
            printf("Adding file %s\n", argv[i]);
            framedata other = framedata_new(argv[i]);
            framedata_add(&base, &other);
            framedata_free(other);
        }
        
        fitsfile *out;
    	int status = 0;
    	
    	// Create a new fits file
        char outbuf[2048];
        sprintf(outbuf, "!%s(%s)", argv[argc-1], argv[1]);
    	fits_create_file(&out, outbuf, &status);

        // Write the frame data to the image
    	if (fits_write_img(out, TINT, 1, base.cols*base.rows, base.data, &status))
    	{
            printf("status: %d\n", status);
    	    error("fits_write_img failed");
    	}    
    	
    	fits_close_file(out, &status);
        printf("Saved to %s\n", argv[argc-1]);

        framedata_free(base);
    }
    return 0;
}
