/*
 * Copyright 2007-2010 The Authors (see AUTHORS)
 * This file is part of Rangahau, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */
package rangahau;

import java.text.DecimalFormat;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.TimeZone;

/**
 * Communicates with a GPS clock via a USB interface. This USB interface is 
 * provided by an FTDI FT232 USB-to-digital I/O device. This is implemented
 * using the user-space libftdi driver (which also uses libusb).
 * 
 * This class relies on a native library called "gpsclock" (on Linux this
 * library is called "libgpsclock.so").
 *
 * Notes:
 *  1) This class must be run as root on Linux.
 *
 *  2) The ftdio_sio kernel module must be loaded (this is part of the linux kernel).
 *
 *  3) USB traffic can be viewed on Linux with the command:
 *       echo 1 > /sys/module/usbcore/parameters/usbfs_snoop
 *     Then watch USB traffic using:
 *       tail -f /var/log/messages
 *
 *  4) An article (a bit out of date) describing USB snooping is described at:
 *     http://www.linuxjournal.com/article/7582   "Snooping the USB Data Stream"
 *
 * Links:
 * <TABLE>
 * 
 *   <TR><TD>libftdi user-space driver</TD>
 *       <TD>http://www.intra2net.com/en/developer/libftdi/index.php</A></TD>
 *   </TR>
 *   </TR>
 * </TABLE>
 * 
 * @author Michael Reid, for Argonaut Ltd.
 */
public class GpsClock implements Clock {
    // Load the attached JNI library.

    static {
        System.loadLibrary("gpsclock");
    }
    /**
     * Indicates whether this FTDI FT232 device has been opened for use.
     */
    private boolean open = false;

    /**
     * The name that identifies the device. Since there is no device node 
     * (entry in /dev) for a userspace device the "deviceName" is instead an 
     * identifier obtained from the getAllDeviceNames() method.
     */
    private String deviceName = null;
    private int DLE = 0x10; // Data-link escape.
    private int ETX = 0x03; // End-of-text.

    /**
     * The packet type identifier which requests the timing unit echo back the
     * same value.
     */
    private int ECHO = 0x01;

    /**
     * The packet type identifier requests the last GPS timestamp received.
     */
    private int GETGPSTIME = 0x23;

    /**
     * The packet type identifier requests the exposure time (in seconds).
     */
    private int GETEXPOSURETIME = 0x24;

    /**
     * The packet type identifier that sets the exposure time (in seconds).
     */
    private int SETEXPOSURETIME = 0x44;

    /**
     * The packet type identifier requests the timestamp the last camera
     * synchronisation pulse was emitted.
     */
    private int GETSYNCTIME = 0x25;
    
    /**
     * A list of open FTDI devices used to connect to the GpsClock.
     */
    static List<String> openDevices;
    SimpleDateFormat dateFormatter;
    TimeZone utcTimeZone;

    /**
     * Creates a new GpsClock. This is not ready for use until the open() method
     * is used.
     */
    public GpsClock() {
        if (openDevices == null) {
            openDevices = new ArrayList<String>();
        }

        utcTimeZone = TimeZone.getTimeZone("UTC");
        dateFormatter = new SimpleDateFormat("yyyy:MM:dd:HH:mm:ss:SSS");
        dateFormatter.setTimeZone(utcTimeZone);
    }

    /**
     * Opens the device for use. The name of the device to open can be obtained
     * from the results of the (static) GpsClock.getAllDeviceNames() method.
     * 
     * @param  identifies the device. This name is one of the names returned by 
     *         the getAllDeviceNames() static method.
     */
    public void open(String device) {
        if (device == null) {
            throw new IllegalArgumentException("Cannot open the userspace GPS Clock (FTDI 232 USB) as the device name given was null.");
        }
        if (device.trim().length() < 1) {
            throw new IllegalArgumentException("Cannot open the userspace GPS Clock (FTDI 232 USB) as the device name given was an empty string.");
        }

        // If the device is already open then there is nothing left to do.
        if (deviceOpened(device)) {
            return;
        }

        nativeOpen(device);
        deviceName = device;
        open = true;
        openDevices.add(device);

        // Give some time for the device to open.
        long endTime = System.currentTimeMillis() + 500;
        while (System.currentTimeMillis() < endTime) {
            try {
                Thread.sleep(1);
            } catch (InterruptedException ex) {
                // Ignore interruptions.
            }
        }

        // Ensure there are no bytes left in the read buffer.
        clearReadBuffer();
    }

