/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

package rangahau;

import java.util.Calendar;

/**
 * Simulates the operation of an exernal timer card.
 * 
 * @author Mike Reid
 */
public class TimerCardSimulator implements TimerCard {

    boolean isOpen = false;
    boolean usingRisingEdge = true;
    boolean inputEnabled = true;
    boolean inputLevel = false;
    int exposureTime = 1;
    
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
        isOpen = false;
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
      
      usingRisingEdge = useRisingEdge;
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
      
      return usingRisingEdge;
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
       
       // Actually don't care about the interrupt status when simulating.
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
      
      inputEnabled = enable;
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
      
      inputLevel = level;
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
      
      setTestSignal(false);
      setTestSignal(true);
      try {
        Thread.sleep(1); // Hold the simulated level high for a millisecond.
      } catch(InterruptedException ex) {
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
      
      final int minExposureTime = 0;
      final int maxExposureTime = 30;
      
      if (exposureTime < minExposureTime) {
          throw new IllegalArgumentException("Cannot set the exposure time as the value given was below the minimum of "
                                             + minExposureTime + ", the value given was " + exposureTime);
      }
      if (exposureTime > maxExposureTime) {
          throw new IllegalArgumentException("Cannot set the exposure time as the value given above the maximum of " 
                                             + maxExposureTime + ", the value given was " + exposureTime);
      }

      this.exposureTime = exposureTime;
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
      
      if (!inputEnabled) {
          return false;
      }
      
      return inputLevel;
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
          throw new IllegalStateException("Cannot get the input signal level as the timing card has not been opened.");
      }         
      
      // If there are no input pulses then there will be no output pulses.
      if (!inputEnabled) {
          return false;
      }        
      
      // Simulate this by putting out an output pulse if the current time
      // has a seconds value that is a multiple of the exposure time.
      Calendar now = Calendar.getInstance();
      if (now.get(Calendar.SECOND) % exposureTime == 0) {
          return true;
      }
      
      return false;
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
      
      return true;
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
}
