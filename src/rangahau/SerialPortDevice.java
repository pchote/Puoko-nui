/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

package rangahau;

/**
 * This class manages a RS-232 serial port device. It uses a native library
 * called "serialport" (eg. "libserialport.so" on Linux) to access the device.
 * 
 * @author Mike Reid
 */
public class SerialPortDevice implements SerialPort {

    // Load the attached JNI library.
    static {
        System.loadLibrary("serialport");
    }
    
    /**
     * The device name of the serial port device. For example, "/dev/ttyS0".
     */
    private String name = null;
   
    /**
     * True if the serial port is open and is ready for use.
     */
    private boolean open = false;
    
    /**
     * Holds the device handle (file descriptor) used to manipulate the 
     * underlying native device.
     */
    private int handle = -1;
    
    /**
     * Opens the serial port for use.
     * 
     * @param portName the name of the port device. For example "/dev/ttyS0".
     * 
     * @throws IllegalArgumentException if portName is null or a blank string.
     */
    public void open(String portName) {
        if (portName == null) {
            throw new IllegalArgumentException("Cannot open the serial port as the device name given was null.");
        }
        if (portName.trim().length() < 1) {
            throw new IllegalArgumentException("Cannot open the serial port as the device name given was an empty string.");
        }
        
        handle = nativeOpen(portName);
        
        name = portName;        
        open = true;
    }
    
    /**
     * Returns true if the serial port has been opened for use using the open()
     * method.
     * 
     * @return true if the port is open for use, false if it is not.
     */
    public boolean isOpen() {
        return open;
    }
    
    /**
     * Closes the serial port. It is safe to call close() on a serial port 
     * that is already been closed or has never been opened.
     */
    public void close() {
        if (isOpen()) {
            // The port is already closed, there is nothing more to do.
            return;
        }
        
        nativeClose(handle);
    }
    
    /**
     * Returns the name of the serial port device. For example "/dev/ttyS0".
     * 
     * @return the name of the serial port device.
     * 
     * @throws IllegalStateException if the port is not open.
     */
    public String getPortName() {
          if (!isOpen()) {
              throw new IllegalStateException("Cannot get the port name as the serial port has not yet been opened.");
          }
          
          return name;
    }
    
    
    /**
     * Returns the number of bytes available for reading.
     * 
     * @return the number of bytes available for reading.
     * 
     * @throws IllegalStateException if the port is not open.
     */
    public int bytesAvailable() {
        if (!isOpen()) {
            throw new IllegalArgumentException("Cannot get the number of bytes available for reading as the port has not yet been opened.");
        }
        
        return nativeBytesAvailable(handle);
    }
    
    /**
     * Reads a byte from the serial port. If no bytes are available for
     * reading then this method will return a negative number (all read bytes are
     * in the range 0-255).
     *  
     * @return the byte read from the serial port.
     * 
     * @throws IllegalStateException if the port is not open.
     */
    public int readByte() {
        if (!isOpen()) {
            throw new IllegalArgumentException("Cannot read a byte as the port has not yet been opened.");
        }
        
        if (bytesAvailable() < 1) {
            return -1;
        }
        
        int value = nativeReadByte(handle);
        
        return (value & 0xFF);
    }
    
    /**
     * Writes a byte to the serial port.
     * 
     * @param value the byte to write to the serial port.
     * 
     * @throws IllegalStateException if the port is not open.
     */
    public void writeByte(int value) {
        if (!isOpen()) {
            throw new IllegalArgumentException("Cannot write a byte as the port has not yet been opened.");
        }
        
        int lowestByte = (value & 0xFF);
        nativeWriteByte(handle, lowestByte);
    }
    
    /**
     * Ensures all bytes written with writeByte() have been written to the 
     * serial port. If any bytes are waiting in a write buffer they will be
     * written.
     * 
     * @throws IllegalStateException if the port is not open.
     */
    public void flush() {
        if (!isOpen()) {
            throw new IllegalArgumentException("Cannot flush the write buffer as the port has not yet been opened.");
        }
        
        nativeFlush(handle);
    }
    
    /**
     * Opens the serial port using a native implementation.
     * 
     * @param portName the name of the port device. For example "/dev/ttyS0"
     * 
     * @returns a handle that is used by the native library to manage the serial port.
     */
    private native int nativeOpen(String deviceName);
    
    /**
     * Closes the serial port.
     * 
     * @param handle the native device handle obtained from the deviceOpen() method.
     * 
     * @throws IllegalArgumentException if handle is not valid.
     */
    private native void nativeClose(int handle);
    
    /**
     * Returns the number of bytes which are available to be read.
     * 
     * @param handle the native device handle obtained from the deviceOpen() method.
     * 
     * @return the number of bytes available to be read from the serial port.
     * 
     * @throws IllegalArgumentException if handle is not valid.
     */
    private native int nativeBytesAvailable(int handle);
    
    /**
     * Reads a byte from the serial port. If no bytes are available for reading 
     * then this method will return a negative number (all read bytes are in the
     * range 0-255).
     *  
     * @param handle the native device handle obtained from the deviceOpen() method.
     * 
     * @return the byte read from the serial port.
     * 
     * @throws IllegalArgumentException if handle is not valid.
     */
    private native int nativeReadByte(int handle);
    
    /**
     * Writes a byte to the serial port.
     * 
     * @param handle the native device handle obtained from the deviceOpen() method.
     * @param value the byte to write to the serial port.
     * 
     * @throws IllegalArgumentException if handle is not valid.
     */
    private native void nativeWriteByte(int handle, int value);
    
    /**
     * Ensures all bytes written with nativeWriteByte() have been written to the 
     * serial port. If any bytes are waiting in a write buffer they will be
     * written.
     * 
     * @param handle the native device handle obtained from the deviceOpen() method.
     * 
     * @throws IllegalArgumentException if handle is not valid.
     */
    private native void nativeFlush(int handle);
}