    /**
     * Closes the GpsClock and releases any resources it used. It is safe to call
     * close() if the device is not open.
     */
    public void close() {
        // If the device is closed already there is nothing else to do.
        if (!deviceOpened(deviceName)) {
            return;
        }

        nativeClose(deviceName);
        openDevices.remove(deviceName);
        deviceName = null;
        open = false;
    }

    /**
     * Writes bytes to the device (across USB to appear as a sequence of values
     * on the digital I/O pins).
     * 
     * @param data the data to be written to the device. Each element of the
     *        buffer represents an 8-bit unsigned byte value (ranging from 0 
     *        to 255).
     * 
     * @throws IllegalArgumentException if data is null.
     * @throws IllegalStateException if the device has not yet been opened with open().
     */
    public void write(int[] data) {
        if (!isOpen()) {
            throw new IllegalStateException("Cannot write to the userspace GPS Clock (FTDI 232 USB) as the device is not open.");
        }
        if (data == null) {
            throw new IllegalArgumentException("Cannot write to " + deviceName + "as the data buffer given was null.");
        }
        if (data.length == 0) {
            return; // There is no data to write.
        }

        // Write the bytes out.
        nativeWrite(deviceName, data);
    }

    /**
     * Reads byyes from the device (across the USB).
     *
     * @param timeoutMillis the maximum number of milliseconds to wait for
     *        a message to appear from the device.
     * @param the number of bytes to read from the device.
     *
     * @return the data read from the device. Each element of the
     *        buffer represents an 8-bit unsigned byte value (ranging from 0
     *        to 255).
     *
     * @throws IllegalStateException if the device has not yet been opened with open().
     * @throws RuntimeException if a message was not read from the device within the timeout limit.
     */
    public int[] read(long timeoutMillis, int bytesToGet) {
        if (!isOpen()) {
            throw new IllegalStateException("Cannot read from the userspace GPS Clock (FTDI 232 USB) as the device is not open.");
        }
        if (timeoutMillis <= 0) {
            throw new IllegalStateException("Cannot read from the userspace GPS Clock (FTDI 232 USB) as the timeout given was " + timeoutMillis);
        }
        if (bytesToGet <= 0) {
            throw new IllegalStateException("Cannot read from the userspace GPS Clock (FTDI 232 USB) as the bytes to get was " + bytesToGet);
        }

        int[] buffer = new int[4096];
        int[] appendBuffer = new int[4096];
        int totalBytes = 0;

        long endTime = System.currentTimeMillis() + timeoutMillis;

        while ((System.currentTimeMillis() < endTime) && (totalBytes < bytesToGet)) {
            int numBytesRead = nativeRead(deviceName, buffer);

            if (numBytesRead < 0) {
                throw new RuntimeException("A problem occured while reading from the userspace GPS Clock (FTDI 232 USB) as the number of bytes read was "
                        + numBytesRead);
            }

            System.arraycopy(buffer, 0, appendBuffer, totalBytes, numBytesRead);
            totalBytes += numBytesRead;

            try {
                Thread.sleep(1);
            } catch (InterruptedException ex) {
                // Ignore interruptions.
            }
        }

        int[] destination = new int[totalBytes];
        System.arraycopy(appendBuffer, 0, destination, 0, totalBytes);

        return destination;
    }

    /**
     * Indicates whether the device has been opened for use or not.
     * 
     * @return true if the device is open, false if it is not.
     */
    public boolean isOpen() {
        return open && deviceOpened(deviceName);
    }

    /**
     * Returns the device the name of the Linux device that represents the FTDI245,
     * as given in the call to open().
     * 
     * @return the device name, or null if the device has not yet been opened.
     */
    public String getDeviceName() {
        if (!isOpen()) {
            return null;
        }

        return deviceName;
    }

    /**
     * Returns the "device names" (identifiers) for all FDTI 232 devices 
     * connected to the system.
     * 
     * @return the device identifiers of all FTDI 232 devices connected to the
     *         system. If no devices were found then the returned array will
     *         have no elements (it will not be null).
     */
    public static List<String> getAllDeviceNames() {
        ArrayList<String> allNames = new ArrayList<String>();

        String[] names = nativeFindAllDeviceIdentifiers();
        if (names != null) {
            allNames.addAll(Arrays.asList(names));
        }
        return allNames;
    }

