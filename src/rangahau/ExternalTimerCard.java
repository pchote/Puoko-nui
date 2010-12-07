/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

package rangahau;

/**
 * An exernal timer card that is connected to the parallel port. This class
 * contains native methods that are implemented in the native "timercard" library.
 * 
 * @author Mike Reid
 */
public class ExternalTimerCard implements TimerCard {

    // Load the attached JNI library.
    static {
        System.loadLibrary("timercard");
    }
    
    /**
     * True if the port the external timer card connects to has been opened
     * for access.
     */
    boolean isOpen = false;
    
    /**
     * An identifier used by the native library to manage the device.
     */
    int deviceHandle = -1;

    /**
     * Closes a parallel port that was opened with openPort().
     *
     * If the port could not be closed successfully (a negative value was returned)
     * then the return value can be given as the argument to the xtcGetErrorMessage()
     * function to obtain a description of the cause of the problem.
     *
     * @throws RuntimeException if there was a problem closing the port.
     */
    public void closePort() {
        if (!isOpen) {
            return; // There device is not open, so we don't need to do anything.
        }

        nativeClosePort(deviceHandle);

        isOpen = false; // indicate the device isn't open anymore.
    }

    /**
     * Opens a parallel port for exclusive access.
     *
     * A port that is successfully opened with this function must be closed with the 
     * closePort() method. If the port was not successfully opened then closePort() 
     * does not need to be called.
     *
     * @parameter deviceName the Linux device name for the parallel port to open
     *            that is connected to the timng card, for example "/dev/parport0".
     *
     * @throws IllegalArgumentException if deviceName is null or an empty string.
     */
    public void openPort(String deviceName) {
        if (deviceName == null) {
            throw new IllegalArgumentException("Cannot open the port for the timer card as the device name for the port was null.");
        }
        if (deviceName.trim().length() < 1) {
            throw new IllegalArgumentException("Cannot open the port for the timer card as the device name for the port was an empty string.");
        }

        System.out.println("Opening ExternalTimerCard device at \'" + deviceName + "\'");
        deviceHandle = nativeOpenPort(deviceName);
        System.out.println("Open gave device at handle = " + deviceHandle);
        
        // If we got here then the device was opened successfully.
        isOpen = true;
    }

    /**
     * Switches power on to the timer card on.
     *
     * @throws IllegalStateException if the timer card has not yet been opened.
     */
    public void powerOn() {
        if (!isOpen) {
            throw new IllegalStateException("Cannot turn the power on as the timing card has not been opened.");
        }

        // Turn the power on by setting all bits to high on the data port.
        int powerOnValue = 0xff;
        nativeWriteData(deviceHandle, powerOnValue);
        try {
            Thread.sleep(100); // Wait for the device to power up (4 RC times).
        } catch (InterruptedException ex) {
        // Ignore any interruptions.
        }
    }

    /**
     * Sets whether the rising edge (low-to-high voltage transition) or falling edge
     * (high-to-low voltage transition) is used as the start of a timing pulse. Note
     * that GPS 1 Hz timing pulses start with a rising edge (they are low-to-high
     * voltage pulses).
     *
     * @parameter useRisingEdge indicates the type of voltage transition to use for the
     *                          start of a timing pulse. Use true if the rising should 
     *                          be used, or false if the falling edge should be used.
     *
     * @throws IllegalStateException if the timer card has not yet been opened.
     */
    public void setTimingEdgeType(boolean useRisingEdge) {
        if (!isOpen) {
            throw new IllegalStateException("Cannot set the timing edge as the timing card has not been opened.");
        }

        int edgeBit = 0x01; // The lowest bit of the control port determines the timing edge type.
        int enable = 0;     // Indicates whether the edge type should be rising or falling.

        if (useRisingEdge) {
            enable = 1;
        }

        nativeWriteControlBit(deviceHandle, edgeBit, enable);
    }

