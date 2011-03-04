/* Copyright 2010-2011 Paul Chote
 * This file is part of Rangahau, which is free software. It is made available
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

/* Represents an aquired frame */
typedef struct
{
    double x;
    double y;
} double2;

typedef struct
{
    double x;
    double y;
    double r1;
    double r2;
} region;

typedef struct
{
    fitsfile *_fptr;
    int rows;
    int cols;
    unsigned short *data;
} framedata;


typedef struct
{
    double star[3];
    double sky[3];
    int time;
    char filename[64];
} record;

void error(const char *msg)
{
    printf("%s\n",msg);
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

int framedata_has_header(framedata *this, const char *key)
{
    //hdulist[0].header.has_key('UTC-BEG'):
    return FALSE;
}

int framedata_get_header_int(framedata *this, const char *key)
{
    int ret, status;
    if (fits_read_key(this->_fptr, TINT, key, &ret, NULL, &status))
        error("framedata_get_header_string failed");
    return ret;
}

void framedata_free(framedata this)
{
    int status;
    free(this.data);
    fits_close_file(this._fptr, &status);
}


// Find the center of the star within the inner circle
//   Takes the search circle and imagedata
//   Returns x,y coordinates for the star center
double2 center_aperture(region reg, double2 bg2, framedata *frame)
{
    // Round to the nearest pixel
    int x = (int)reg.x;
    int y = (int)reg.y;
    int r = (int)reg.r1;
    double bg = bg2.x;
    double std = bg2.y;
    
    // Calculate x and y marginals (sum the image into 1d lines in x and y)
    double total = 0;
    double *xm = (double *)malloc(2*r*sizeof(double));
    if (xm == NULL)
        error("malloc failed");
    for (int i = 0; i < 2*r; i++)
        xm[i] = 0;

    double *ym = (double *)malloc(2*r*sizeof(double));
    if (ym == NULL)
        error("malloc failed");
    
    for (int j = 0; j < 2*r; j++)
        ym[j] = 0;
        
    for (int j = 0; j < 2*r; j++)
        for (int i = 0; i < 2*r; i++)
        {            
            // Ignore points outside the circle
            if ((i-r)*(i-r) + (j-r)*(j-r) > r*r)
                continue;

            double px = frame->data[frame->cols*(y+j-r) + (x+i-r)] - bg;
            if (fabs(px) < 3*std)
                continue;
            
            xm[i] += px;
            ym[j] += px;
            total += px;
        }
    /*
    printf("total: %f\n",total);
    printf("xm: { %f", xm[0]);
    for (int i = 1; i < 2*r; i++)
        printf(", %f", xm[i]);
    printf("}\n");

    printf("ym: { %f", ym[0]);
    for (int i = 1; i < 2*r; i++)
        printf(", %f", ym[i]);
    printf("}\n");
    */   
    // Calculate x and y moments
    double xc = 0;
    double yc = 0;
    for (int i = 0; i < 2*r; i++)
        xc += (double)i*xm[i] / total;
    
    for (int j = 0; j < 2*r; j++)
        yc += (double)j*ym[j] / total;
    
    free(xm);
    free(ym);
    
    double2 ret = {xc + x - r,yc + y - r};
    return ret;
}

// Public domain code obtained from http://alienryderflex.com/quicksort/ @ 2011-03-04
void quickSort(unsigned short *arr, int elements)
{
    #define  MAX_LEVELS  1000
    int piv, beg[MAX_LEVELS], end[MAX_LEVELS], i=0, L, R;
    beg[0] = 0; end[0] = elements;
    while (i >= 0)
    {
        L = beg[i]; R = end[i] - 1;
        if (L < R)
        {
            piv=arr[L];
            if (i==MAX_LEVELS-1)
                error("quicksort required too many levels");

            while (L<R)
            {
                while (arr[R] >= piv && L<R) R--; if (L < R) arr[L++] = arr[R];
                while (arr[L] <= piv && L<R) L++; if (L < R) arr[R--] = arr[L];
            }
            arr[L] = piv;
            beg[i + 1] = L + 1;
            end[i + 1] = end[i];
            end[i++] = L;
        }
        else
            i--;
    }
}

// Calculate the mode intensity and standard deviation within an annulus
double2 calculate_background(region r, framedata *frame)
{
    int minx = floor(r.x - r.r2);
    int maxx = ceil(r.x + r.r2);
    int miny = floor(r.y - r.r2);
    int maxy = ceil(r.y + r.r2);
    
    // Copy pixels into a flat list that can be sorted
    // Allocate enough space to store the entire region, but only copy pixels
    // within the annulus.
    unsigned short *data = (unsigned short *)malloc((maxy - miny + 1)*(maxx - minx + 1)*sizeof(unsigned short));
    int n = 0;
    for (int j = miny; j <= maxy; j++)
        for (int i = minx; i <= maxx; i++)
        {
            double d2 = (r.x-i)*(r.x-i) + (r.y-j)*(r.y-j);
            if (d2 > r.r1*r.r1 && d2 < r.r2*r.r2)
                data[n++] = frame->data[frame->cols*j + i];
        }    
    
    // Calculate mean
    double mean = 0;
    for (int i = 0; i < n; i++)
        mean += data[i];
    mean /= n;
    
    // Calculate median
    quickSort(data, n);
    double median = (data[n/2] + data[n/2+1])/2;

    // Calculate standard deviation
    double std = 0;
    for (int i = 0; i < n; i++)
        std += (data[i] - mean)*(data[i] - mean);
    std = sqrt(std/n);
    
    free(data);
    double2 ret = {3*mean - 2*median, std};
    return ret;
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
double integrate_aperture(double2 xy, double r, framedata *frame)
{
    double total = 0;
    int bx = floor(xy.x), by = floor(xy.y), br = floor(r) + 1;
    for (int i = bx-br; i < bx+br; i++)
        for (int j = by-br; j < by+br; j++)
            total += pixel_aperture_intesection(xy.x-i, xy.y-j, r)*frame->data[i + frame->cols*j];

    return total;
}

double2 process_region(region r, framedata *frame, double exptime)
{
    double2 bg = calculate_background(r, frame);
    double2 xy = center_aperture(r, bg, frame);
    double2 ret = {0,0};
    if (xy.x - r.r2 < 0 || xy.x + r.r2 > frame->cols || xy.y - r.r2 < 0 || xy.y + r.r2 > frame->rows)
    {
        fprintf(stderr, "Aperture outside chip - skipping\n");
        return ret;
    }
    
    ret.y = bg.x*M_PI*r.r1*r.r1 / exptime;
    ret.x = integrate_aperture(xy,r.r1, frame) / exptime - ret.y;

    return ret;
}

int main( int argc, char *argv[] )
{
    if (argc > 2)
    {
        char buf[PATH_MAX+8];

        region regions[10];
        int numregions = 0;
    

        region reg = {412, 351, 10, 20};
        regions[numregions++] = reg;
        reg.x = 117; reg.y = 460;
        regions[numregions++] = reg;
        reg.x = 162; reg.y = 41;
        regions[numregions++] = reg;
        
        chdir(argv[1]);
        
        FILE *data = fopen("data.dat", "r+");
        
                    
        record records[10000];
        int numrecords = 0;
    
    
        // Read any existing config and data
        while (fgets(buf, sizeof(buf)-1, data) != NULL)
        {
            // Ignore headers for now
            if (buf[0] == '#')
                continue;
                
            /*
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
            */
            sscanf(buf, "%d %lf %lf %lf %lf %lf %lf %s\n",
                &records[numrecords].time,
                &records[numrecords].star[0],
                &records[numrecords].sky[0],
                &records[numrecords].star[1],
                &records[numrecords].sky[1],
                &records[numrecords].star[2],
                &records[numrecords].sky[2],
                records[numrecords].filename
            );
            numrecords++;
        }
                
        // Iterate through the list of files matching the filepattern
        sprintf(buf, "/bin/ls %s", argv[2]);
        FILE *ls = popen(buf, "r");
        if (ls == NULL)
            error("failed to list directory");

        while (fgets(buf, sizeof(buf)-1, ls) != NULL)
        {
            // Strip the newline character from the end of the filename
            buf[strlen(buf)-1] = '\0';
            
            char *filename = buf;
            printf("%s\n", filename);
            
            // Check whether the frame has been processed
            int found = FALSE;
            for (int i = 0; i < numrecords; i++)
                if (strcmp(filename, records[numrecords].filename) == 0)
                {
                    found = TRUE;
                    break;
                }
            if (found)
                continue;
                     
            framedata frame = framedata_new(filename);
            /*
            if (framedata_has_header(&frame, "UTC-BEG"))
                sprintf(buf, "%s %s", framedata_header(&frame, "UTC-DATE"), framedata_header(&frame, "UTC-BEG"));
            else if (framedata_has_header(&frame, "GPSTIME"))
                strncpy(buf, framedata_header(&frame, "GPSTIME"), 128);
            else if (framedata_has_header(&frame, "UTC"))
            {
                strncpy(buf, framedata_header(&frame, "UTC"), 23);
                buf[23] = '\0';
            }
            else
                error("No valid time header found");
            */

            // TODO: calculate startdate
            /*
            try:
                startdate = calendar.timegm(time.strptime(datestart, "%Y-%m-%d %H:%M:%S.%f"))
            except ValueError as e:
                startdate = calendar.timegm(time.strptime(datestart, "%Y-%m-%d %H:%M:%S"))
            */
            
            // TODO: subtract master dark
            /*
            if dark is not -1:
                imagedata -= dark
            */
            
            // Process regions
            int exptime = framedata_get_header_int(&frame, "EXPTIME");
            
            // TODO: calculate relative time
            fprintf(data, "%d ", 0);
            for (int i = 0; i < numregions; i++)
            {
                double2 ret = process_region(regions[i], &frame, exptime);
                fprintf(data, "%f %f ", ret.x, ret.y);
            }
            fprintf(data, "%s\n", filename);
            
            framedata_free(frame);

        }
        pclose(ls);
        fclose(data);
    }
    return 0;
}

int main2( int argc, char *argv[] )
{
    char *filename = "EC20058_0001.fit.gz";
    region r = {360, 750, 10, 20};
    
    framedata frame = framedata_new(filename);
    process_region(r, &frame, 1);
    framedata_free(frame);

	return 0;
}
