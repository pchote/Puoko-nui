/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

/******************************************************************************
*
*  Module    :  ImageCanvas
*  File      :  ImageCanvas.java
*  Language  :  Java 1.2
*  Author    :  Mike Reid
*  Copyright :  Mike Reid, 2000-
*  Created   :  13 December 2000
*  Modified  :  4 May 2004
*
*  Overview
*  ========
*
*    The ImageCanvas class displays an image.
*
******************************************************************************/

package rangahau;

import java.awt.BasicStroke;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.image.BufferedImage;

import javax.swing.JComponent;


/**
 *  The ImageCanvas class displays an image.
 */
public class ImageCanvas extends JComponent
{
  /**
   * image to display on the canvas.
   * @serial
   */
  BufferedImage imageBuffer;

  /**
   * True if pixels should be drawn where horizontal scale is the same as the
   * vertical scale (no stretching in one direction only).
   */
  private boolean fixedAspectRatio;

  /**
   * The application model holding marker information.
   */
  Model model = null;
  
  //*******************************************************************
  /**
   * Returns the image which the ImageCanvas displays.
   * @return the image which is displayed (which may be null).
   */
  public BufferedImage getImage()
  {
    return imageBuffer;
  };
  
  //*******************************************************************
  /**
   * Sets the image which this ImageCanvas should display.
   * @param image - the image to display.
   */
  public void setImage(BufferedImage image)
  {
    if (imageBuffer != null) {
      imageBuffer.getGraphics().dispose();
    }
    
    imageBuffer = image;

    // If the image has changed size then we may need to resize the canvas.
    if (image != null)
    {
      if ( (imageBuffer == null) || (imageBuffer.getWidth(null) != image.getWidth(null)) 
            || (imageBuffer.getHeight(null) != image.getHeight(null)) )
      { 
        setPreferredSize(new Dimension(image.getWidth(null), image.getHeight(null)));
        setSize(new Dimension(image.getWidth(null), image.getHeight(null)));
    
        // Since we have changed the image we should revalidate the canvas
        // at the first opportunity.
        revalidate();
      }
    }
  };
  
  //*******************************************************************
  // Program entry point.
  public static void main(String argv[])
  {
    javax.swing.JFrame frame = new javax.swing.JFrame("ImageCanvas test");
    ImageCanvas canvas = new ImageCanvas();
    canvas.setSize(300, 400);
  
    frame.getContentPane().add(canvas, java.awt.BorderLayout.CENTER);
//    frame.getContentPane().add(new JLabel("ImageCanvas Test"), BorderLayout.CENTER);

    // Set the application to exit when the frame is closed.
    frame.setDefaultCloseOperation(javax.swing.JFrame.EXIT_ON_CLOSE);
    frame.pack();
    frame.setVisible(true);
  }  

  //*******************************************************************
  /**
   * Constructs an ImageCanvas object with no associated image.
   */
  public ImageCanvas()
  {
    setPreferredSize(new java.awt.Dimension(200, 150));
    
    revalidate();
  }

  //*******************************************************************
  /**
   * Constructs an ImageCanvas object with an associated image.
   */
  public ImageCanvas(BufferedImage image)
  {
    imageBuffer = image;
    
    if (image != null) {
      setPreferredSize(new Dimension(image.getWidth(null), image.getHeight(null)));
      setSize(new Dimension(image.getWidth(null), image.getHeight(null)));
    } else
      setPreferredSize(new Dimension(200, 150));
    
    revalidate();
  }
  
  //*******************************************************************
  // Update the enitre display area (without doing the default clear first).
  /*public void update(Graphics graphics)
  {
    paint(graphics);
  };
*/
  
  //*******************************************************************
  /** 
   * Paints the ImageCanvas. 
   * @param graphics - the graphics context to paint on to.
   */
  @Override
  public void paintComponent(Graphics graphics)
  {	    
    // Draw the image into the buffer.
    if (imageBuffer != null)
    {
      if (fixedAspectRatio) {
        // Draw with the horizontal and vertical scales the same.
        double horizontalScale = imageBuffer.getWidth(null) / (double) getSize().width;
        double verticalScale = imageBuffer.getHeight(null) / (double) getSize().height;

        // The scale is the one that fits all of the image on (the larger of the
        // two numbers here).
        int displayWidth = getSize().width;
        int displayHeight = getSize().height;

        if (horizontalScale > verticalScale) {
          displayHeight *= verticalScale / horizontalScale; // scale the height so that the horizontal scale is preserved.
        } else {
          displayWidth *= horizontalScale / verticalScale; // scale the height so that the horizontal scale is preserved.
        }

        int horizontalOffset = (getSize().width - displayWidth) /2;
        graphics.drawImage(imageBuffer, horizontalOffset, 0, displayWidth + horizontalOffset, displayHeight,
                                        0, 0, imageBuffer.getWidth(null), imageBuffer.getHeight(null), null);
      } else {
        // Allow the horizontal and vertical scales to change so the image
        // is strecthed to the edges of the screen.

        // Convert into a Graphcis2D graphics context.
        //Graphics2D gc = (Graphics2D) graphics;
        //graphics.setClip(0, 0, getWidth(), getHeight());
        graphics.drawImage(imageBuffer, 0, 0, getSize().width, getSize().height,
                                        0, 0, imageBuffer.getWidth(null), imageBuffer.getHeight(null), null);
      }
      
      showMarkers(graphics);
    } else {
      // Let the parent class paint the component.
      super.paintComponent(graphics);   
    }
  }
  
