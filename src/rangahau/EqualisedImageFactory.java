/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

/******************************************************************************
*
*  Module    :  JPlanet
*  File      :  EqualisedImageFactory.java
*  Language  :  Java 1.2
*  Author    :  Mike Reid
*  Copyright :  Mike Reid, 2000-
*  Created   :  4 May 2004
*  Modified  :  19 July 2004
*
*  Overview
*  ========
*
*    The EqualisedImageFactory takes two-dimensional arrays of data
*  and generates Image suitable for display. These images have been
*  produced using a process called 'histogram equalisation' where
*  all the colours in the image occur with nearly equal frequency
*  (producing good contrast between different values in the input
*  data array). The images produced by an EqualisedImageFactory
*  may either be greyscale or may have a false colourmap applied.
*
******************************************************************************/

package rangahau;

import java.awt.BorderLayout;
import java.awt.Image;
import java.awt.image.BufferedImage;
import javax.swing.ImageIcon;
import javax.swing.JFrame;

/**
*    The EqualisedImageFactory takes two-dimensional arrays of data
*  and generates Image suitable for display. These images have been
*  produced using a process called 'histogram equalisation' where
*  all the colours in the image occur with nearly equal frequency
*  (producing good contrast between different values in the input
*  data array). The images produced by an EqualisedImageFactory
*  may either be greyscale or may have a false colourmap applied.
*/
public class EqualisedImageFactory
{
  BufferedImage image;     // Image rendered into RGB pixels.
  double[][] map;          // Contains the source intensity levels.
   
  int    numBins;     // Number of pixel intensity bins to use.
  double lowestLevel; // Lowest intensity level (bottom of lowest bin).
  double binWidth;    // Width of an intensity bin.
    
  int[] equalisationTable; // Table mapping pixel intensity level to colourmap index.

  
  
  int[] colourmap;         // ARGB colourmap.
  int maxComponent;        // maximum colour index.
  int imageWidth;          // Width of the FITS image (in pixels).
  int imageHeight;         // Height of the FITS image (in pixels).

  //*******************************************************************
  /**
   * Constructs an EqualisedImageFactory object which produces
   * greyscale images.
   */
  public EqualisedImageFactory()
  {    
    numBins = 100001; // use this many intensity bins.
    maxComponent = 256;
    
    loadColourmapGrey();
  };
  
  //*******************************************************************
  // Program entry point.
  public static void main(String argv[])
  {
    JFrame frame = new JFrame("EqualisedImageFactory test");
  
    // Generate image data.
    final int imageWidth = 256;
    final int imageHeight = 512;
    double[][] data = new double[imageHeight][imageWidth];

    // Ensure the frame is large enough to display the data.
    //frame.setSize(imageWidth, imageHeight);

    // Generate sample data to display.
    for (int row = 0; row < imageHeight; ++row)
    {
      for (int column = 0; column < imageWidth; ++column)
      {
        data[row][column] = Math.random();
      }
    }
    
    // Create an EqualisedImageFactory and set its colourmap to
    // non-greyscale.
    EqualisedImageFactory imageFactory = new EqualisedImageFactory();
    imageFactory.loadColourmapB();
    
    BufferedImage image = imageFactory.generateImage(data);
    
    frame.getContentPane().setLayout(new BorderLayout());
    //frame.getContentPane().add(new ImageIcon(image), BorderLayout.CENTER);
    ImageCanvas canvas = new ImageCanvas(image);
    frame.getContentPane().add(canvas);
    
    //frame.getContentPane().add(canvas, BorderLayout.CENTER);
//    frame.getContentPane().add(new JLabel("JCanvas Test"), BorderLayout.CENTER);

    // Set the application to exit when the frame is closed.
    frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
    frame.pack();
    frame.show();
    frame.setVisible(true);
  }  
  

