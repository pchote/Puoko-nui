/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

package rangahau.plot;

import java.awt.geom.Point2D;

/**
 * Represents a horizontal ('absisca') or vertical ('ordinate') axis of a plot.
 *
 * @author Mike Reid
 */
public interface Axis {

  /**
   * Describes the orientation and direction (from the min value to the max
   * value) of the axis.
   */
  public enum Orientation {
    VERTICAL_UP,
    VERTICAL_DOWN,
    HORIZONTAL_LEFT,
    HORIZONTAL_RIGHT,
    ANGLED
  };

  /**
   * Get the minimum data value that can be displayed by the axis (that is,
   * the lower data value limit). This lower limit is intended to be set to a
   * 'nice' value convenient for humans to read. Note that this may not be the
   * same as the lower data value that is actually displayed on the axis.
   *
   * @return the lower data value limit.
   */
  public Number getMinLimit();

  /**
   * Sets the minimum data value that can be displayed by the axis (that is,
   * the lower data value limit). This lower limit is intended to be set to a
   * 'nice' value convenient for humans to read. Note that this may not be the
   * same as the lower data value that is actually displayed on the axis.
   *
   * @param minLimit the lower data value limit.
   *
   * @throws IllegalArgumentException if minLimit is null.
   */
  public void setMinLimit(Number minLimit);

  /**
   * Get the maximum data value that can be displayed by the axis (that is,
   * the upper data value limit). This upper limit is intended to be set to a
   * 'nice' value convenient for humans to read. Note that this may not be the
   * same as the upper data value that is actually displayed on the axis.
   *
   * @return the upper data value limit.
   */
  public Number getMaxLimit();

  /**
   * Set the maximum data value that can be displayed by the axis (that is,
   * the upper data value limit). This upper limit is intended to be set to a
   * 'nice' value convenient for humans to read. Note that this may not be the
   * same as the upper data value that is actually displayed on the axis.
   *
   * @param maxLimit the upper data value limit.
   *
   * @throws IllegalArgumentException if maxLimit is null.
   */
  public void setMaxLimit(Number maxLimit);

  /**
   * The range between the min and max data limits.
   *
   * @return the range of data that can be displayed on the axis.
   */
  public Number getRange();

  /**
   * The minimum data value that is actually displayed on the axis.
   *
   * @return the minimum data value that is displayed.
   */
  public Number getMinDataValue();

  /**
   * The minimum data value that is actually displayed on the axis.
   *
   * @param minData the minimum data value that is displayed.
   *
   * @throws IllegalArgumentException if minDataValue is null.
   */
  public void setMinDataValue(Number minDataValue);

  /**
   * The maximum data value that is actually displayed on the axis.
   *
   * @return the maximum data value that is displayed.
   */
  public Number getMaxDataValue();

  /**
   * The maximum data value that is actually displayed on the axis.
   *
   * @param maxData the maximum data value that is displayed.
   *
   * @throws IllegalArgumentException if maxDataValue is null.
   */
  public void setMaxDataValue(Number maxDataValue);

  /**
   * The range between the min and max data values.
   *
   * @return the range of data that is displayed on the axis.
   */
  public Number getDataRange();

  /**
   * The screen coordinate value of the lower axis limit ('start' of axis).
   *
   * @return the screen coordinate of the minimum end of the axis.
   */
  public Point2D getMinScreenValue();

  /**
   * Sets the screen coordinate value of the lower axis limit ('start' of axis).
   *
   * @param minScreenValue the screen coordinate of the minimum end of the axis.
   *
   * @throws IllegalArgumentException if minScreenValue is null.
   */
  public void setMinScreenValue(Point2D minScreenValue);

  /**
   * The screen coordinate value of the upper axis limit ('end' of axis).
   *
   * @return the screen coordinate of the maximum end of the axis.
   */
  public Point2D getMaxScreenValue();

  /**
   * Sets the screen coordinate value of the upper axis limit ('end' of axis).
   *
   * @param maxScreenValue the screen coordinate of the maximum end of the axis.
   *
   * @throws IllegalArgumentException is maxScreenValue is null.
   */
  public void getMaxScreenValue(Point2D maxScreenValue);

  /**
   * The length of the axis in screen coordinates.
   *
   * @return the axis screen length.
   */
  public double getScreenLength();

  /**
   * Gets a description of the axis orientation direction (from minimum value
   * to maximum value).
   *
   * @return the axis orientation.
   */
  public Orientation getOrientation();

  /**
   * Sets a description of the axis orientation direction (from minimum value
   * to maximum value).
   *
   * @param orientation the axis orientation.
   */
  public void setOrientation(Orientation orientation);

  /**
   * Gets the screen coordinate of a data value.
   *
   * @param dataValue the value in the 'data space'.
   *
   * @return the screen coordinate (lying on the axis) equivalent the data value.
   */
  public Point2D getScreenValue(Number dataValue);

  /**
   * The axis label.
   *
   * @return the axis label.
   */
  public String getLabel();

  /**
   * Sets the axis label.
   *
   * @param label the axis label.
   */
  public void getLabel(String label);

  /**
   * The axis description.
   *
   * @return the axis description.
   */
  public String getDescription();

  /**
   * Sets the axis description.
   *
   * @param description the axis description.
   */
  public void setDescription(String description);

  /**
   * An object that the user wishes to associate with the axis.
   *
   * @return the object associated with the axis by the user.
   */
  public Object getUserObject();

  /**
   * Sets an object that the user wishes to associate with the axis.
   *
   * @param object the object associated with the axis by the user.
   */
  public void getUserObject(Object object);

}