    /**
     * Shows any markers (for the target and comparison objects) on the 
     * displayed image.
     * 
     * @param graphics the graphics object to draw into.
     */
    public void showMarkers(Graphics graphics) { 
        Graphics2D graphics2D = (Graphics2D) graphics;
        
        // If there is no image we cannot overlay markers on it.
        if (imageBuffer == null) {
            return;
        }
        
        // If there is no model there will be no markers to draw.
        if (model == null) {
            return;
        }
        
        // Draw and label the target object if the user has provided a target
        // location.
        if (model.getTargetLocation() != null) {
          // Convert the location of the marker into the imageCanvas coordinate
          // system and place a marker and text there.
          float red = 1.0f;
          float green = 1.0f;
          float blue = 0.0f;
          float opacity = 0.8f;
          Color markerBackgroundColor = new Color(red, green, blue, opacity);
           
          red = 0.0f;
          green = 0.0f;
          blue = 0.0f;
          opacity = 0.8f;         
          Color markerColor = new Color(red, green, blue, opacity);
          
          int x = (int) Math.round(model.getTargetLocation().getX()/imageBuffer.getWidth() * getWidth());
          
          // Note: that we have to reverse the y coordinate when moving from 
          // image coordinates into image canvas coordinates.
          int y = (int) Math.round((imageBuffer.getHeight() - model.getTargetLocation().getY())/imageBuffer.getHeight() * getHeight());
                             
          // Draw a small cross on the image canvas. Note we don't
          // draw on the image itself as the image will be scaled and we don't
          // want our text label to be scaled.
          String markerLabel = "Target";
          
          int markerHalfLength = 3;
          int labelXOffset = 10;
          int labelYOffset = 10;
         
          // Draw the marker background. Note we can use thicker stroke for the
          // marker but this doesn't change the font width so we use a 'drop
          // shadow' effect for the marker label.
          graphics2D.setStroke(new BasicStroke(5.0f));
          graphics2D.setColor(markerBackgroundColor);
          graphics2D.drawLine(x - markerHalfLength, y, x + markerHalfLength, y);
          graphics2D.drawLine(x, y - markerHalfLength, x, y + markerHalfLength);
          graphics2D.drawString(markerLabel, x + labelXOffset +1, y + labelYOffset +2);
          
          // Draw the marker and its label over the marker background.
          graphics2D.setStroke(new BasicStroke(2.0f));
          graphics2D.setColor(markerColor);
          graphics2D.drawLine(x - markerHalfLength, y, x + markerHalfLength, y);
          graphics2D.drawLine(x, y - markerHalfLength, x, y + markerHalfLength);
          graphics2D.drawString(markerLabel, x + labelXOffset, y + labelYOffset);
        }
        
        // Draw and label the comparison object if the user has provided a 
        // comparison location.
        if (model.getComparisonLocation() != null) {
          // Convert the location of the marker into the imageCanvas coordinate
          // system and place a marker and text there.
          float red = 1.0f;
          float green = 1.0f;
          float blue = 0.0f;
          float opacity = 0.8f;          
          Color markerBackgroundColor = new Color(red, green, blue, opacity);
           
          red = 0.0f;
          green = 0.8f;
          blue = 0.0f;
          opacity = 0.8f; 
          Color markerColor = new Color(red, green, blue, opacity);
          
          int x = (int) Math.round(model.getComparisonLocation().getX()/imageBuffer.getWidth() * getWidth());
          
          // Note: that we have to reverse the y coordinate when moving from 
          // image coordinates into image canvas coordinates.
          int y = (int) Math.round((imageBuffer.getHeight() - model.getComparisonLocation().getY())/imageBuffer.getHeight() * getHeight());
          
          // Draw a small circle cross on the canvas. Note we don't
          // draw on the image itself as the image will be scaled and we don't
          // want our text label to be scaled.
          String markerLabel = "Comparison";
          int markerHalfLength = 3;
          int labelXOffset = 10;
          int labelYOffset = 10;
          
          // Draw the marker background. Note we can use thicker stroke for the
          // marker but this doesn't change the font width so we use a 'drop
          // shadow' effect for the marker label.
          graphics2D.setStroke(new BasicStroke(5.0f));
          graphics2D.setColor(markerBackgroundColor);
          graphics2D.drawLine(x - markerHalfLength, y, x + markerHalfLength, y);
          graphics2D.drawLine(x, y - markerHalfLength, x, y + markerHalfLength);
          graphics2D.drawString(markerLabel, x + labelXOffset +1, y + labelYOffset +1);
          
          // Draw the marker and its label over the marker background.
          graphics2D.setStroke(new BasicStroke(2.0f));
          graphics2D.setColor(markerColor);
          graphics2D.drawLine(x - markerHalfLength, y, x + markerHalfLength, y);
          graphics2D.drawLine(x, y - markerHalfLength, x, y + markerHalfLength);
          graphics2D.drawString(markerLabel, x + labelXOffset, y + labelYOffset);
        }
    }
    
    public Model getModel() {
        return model;
    }
    
    public void setModel(Model model) {
        this.model = model;
    }

  /**
   * True if pixels should be drawn where horizontal scale is the same as the
   * vertical scale (no stretching in one direction only).
   * @return the fixedAspectRatio
   */
  public /**
   * True if pixels should be drawn where horizontal scale is the same as the
   * vertical scale (no stretching in one direction only).
   */
  boolean isFixedAspectRatio() {
    return fixedAspectRatio;
  }

  /**
   * True if pixels should be drawn where horizontal scale is the same as the
   * vertical scale (no stretching in one direction only).
   * @param fixedAspectRatio the fixedAspectRatio to set
   */
  public void setFixedAspectRatio(boolean fixedAspectRatio) {
    this.fixedAspectRatio = fixedAspectRatio;
  }

};