    /** Gets whether the rising edge (low-to-high voltage transition) or falling edge
     * (high-to-low voltage transition) is used as the start of a timing pulse. Note
     * that GPS 1 Hz timing pulses start with a rising edge (they are low-to-high
     * voltage pulses).
     *
     * @returns  true if rising edges are being used, and false means falling 
     *           edges are being used.   
     * 
     * @throws IllegalStateException if the timer card has not yet been opened.
     */
    public boolean getTimingEdgeType() {
        if (!isOpen) {
            throw new IllegalStateException("Cannot get the timing edge as the timing card has not been opened.");
        }

        int edgeBit = 0x01; // The lowest bit of the control port determines the timing edge type.
        int edgeType = nativeReadControl(deviceHandle);

        if (edgeType == 0) {
            return true; // nb. 0 means rising edges are being used.
        } else {
            return false; // nb. non-zero means falling edges are being used. 
        }
    }

    /**
     * Sets whether the external timing card will generate interrupts or not whenever
     * an output pulse (check this, is it for input pulses instead?) is generated 
     * by the card.
     *
     * @parameter enable indicates whether interrupts should be generated or not. 
     *            Use false to disable interrupts, and true to enable them. 
     * 
     * @throws IllegalStateException if the timer card has not yet been opened.   
     */
    public void setInterrupts(boolean enable) {
        if (!isOpen) {
            throw new IllegalStateException("Cannot set the interrupt type as the timing card has not been opened.");
        }

        int interruptBit = 0x10; // The bit 4 of the control port determines whether interrupts are generated.
        int useInterrupts = 0;   // Indicates whether the interrupts should be enabled or disabled.

        if (enable) {
            useInterrupts = 1;
        }
        nativeWriteControlBit(deviceHandle, interruptBit, useInterrupts);
    }

    /**
     * Enables whether GPS input pulses will be read by the external timer card.
     * Disabling input pulses from the GPS allows input pulses to be simulated using
     * the simulateTestSignal() function.
     *
     * @parameter enable indicates whether input pulses should be obtained from the
     *            GPS input or not. Use false to disable GPS input pulses, 
     *            and true to enable them.   
     *
     * @throws IllegalStateException if the timer card has not yet been opened.
     */
    public void enableInputPulses(boolean enable) {
        if (!isOpen) {
            throw new IllegalStateException("Cannot enable input pulses as the timing card has not been opened.");
        }

        int enableBit = 0x04;  // Bit 2 of the control port determines whether input pulses are enabled.
        int usePulses = 0;     // Indicates whether GPS input pulses should be enabled.

        if (enable) {
            usePulses = 1;
        }
        nativeWriteControlBit(deviceHandle, enableBit, usePulses);
    }

    /**
     * Instructs the external timer card to set the value of the input signal line.
     * This can be used for testing the input pulse operation (and for detecting
     * the presence of the external timer card itself). 
     *
     * This function should only be called when the timer card is in simulation mode,
     * which is entered by disabling GPS input pulses with enableInputPulses().
     * Once in simulation mode this setTestSignal() can be used to set the logic
     * level that can be read by the getInputPulseLevel() function.
     *
     * @parameter level indicates the logic level that should be used for the
     *            (simulated) input signal level.   
     *
     * @throws IllegalStateException if the timer card has not yet been opened.
     */
    public void setTestSignal(boolean level) {
        if (!isOpen) {
            throw new IllegalStateException("Cannot set the test signal level as the timing card has not been opened.");
        }

        int testBit = 0x02; // Bit 1 of the control port sets the test input signal level. Note: this bit has inverted voltage level, but this is handled by the timer card. 
        int testLevel = 0;  // The logic level of the test input signal.

        if (level) {
            testLevel = 1;
        }

        nativeWriteControlBit(deviceHandle, testBit, testLevel);
    }

