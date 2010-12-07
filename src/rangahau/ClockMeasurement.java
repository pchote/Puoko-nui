/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

package rangahau;

/**
 * Represents a GPS time measurement and the equivalent host system time.
 * Can be used to determine the equivalent GPS time from system time or
 * vice versa. Can also be used to determine the host's system clock drift.
 *
 * @author Mike Reid
 */
public class ClockMeasurement {

  /**
   * The time measured on the hosts's system clock (in milliseconds since the Epoch).
   */
  private long systemTimeMillis;

  /**
   * The time measured from GPS (in milliseconds since the Epoch).
   */
  private long gpsTimeMillis;

  /**
   * Creates a clock measurement with no values for the system or GPS time.
   */
  public ClockMeasurement() {
  }

  /**
   * Creates a clock measurement with the specified system and GPS times.
   *
   * @param systemTime the time measured on the host's system clock (in millisecond since the Epoch).
   * @param gpsTime time time measured by GPS (in milliseconds since the Epoch).
   */
  public ClockMeasurement(long systemTime, long gpsTime) {
    this.systemTimeMillis = systemTime;
    this.gpsTimeMillis = gpsTime;
  }

  /**
   * The number of milliseconds that should be subtracted from the system time
   * to determine the equivalent GPS time.
   *
   * @return the correction between system time and GPS time.
   */
  public long systemToGpsCorrection() {
    return systemTimeMillis - gpsTimeMillis;
  }

  /**
   * The time measured on the hosts's system clock (in milliseconds since the Epoch).
   * 
   * @return the systemTimeMillis
   */
  public long getSystemTimeMillis() {
    return systemTimeMillis;
  }

  /**
   * The time measured on the hosts's system clock (in milliseconds since the Epoch).
   *
   * @param systemTimeMillis the systemTimeMillis to set
   */
  public void setSystemTimeMillis(long systemTimeMillis) {
    this.systemTimeMillis = systemTimeMillis;
  }

  /**
   * The time measured from GPS (in milliseconds since the Epoch).
   *
   * @return the gpsTimeMillis
   */
  public long getGpsTimeMillis() {
    return gpsTimeMillis;
  }

  /**
   * The time measured from GPS (in milliseconds since the Epoch).
   *
   * @param gpsTimeMillis the gpsTimeMillis to set
   */
  public void setGpsTimeMillis(long gpsTimeMillis) {
    this.gpsTimeMillis = gpsTimeMillis;
  }
}