  //*******************************************************************
  // Draws pixels from an object into an image buffer. 
  public BufferedImage generateImage(double[][] data)
  {
    if (data == null)
    {
      throw new IllegalArgumentException("Cannot generate an image if the source data array is null");
    }

    intensityEqualise(data);
    //intensityZScale(data);


    int row = 0;
    int column = 0;
    int numrows = data.length;
    int numcolumns = data[0].length;

    // Compute the position of the image so it is placed in the middle of the canvas.
    //int xOffset = (imagecolumns - numcolumns) / 2;
    //int yOffset = (imagerows - numrows) / 2;
    int xOffset = 0;
    int yOffset = 0;

    // Now draw the FITS pixels into the image buffer.
    int alpha = 255;
    int pixel = 0;
    int xDest = 0;
    int yDest = 0;
    int binNumber = 0;
    double intensity = 0.0;
    
    BufferedImage image = new BufferedImage(numcolumns, numrows, BufferedImage.TYPE_INT_ARGB);
    
    for (row = 0; row < numrows; ++row)
    {
      for (column = 0; column < numcolumns; ++column)
      {
        // Only draw pixels of the image that actually fit on the canvas drawing surface.
        xDest = column + xOffset;
        //yDest = (numrows-row-1) + yOffset;  // nb. reverse image in y-direction.
	yDest = row + yOffset;
        //if ( (xDest >= 0) && (xDest < imagecolumns) &&  (yDest >= 0) && (yDest < imagerows) )
        {
          intensity = data[row][column];

          // Determine which bin this intensity belongs within.
          binNumber = (int) Math.floor((intensity - lowestLevel) / binWidth);
          
          // Convert the intensity to a colour using the colourmap and store
          // in the destination image.
          pixel = colourmap[equalisationTable[binNumber]];          
          image.setRGB(xDest, yDest, pixel);
        }
      }
    }
    
    return image;
   };

     //*******************************************************************
  // Draws pixels from an object into an image buffer. 
  public BufferedImage generateImage(int[][] data)
  {
    if (data == null)
    {
      throw new IllegalArgumentException("Cannot generate an image if the source data array is null");
    }

    intensityEqualise(data);
    //intensityZScale(data);


    int row = 0;
    int column = 0;
    int numrows = data.length;
    int numcolumns = data[0].length;

    // Compute the position of the image so it is placed in the middle of the canvas.
    //int xOffset = (imagecolumns - numcolumns) / 2;
    //int yOffset = (imagerows - numrows) / 2;
    int xOffset = 0;
    int yOffset = 0;

    // Now draw the FITS pixels into the image buffer.
    int alpha = 255;
    int pixel = 0;
    int xDest = 0;
    int yDest = 0;
    int binNumber = 0;
    int intensity = 0;
    
    BufferedImage image = new BufferedImage(numcolumns, numrows, BufferedImage.TYPE_INT_ARGB);
    
    for (row = 0; row < numrows; ++row)
    {
      for (column = 0; column < numcolumns; ++column)
      {
        // Only draw pixels of the image that actually fit on the canvas drawing surface.
        xDest = column + xOffset;
        //yDest = (numrows-row-1) + yOffset;  // nb. reverse image in y-direction.
        yDest = row + yOffset;
	//if ( (xDest >= 0) && (xDest < imagecolumns) &&  (yDest >= 0) && (yDest < imagerows) )
        {
          intensity = data[row][column];

          // Determine which bin this intensity belongs within.
          binNumber = (int) Math.floor((intensity - lowestLevel) / binWidth);
          
          // Convert the intensity to a colour using the colourmap and store
          // in the destination image.
          pixel = colourmap[equalisationTable[binNumber]];          
          image.setRGB(xDest, yDest, pixel);
        }
      }
    }
    
    return image;
   };
   
