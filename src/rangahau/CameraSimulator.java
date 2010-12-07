/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */

package rangahau;

import java.awt.Color;
import java.awt.Font;
import java.awt.Graphics2D;
import java.awt.geom.Rectangle2D;
import java.awt.image.BufferedImage;
import java.awt.image.ColorModel;
import java.awt.image.DataBufferInt;
import java.awt.image.DirectColorModel;
import java.awt.image.Raster;
import java.awt.image.WritableRaster;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;
import java.util.Random;

/**
 *
 * @author reid
 */
public class CameraSimulator implements Camera {
    
    /**
     * True if the camera has been 'opened' for use.
     */
    boolean isOpen = false;
    
    /**
     * The time (in milliseconds since the Epoch) when the last image was 
     * considered 'acquired'.
     */
    long lastImageTime = 0;
    
    /**
     * The time (in milliseconds since the Epoch) where the image was considered
     * 'started'.
     */
    long imageStartTime = 0;
    
    /**
     * The simulated exposureTime (in seconds).
     */
    private int exposureTime = 1;

    /**
     * Opens the camera and prepares it for use.
     * 
     * @parameter identifier identifies which camera should be opened for use.
     */
    public void open(String identifier) {
      isOpen = true;   
    }
    
    /**
     * Closes the camera and releases any resources it was using.
     */
    public void close() {
        isOpen = false;
    }
    
    /**
     * Gets the width (number of columns) of the images produced by the camera 
     * (in pixels). 
     * 
     * @return the width (in pixels) of the camera's images. 
     */
    public int getWidth() {
        if (!isOpen) {
            throw new IllegalStateException("Cannot get the width as the camera is not open.");
        }
        
        return 1024;
    }
    
    /**
     * Gets the height (number of rows) of the images produced by the camera 
     * (in pixels).
     * 
     * @return the height (in pixels) of the camera's images.
     */
    public int getHeight() {
        if (!isOpen) {
            throw new IllegalStateException("Cannot get the height as the camera is not open.");
        }
        
        return 1024;
    }
    
    /**
     * Starts the acquisition of an image with the camera. 
     */
    public void startAcquisition() {
        if (!isOpen) {
            throw new IllegalStateException("Cannot start acquisition as the camera is not open.");
        }        
    }

    /**
     * Stops acquisition images with the camera.
     */
    public void stopAcquisition() {
        if (!isOpen) {
            throw new IllegalStateException("Cannot stop acquisition as the camera is not open.");
        }
    }


    /**
     * Indicates whether the camera acquisition has produced an image yet.
     * 
     * @return true if an image is ready to be read from the camera, false
     *         if the acquisition is not complete yet.
     */
    public boolean isImageReady() {
        if (!isOpen) {
            throw new IllegalStateException("Cannot determine whether an image is ready as the camera is not open.");
        }
         
         // Simulate an acquisition delay by saying an acquisition falls at the
         // end of a certain number of seconds.
         long millisPerSecond = 1000;  // the number of milliseconds in one second.

//         Calendar now = Calendar.getInstance();
//         int currentSecond = now.get(Calendar.SECOND);

         // Last image time holds the time the last image was read-out, and when
         // the current image was started.
         //imageStartTime = lastImageTime;

         // Calculate the time the image currently being acquired is expected.
         if (lastImageTime == 0) {
           long now = System.currentTimeMillis();
           lastImageTime = now - (now % (10*millisPerSecond)); // Initialise lastImageTime to a 10-second boundary.
         }

         long nextTime = lastImageTime + (millisPerSecond*exposureTime);

         if (System.currentTimeMillis() >= nextTime) {
           System.out.println("CameraSimulator.isImageReady() : exposureTime = " + exposureTime);
           imageStartTime = lastImageTime;
           lastImageTime = nextTime;

           return true;
         }

         return false;
    }
    
    /**
     * Obtains the image pixels obtained from the last acquisition and 
     * places them in the array pointed to by pixels. Each element of the
     * pixels array contains a single pixel.
     * 
     * @param pixels the destination of the last image acquired by the camera.
     * 
     * @throws IllegalArgumentException if pixels is null.
     * @throws IllegalArgumentException if pixels does not have a number of 
     *         elements which is the same as getWidth()*getHeight() or greater.
     */
    public void getImage(int[] pixels) {
        if (!isOpen) {
            throw new IllegalStateException("Cannot get an image as the camera is not open.");
        }
        
        if (pixels == null) {
            throw new IllegalStateException("Cannot get an image as the destination pixels given was null.");            
        }
        
        int width = getWidth();
        int height = getHeight();
        
        int pixelsRequired = width*height;
        if (pixels.length < pixelsRequired) {
            throw new IllegalArgumentException("Cannot get the image as the destination pixels array is too small (it has " 
                                               + pixels.length + " elements when this camera has images with " + pixelsRequired + " pixels).");
        }
        
        Random random = new Random();
        
        // Write some random data to the image.
        int destinationPixel = 0;
        int noiseAmplitude = 300;
        int value = 0;
        for (int row = 0; row < height; ++row) {
            for (int column = 0; column < width; ++column) {
                value = 2*row + column + random.nextInt(noiseAmplitude);
                if (value < 0) {
                    value = 0;
                }
                pixels[destinationPixel] = value;
                ++destinationPixel;
            }
        }
        
        // Create a buffered image from these pixels (note they are intensities
        // not RGB, but this won't affect our processing here). We'll use the buffered
        // image to write the image start time into the image (for testing
        // purposes).
        DataBufferInt dataBuffer = new DataBufferInt(pixels, pixels.length);
        
        final int scanlineStride = width;
        int[] bandMasks = {0xFF0000, 0xFF00, 0xFF}; // Masks applied to each pixel to extract RGB colour components.
        WritableRaster raster = Raster.createPackedRaster(dataBuffer, width, height, scanlineStride, bandMasks, null);
        ColorModel colourModel = new DirectColorModel(32, 0xFF0000, 0xFF00, 0xFF);
        BufferedImage image = new BufferedImage(colourModel, raster, false, null);
        
        // Now write the start time of the image into the image.
        Graphics2D graphics = (Graphics2D) image.getGraphics();
        SimpleDateFormat formatter = new SimpleDateFormat("yyyy-MM-dd hh:mm:ss.SSS");
        graphics.setPaint(new Color(0xFF,0xFF,0xFF));
        Font font = new Font("System", Font.BOLD, 24);        
        graphics.setFont(font);
        
        String imageStart = "Started at : " + formatter.format(new Date(imageStartTime));
        
        Rectangle2D bounds = graphics.getFontMetrics().getStringBounds(imageStart, graphics);
 
        graphics.translate(width/2 - bounds.getWidth()/2, height/2 - bounds.getHeight()/2);
        graphics.scale(1.0, -1.0);

        graphics.drawString(imageStart, 0, 0);
    }
    
    /**
     * Removes all images that may be buffered on the camera. The next call to
     * isImageReady() should return false, although this not strictly true if
     * a camera is acquiring images in the background.
     */
    public void purgeImages() {
        // The camera simulator doesn't buffer images, so we don't need to do
        // anything.
    }

  /**
   * The simulated exposureTime (in seconds).
   * @return the exposureTime
   */
  public int getExposureTime() {
    return exposureTime;
  }

  /**
   * The simulated exposureTime (in seconds).
   * @param exposureTime the exposureTime to set
   */
  public void setExposureTime(int exposureTime) {
    this.exposureTime = exposureTime;
  }
}