    /**
     * Instructs the external timer card to send a short low-to-high pulse along
     * the input signal line. This can be used for testing the input pulse operation
     * (and for detecting the presence of the external timer card itself). 
     *
     * This function should only be called when the timer card is in simulation mode,
     * which is entered by disabling GPS input pulses with enableInputPulses().
     * Once in simulation mode this sendTestPulse() can be used to simulate 
     * incoming GPS timing pulses. After enough of these pulses have been received 
     * the output signal level should change (which can be detected using 
     * getOutputSignalLevel()). 
     * 
     * @throws IllegalStateException if the timer card has not yet been opened.
     */
    public void sendTestPulse() {
        if (!isOpen) {
            throw new IllegalStateException("Cannot send a test pulse as the timing card has not been opened.");
        }

        // Generate the pulse by simulating the transition of the input logic level
        // from low-to-high and back to low again.
        setTestSignal(true);
        try {
            Thread.sleep(1); // Hold the simulated level high for a millisecond.
        } catch (InterruptedException ex) {
        // Ignore any interruptions.
        }
        setTestSignal(false);
    }

    /**
     * Sets the exposure time (in seconds). The exposure time is the number of input
     * pulses that must occur before the external timer card will output a pulse
     * on its output signal line.
     *
     * At present the exposure time must be a value between 0 and 30 seconds. An
     * exposure time of 0 stops the external timer card from producing output pulses.
     *
     * @parameter exposureTime the number of seconds that should elapse between
     *            pulses on the output signal line of the external timer card 
     *            (which defines the exposure time of the connected camera). A value
     *            of zero stops output signals from being emitted by the external
     *            timer card.
     *
     * @throws IllegalArgumentException if exposureTime is less than 0 seconds or
     *                                  more than 30 seconds.
     * @throws IllegalStateException if the timer card has not yet been opened.
     */
    public void setExposureTime(int exposureTime) {
        if (!isOpen) {
            throw new IllegalStateException("Cannot set the exposure time as the timing card has not been opened.");
        }

        final int minExposureTime = 0;  // The minimum exposure time (in seconds).
        final int maxExposureTime = 30; // The maximum exposure time (in seconds).

        // Check that the requested exposure time is within the range of valid
        // exposure times. At present this range is 0-31 seconds as only 5 bits are 
        // accepted by the external timer card for the exposure time).
        if (exposureTime < minExposureTime) {
            throw new IllegalArgumentException("Cannot set the exposure time as the value given was below the minimum of " + minExposureTime + ", the value given was " + exposureTime);
        }
        if (exposureTime > maxExposureTime) {
            throw new IllegalArgumentException("Cannot set the exposure time as the value given above the maximum of " + maxExposureTime + ", the value given was " + exposureTime);
        }

        int setExposureBit = 0x20;   // Bit 5 of the data port is used to indicate the exposure time is being set.
        int exposureTimeMask = 0x1F; // Bits 0-4 of the data port holds the exposure time (in seconds).   
        int byteValue = 0;           // Used to compose the value to be written to the data port.
        int upperBits = 0xC0;        // Bits 6-7 of the byte switched on, the others switched off.
        int allBits = 0xFF;          // All bits of the byte turned on. */

        // Write the exposure time into the data port. Ensure the set exposure bit
        // (bit 5) is low.
        byteValue = upperBits | (exposureTime & exposureTimeMask);
        nativeWriteData(deviceHandle, byteValue);
        try {
            Thread.sleep(1); // Allow a little bit of time for the external timer card to register the change.
        } catch (InterruptedException ex) {
        // Ignore interruptions.
        }

        // Toggle the set exposure time bit high and write it into the data port.
        byteValue |= setExposureBit;
        nativeWriteData(deviceHandle, byteValue);
        try {
            Thread.sleep(1); // Allow a little bit of time for the external timer card to register the change.
        } catch (InterruptedException ex) {
        // Ignore interruptions.
        }

        // Restore all bits to 1 on the data byte.
        nativeWriteData(deviceHandle, allBits);
    }

    /**
     * Reads the input signal line from the external timer card. This line shows
     * pulses from the GPS, or simulated pulses if sendTestPulse() is being used
     * to generate pulses for testing purposes.
     *
     * @returns true if the GPS 1Hz input signal is high, or false if it is low.
     * 
     * @throws IllegalStateException if the timer card has not yet been opened.
     */
    public boolean getInputSignalLevel() {
        if (!isOpen) {
            throw new IllegalStateException("Cannot get the input signal level as the timing card has not been opened.");
        }

        int inputBit = 0x40; // Bit 6 of the status port is used to read the input signal level. 

        int value = nativeReadStatus(deviceHandle);
        if ((inputBit & value) != 0) {
            return true;
        } else {
            return false;
        }
    }

