/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

package rangahau;

/**
 * Represents the state of the GPS Clock (which may include error conditions etc).
 *
 * @author Mike Reid
 */
public class GpsClockState {


  /**
   * Indicates that no error has occured.
   */
  public static int NO_ERROR = 0x01;

  /**
   * Indicates that a request packet was sent to the clock with a bad type
   * identifier. This will only happen due to software bugs or data corruption.
   * This is a non-recoverable error (the packet must be discarded).
   */
  public static int PACKET_ID_INVALID = 0x02;

  /**
   * Indicates that a request for the GPS timestamp was received by the clock
   * while the GPS timestamp was still being determined. This is a recoverable
   * condition, the user should wait a few milliseconds before trying again.
   */
  public static int UTC_ACCESS_ON_UPDATE = 0x04;

  /**
   * Indicates that a request for the exposure sync time was received by the
   * clock while the sync timestamp was still being determined. This is a
   * recoverable condition, the user should wait a few milliseconds before
   * trying again.
   */
  public static int SYNC_ACCESS_ON_UPDATE = 0x08;

  /**
   * Indicates that the Primary timing packet received from the GPS unit is
   * of incorrect length. This indicates that the latest timestamp may be
   * incorrect.
  */
  public static int BAD_TIMING_PACKET = 0x10;

  /**
   * Indicates that the GPS unit is reporting times that are not locked to GPS
   * time (mostly if the unit cannot obtain a signal from the GPS satellites).
   * This indicates that the latest timestamp may be incorrect. This is a
   * non-recoverable error (the packet must be discarded).
   */
  public static int NO_LOCK = 0x20;

  /**
   * Indicates that the a serial time string has not been received from the GPS
   * unit within the last two seconds. This is a non-recoverable error (the
   * packet must be discarded).
   */
  public static int NO_SERIAL = 0x40;

  /**
   * Represents the state of the GPS clock.
   */
  private int state;

  /**
   * True if the GPS Clock completed the operation with no problems.
   */
  private boolean good;

  /**
   * True if the GPS Clock completed the operation successfully but there
   * were some warnings.
   */
  private boolean ok;

  /**
   * True if the problem is temporary and the operation should be attempted again
   * after a few milliseconds delay.
   */
  private boolean recoverable;

  /**
   * True if an error condition has been reported.
   */
  private boolean hasError;

  /**
   * True if the request packet type was invalid.
   */
  private boolean packetTypeInvalid;

  /**
   * True if the request for GPS time was made when the GPS time was being updated.
   */
  private boolean gpsTimeUpdating;

  /**
   * True if the request for the last sync/exposure time was made when the
   * sync/exposure time was being updated.
   */
  private boolean syncTimeUpdating;

  /**
   * True if the length of the Primary timing packet was not invalid
   */
  private boolean badTimingPacket;

  /**
   * True if the timing unit doesn't have a lock to a GPS time source/
   */
  private boolean noGpsLock;

  /**
   * True if the timing unit has not received a GPS serial string within the
   * last 2 seconds.
   */
  private boolean noGpsSerial;

  public GpsClockState() {
  }

  public GpsClockState(int state) {
    setState(state);
  }

  /**
   * Represents the state of the GPS clock.
   * @return the state
   */
  public int getState() {
    return state;
  }

  /**
   * Represents the state of the GPS clock.
   * @param state the state to set
   */
  public void setState(int state) {
    this.state = state;

    if ( (state & NO_ERROR) == NO_ERROR) {
      hasError = false;
    } else {
      hasError = true;
    }

    if ( (state & PACKET_ID_INVALID) == PACKET_ID_INVALID) {
      packetTypeInvalid = true;
    } else {
      packetTypeInvalid = false;
    }

    if ( (state & UTC_ACCESS_ON_UPDATE) == UTC_ACCESS_ON_UPDATE) {
      gpsTimeUpdating = true;
    } else {
      gpsTimeUpdating = false;
    }

    if ( (state & SYNC_ACCESS_ON_UPDATE) == SYNC_ACCESS_ON_UPDATE) {
      syncTimeUpdating = true;
    } else {
      syncTimeUpdating = false;
    }

    if ( (state & BAD_TIMING_PACKET) == BAD_TIMING_PACKET) {
      badTimingPacket = true;
    } else {
      badTimingPacket = false;
    }

    if ( (state & NO_LOCK) == NO_LOCK) {
      noGpsLock = true;
    } else {
      noGpsLock = false;
    }

    if ( (state & NO_SERIAL) == NO_SERIAL) {
      noGpsSerial = true;
    } else {
      noGpsSerial = false;
    }

    good = false;
    if (state == NO_ERROR) {
      good = true;
    }

    ok = false;
    if (!good && !hasError && !badTimingPacket && !packetTypeInvalid) {
      ok = true;
    }

    recoverable = true;
    if (hasError || badTimingPacket || packetTypeInvalid) {
      recoverable = false;
    }
  }

