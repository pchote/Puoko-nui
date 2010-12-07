/*
 * Copyright 2007-2010 The Authors (see AUTHORS)
 * This file is part of Rangahau, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */
package rangahau;

import java.util.Date;

/**
 * Wrap the standard system clock so that the software can be run with
 * in simulated hardware mode without the karaka unit present.
 *
 * @author Paul Chote.
 */
public class ClockSimulator implements Clock {
    private boolean open = false;
    private int exposureTime = 5;

    /**
     * Opens the device for use.
     * @param  identifies the device. This name is irrelevant as it is not used.
     */
    public void open(String device) {
        open = true;
    }

    /**
     * Closes the ClockSimulator. It is safe to call
     * close() if the device is not open.
     */
    public void close() {
        open = false;
    }

    /**
     * Indicates whether the device has been opened for use or not.
     *
     * @return true if the device is open, false if it is not.
     */
    public boolean isOpen() {
        return open;
    }

    /**
     * Sets the exposure time (in seconds) controlled by the ClockSimulator.
     *
     * @param exposureTime the desired camera exposure time (in seconds). Valid
     *        values range from 0 (camera will not expose) to 9999 seconds.
     *
     * @throws IllegalAsumentException if exposureTime is outside the range 0 to 9999.
     * @throws IllegalStateException if the Clock has not yet been opened for use.
     */
    public void setExposureTime(int exposureTime) {
        if ((exposureTime < 0) || (exposureTime > 9999)) {
            throw new IllegalArgumentException("Cannot get the set exposure time as valid exposure times are between 0 and 9999 seconds and the exposure time given was "
                    + exposureTime);
        }

        if (!isOpen()) {
            throw new IllegalStateException("Cannot get the set exposure time as the SystemClock has not yet been opened for use.");
        }
        this.exposureTime = exposureTime;
    }

    /**
     * Gets the exposure time (in seconds) controlled by the ClockSimulator.
     *
     * @return
     *
     * @throws IllegalStateException if the ClockSimulator has not yet been opened for use.
     */
    public int getExposureTime() {
        if (!isOpen()) {
            throw new IllegalStateException("Cannot get the get exposure time as the SystemClock has not yet been opened for use.");
        }
        return exposureTime;
    }

    /**
     * Gets the time that the last camera synchronization/exposure pulse was
     * last emitted by the ClockSimulator.
     *
     * @return the time the last GPS time was emitted by the clock, or null if
     *         a sync/exposure pulse has not yet been set.
     *
     * @throws IllegalStateException if the ClockSimulator has not yet been opened for use.
     */
    public Date getLastSyncPulseTime() {
        if (!isOpen()) {
            throw new IllegalStateException("Cannot get the get the last sync pulse time as the SystemClock has not yet been opened for use.");
        }
        
        // TODO
        return new Date();
    }

    /**
     * Gets the time that the last GPS timing pulse was last received by the
     * ClockSimulator.
     *
     * @return the time the last GPS time was received by the clock, or null if
     *         a GPS time pulse has not yet been received.
     *
     * @throws IllegalStateException if the ClockSimulator has not yet been opened for use.
     */
    public Date getLastGpsPulseTime() {
        if (!isOpen()) {
            throw new IllegalStateException("Cannot get the get the last GPS pulse time as the GpsClock has not yet been opened for use.");
        }

        // TODO
        return new Date();
    }
}