  //****************************************************************************
  // Computes the mapping between pixel intensity values and dislay colour
  // map indices. This is used so that contrast is enhanced. The technique
  // implemented in this method is called histogram equalisation.
  void intensityEqualise(double[][] data)
  {
    if (data == null)
    {
      throw new IllegalArgumentException("Cannot perform intensity equalisation if the input data array is null.");
    }
    
    // Get the dimensions of the input data array.
    final int numRows = data.length;
    
    if (numRows <= 0)
    {
      throw new IllegalArgumentException("Cannot perform intensity equalisation as the input data array has " + numRows + " rows.");      
    }
    
    final int numColumns = data[0].length;

    if (numColumns <= 0)
    {
      throw new IllegalArgumentException("Cannot perform intensity equalisation as the input data array has " + numColumns + " columns.");      
    }

    // Check to see that the input data array is rectangular.
    for (int row = 0; row < numRows; ++row)
    {
      if (data[row].length != numColumns)
      {
        throw new IllegalArgumentException("Cannot perform intensity equalisation as the input data array has " 
                                           + data[row].length + " columns in row " + row + " when " + numColumns + " columns were expected.");
      }
    }
    
    // Determine the range of intensity values.
    double minValue = Double.MAX_VALUE;
    double maxValue = -Double.MAX_VALUE;
    double value = 0.0;

    for (int row = 0; row < numRows; ++row)
    {
      for (int column = 0; column < numColumns; ++column)
      {
        value = data[row][column];
        
        if (value > maxValue)
        {
          maxValue = value;
        }

        if (value < minValue)
        {
          minValue = value;
        }
      }
    }
    
    final double range = maxValue - minValue;
    final int logRange = (int) Math.ceil(Math.log(range) / Math.log(10.0)); // power-of-10 that covers the range.

    //System.out.println("EqualisedImageFactory.intensityEqualise() : min = " + minValue + ", max = " + maxValue 
    //                   + ", range = " + range + ", logRange = " + logRange);
    
    // Determine the lowest intensity value of the lowest bin and the width of 
    // the bins.
    binWidth = (Math.pow(10.0, logRange) / (numBins-1.0));
    int numBinsToLowest = (int) Math.ceil( Math.abs(minValue / binWidth) );
    if (minValue >= 0)
      lowestLevel = binWidth * (int) Math.floor(minValue / binWidth);
    else
      lowestLevel = -binWidth * (int) Math.ceil( Math.abs(minValue / binWidth) );

    int frequencies[] = new int[numBins];

    // Initialise the frequency values to zero.
    for (int index = 0; index < numBins; ++index)
    {
      frequencies[index] = 0;
    }

    // Loop over the pixels of the mapand find the frequency of intensities
    // within each bin.
    int row = 0;
    int column = 0;
    double intensity = 0.0;
    int binNumber = 0;

    for (row = 0; row < numRows; ++row)
    {
      for (column = 0; column < numColumns; ++column)
      {
       intensity = data[row][column];

       // Determine which bin this intensity belongs within.
       binNumber = (int) ((intensity - lowestLevel) / binWidth);
       
       ++frequencies[binNumber];
      }
    }

    // This code does intensity histogram equalisation.
    int numPixels = numRows * numColumns;
    double scalingFactor = (maxComponent-1.0) / (double) numPixels;
    int count = 0;
    int level = 0;
    int cumulative = 0;
    int colourmapIndex = 0;
    
    equalisationTable = new int[numBins];
    
    for (count = 0; count < numBins; ++count)
    {
      // Use histogram equalisation to compute the intensity scaling factor.
      cumulative += frequencies[count];
      colourmapIndex = (int) Math.floor(cumulative * scalingFactor);

       // Store the mapping between the pixel value and equalised colourmap
       // index to use.
       equalisationTable[count] = colourmapIndex;
    }
  };

//****************************************************************************
  // Computes the mapping between pixel intensity values and dislay colour
  // map indices. This is used so that contrast is enhanced. The technique
  // implemented in this method is called histogram equalisation.
  void intensityEqualise(int[][] data)
  {
    if (data == null)
    {
      throw new IllegalArgumentException("Cannot perform intensity equalisation if the input data array is null.");
    }
    
    // Get the dimensions of the input data array.
    final int numRows = data.length;
    
    if (numRows <= 0)
    {
      throw new IllegalArgumentException("Cannot perform intensity equalisation as the input data array has " + numRows + " rows.");      
    }
    
    final int numColumns = data[0].length;

    if (numColumns <= 0)
    {
      throw new IllegalArgumentException("Cannot perform intensity equalisation as the input data array has " + numColumns + " columns.");      
    }

    // Check to see that the input data array is rectangular.
    for (int row = 0; row < numRows; ++row)
    {
      if (data[row].length != numColumns)
      {
        throw new IllegalArgumentException("Cannot perform intensity equalisation as the input data array has " 
                                           + data[row].length + " columns in row " + row + " when " + numColumns + " columns were expected.");
      }
    }
    
    // Determine the range of intensity values.
    int minValue = Integer.MAX_VALUE;
    int maxValue = Integer.MIN_VALUE;
    int value = 0;

    for (int row = 0; row < numRows; ++row)
    {
      for (int column = 0; column < numColumns; ++column)
      {
        value = data[row][column];
        
        if (value > maxValue)
        {
          maxValue = value;
        }

        if (value < minValue)
        {
          minValue = value;
        }
      }
    }
    
    final int range = maxValue - minValue;
    final int logRange = (int) Math.ceil(Math.log(range) / Math.log(10.0)); // power-of-10 that covers the range.

    //System.out.println("EqualisedImageFactory.intensityEqualise() : min = " + minValue + ", max = " + maxValue 
    //                   + ", range = " + range + ", logRange = " + logRange);
    
    // Determine the lowest intensity value of the lowest bin and the width of 
    // the bins.
    binWidth = (Math.pow(10.0, logRange) / (numBins-1.0));
    int numBinsToLowest = (int) Math.ceil( Math.abs(minValue / binWidth) );
    if (minValue >= 0)
      lowestLevel = binWidth * (int) Math.floor(minValue / binWidth);
    else
      lowestLevel = -binWidth * (int) Math.ceil( Math.abs(minValue / binWidth) );

    int frequencies[] = new int[numBins];

    // Initialise the frequency values to zero.
    for (int index = 0; index < numBins; ++index)
    {
      frequencies[index] = 0;
    }

    // Loop over the pixels of the mapand find the frequency of intensities
    // within each bin.
    int row = 0;
    int column = 0;
    int intensity = 0;
    int binNumber = 0;

    for (row = 0; row < numRows; ++row)
    {
      for (column = 0; column < numColumns; ++column)
      {
       intensity = data[row][column];

       // Determine which bin this intensity belongs within.
       binNumber = (int) ((intensity - lowestLevel) / binWidth);
       
       ++frequencies[binNumber];
      }
    }

    // This code does intensity histogram equalisation.
    int numPixels = numRows * numColumns;
    double scalingFactor = (maxComponent-1.0) / (double) numPixels;
    int count = 0;
    int level = 0;
    int cumulative = 0;
    int colourmapIndex = 0;
    
    equalisationTable = new int[numBins];
    
    for (count = 0; count < numBins; ++count)
    {
      // Use histogram equalisation to compute the intensity scaling factor.
      cumulative += frequencies[count];
      colourmapIndex = (int) Math.floor(cumulative * scalingFactor);

       // Store the mapping between the pixel value and equalised colourmap
       // index to use.
       equalisationTable[count] = colourmapIndex;
    }
  };

