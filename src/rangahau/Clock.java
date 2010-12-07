/*
 * Copyright 2007-2010 The Authors (see AUTHORS)
 * This file is part of Rangahau, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */
package rangahau;

import java.util.Date;

/**
 * Defines a generic clock that can be queried for time/sync information
 *
 * @author Paul Chote.
 */
public interface Clock {

    /**
     * Gets the exposure time (in seconds) controlled by the GpsClock.
     *
     * @return
     *
     * @throws IllegalStateException if the GpsClock has not yet been opened for use.
     */
    public int getExposureTime();

    /**
     * Sets the exposure time (in seconds) controlled by the GpsClock.
     *
     * @param exposureTime the desired camera exposure time (in seconds). Valid
     *        values range from 0 (camera will not expose) to 9999 seconds.
     *
     * @throws IllegalAsumentException if exposureTime is outside the range 0 to 9999.
     * @throws IllegalStateException if the GpsClock has not yet been opened for use.
     */
    public void setExposureTime(int exposureTime);

    /**
     * Gets the time that the last GPS timing pulse was last received by the
     * GpsClock.
     *
     * @return the time the last GPS time was received by the clock, or null if
     *         a GPS time pulse has not yet been received.
     *
     * @throws IllegalStateException if the GpsClock has not yet been opened for use.
     */
    public Date getLastGpsPulseTime();

    /**
     * Gets the time that the last camera synchronization/exposure pulse was
     * last emitted by the GpsClock.
     *
     * @return the time the last GPS time was emitted by the clock, or null if
     *         a sync/exposure pulse has not yet been set.
     *
     * @throws IllegalStateException if the GpsClock has not yet been opened for use.
     */
    public Date getLastSyncPulseTime();

    /**
     * Opens the device for use. The name of the device to open can be obtained
     * from the results of the (static) GpsClock.getAllDeviceNames() method.
     *
     * @param  identifies the device. This name is one of the names returned by
     *         the getAllDeviceNames() static method.
     */
    public void open(String device);

    /**
     * Indicates whether the device has been opened for use or not.
     *
     * @return true if the device is open, false if it is not.
     */
    public boolean isOpen();

    /**
     * Closes the Clock and releases any resources it used. It is safe to call
     * close() if the device is not open.
     */
    public void close();
}