    /**
     * Reads the output signal line from the external timer card. This line shows
     * pulses from the external timer card that are intended to trigger the camera,
     * or simulated pulses if sendTestPulse() is being used to generate pulses 
     * for testing purposes.
     *
     * @returns true if the output (exposure sync pulse) signal is high, or 
     *          false if it is low.
     *
     * @throws IllegalStateException if the timer card has not yet been opened.
     */
    public boolean getOutputSignalLevel() {
        if (!isOpen) {
            throw new IllegalStateException("Cannot get the output signal level as the timing card has not been opened.");
        }

        int outputBit = 0x20; // Bit 5 of the status port is used to read the input signal level. 

        
        if (nativeGetOutputSignalLevel(deviceHandle)) {
            return true;
        } else {
            return false;
        }
        /*
        int value = nativeReadStatus(deviceHandle);

        if ((outputBit & value) != 0) {
                    System.out.println("0 ExternalTimerCard.getOutputSignalLevel() : status value = " + value 
                           + ", (outputBit & value) = " + (outputBit & value));
            return true;
        } else {
            return false;
        }
         * */
    }

    /**
     * Determines whether there is a external timer card connected to the parallel
     * port or not.
     *
     * @returns true if an external timer card was detected, false if it was not.
     * 
     * @throws IllegalStateException if the timer card port has not yet been opened.
     */
    public boolean detectTimerCard() {
        if (!isOpen) {
            throw new IllegalStateException("Cannot detect whether a timer card is present as the timing card port has not been opened.");
        }

        // Turn the signal input off and the test/simulation signal off. We expect
        // no signals to be received.
        enableInputPulses(false);
        setTestSignal(false);

        boolean inputLevel = getInputSignalLevel();
        if (inputLevel) {
            // A pulse was received, therefore we don't have a working external timer card.
            return false;
        }

        // Turn the signal input on and the test/simulation signal on. We expect
        // a signal to be received.
        enableInputPulses(true);
        setTestSignal(true);

        inputLevel = getInputSignalLevel();
        if (!inputLevel) {
            // A pulse was received, therefore we don't have a working external timer card.
            return false;
        }

        // Clear the test signal level, but leave the input signal line enabled.
        setTestSignal(false);

        // If we got here then there is an external timing card present.
        return true;
    }

    public int readStatusPort() {
        return nativeReadStatus(deviceHandle);
    }
      
    /**
     * Waits until an output signal pulse has been received or a time limit has
     * elapsed without receiving a pulse.
     * 
     * @param timelimit the maximum time to wait (in milliseconds) for an output
     *        pulse to be detected. 
     * 
     * @return true if an output pulse was detected, false if the time limit
     *         was reached without an output pulse being detected.
     */
    public boolean waitForOutputSignal(long timelimit) {
      if (!isOpen) {
          throw new IllegalStateException("Cannot detect wait for an output pulse from the timer card as the timing card port has not been opened.");
      }     
      
      final long endTime = System.currentTimeMillis() + timelimit;
      while(System.currentTimeMillis() < endTime) {
          // If the output signal level is high then we have found the sync
          // pulse we were looking for.
          if (getOutputSignalLevel()) {
              return true;
          }
      }
      
      // If we got here then the time limit was reached but the output signal
      // was not detected.
      return false;
    }

    /**
     * Waits until an intput signal pulse has been received or a time limit has
     * elapsed without receiving a pulse.
     *
     * @param timelimit the maximum time to wait (in milliseconds) for an input
     *        pulse to be detected.
     *
     * @return true if an input pulse was detected, false if the time limit
     *         was reached without an input pulse being detected.
     */
    public boolean waitForInputSignal(long timelimit) {
      if (!isOpen) {
          throw new IllegalStateException("Cannot wait for an input pulse from the timer card as the timing card port has not been opened.");
      }

      final long endTime = System.currentTimeMillis() + timelimit;
      while(System.currentTimeMillis() < endTime) {
          // If the input signal level is high then we have found the sync
          // pulse we were looking for.
          if (getInputSignalLevel()) {
              return true;
          }
      }

      // If we got here then the time limit was reached but the input signal
      // was not detected.
      return false;
    }