  //****************************************************************************
  // 
  public void loadColourmapGrey()
  {
    int index = 0;
    int alpha = 255;
    int red = 0;
    int green = 0;
    int blue = 0;
    int pixel = 0;

    colourmap = new int[maxComponent];
    
    for (index = 0; index < maxComponent; ++index)
    {
      red = index;
      green = index;
      blue = index;

      pixel = ((alpha & 0xff) << 24) | ((red & 0xff) << 16) | 
                  ((green & 0xff) << 8) | (blue & 0xff);

      colourmap[index] = pixel;
    }
  };

  //****************************************************************************
  // 
  public void loadColourmapB()
  {
    int index = 0;
    int alpha = 255;
    int red = 0;
    int green = 0;
    int blue = 0;
    int pixel = 0;

    int numColourComponents = 3;
    double values[] = {
0.00000, 0.00000, 0.00000,
0.00000, 0.00000, 0.00000,
0.00000, 0.00000, 0.00000,
0.00000, 0.00000, 0.00000,
0.00000, 0.00000, 0.00000,
0.00000, 0.00000, 0.00000,
0.00000, 0.00000, 0.00000,
0.00000, 0.00000, 0.00000,
0.00000, 0.00000, 0.06667,
0.00000, 0.00000, 0.06667,
0.00000, 0.00000, 0.06667,
0.00000, 0.00000, 0.06667,
0.00000, 0.00000, 0.06667,
0.00000, 0.00000, 0.06667,
0.00000, 0.00000, 0.06667,
0.00000, 0.00000, 0.06667,
0.00000, 0.00000, 0.13333,
0.00000, 0.00000, 0.13333,
0.00000, 0.00000, 0.13333,
0.00000, 0.00000, 0.13333,
0.00000, 0.00000, 0.13333,
0.00000, 0.00000, 0.13333,
0.00000, 0.00000, 0.13333,
0.00000, 0.00000, 0.13333,
0.00000, 0.00000, 0.20000,
0.00000, 0.00000, 0.20000,
0.00000, 0.00000, 0.20000,
0.00000, 0.00000, 0.20000,
0.00000, 0.00000, 0.20000,
0.00000, 0.00000, 0.20000,
0.00000, 0.00000, 0.20000,
0.00000, 0.00000, 0.20000,
0.00000, 0.00000, 0.26667,
0.00000, 0.00000, 0.26667,
0.00000, 0.00000, 0.26667,
0.00000, 0.00000, 0.26667,
0.00000, 0.00000, 0.26667,
0.00000, 0.00000, 0.26667,
0.00000, 0.00000, 0.26667,
0.00000, 0.00000, 0.26667,
0.00000, 0.00000, 0.33333,
0.00000, 0.00000, 0.33333,
0.00000, 0.00000, 0.33333,
0.00000, 0.00000, 0.33333,
0.00000, 0.00000, 0.33333,
0.00000, 0.00000, 0.33333,
0.00000, 0.00000, 0.33333,
0.00000, 0.00000, 0.33333,
0.00000, 0.00000, 0.40000,
0.00000, 0.00000, 0.40000,
0.00000, 0.00000, 0.40000,
0.00000, 0.00000, 0.40000,
0.00000, 0.00000, 0.40000,
0.00000, 0.00000, 0.40000,
0.00000, 0.00000, 0.40000,
0.00000, 0.00000, 0.40000,
0.00000, 0.00000, 0.46667,
0.00000, 0.00000, 0.46667,
0.00000, 0.00000, 0.46667,
0.00000, 0.00000, 0.46667,
0.00000, 0.00000, 0.46667,
0.00000, 0.00000, 0.46667,
0.00000, 0.00000, 0.46667,
0.00000, 0.00000, 0.46667,
0.00000, 0.00000, 0.53333,
0.00000, 0.00000, 0.53333,
0.00000, 0.00000, 0.53333,
0.00000, 0.00000, 0.53333,
0.00000, 0.00000, 0.53333,
0.00000, 0.00000, 0.53333,
0.00000, 0.00000, 0.53333,
0.00000, 0.00000, 0.53333,
0.06667, 0.00000, 0.53333,
0.06667, 0.00000, 0.53333,
0.06667, 0.00000, 0.53333,
0.06667, 0.00000, 0.53333,
0.06667, 0.00000, 0.53333,
0.06667, 0.00000, 0.53333,
0.06667, 0.00000, 0.53333,
0.06667, 0.00000, 0.53333,
0.13333, 0.00000, 0.53333,
0.13333, 0.00000, 0.53333,
0.13333, 0.00000, 0.53333,
0.13333, 0.00000, 0.53333,
0.13333, 0.00000, 0.53333,
0.13333, 0.00000, 0.53333,
0.13333, 0.00000, 0.53333,
0.13333, 0.00000, 0.53333,
0.20000, 0.00000, 0.53333,
0.20000, 0.00000, 0.53333,
0.20000, 0.00000, 0.53333,
0.20000, 0.00000, 0.53333,
0.20000, 0.00000, 0.53333,
0.20000, 0.00000, 0.53333,
0.20000, 0.00000, 0.53333,
0.20000, 0.00000, 0.53333,
0.26667, 0.00000, 0.53333,
0.26667, 0.00000, 0.53333,
0.26667, 0.00000, 0.53333,
0.26667, 0.00000, 0.53333,
0.26667, 0.00000, 0.53333,
0.26667, 0.00000, 0.53333,
0.26667, 0.00000, 0.53333,
0.26667, 0.00000, 0.53333,
0.33333, 0.00000, 0.53333,
0.33333, 0.00000, 0.53333,
0.33333, 0.00000, 0.53333,
0.33333, 0.00000, 0.53333,
0.33333, 0.00000, 0.53333,
0.33333, 0.00000, 0.53333,
0.33333, 0.00000, 0.53333,
0.33333, 0.00000, 0.53333,
0.40000, 0.00000, 0.53333,
0.40000, 0.00000, 0.53333,
0.40000, 0.00000, 0.53333,
0.40000, 0.00000, 0.53333,
0.40000, 0.00000, 0.53333,
0.40000, 0.00000, 0.53333,
0.40000, 0.00000, 0.53333,
0.40000, 0.00000, 0.53333,
0.46667, 0.00000, 0.53333,
0.46667, 0.00000, 0.53333,
0.46667, 0.00000, 0.53333,
0.46667, 0.00000, 0.53333,
0.46667, 0.00000, 0.53333,
0.46667, 0.00000, 0.53333,
0.46667, 0.00000, 0.53333,
0.46667, 0.00000, 0.53333,
0.53333, 0.00000, 0.53333,
0.53333, 0.00000, 0.53333,
0.53333, 0.00000, 0.53333,
0.53333, 0.00000, 0.53333,
0.53333, 0.00000, 0.46667,
0.53333, 0.00000, 0.46667,
0.53333, 0.00000, 0.46667,
0.53333, 0.00000, 0.46667,
0.60000, 0.00000, 0.40000,
0.60000, 0.00000, 0.40000,
0.60000, 0.00000, 0.40000,
0.60000, 0.00000, 0.40000,
0.60000, 0.00000, 0.33333,
0.60000, 0.00000, 0.33333,
0.60000, 0.00000, 0.33333,
0.60000, 0.00000, 0.33333,
0.66667, 0.00000, 0.26667,
0.66667, 0.00000, 0.26667,
0.66667, 0.00000, 0.26667,
0.66667, 0.00000, 0.26667,
0.66667, 0.00000, 0.20000,
0.66667, 0.00000, 0.20000,
0.66667, 0.00000, 0.20000,
0.66667, 0.00000, 0.20000,
0.73333, 0.00000, 0.13333,
0.73333, 0.00000, 0.13333,
0.73333, 0.00000, 0.13333,
0.73333, 0.00000, 0.13333,
0.73333, 0.00000, 0.06667,
0.73333, 0.00000, 0.06667,
0.73333, 0.00000, 0.06667,
0.73333, 0.00000, 0.06667,
0.80000, 0.00000, 0.00000,
0.80000, 0.00000, 0.00000,
0.80000, 0.00000, 0.00000,
0.80000, 0.00000, 0.00000,
0.80000, 0.00000, 0.00000,
0.80000, 0.00000, 0.00000,
0.80000, 0.00000, 0.00000,
0.80000, 0.00000, 0.00000,
0.86667, 0.00000, 0.00000,
0.86667, 0.00000, 0.00000,
0.86667, 0.00000, 0.00000,
0.86667, 0.00000, 0.00000,
0.86667, 0.00000, 0.00000,
0.86667, 0.00000, 0.00000,
0.86667, 0.00000, 0.00000,
0.86667, 0.00000, 0.00000,
0.93333, 0.00000, 0.00000,
0.93333, 0.00000, 0.00000,
0.93333, 0.00000, 0.00000,
0.93333, 0.00000, 0.00000,
0.93333, 0.00000, 0.00000,
0.93333, 0.00000, 0.00000,
0.93333, 0.00000, 0.00000,
0.93333, 0.00000, 0.00000,
1.00000, 0.00000, 0.00000,
1.00000, 0.00000, 0.00000,
1.00000, 0.00000, 0.00000,
1.00000, 0.00000, 0.00000,
1.00000, 0.00000, 0.00000,
1.00000, 0.00000, 0.00000,
1.00000, 0.00000, 0.00000,
1.00000, 0.00000, 0.00000,
1.00000, 0.00000, 0.00000,
1.00000, 0.00000, 0.00000,
1.00000, 0.00000, 0.00000,
1.00000, 0.00000, 0.00000,
1.00000, 0.00000, 0.00000,
1.00000, 0.00000, 0.00000,
1.00000, 0.06667, 0.00000,
1.00000, 0.06667, 0.00000,
1.00000, 0.13333, 0.00000,
1.00000, 0.13333, 0.00000,
1.00000, 0.20000, 0.00000,
1.00000, 0.20000, 0.00000,
1.00000, 0.26667, 0.00000,
1.00000, 0.26667, 0.00000,
1.00000, 0.33333, 0.00000,
1.00000, 0.33333, 0.00000,
1.00000, 0.40000, 0.00000,
1.00000, 0.40000, 0.00000,
1.00000, 0.46667, 0.00000,
1.00000, 0.46667, 0.00000,
1.00000, 0.53333, 0.00000,
1.00000, 0.53333, 0.00000,
1.00000, 0.60000, 0.00000,
1.00000, 0.60000, 0.00000,
1.00000, 0.66667, 0.00000,
1.00000, 0.66667, 0.00000,
1.00000, 0.73333, 0.00000,
1.00000, 0.73333, 0.00000,
1.00000, 0.80000, 0.00000,
1.00000, 0.80000, 0.00000,
1.00000, 0.86667, 0.00000,
1.00000, 0.86667, 0.00000,
1.00000, 0.93333, 0.00000,
1.00000, 0.93333, 0.00000,
1.00000, 1.00000, 0.00000,
1.00000, 1.00000, 0.00000,
1.00000, 1.00000, 0.00000,
1.00000, 1.00000, 0.00000,
1.00000, 1.00000, 0.06667,
1.00000, 1.00000, 0.06667,
1.00000, 1.00000, 0.13333,
1.00000, 1.00000, 0.13333,
1.00000, 1.00000, 0.20000,
1.00000, 1.00000, 0.20000,
1.00000, 1.00000, 0.26667,
1.00000, 1.00000, 0.26667,
1.00000, 1.00000, 0.33333,
1.00000, 1.00000, 0.33333,
1.00000, 1.00000, 0.40000,
1.00000, 1.00000, 0.40000,
1.00000, 1.00000, 0.46667,
1.00000, 1.00000, 0.46667,
1.00000, 1.00000, 0.53333,
1.00000, 1.00000, 0.53333,
1.00000, 1.00000, 0.60000,
1.00000, 1.00000, 0.60000,
1.00000, 1.00000, 0.66667,
1.00000, 1.00000, 0.66667,
1.00000, 1.00000, 0.73333,
1.00000, 1.00000, 0.73333,
1.00000, 1.00000, 0.80000,
1.00000, 1.00000, 0.80000,
1.00000, 1.00000, 0.86667,
1.00000, 1.00000, 1.00000
    };

    colourmap = new int[maxComponent];

    for (index = 0; index < maxComponent; ++index)
    {
      red = (int) ((maxComponent-1.0)*values[index*numColourComponents]);
      green = (int) ((maxComponent-1.0)*values[index*numColourComponents +1]);
      blue = (int) ((maxComponent-1.0)*values[index*numColourComponents  +2]);
      
      pixel = ((alpha & 0xff) << 24) | ((red & 0xff) << 16) | 
                  ((green & 0xff) << 8) | (blue & 0xff);

      colourmap[index] = pixel;
    }
  };
};
