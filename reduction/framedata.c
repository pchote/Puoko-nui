/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Puoko-nui, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include "framedata.h"

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
    
    this.data = (int *)malloc(this.cols*this.rows*sizeof(int));
    if (this.data == NULL)
        error("malloc failed");
    
    long fpixel[2] = {1,1}; // Read the entire image
    if (fits_read_pix(this._fptr, TINT, fpixel, this.cols*this.rows, 0, this.data, NULL, &status))
        error("fits_read_pix failed");

    return this;
}

int framedata_get_header_int(framedata *this, const char *key)
{
    int ret, status = 0;
    if (fits_read_key(this->_fptr, TINT, key, &ret, NULL, &status))
        error("framedata_get_header_int failed");
    return ret;
}

int framedata_has_header_string(framedata *this, const char *key)
{
    int status = 0;
    char buf[128];
    fits_read_key(this->_fptr, TSTRING, key, &buf, NULL, &status);
    return status != KEY_NO_EXIST;
}

void framedata_get_header_string(framedata *this, const char *key, char *ret)
{
    int status = 0;
    if (fits_read_key(this->_fptr, TSTRING, key, ret, NULL, &status))
        error("framedata_get_header_string failed");
}

void framedata_subtract(framedata *this, framedata *other)
{
    if (this->cols != other->cols || this->rows != other->rows)
        error("Attempting to subtract frame with different size");
    
    for (int i = 0; i < this->cols*this->rows; i++)
        this->data[i] -= other->data[i];
}

void framedata_add(framedata *this, framedata *other)
{
    if (this->cols != other->cols || this->rows != other->rows)
        error("Attempting to add frame with different size");
    
    for (int i = 0; i < this->cols*this->rows; i++)
        this->data[i] += other->data[i];
}

void framedata_multiply(framedata *this, int mul)
{
    for (int i = 0; i < this->cols*this->rows; i++)
        this->data[i] *= mul;
}

void framedata_divide(framedata *this, int div)
{
    for (int i = 0; i < this->cols*this->rows; i++)
        this->data[i] /= div;
}

void framedata_free(framedata this)
{
    int status;
    free(this.data);
    fits_close_file(this._fptr, &status);
}