  /**
   * True if the GPS Clock completed the operation successfully.
   * @return the ok
   */
  public boolean isOk() {
    return ok;
  }

  /**
   * True if the GPS Clock completed the operation successfully.
   * @param ok the ok to set
   */
  public void setOk(boolean ok) {
    this.ok = ok;
  }

  /**
   * True if the problem is temporary and the operation should be attempted again
   * after a few milliseconds delay.
   * @return the recoverable
   */
  public boolean isRecoverable() {
    return recoverable;
  }

  /**
   * True if the problem is temporary and the operation should be attempted again
   * after a few milliseconds delay.
   * @param recoverable the recoverable to set
   */
  public void setRecoverable(boolean recoverable) {
    this.recoverable = recoverable;
  }

  /**
   * True if an error condition has been reported.
   * @return the hasError
   */
  public boolean isHasError() {
    return hasError;
  }

  /**
   * True if an error condition has been reported.
   * @param hasError the hasError to set
   */
  public void setHasError(boolean hasError) {
    this.hasError = hasError;
  }

  /**
   * True if the request packet type was invalid.
   * @return the packetTypeInvalid
   */
  public boolean isPacketTypeInvalid() {
    return packetTypeInvalid;
  }

  /**
   * True if the request packet type was invalid.
   * @param packetTypeInvalid the packetTypeInvalid to set
   */
  public void setPacketTypeInvalid(boolean packetTypeInvalid) {
    this.packetTypeInvalid = packetTypeInvalid;
  }

  /**
   * True if the request for GPS time was made when the GPS time was being updated.
   * @return the gpsTimeUpdating
   */
  public boolean isGpsTimeUpdating() {
    return gpsTimeUpdating;
  }

  /**
   * True if the request for GPS time was made when the GPS time was being updated.
   * @param gpsTimeUpdating the gpsTimeUpdating to set
   */
  public void setGpsTimeUpdating(boolean gpsTimeUpdating) {
    this.gpsTimeUpdating = gpsTimeUpdating;
  }

  /**
   * True if the request for the last sync/exposure time was made when the
   * sync/exposure time was being updated.
   * @return the syncTimeUpdating
   */
  public boolean isSyncTimeUpdating() {
    return syncTimeUpdating;
  }

  /**
   * True if the request for the last sync/exposure time was made when the
   * sync/exposure time was being updated.
   * @param syncTimeUpdating the syncTimeUpdating to set
   */
  public void setSyncTimeUpdating(boolean syncTimeUpdating) {
    this.syncTimeUpdating = syncTimeUpdating;
  }

  /**
   * True if the length of the Primary timing packet was not invalid
   * @return the badTimingPacket
   */
  public boolean isBadTimingPacket() {
    return badTimingPacket;
  }

  /**
   * True if the length of the Primary timing packet was not invalid
   * @param badTimingPacket the badTimingPacket to set
   */
  public void setBadTimingPacket(boolean badTimingPacket) {
    this.badTimingPacket = badTimingPacket;
  }

  /**
   * True if the timing unit doesn't have a lock to a GPS time source/
   * @return the noGpsLock
   */
  public boolean isNoGpsLock() {
    return noGpsLock;
  }

  /**
   * True if the timing unit doesn't have a lock to a GPS time source/
   * @param noGpsLock the noGpsLock to set
   */
  public void setNoGpsLock(boolean noGpsLock) {
    this.noGpsLock = noGpsLock;
  }

  /**
   * True if the timing unit has not received a GPS serial string within the
   * last 2 seconds.
   * @return the noGpsSerial
   */
  public boolean isNoGpsSerial() {
    return noGpsSerial;
  }

  /**
   * True if the timing unit has not received a GPS serial string within the
   * last 2 seconds.
   * @param noGpsSerial the noGpsSerial to set
   */
  public void setNoGpsSerial(boolean noGpsSerial) {
    this.noGpsSerial = noGpsSerial;
  }

  /**
   * Reports the state as a string.
   *
   * @return the state as a string.
   */
  public String toString() {
    String value = "state = 0x" + Integer.toHexString(state)
                 + ", good = " + good
                 + ", ok = " + ok
                 + ", recoverable = " + recoverable
                 + ", hasError = " + hasError
                 + ", packet type invalid  = " + packetTypeInvalid
                 + ", GPS time updating = " + gpsTimeUpdating
                 + ", sync time updating = " + syncTimeUpdating
                 + ", bad timing packet length = " + badTimingPacket
                 + ", no GPS lock = " + noGpsLock
                 + ", no GPS serial = " + noGpsSerial;

    return value;
  }

  /**
   * True if the GPS Clock completed the operation with no problems.
   * @return the good
   */
  public boolean isGood() {
    return good;
  }

  /**
   * True if the GPS Clock completed the operation with no problems.
   * @param good the good to set
   */
  public void setGood(boolean good) {
    this.good = good;
  }

}