    /**
     * Closes a parallel port that was opened with nativeOpenPort().
     *
     * @parameter portHandle the (non-negative) handle to the open parallel port 
     *            that was obtained from a call to nativeOpenPort().
     * 
     * @throws IllegalArgumentException if portHandle is not a valid handle.
     */
    private native void nativeClosePort(int portHandle);

    /**
     * Opens a parallel port for exclusive access. If the port was opened successfully
     * then the return value is a file descriptor that can be used to manipulate the
     * parallel port (and can be passed as the portHandle parameter in other functions
     * within this file). If the parallel port could not be opened for exclusive
     * access then a RuntimeException is thrown.
     *
     * A port that is successfully opened with this function must be closed with the 
     * nativeClosePort(portHandle) function. If the port was not successfully opened 
     * then nativeClosePort() does not need to be called.
     *
     * @parameter deviceName the Linux device name for the parallel port to open,
     *            for example "/dev/parport0".
     *
     * @return a handle to the open parallel port.
     * 
     * @throww IllegalArgumentException if deviceName is null or a blank string.
     * @throws RuntimeException if there was a problem opening the specified port.
     */
    private native int nativeOpenPort(String deviceName);

    /**
     * Writes to the data port within the parallel port. The data port is the byte
     * located at the base address of the parallel port.
     *
     * @parameter portHandle the (non-negative) handle to the open parallel port 
     *            that was obtained from a call to nativeOpenPort().
     *
     * @parameter value the byte of data to be written to the data port within the
     *            parallel port.
     * 
     * @throws IllegalArgumentException if portHandle is not a valid handle.
     */
    private native void nativeWriteData(int portHandle, int value);

    /**
     * Reads from the status port within the parallel port. The status port is the byte
     * located at one byte past the base address of the parallel port (that is, 
     * base address+1).
     *
     * @parameter portHandle the (non-negative) handle to the open parallel port 
     *            that was obtained from a call to nativeOpenPort().
     *
     * @returns the value read from the 'status' byte of the parallel port.
     * 
     * @throws IllegalArgumentException if portHandle is not a valid handle.
     */
    private native int nativeReadStatus(int portHandle);

    /**
     * Reads from the control port within the parallel port. The control port is the byte
     * located at two bytes past the base address of the parallel port (that is, 
     * base address+2).
     *
     * @parameter portHandle the (non-negative) handle to the open parallel port 
     *            that was obtained from a call to nativeOpenPort().
     * 
     * @returns the value read from the 'control' byte of the parallel port.
     * 
     * @throws IllegalArgumentException if portHandle is not a valid handle.
     */
    private native int nativeReadControl(int portHandle);

    /**
     * Writes to the control port within the parallel port. The control port is the 
     * byte located at two bytes past the base address of the parallel port (that is, 
     * base address+2).
     *
     * @parameter portHandle the (non-negative) handle to the open parallel port 
     *            that was obtained from a call to native OpenPort().
     *
     * @parameter value is the value that should be written to the control port.
     * 
     * @throws IllegalArgumentException if portHandle is not a valid handle.
     */
    private native void nativeWriteControl(int portHandle, int value);

    /**
     * Writes a single bit of the control port within the parallel port. The control 
     * port is the byte located at two bytes past the base address of the parallel 
     * port (that is, base address+2).
     *
     * @parameter portHandle the (non-negative) handle to the open parallel port 
     *            that was obtained from a call to xtcOpenPort().
     *
     * @parameter bitmask identifies which bit should be written (for example,
     *            0x01 to write the zeroth bit of the byte).
     *
     * @parameter value is 0 if the bit should be switched off, and non-zero if the
     *            bit should be switched on.
     * 
     */
    private native void nativeWriteControlBit(int portHandle, int bitmask, int value);

    private native boolean nativeGetOutputSignalLevel(int portHandle);
            
}