    /**
     * Opens the native device. This is implemented as a native method.
     * 
     * @param name identifies the device. This name is one of the names 
     *             returned by the findAllDeviceNames() method. 
     * 
     * @throws IllegalArgumentException if name is null or an empty string.
     * @throws RuntimeException if there was a problem opening the device.
     */
    private native void nativeOpen(String name);

    /**
     * Closes the device.
     * 
     * @param name the device name used to open device with nativeOpen(). 
     * @throws IllegalArgumentException if handle is not a valid handle. 
     */
    private native void nativeClose(String name);

    /**
     * Performs the writing of a buffer to the device (across the USB. This is 
     * implemented as a native method.
     * 
     * @param name identifies the device. This name is one of the names 
     *             returned by the findAllDeviceNames() method. 
     * @param data the data to be written to the device. Each element of the
     *        buffer represents an 8-bit unsigned byte value (ranging from 0 
     *        to 255).
     * 
     * @throws IllegalArgumentException if name is not a valid device name.
     * @throws IllegalArgumentException if data is null.
     * @throws IllegalStateException if the device has not yet been opened with open().
     */
    private native void nativeWrite(String name, int[] data);

    /**
     * Performs reading from the device (across the USB). This is implemented 
     * as a native method.
     * 
     * @param name identifies the device. This name is one of the names 
     *             returned by the findAllDeviceNames() method.
     * @param data where data read from the device is to be put. Each element of
     *        the buffer represents an 8-bit unsigned byte value (ranging from 0
     *        to 255).
     *
     * @return the number of bytes read from the device.
     * 
     * @throws IllegalArgumentException if name is not a valid device name.
     * @throws IllegalStateException if the device has not yet been opened with open().
     * @throws RuntimeException if a message was not read from the device within the timeout limit.
     */
    private native int nativeRead(String name, int[] data);

    /**
     * Finds the identifiers for all FTDI 232 devices connected to the system.
     * These identifiers can be given to the nativeOpen() method to select
     * which FTDI 232 device should be opened for use.
     * 
     * @return identifiers for all FTDI 232 connected to the system.
     */
    private static native String[] nativeFindAllDeviceIdentifiers();

    /**
     * Returns the message bytes which request an echo response from the device.
     * Each byte of the message is contained within the least significant byte
     * of the elements of an integer array.
     *
     * @return the bytes that should be sent to receive an echo response.
     */
    public int[] getEchoRequest() {
        int errorCode = 0;

        int[] message = {DLE,
            ECHO,
            errorCode,
            DLE,
            ETX};

        return message;
    }

    /**
     * Sends an echo request to the GpsClock device.
     */
    public void sendEchoRquest() {
        if (!isOpen()) {
            throw new RuntimeException("Could not send an echo request as the GpsClock is not opened for use.");
        }

        int[] message = getEchoRequest();
        write(message);
    }

    /**
     * Gets the echo response message. If echo response was received correctly
     * then this method does nothing, if the echo response is not received then
     * a runtime exception will be thrown.
     */
    public void getEchoResponse() {
        int timeoutMillis = 20;
        final int expectedResponseLength = 6;

        int[] message = read(timeoutMillis, expectedResponseLength);

        if (message == null) {
            throw new RuntimeException("Could not get GpsClock echo response as the response returned was null within the time limit of "
                    + timeoutMillis + " milliseconds");
        }

        if (message.length != expectedResponseLength) {
            throw new RuntimeException("The GpsClock echo response had a length of "
                    + message.length + " when " + expectedResponseLength + " bytes were expected");
        }

        if (message[3] != ECHO) {
            throw new RuntimeException("The GpsClock echo response had a payload value of "
                    + message[3] + " when  a value of " + ECHO + " was expected");

        }

        // If we got here then the echo response was received correctly.
    }

    /**
     * Returns true if the device with the given name is in the list of open
     * devices.
     *
     * @param name the name of the device.
     *
     * @return true if the device with that name is open, false if it is not.
     */
    private boolean deviceOpened(String name) {
        if (openDevices != null) {
            for (String device : openDevices) {
                if (device.equalsIgnoreCase(name)) {
                    return true;
                }
            }
        }

        // If we got here then we didn't find the device.
        return false;
    }

