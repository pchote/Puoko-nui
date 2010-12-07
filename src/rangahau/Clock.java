/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

package rangahau;

import java.util.ArrayList;
import java.util.List;

/**
 * Manages the system and GPS clocks.
 *
 * @author Mike Reid
 */
public class Clock {
  
  /**
   * Measurements made of the GPS and host system clock.
   */
  private List<ClockMeasurement> measurements;
  
  public Clock() {
    measurements = new ArrayList<ClockMeasurement>();
  }

  /**
   * Adds a clock measurement.
   *
   * @param measurement the clock measurement to add.
   */
  public void addMeasurement(ClockMeasurement measurement) {
    if (measurement == null) {
      throw new IllegalArgumentException("Cannot add a clock measurement as the meausurement given was null.");
    }
    
    measurements.add(measurement);
  }

  /**
   * Returns the most recently added measurement (which is not necessarilly the
   * latest).
   *
   * @return the clock measurement added most recently.
   */
  public ClockMeasurement getMostRecentMeasurement() {
    if ( (measurements == null) || (measurements.size() < 1) ) {
      return null;
    }
    
    return measurements.get(measurements.size() -1);
  }

  /**
   * The number of milliseconds that should be subtracted from the system time
   * to determine the equivalent GPS time.
   *
   * @return the correction between system time and GPS time.
   */
  public long getSystemToGpsCorrection() {
    if ( (measurements == null) || (measurements.size() < 1) ) {
      return 0;
    }

    return getMostRecentMeasurement().systemToGpsCorrection();
  }
}
