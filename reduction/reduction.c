/* Copyright 2010-2011 Paul Chote
 * This file is part of Rangahau, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE. */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include <fitsio.h>

/* Represents an aquired frame */
typedef struct
{
    double x;
    double y;
} double2;

typedef struct
{
    fitsfile *_fptr;
    int rows;
    int cols;
    unsigned short *data;
} framedata;

void error(const char *msg)
{
    printf(msg);
    exit(1);
}

framedata framedata_new(const char *filename)
{
    framedata this;
	int status = 0;
    if (fits_open_image(&this._fptr, filename, READONLY, &status))
        error("fits_open_image failed");
    
    // Query the image size
    fits_read_key(this._fptr, TINT, "NAXIS1", &this.cols, NULL, &status);
    fits_read_key(this._fptr, TINT, "NAXIS2", &this.rows, NULL, &status);
    if (status)
        error("querying NAXIS failed");
    
    this.data = (unsigned short *)malloc(this.cols*this.rows*sizeof(unsigned short));
    if (this.data == NULL)
        error("malloc failed");
    
    long fpixel[2] = {1,1}; // Read the entire image
    if (fits_read_pix(this._fptr, TUSHORT, fpixel, this.cols*this.rows, 0, this.data, NULL, &status))
        error("fits_read_pix failed");

    return this;
}

void framedata_free(framedata this)
{
    int status;
    free(this.data);
    fits_close_file(this._fptr, &status);
}

// Finds the intersection point of the line defined by p1 and p2 (both x,y)
// with the circle c (x,y,r).
//   Assumes that there is only one intersection (one point inside, one outside)
//   Returns (x,y) of the intersection
// See logbook 07/02/11 for calculation workthrough
double2 line_circle_intersection(double x, double y, double r, double2 p0, double2 p1)
{
    // Line from p1 to p2
    double2 dp = {p1.x - p0.x, p1.y - p0.y};
    
    // Line from c to p1
    double2 dc = {p0.x - x, p0.y - y};
    
    // Polynomial coefficients
    double a = dp.x*dp.x + dp.y*dp.y;
    double b = 2*(dc.x*dp.x + dc.y*dp.y);
    double c = dc.x*dc.x + dc.y*dc.y - r*r;

    // Solve for line parameter x.
    double d = sqrt(b*b - 4*a*c);
    double x1 = (-b + d)/(2*a);
    double x2 = (-b - d)/(2*a);
    
    // The solution we want will be 0<=x<=1
    double sol = (x1 >= 0 && x1 <= 1) ? x1 : x2;
    
    double2 ret = {p0.x + sol*dp.x, p0.y + sol*dp.y};
    return ret;
}

// Calculate the area inside a chord, defined by p1,p2 on the edge of a circle radius r
double chord_area(double2 p1, double2 p2, double r)
{
    // b is 0.5*the length of the chord defined by p1 and p2
    double b = sqrt((p2.x-p1.x)*(p2.x-p1.x) + (p2.y-p1.y)*(p2.y-p1.y))/2;
    return r*r*asin(b/r) - b*sqrt(r*r-b*b);
}

// Calculate the area of a polygon defined by a list of points
//   Returns the area
double polygon_area(double2 v[], int nv)
{
    double a = 0;
    int n = 0;
    for (int i = 0; i < nv; i++)
    {
        n = i == 0 ? nv-1 : i-1;
        a += v[n].x*v[i].y - v[i].x*v[n].y;
    }
    return fabs(a/2);
}


// Convenience function to get a corner, with indices that go outside the indexed range
double2 corners[4] = {
    {0,0},
    {0,1},
    {1,1},
    {1,0}
};

double2 c(int k)
{
    while (k < 0) k += 4;
    while (k > 3) k -= 4;
    return corners[k];
}

// Calculate the intesection between the unit pixel (with TL corner at the origin)
// and the aperture defined by x,y,r.
//   Returns a number between 0 and 1 specifying the intersecting area
double pixel_aperture_intesection(double x, double y, double r)
{    
    int hit[4];
    int numhit = 0;
    for (int k = 0; k < 4; k++)
        if ((corners[k].x - x)*(corners[k].x - x) + (corners[k].y - y)*(corners[k].y - y) <= r*r)
            hit[numhit++] = k;
    
    switch (numhit)
    {
        case 0: return 0;
        case 4: return 1;
        case 1:
        {
            // Intersection points
            double2 pts[3] =
            {
                line_circle_intersection(x,y,r, c(hit[0] - 1), c(hit[0])),
                c(hit[0]),
                line_circle_intersection(x,y,r, c(hit[0]), c(hit[0] + 1))
            };

            // Area is triangle + chord
            return polygon_area(pts, 3) + chord_area(pts[0], pts[2], r);    
        }
        break;
        case 2:
        {
            // Find the first inside the aperture
            int first = (hit[1] - hit[0] == 3) ? hit[1] : hit[0];
            
            // Intersection points
            double2 pts[4] =
            {
                line_circle_intersection(x,y,r, c(first-1), c(first)),
                c(first),
                c(first+1),
                line_circle_intersection(x,y,r, c(first+1), c(first+2))
            };
            
            // Area is a quadralateral + chord
            return polygon_area(pts,4) + chord_area(pts[0], pts[3], r);
        }
        break;
        case 3:
        {
            int outside = 3;
            for (int k = 0; k < numhit; k++)
                if (hit[k] != k)
                {
                    outside = k;
                    break;
                }
    
            // Intersection points
            double2 pts[3] =
            {
                line_circle_intersection(x,y,r, c(outside-1), c(outside)),
                c(outside),
                line_circle_intersection(x,y,r, c(outside), c(outside+1))
            };
            
            // Area is square - triangle + chord
            return 1 - polygon_area(pts,3) + chord_area(pts[0], pts[2], r);
        }
        break;
    }
    return 0;
}

// Integrates the flux within the specified aperture, 
// accounting for partially covered pixels.
//   Takes the aperture (x,y,r) and the image data (2d numpy array)
//   Returns the contained flux (including background)
double integrate_aperture(double x, double y, double r, framedata *frame)
{
    double total = 0;
    int bx = floor(x), by = floor(y), br = floor(r) + 1;
    for (int i = bx-br; i < bx+br; i++)
        for (int j = by-br; j < by+br; j++)
            total += pixel_aperture_intesection(x-i, y-j, r)*frame->data[i + frame->cols*j];

    return total;
}

int main( int argc, char *argv[] )
{   
    char *filename = "EC20058_0001.fit.gz";
    framedata frame = framedata_new(filename);

    //for (int i = 0; i < 10; i++)
    //    for (int j = 0; j < 10; j++) // j = row, i = col
    //    printf("%d %d %d\n", i,j, frame.data[frame.cols*j + i]);
    
    struct timeval starttime, endtime;
	gettimeofday(&starttime, NULL);
    double total = integrate_aperture(359.14096264375621, 748.35848584951259, 20, &frame);
    gettimeofday(&endtime, NULL);
    
    double t1=starttime.tv_sec+(starttime.tv_usec/1000000.0);
    double t2=endtime.tv_sec+(endtime.tv_usec/1000000.0);

    printf("total: %f in %f ms\n", total, (t2-t1)*1000);
    
    framedata_free(frame);
	return 0;
}