    /**
     * Returns the message bytes which set the exposure time on the device.
     * Each byte of the message is contained within the least significant byte
     * of the elements of an integer array.
     *
     * @param exposureTime the desired camera exposure time (in seconds). Valid
     *        values range from 0 (camera will not expose) to 9999 seconds.
     *
     * @return the bytes that should be sent to receive an echo response.
     *
     * @throws IllegalAsumentException if exposureTime is outside the range 0 to 9999.
     */
    public int[] getSetExposureTimeRequest(int exposureTime) {
        if ((exposureTime < 0) || (exposureTime > 9999)) {
            throw new IllegalArgumentException("Cannot get the set exposure time message as valid exposure times are between 0 and 9999 seconds and the exposure time given was "
                    + exposureTime);
        }

        int errorCode = 0;

        DecimalFormat formatter = new DecimalFormat("0000");
        String exposure = formatter.format(exposureTime);
        int[] message = {DLE,
            SETEXPOSURETIME,
            errorCode,
            exposure.codePointAt(0), // Most significant digit first.
            exposure.codePointAt(1),
            exposure.codePointAt(2),
            exposure.codePointAt(3),
            DLE,
            ETX};

        return message;
    }

    /**
     * Gets the message bytes which get the exposure time from the device.
     * Each byte of the message is contained within the least significant byte
     * of the elements of an integer array.
     *
     * @return the bytes that should be sent to receive the exposure time response.
     */
    public int[] getGetExposureTimeRequest() {

        int errorCode = 0;

        int[] message = {DLE,
            GETEXPOSURETIME,
            errorCode,
            DLE,
            ETX};

        return message;
    }

    /**
     * Sets the exposure time (in seconds) controlled by the GpsClock.
     *
     * @param exposureTime the desired camera exposure time (in seconds). Valid
     *        values range from 0 (camera will not expose) to 9999 seconds.
     * 
     * @throws IllegalAsumentException if exposureTime is outside the range 0 to 9999.
     * @throws IllegalStateException if the GpsClock has not yet been opened for use.
     */
    public void setExposureTime(int exposureTime) {
        if ((exposureTime < 0) || (exposureTime > 9999)) {
            throw new IllegalArgumentException("Cannot get the set exposure time as valid exposure times are between 0 and 9999 seconds and the exposure time given was "
                    + exposureTime);
        }

        if (!isOpen()) {
            throw new IllegalStateException("Cannot get the set exposure time as the GpsClock has not yet been opened for use.");
        }

        int[] message = getSetExposureTimeRequest(exposureTime);
        write(message);

        // Read the acknowledgement response from the server.
        long timeLimitMillis = 1100;
        long endTime = System.currentTimeMillis() + timeLimitMillis;
        boolean done = false;

        while (!done && (System.currentTimeMillis() < endTime)) {
            // Read the acknowledgement response from the server.
            final int timeoutMillis = 10;
            final int minimumLength = 9;
            int[] response = read(timeoutMillis, minimumLength);

            // If enough bytes were received then we can being parsing the response.
            if ((response != null) && (response.length >= minimumLength)) {
                // Check the packet start marker is present.
                if ((response[0] & 0xFF) != DLE) {
                    throw new RuntimeException("A problem occurred when setting the exposure time, the acknowledge response did not start " + " with the DLE byte (0x" + Integer.toHexString(DLE) + ") as the response started with 0x" + Integer.toHexString(response[0] & 0xFF));
                }

                // The second byte is the packet type identifier.
                if ((response[1] & 0xFF) != SETEXPOSURETIME) {
                    throw new RuntimeException("A problem occurred when getting the exposure time, the response did not have a message type of  0x" + Integer.toHexString(SETEXPOSURETIME) + " as the response has a message type of 0x" + Integer.toHexString(response[1] & 0xFF));
                }

                int messageStart = 3; // The index of the start of the message.
                int errorCode = response[2] & 0xFF;

                // If the error code has the same value as the DLE byte then the next value
                // is also DLE as it will be byte 'stuffed'. The message will start after
                // this extra byte.
                if (errorCode == DLE) {
                    ++messageStart;
                }

                // Extract the exposure time from the message. Note we don't need to take
                // into account 'DLE byte-stuffing' in the exposure time since each digit
                // in the exposure time must always be less than the DLE value.
                byte[] bytes = new byte[4];
                bytes[0] = (byte) (response[messageStart] & 0xFF);
                bytes[1] = (byte) (response[messageStart + 1] & 0xFF);
                bytes[2] = (byte) (response[messageStart + 2] & 0xFF);
                bytes[3] = (byte) (response[messageStart + 3] & 0xFF);

                String exposureTimeString = new String(bytes, 0, 4);

                exposureTime = Integer.parseInt(exposureTimeString);

                done = true;
            }
        }
    }

