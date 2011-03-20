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

typedef enum 
{
    ADD,
    SUBTRACT,
    AVERAGE,
    MULTIPLY,
    DIVIDE,
} Mode;

int compare_double(const void *a, const void *b)
{
    const double *da = (const double *)a;
    const double *db = (const double *)b;
    
    return (*da > *db) - (*da < *db);
}

// TODO: FRAMEDATA_DBL is a giant hack -- FIX
void load_reject_minmax( const char **frames, int numFrames, int rows, int cols, int rejectHigh, int rejectLow, double *outFrame, void (*preprocess_func)(framedata*, void*), void *preprocess_data)
{   
    // Load all the flat field frames into a big int array interleaved by pixel value:
    // so big[0] = flat0[0,0], big[1] = flat1[0,0] ... big[numFrames] = flat0[0,1] etc
    double *big = (double *)malloc(numFrames*cols*rows*sizeof(double));
    for( int i = 0; i < numFrames; i++)
    {
        framedata f = framedata_new(frames[i], FRAMEDATA_DBL);
        
        // Preprocess the frame
        preprocess_func(&f, preprocess_data);

        for (int j = 0; j < rows*cols; j++)
            big[numFrames*j+i] = f.dbl_data[j];
        
        framedata_free(f);
    }
    
    // Loop over the pixels, sorting the values from each image into increasing order
    for (int j = 0; j < rows*cols; j++)
    {
        //quickSort_dbl(big + numFrames*j, numFrames);
        qsort(big + numFrames*j, numFrames, sizeof(double), compare_double);
        
        // then average the non-rejected pixels into the output array
        outFrame[j] = 0;
        for (int i = rejectLow; i < numFrames - rejectHigh; i++)
            outFrame[j] += big[numFrames*j + i];
        outFrame[j] /= (numFrames - rejectHigh - rejectLow);
    }
    free(big);
}

// Subtracts the dark count, then normalizes the frame to average to unity
// (not that we actually care about the total photometric counts)
void normalize_flat(framedata *flat, void *data)
{
    framedata *dark = (framedata *)data;
    
    if (dark->dtype != FRAMEDATA_DBL || flat->dtype != FRAMEDATA_DBL)
        error("normalize_flat frames must be type DBL");
    
    if (dark->rows != flat->rows || dark->cols != flat->rows)
        error("normalize_flat frames must have same size");
    
    int flatexp = framedata_get_header_int(flat, "EXPTIME");
    int darkexp = framedata_get_header_int(dark, "EXPTIME");
    int n = flat->rows*flat->cols;
    
    // Calculate mean
    double mean = 0;
    for (int i = 0; i < n; i++)
    {
        flat->dbl_data[i] -= flatexp*1.0/darkexp*dark->dbl_data[i];
        mean += flat->dbl_data[i];
    }
    mean /= n;

    // Calculate standard deviation
    double std = 0;
    for (int i = 0; i < n; i++)
        std += (flat->dbl_data[i] - mean)*(flat->dbl_data[i] - mean);
    std = sqrt(std/n);
    
    // Recalculate the mean, excluding outliers at 3 sigma
    double mean_new = 0;
    for (int i = 0; i < n; i++)
        if (fabs(flat->dbl_data[i] - mean) < 3*std)
            mean_new += flat->dbl_data[i];
    mean_new /= n;

    // Normalize flat to unity counts
    for (int i = 0; i < n; i++)
        flat->dbl_data[i] /= mean_new;
}

