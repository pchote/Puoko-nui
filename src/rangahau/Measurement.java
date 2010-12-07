/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

package rangahau;

/**
 * Represents a luminous object or the sky measured photometrically on a single
 * image.
 *
 * @author Mike Reid.
 */
public class Measurement {

  /**
   * The maximum aperture radius (in pixels) for which photometry was measured.
   */
  private int maxRadius;

  /**
   * An array containing the 'raw' photometric counts (without sky/background
   * subtracted) within circles of increasing radius. The elements of the array
   * are the counts at an aperture pixel radius equivalent to the element index
   * (that is, element[2] contains the raw counts for a circle with a radius of
   * 2 pixels).
   */
  private double rawCounts[];

  /**
   * An array containing the number of pixels within circles of increasing radius.
   * The elements of the array are the number of pixels within an aperture
   * pixel radius equivalent to the element index (that is, element[2]
   * contains the number if pixels within a circle of radius of 2 pixels).
   */
  private double numPixels[];

  /**
   * An array containing the estimated number of counts due to sky background
   * within circles of increasing radius. The elements of the array are the
   * sky background count within an aperture pixel radius equivalent to the
   * element index (that is, element[2] contains the number counts due to sky
   * background within a circle of radius of 2 pixels).
   */
  private double backgroundCounts[];

  /**
   * The x-coordinate (in pixels) of the centre of the star/object.
   */
  private double x;

  /**
   * The y-coordinate (in pixels) of the centre of the star/object.
   */
  private double y;

  /**
   * Estimated total flux of the object (with sky background removed).
   */
  private double flux;

  /**
   * Flux of the centre-most pixel of the object (not background subtracted).
   */
  private double centralFlux;
  /**
   * The name of the frame (exposure) that the object was measured on.
   */
  private String frame;

  /**
   * The time the exposure was started, measured in milliseconds since the
   * Epoch.
   */
  private long startTimeMillis;

  /**
   * The time the exposure finished, nmeasured in milliseconds since the
   * Epoch.
   */
  private long endTimeMillis;

  /**
   * The per-pixel background flux. 
   */
  private double background;

  public Measurement() {
  }

  /**
   * The maximum aperture radius (in pixels) for which photometry was measured.
   * @return the maxRadius
   */
  public int getMaxRadius() {
    return maxRadius;
  }

  /**
   * The maximum aperture radius (in pixels) for which photometry was measured.
   * @param maxRadius the maxRadius to set
   */
  public void setMaxRadius(int maxRadius) {
    this.maxRadius = maxRadius;
  }

  /**
   * An array containing the 'raw' photometric counts (without sky/background
   * subtracted) within circles of increasing radius. The elements of the array
   * are the counts at an aperture pixel radius equivalent to the element index
   * (that is, element[2] contains the raw counts for a circle with a radius of
   * 2 pixels).
   * @return the rawCounts
   */
  public double[] getRawCounts() {
    return rawCounts;
  }

  /**
   * An array containing the 'raw' photometric counts (without sky/background
   * subtracted) within circles of increasing radius. The elements of the array
   * are the counts at an aperture pixel radius equivalent to the element index
   * (that is, element[2] contains the raw counts for a circle with a radius of
   * 2 pixels).
   * @param rawCounts the rawCounts to set
   */
  public void setRawCounts(double[] rawCounts) {
    this.rawCounts = rawCounts;
  }

  /**
   * An array containing the number of pixels within circles of increasing radius.
   * The elements of the array are the number of pixels within an aperture
   * pixel radius equivalent to the element index (that is, element[2]
   * contains the number if pixels within a circle of radius of 2 pixels).
   * @return the numPixels
   */
  public double[] getNumPixels() {
    return numPixels;
  }