    /**
     * Gets the exposure time (in seconds) controlled by the GpsClock.
     *
     * @return
     *
     * @throws IllegalStateException if the GpsClock has not yet been opened for use.
     */
    public int getExposureTime() {
        if (!isOpen()) {
            throw new IllegalStateException("Cannot get the get exposure time as the GpsClock has not yet been opened for use.");
        }

        long timeLimitMillis = 1100;
        long endTime = System.currentTimeMillis() + timeLimitMillis;
        boolean done = false;

        final int[] request = getGetExposureTimeRequest();

        int exposureTime = 0;

        while (!done && (System.currentTimeMillis() < endTime)) {
            write(request);

            // Read the acknowledgement response from the server.
            final int timeoutMillis = 10;
            final int minimumLength = 9;
            int[] response = read(timeoutMillis, minimumLength);

            // If enough bytes were received then we can being parsing the response.
            if ((response != null) && (response.length >= minimumLength)) {
                // Check the packet start marker is present.
                if ((response[0] & 0xFF) != DLE) {
                    throw new RuntimeException("A problem occurred when getting the exposure time, the acknowledge response did not start " + " with the DLE byte (0x" + Integer.toHexString(DLE) + ") as the response started with 0x" + Integer.toHexString(response[0] & 0xFF));
                }

                // The second byte is the packet type identifier.
                if ((response[1] & 0xFF) != GETEXPOSURETIME) {
                    throw new RuntimeException("A problem occurred when getting the exposure time, the response did not have a message type of  0x" + Integer.toHexString(GETEXPOSURETIME) + " as the response has a message type of 0x" + Integer.toHexString(response[1] & 0xFF));
                }

                int messageStart = 3; // The index of the start of the message.
                int errorCode = response[2] & 0xFF;

                // If the error code has the same value as the DLE byte then the next value
                // is also DLE as it will be byte 'stuffed'. The message will start after
                // this extra byte.
                if (errorCode == DLE) {
                    ++messageStart;
                }

                // Extract the exposure time from the message. Note we don't need to take
                // into account 'DLE byte-stuffing' in the exposure time since each digit
                // in the exposure time must always be less than the DLE value.
                byte[] bytes = new byte[4];
                bytes[0] = (byte) (response[messageStart] & 0xFF);
                bytes[1] = (byte) (response[messageStart + 1] & 0xFF);
                bytes[2] = (byte) (response[messageStart + 2] & 0xFF);
                bytes[3] = (byte) (response[messageStart + 3] & 0xFF);

                String exposureTimeString = new String(bytes, 0, 4);

                exposureTime = Integer.parseInt(exposureTimeString);

                done = true;
            }
        }

        return exposureTime;
    }

    /**
     * Gets the message bytes which gets the last synchronisation/exposure time
     * from the device. Each byte of the message is contained within the least
     * significant byte of the elements of an integer array.
     *
     * @return the bytes that should be sent to receive the last synchronisation
     *         time response.
     */
    public int[] getLastSyncTimeRequest() {

        int errorCode = 0;

        int[] message = {DLE,
            GETSYNCTIME,
            errorCode,
            DLE,
            ETX};

        return message;
    }

    /**
     * Gets the message bytes which gets the last GPS time received by the device.
     * Each byte of the message is contained within the least significant byte
     * of the elements of an integer array.
     *
     * @return the bytes that should be sent to receive the last GPS timestamp.
     */
    public int[] getLastGpsTimeRequest() {

        int errorCode = 0;

        int[] message = {DLE,
            GETGPSTIME,
            errorCode,
            DLE,
            ETX};

        return message;
    }

