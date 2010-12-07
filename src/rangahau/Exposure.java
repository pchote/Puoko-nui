/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

package rangahau;

/**
 * Represents a single exposure (CCD image) and related information.
 *
 * @author Mike Reid.
 */
public class Exposure {

  /**
   * The exposure start time according to the acquiring system's computer clock. 
   */
  private long systemStartTimeMillis;

  /**
   * The exposure finish time according to the acquiring system's computer clock.
   */
  private long systemEndTimeMillis;

  /**
   * The exposure start time according to GPS.
   */
  private long gpsStartTimeMillis;

  /**
   * The exposure end time according to GPS.
   */
  private long gpsEndTimeMillis;

  /**
   * The pixels that make up the image.
   */
  private int[][] image;

  /**
   * The number of pixel rows in the image.
   */
  private int height;

  /**
   * The number of pixel columns in the image.
   */
  private int width;

  /**
   * The type of exposure, examples are: TARGET, FLAT, DARK.
   */
  private String type;

  /**
   * The nominal duration of the exposure (in millis) which was specified to
   * the GPS synchronisation pulse card ('external timing card'/xtc).
   */
  private long durationMillis;

  /**
   * The name of the target object for frames of type TARGET.
   */
  private String target;

  /**
   * The running sequence number of the exposure relative to other exposures
   * taken in the same observing ession.
   */
  private int sequenceNumber;

  /**
   * The exposure start time according to the acquiring system's computer clock.
   * @return the systemStartTimeMillis
   */
  public long getSystemStartTimeMillis() {
    return systemStartTimeMillis;
  }

  /**
   * The exposure start time according to the acquiring system's computer clock.
   * @param systemStartTimeMillis the systemStartTimeMillis to set
   */
  public void setSystemStartTimeMillis(long systemStartTimeMillis) {
    this.systemStartTimeMillis = systemStartTimeMillis;
  }

  /**
   * The exposure finish time according to the acquiring system's computer clock.
   * @return the systemEndTimeMillis
   */
  public long getSystemEndTimeMillis() {
    return systemEndTimeMillis;
  }

  /**
   * The exposure finish time according to the acquiring system's computer clock.
   * @param systemEndTimeMillis the systemEndTimeMillis to set
   */
  public void setSystemEndTimeMillis(long systemEndTimeMillis) {
    this.systemEndTimeMillis = systemEndTimeMillis;
  }

  /**
   * The exposure start time according to GPS.
   * @return the gpsStartTimeMillis
   */
  public long getGpsStartTimeMillis() {
    return gpsStartTimeMillis;
  }

  /**
   * The exposure start time according to GPS.
   * @param gpsStartTimeMillis the gpsStartTimeMillis to set
   */
  public void setGpsStartTimeMillis(long gpsStartTimeMillis) {
    this.gpsStartTimeMillis = gpsStartTimeMillis;
  }

  /**
   * The exposure end time according to GPS.
   * @return the gpsEndTimeMillis
   */
  public long getGpsEndTimeMillis() {
    return gpsEndTimeMillis;
  }

  /**
   * The exposure end time according to GPS.
   * @param gpsEndTimeMillis the gpsEndTimeMillis to set
   */
  public void setGpsEndTimeMillis(long gpsEndTimeMillis) {
    this.gpsEndTimeMillis = gpsEndTimeMillis;
  }

  /**
   * The pixels that make up the image.
   * @return the image
   */
  public int[][] getImage() {
    return image;
  }

  /**
   * The pixels that make up the image.
   * @param image the image to set
   */
  public void setImage(int[][] image) {
    this.image = image;
  }

  /**
   * The type of exposure, examples are: TARGET, FLAT, DARK.
   * @return the type
   */
  public String getType() {
    return type;
  }

  /**
   * The type of exposure, examples are: TARGET, FLAT, DARK.
   * @param type the type to set
   */
  public void setType(String type) {
    this.type = type;
  }

  /**
   * The nominal duration of the exposure (in millis) which was specified to
   * the GPS synchronisation pulse card ('external timing card'/xtc).
   * @return the durationMillis
   */
  public long getDurationMillis() {
    return durationMillis;
  }

  /**
   * The nominal duration of the exposure (in millis) which was specified to
   * the GPS synchronisation pulse card ('external timing card'/xtc).
   * @param durationMillis the durationMillis to set
   */
  public void setDurationMillis(long durationMillis) {
    this.durationMillis = durationMillis;
  }

  /**
   * The name of the target object for frames of type TARGET.
   * @return the target
   */
  public String getTarget() {
    return target;
  }

  /**
   * The name of the target object for frames of type TARGET.
   * @param target the target to set
   */
  public void setTarget(String target) {
    this.target = target;
  }

  /**
   * The running sequence number of the exposure relative to other exposures
   * taken in the same observing ession.
   * @return the sequenceNumber
   */
  public int getSequenceNumber() {
    return sequenceNumber;
  }

  /**
   * The running sequence number of the exposure relative to other exposures
   * taken in the same observing ession.
   * @param sequenceNumber the sequenceNumber to set
   */
  public void setSequenceNumber(int sequenceNumber) {
    this.sequenceNumber = sequenceNumber;
  }

  /**
   * The number of pixel rows in the image.
   * @return the height
   */
  public int getHeight() {
    return height;
  }

  /**
   * The number of pixel rows in the image.
   * @param height the height to set
   */
  public void setHeight(int height) {
    this.height = height;
  }

  /**
   * The number of pixel columns in the image.
   * @return the width
   */
  public int getWidth() {
    return width;
  }

  /**
   * The number of pixel columns in the image.
   * @param width the width to set
   */
  public void setWidth(int width) {
    this.width = width;
  }
}
