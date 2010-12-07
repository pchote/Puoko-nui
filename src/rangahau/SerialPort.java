/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

package rangahau;

/**
 * This interface represents an RS-232 serial port device.
 * 
 * @author Mike Reid
 */
public interface SerialPort {

    /**
     * Opens the serial port for use.
     * 
     * @param portName the name of the port device. For example "/dev/ttyS0".
     * 
     * @throws IllegalArgumentException if portName is null or a blank string.
     */
    public void open(String portName);
    
    /**
     * Returns true if the serial port has been opened for use using the open()
     * method.
     * 
     * @return true if the port is open for use, false if it is not.
     */
    public boolean isOpen();
    
    /**
     * Closes the serial port. It is safe to call close() on a serial port 
     * that is already been closed or has never been opened.
     */
    public void close();
    
    /**
     * Returns the name of the serial port device. For example "/dev/ttyS0".
     * 
     * @return the name of the serial port device.
     * 
     * @throws IllegalStateException if the port is not open.
     */
    public String getPortName();
    
    
    /**
     * Returns the number of bytes available for reading.
     * 
     * @return the number of bytes available for reading.
     * 
     * @throws IllegalStateException if the port is not open.
     */
    public int bytesAvailable();
    
    /**
     * Reads a byte from the serial port. If no bytes are available for
     * reading then this method will block until a byte becomes available.
     *  
     * @return the byte read from the serial port.
     * 
     * @throws IllegalStateException if the port is not open.
     */
    public int readByte();
    
    /**
     * Write a byte to the serial port.
     * 
     * @param value the byte to write to the serial port.
     * 
     * @throws IllegalStateException if the port is not open.
     */
    public void writeByte(int value);
    
    /**
     * Ensures all bytes written with writeByte() have been written to the 
     * serial port. If any bytes are waiting in a write buffer they will be
     * written.
     * 
     * @throws IllegalStateException if the port is not open.
     */
    public void flush();
}