    /**
     * Gets the time that the last camera synchronisation/exposure pulse was
     * last emitted by the GpsClock.
     *
     * @return the time the last GPS time was emitted by the clock, or null if
     *         a sync/exposure pulse has not yet been set.
     *
     * @throws IllegalStateException if the GpsClock has not yet been opened for use.
     */
    public Date getLastSyncPulseTime() {
        if (!isOpen()) {
            throw new IllegalStateException("Cannot get the get the last sync pulse time as the GpsClock has not yet been opened for use.");
        }

        long timeLimitMillis = 11000;
        long endTime = System.currentTimeMillis() + timeLimitMillis;
        boolean done = false;

        Date syncDate = null;
        final int[] request = getLastSyncTimeRequest();

        while (!done && (System.currentTimeMillis() < endTime)) {
            write(request);

            // Read the acknowledgement response from the server.
            final int timeoutMillis = 20;
            final int minimumLength = 5;
            int[] response = read(timeoutMillis, minimumLength);

            // If enough bytes were received then we can being parsing the response.
            if ((response != null) && (response.length >= minimumLength)) {
                // The first byte should be the packet start byte.
                if ((response[0] & 0xFF) != DLE) {
                    throw new RuntimeException("A problem occurred when getting the last sync time, the response did not start " + " with the DLE byte (0x" + Integer.toHexString(DLE) + ") as the response started with 0x" + Integer.toHexString(response[0] & 0xFF));
                }

                // The second byte is the packet type identifier.
                if ((response[1] & 0xFF) != GETSYNCTIME) {
                    throw new RuntimeException("A problem occurred when getting the last sync time, the response did not have a message type of  0x" + Integer.toHexString(GETSYNCTIME) + " as the response has a message type of 0x" + Integer.toHexString(response[1] & 0xFF));
                }

                int messageStart = 3; // The index of the start of the timeout message.
                int errorCode = response[2] & 0xFF;

                GpsClockState state = new GpsClockState(errorCode);
                if (!state.isGood() && !state.isSyncTimeUpdating()) {
                    System.out.println("requested last sync time : clock state = " + state);
                }
                // If the state indicated a read was in progress then we don't process
                // the response any further, instead we wait until the last sync time
                // is ready.
                if (state.isSyncTimeUpdating()) {
                    // The sync time is still being determined. Sleep a little to let the
                    // system do other processing while we wait.
                    try {
                        Thread.sleep(2);
                    } catch (InterruptedException ex) {
                        // Ignore interruptions.
                    }
                } else {
                    // Process the message.

                    // If the error code has the same value as the DLE byte then the next value
                    // is also DLE as it will be byte 'stuffed'. The message will start after
                    // this extra byte.
                    if (errorCode == DLE) {
                        ++messageStart;
                    }

                    // Check that the last two bytes of the message are the end packet bytes.
                    if (((response[response.length - 2] & 0xFF) != DLE) && ((response[response.length - 1] & 0xFF) != ETX)) {
                        throw new RuntimeException("A problem occurred when getting the last sync time, the response did not have two-byte packet boundary marker of  0x" + Integer.toHexString(DLE) + " and 0x" + Integer.toHexString(ETX) + " as the response had end boundary marker of 0x" + Integer.toHexString(response[response.length - 2] & 0xFF) + " and 0x " + Integer.toHexString(response[response.length - 1] & 0xFF));
                    }

                    int numPayloadBytes = response.length - 5;
                    if (numPayloadBytes > 0) {
                        byte[] bytes = new byte[numPayloadBytes];
                        for (int count = messageStart; count < response.length - 2; ++count) {
                            bytes[count - messageStart] = (byte) (response[count] & 0xFF);
                        }

                        String lastSyncTime = new String(bytes, 0, bytes.length);

                        //System.out.println("Last sync time recieved at : " + lastSyncTime);

                        try {
                            syncDate = dateFormatter.parse(lastSyncTime);
                        } catch (ParseException ex) {
                            throw new RuntimeException("Could not parse a sync time from the string '" + lastSyncTime + "'", ex);
                        }
                    }

                    done = true; // We have a sync time stamp.
                }
            }
        }

        // Debugging info.
//    if (syncDate != null) {
//      long timeDifference = System.currentTimeMillis() - syncDate.getTime();
//      System.out.println("Sync time difference = " + timeDifference);
//    }

        return syncDate;
    }