  /**
   * An array containing the number of pixels within circles of increasing radius.
   * The elements of the array are the number of pixels within an aperture
   * pixel radius equivalent to the element index (that is, element[2]
   * contains the number if pixels within a circle of radius of 2 pixels).
   * @param numPixels the numPixels to set
   */
  public void setNumPixels(double[] numPixels) {
    this.numPixels = numPixels;
  }

  /**
   * An array containing the estimated number of counts due to sky background
   * within circles of increasing radius. The elements of the array are the
   * sky background count within an aperture pixel radius equivalent to the
   * element index (that is, element[2] contains the number counts due to sky
   * background within a circle of radius of 2 pixels).
   * @return the backgroundCounts
   */
  public double[] getBackgroundCounts() {
    return backgroundCounts;
  }

  /**
   * An array containing the estimated number of counts due to sky background
   * within circles of increasing radius. The elements of the array are the
   * sky background count within an aperture pixel radius equivalent to the
   * element index (that is, element[2] contains the number counts due to sky
   * background within a circle of radius of 2 pixels).
   * @param backgroundCounts the backgroundCounts to set
   */
  public void setBackgroundCounts(double[] backgroundCounts) {
    this.backgroundCounts = backgroundCounts;
  }

  /**
   * The x-coordinate (in pixels) of the centre of the star/object.
   * @return the x
   */
  public double getX() {
    return x;
  }

  /**
   * The x-coordinate (in pixels) of the centre of the star/object.
   * @param x the x to set
   */
  public void setX(double x) {
    this.x = x;
  }

  /**
   * The y-coordinate (in pixels) of the centre of the star/object.
   * @return the y
   */
  public double getY() {
    return y;
  }

  /**
   * The y-coordinate (in pixels) of the centre of the star/object.
   * @param y the y to set
   */
  public void setY(double y) {
    this.y = y;
  }

  /**
   * The frame (exposure) that the object was measured on.
   * @return the frame
   */
  public String getFrame() {
    return frame;
  }

  /**
   * The frame (exposure) that the object was measured on.
   * @param frame the frame to set
   */
  public void setFrame(String frame) {
    this.frame = frame;
  }

  /**
   * Estimated total flux of the object (with sky background removed).
   * @return the flux
   */
  public double getFlux() {
    return flux;
  }

  /**
   * Estimated total flux of the object (with sky background removed).
   * @param flux the flux to set
   */
  public void setFlux(double flux) {
    this.flux = flux;
  }

  /**
   * Flux of the centre-most pixel of the object.
   * @return the centralFlux
   */
  public double getCentralFlux() {
    return centralFlux;
  }

  /**
   * Flux of the centre-most pixel of the object.
   * @param centralFlux the centralFlux to set
   */
  public void setCentralFlux(double centralFlux) {
    this.centralFlux = centralFlux;
  }

  /**
   * The time the exposure was started, measured in milliseconds since the
   * Epoch.
   * @return the startTimeMillis
   */
  public long getStartTimeMillis() {
    return startTimeMillis;
  }

  /**
   * The time the exposure was started, measured in milliseconds since the
   * Epoch.
   * @param startTimeMillis the startTimeMillis to set
   */
  public void setStartTimeMillis(long startTimeMillis) {
    this.startTimeMillis = startTimeMillis;
  }

  /**
   * The time the exposure finished, nmeasured in milliseconds since the
   * Epoch.
   * @return the endTimeMillis
   */
  public long getEndTimeMillis() {
    return endTimeMillis;
  }

  /**
   * The time the exposure finished, nmeasured in milliseconds since the
   * Epoch.
   * @param endTimeMillis the endTimeMillis to set
   */
  public void setEndTimeMillis(long endTimeMillis) {
    this.endTimeMillis = endTimeMillis;
  }

  /**
   * The per-pixel background flux.
   * @return the background
   */
  public double getBackground() {
    return background;
  }

  /**
   * The per-pixel background flux.
   * @param background the background to set
   */
  public void setBackground(double background) {
    this.background = background;
  }

}
