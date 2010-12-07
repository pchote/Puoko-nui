/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

/*******************************************************************************
*  Module    : subtract
*  File      : FitsWriter.java
*  Language  : Java 1.2 (JDK 1.4)
*  Copyright : Michael Reid, 2004-
*  Created   : 16 September 2004
*  Modified  : $Date: $
*  Revision  : $Revision: $
*
*  Overview
*  --------
*
*    The FitsWriter class converts between arrays of pixels and FITS format 
*  files.
*
*  History
*  -------
*  $Log: $
*
*  CVS ID: $Id: $
*
*******************************************************************************/

package rangahau;

import java.io.DataOutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;

import nom.tam.fits.*;

public class FitsWriter
{
  public FitsWriter()
  {
  };
  
  //***************************************************************************
  /**
   * Loads the named FITS image from disk and returns the pixel contents
   * of the image as a two-dimensional array of intensity values.
   * @param filename - the name of the FITS file on disk to load.
   * @returns the loaded image as an two-dimensional array.
   * @throws java.io.IOException if there was a problem loading the file. 
   */
  public int[][] loadFitsImage(String filename) throws java.io.IOException
  {
    if ( (filename == null) || (filename.equals("")) )
    {
      throw new IllegalArgumentException("Cannot load a FITS image as the filename provided is a null string.");
    }
    
    int[][] pixelBuffer = null;
    
    try
    {
      nom.tam.fits.Fits fits = new nom.tam.fits.Fits();
      FileInputStream fis = new java.io.FileInputStream(filename);
      fits.read(fis);
    
      // Get the size of the FITS image.
      nom.tam.fits.BasicHDU header = (nom.tam.fits.BasicHDU) fits.getHDU(0);
      int axes[] = header.getAxes();
      int imageHeight = axes[0];
      int imageWidth = axes[1];
      //short values[][] = null;
      double bscale = header.getBScale();
      double bzero = header.getBZero();

      // Create a new image buffer and copy the data to there.
      pixelBuffer = new int[imageHeight][imageWidth];
      
      int pixelType = header.getBitPix();
      
      switch(pixelType)
      {     
        case nom.tam.fits.BasicHDU.BITPIX_SHORT:
        {
          short[][] values = (short[][]) header.getData().getData();
          for (int row = 0; row < imageHeight; ++row)
          {
            for (int column = 0; column < imageWidth; ++column)
            {
              pixelBuffer[row][column] = (int) (bzero + bscale*values[row][column]);
            }
          }
        }
        break;
          

        case nom.tam.fits.BasicHDU.BITPIX_FLOAT:
        {
          float[][] values = (float[][]) header.getData().getData();
          for (int row = 0; row < imageHeight; ++row)
          {
            for (int column = 0; column < imageWidth; ++column)
            {
              pixelBuffer[row][column] = (int) (bzero + bscale*values[row][column]);
            }
          }
        }
        break;
 
        case nom.tam.fits.BasicHDU.BITPIX_DOUBLE:
        {
          double[][] values = (double[][]) header.getData().getData();
          for (int row = 0; row < imageHeight; ++row)
          {
            for (int column = 0; column < imageWidth; ++column)
            {
              pixelBuffer[row][column] = (int) (bzero + bscale*values[row][column]);
            }
          }
        }
        break;
        
        case nom.tam.fits.BasicHDU.BITPIX_BYTE:
        {
          byte[][] values = (byte[][]) header.getData().getData();
          for (int row = 0; row < imageHeight; ++row)
          {
            for (int column = 0; column < imageWidth; ++column)
            {
              pixelBuffer[row][column] = (int) (bzero + bscale*values[row][column]);
            }
          }
        }
        break;
        
        case nom.tam.fits.BasicHDU.BITPIX_INT:
        {
          int[][] values = (int[][]) header.getData().getData();
          for (int row = 0; row < imageHeight; ++row)
          {
            for (int column = 0; column < imageWidth; ++column)
            {
              pixelBuffer[row][column] = (int) (bzero + bscale*values[row][column]);
            }
          }
        }
        break;
        
        default:
          throw new java.io.IOException("Pixel type is not yet supported (BITPIX = " + pixelType + ").");
      }
    }
    catch(nom.tam.fits.FitsException ex)
    {
      java.io.IOException newException = new java.io.IOException("Unable to load the FITS file called \'" + filename + "\'");
      newException.initCause(ex);
      throw newException;
    }
    
    return pixelBuffer;
  }
  
  //****************************************************************************
  public static void saveFitsImage(String imageName, short[][] data)
  {
    try
    {
      FileOutputStream fos = new FileOutputStream(imageName);
      DataOutputStream dos = new DataOutputStream(fos);

      Fits image = new Fits();
      BasicHDU header = Fits.makeHDU(data);
      //header.setBScale(1.0);
      //header.setBZero(0);
      image.addHDU(header);
      image.write(dos);
      
      dos.close();
      fos.close();
    }
    catch(Exception ex)
    {
      ex.printStackTrace();
      System.exit(3);
    }
  }
  
  //****************************************************************************
  public static void saveFitsImage(String imageName, int[][] data)
  {
    try
    {
      FileOutputStream fos = new FileOutputStream(imageName);
      DataOutputStream dos = new DataOutputStream(fos);

      Fits image = new Fits();
      BasicHDU header = Fits.makeHDU(data);
      //header.setBScale(1.0);
      //header.setBZero(0);
      image.addHDU(header);
      image.write(dos);
      
      dos.close();
      fos.close();
    }
    catch(Exception ex)
    {
      ex.printStackTrace();
      System.exit(3);
    }
  }
  
  //****************************************************************************
  public static void saveFitsImage(String imageName, double[][] data)
  {
    try
    {
      FileOutputStream fos = new FileOutputStream(imageName);
      DataOutputStream dos = new DataOutputStream(fos);

      Fits image = new Fits();
      BasicHDU header = Fits.makeHDU(data);
      //header.setBScale(1.0);
      //header.setBZero(0);
      image.addHDU(header);
      image.write(dos);
      
      dos.close();
      fos.close();
    }
    catch(Exception ex)
    {
      ex.printStackTrace();
      System.exit(3);
    }
  }
  
}