    /**
     * Gets the time that the last GPS timing pulse was last received by the
     * GpsClock.
     *
     * @return the time the last GPS time was received by the clock, or null if
     *         a GPS time pulse has not yet been received.
     *
     * @throws IllegalStateException if the GpsClock has not yet been opened for use.
     */
    public Date getLastGpsPulseTime() {
        if (!isOpen()) {
            throw new IllegalStateException("Cannot get the get the last GPS pulse time as the GpsClock has not yet been opened for use.");
        }

        long timeLimitMillis = 900;
        long endTime = System.currentTimeMillis() + timeLimitMillis;
        boolean done = false;

        Date gpsDate = null;
        final int[] request = getLastGpsTimeRequest();

        while (!done && (System.currentTimeMillis() < endTime)) {
            write(request);

            // Read the acknowledgement response from the server.
            final int timeoutMillis = 20;
            final int minimumLength = 5;
            int[] response = read(timeoutMillis, minimumLength);

            // If enough bytes were received then we can being parsing the response.
            if ((response != null) && (response.length >= minimumLength)) {
                // The first byte should be the packet start byte.
                if ((response[0] & 0xFF) != DLE) {
                    throw new RuntimeException("A problem occurred when getting the last gps time, the response did not start " + " with the DLE byte (0x" + Integer.toHexString(DLE) + ") as the response started with 0x" + Integer.toHexString(response[0] & 0xFF));
                }

                // The second byte is the packet type identifier.
                if ((response[1] & 0xFF) != GETGPSTIME) {
                    throw new RuntimeException("A problem occurred when getting the last gps time, the response did not have a message type of  0x" + Integer.toHexString(GETGPSTIME) + " as the response has a message type of 0x" + Integer.toHexString(response[1] & 0xFF));
                }

                int messageStart = 3; // The index of the start of the timeout message.
                int errorCode = response[2] & 0xFF;

                GpsClockState state = new GpsClockState(errorCode);
                if (!state.isGood() && !state.isGpsTimeUpdating()) {
                    System.out.println("requested last GPS time : clock state = " + state);
                }
                // If the state indicated a read was in progress then we don't process
                // the response any further, instead we wait until the last gps time
                // is ready.
                if (state.isGpsTimeUpdating()) {
                    // The gps time is still being determined. Sleep a little to let the
                    // system do other processing while we wait.
                    try {
                        Thread.sleep(2);
                    } catch (InterruptedException ex) {
                        // Ignore interruptions.
                    }
                } else {
                    // Process the message.

                    // If the error code has the same value as the DLE byte then the next value
                    // is also DLE as it will be byte 'stuffed'. The message will start after
                    // this extra byte.
                    if (errorCode == DLE) {
                        ++messageStart;
                    }

                    // Check that the last two bytes of the message are the end packet bytes.
                    if (((response[response.length - 2] & 0xFF) != DLE) && ((response[response.length - 1] & 0xFF) != ETX)) {
//            throw new RuntimeException("A problem occurred when getting the last gps time, the response did not have two-byte packet boundary marker of  0x" + Integer.toHexString(DLE) + " and 0x" + Integer.toHexString(ETX) + " as the response had end boundary marker of 0x" + Integer.toHexString(response[response.length - 2] & 0xFF) + " and 0x " + Integer.toHexString(response[response.length - 1] & 0xFF));
                    }

                    int numPayloadBytes = response.length - 5;
                    if (numPayloadBytes > 0) {
                        byte[] bytes = new byte[numPayloadBytes];
                        for (int count = messageStart; count < response.length - 2; ++count) {
                            bytes[count - messageStart] = (byte) (response[count] & 0xFF);
                        }

                        String lastGpsTime = new String(bytes, 0, bytes.length);

                        //System.out.println("Last gps time recieved at : " + lastGpsTime);

                        try {
                            gpsDate = dateFormatter.parse(lastGpsTime);
                        } catch (ParseException ex) {
                            throw new RuntimeException("Could not parse a gps time from the string '" + lastGpsTime + "'", ex);
                        }
                    }

                    done = true; // We have a gps time stamp.
                }
            }
        }

        // Debugging info.
//    if (gpsDate != null) {
//      long timeDifference = System.currentTimeMillis() - gpsDate.getTime();
//      System.out.println("Gps time difference = " + timeDifference);
//    }

        return gpsDate;
    }

    /**
     * Clear any waiting bytes on the read buffer.
     */
    public void clearReadBuffer() {
        if (!isOpen()) {
            return; // There is nothing to do as the clock is not yet open for use.
        }

        final long endTime = System.currentTimeMillis() + 20;
        boolean done = false;

        while ((System.currentTimeMillis() < endTime) && !done) {
            int[] buffer = read(1, 1);

            if ((buffer == null) || (buffer.length < 1)) {
                done = true;
            }
        }
    }
}