int create_flat()
{
    const char *frames[32] = 
    {
        "dome-0000.fits.gz",
        "dome-0001.fits.gz",
        "dome-0002.fits.gz",
        "dome-0003.fits.gz",
        "dome-0004.fits.gz",
        "dome-0005.fits.gz",
        "dome-0006.fits.gz",
        "dome-0007.fits.gz",
        "dome-0008.fits.gz",
        "dome-0009.fits.gz",
        "dome-0010.fits.gz",
        "dome-0011.fits.gz",
        "dome-0012.fits.gz",
        "dome-0013.fits.gz",
        "dome-0014.fits.gz",
        "dome-0015.fits.gz",
        "dome-0016.fits.gz",
        "dome-0017.fits.gz",
        "dome-0018.fits.gz",
        "dome-0019.fits.gz",
        "dome-0020.fits.gz",
        "dome-0021.fits.gz",
        "dome-0022.fits.gz",
        "dome-0023.fits.gz",
        "dome-0024.fits.gz",
        "dome-0025.fits.gz",
        "dome-0026.fits.gz",
        "dome-0027.fits.gz",
        "dome-0028.fits.gz",
        "dome-0029.fits.gz",
        "dome-0030.fits.gz",
        "dome-0031.fits.gz"
    };
    
    double *flat = (double *)malloc(512*512*sizeof(double));
    
    framedata dark = framedata_new("dark-0706.fits.gz", FRAMEDATA_DBL);
    
    // Load the flat frames, discarding the 5 outermost pixels for each
    load_reject_minmax( frames, 32, 512, 512, 5, 5, flat, &normalize_flat, (void *)&dark);
    
    framedata_free(dark);
    
    // Create a new fits file
    fitsfile *out;
    int status = 0;
    char outbuf[2048];
    sprintf(outbuf, "!%s(%s)", "out.fits.gz", frames[0]);
    fits_create_file(&out, "!out.fits.gz", &status);
    
    /* Create the primary array image (16-bit short integer pixels */
	long size[2] = { 512, 512 };
	fits_create_img(out, DOUBLE_IMG, 2, size, &status);
    
    framedata testframe = framedata_new("ec-darksubtracted.fits.gz", FRAMEDATA_DBL);
    
    for (int i = 0; i < 512*512; i++)
        flat[i] = testframe.dbl_data[i]/flat[i];
    
    // Write the frame data to the image
    if (fits_write_img(out, TDOUBLE, 1, 512*512, flat, &status))
    {
        printf("status: %d\n", status);
        error("fits_write_img failed");
    }    
    
    fits_close_file(out, &status);
    free(flat);

    return 0;
}

int main( int argc, char *argv[] )
{
    // First arg gives the mode (add / avg)
    // Second arg gives the file to take the metadata from
    // Last arg gives the file to save to
    create_flat();
    return 0;
    
    if (argc > 3)
    {                
        printf("Opening file %s\n", argv[2]);
        framedata base = framedata_new(argv[2], FRAMEDATA_INT);
        
        Mode mode;
        if (strncmp(argv[1], "add",3) == 0)
            mode = ADD;
        else if (strncmp(argv[1], "subtract",7) == 0)
            mode = SUBTRACT;
        else if(strncmp(argv[1], "avg",3) == 0)
            mode = AVERAGE;
        else if(strncmp(argv[1], "multiply",8) == 0)
            mode = MULTIPLY; 
        else if(strncmp(argv[1], "divide",6) == 0)
            mode = DIVIDE; 
        else
            error("Invalid mode");
        
        if (mode == DIVIDE)
        {
            framedata_divide(&base, atoi(argv[3]));
        }
        else if (mode == MULTIPLY)
        {
            framedata_multiply(&base, atoi(argv[3]));
        }
        else // add, subtract, average
        {
            for (int i = 3; i < argc-1; i++)
            {
                printf("Adding file %s\n", argv[i]);
                framedata other = framedata_new(argv[i], FRAMEDATA_INT);
                if (mode == SUBTRACT)
                    framedata_subtract(&base, &other);
                else
                    framedata_add(&base, &other);
                framedata_free(other);
            }
            
            if (mode == AVERAGE)
            {
                for (int i = 0; i < base.cols*base.rows; i++)
                    base.data[i] /= argc - 3;
            }
        }
        
        fitsfile *out;
    	int status = 0;
    	
    	// Create a new fits file
        char outbuf[2048];
        sprintf(outbuf, "!%s(%s)", argv[argc-1], argv[2]);
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
    else
        error("Invalid args");
    return 0;
}
