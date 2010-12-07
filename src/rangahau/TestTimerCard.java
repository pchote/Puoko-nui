/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */

package rangahau;

/**
 *
 * @author reid
 */
public class TestTimerCard {
    public static void main(String[] args) {
        TestTimerCard application = new TestTimerCard();
    }
    
    public TestTimerCard() {
        ExternalTimerCard timercard = new ExternalTimerCard();

        timercard.openPort("/dev/parport0");
        timercard.powerOn();
        timercard.setInterrupts(false);
        timercard.setTimingEdgeType(false); // Use the falling edge of the GPS input pulse.
        timercard.enableInputPulses(true);
        System.out.println("detect timer card = " + timercard.detectTimerCard());
        
        final int exposureTime = 3; // The exposure time (in seconds).
        final long exposureTimeMillis = exposureTime * 1000; // the exposure time (in milliseconds).
        timercard.setExposureTime(exposureTime);
        
        int count = 0;
        
        long lastSyncPulse = 0;
        long lastInputPulse = 0;
        while(count < 20) {
            boolean found = false;
            while (!found) {
//                if (timercard.getInputSignalLevel()) {
//                    long inputPulseTime = System.currentTimeMillis();
//                    //System.out.println("Timercard input pulse detected at " + inputPulseTime);
//                    if (lastInputPulse != 0) {
//                        System.out.println("Time between input pulses (in milliseconds) = " 
//                                           + (inputPulseTime - lastInputPulse) + ", expected Time = 1000");
//   
//                    }
//                    lastInputPulse = inputPulseTime;
//                }
                if (timercard.getOutputSignalLevel()) {
                    long syncPulseTime = System.currentTimeMillis();
                    //System.out.println("Timercard output pulse detected at " + syncPulseTime);
                    if (lastSyncPulse != 0) {
                        System.out.println("Time between sync pulses (in milliseconds) = " 
                                           + (syncPulseTime - lastSyncPulse) + ", expected Time = " + exposureTimeMillis);
                    }
                    lastSyncPulse = syncPulseTime;
                    found = true;
                }
                
                try {
                    Thread.sleep(0, 500);
                } catch(Throwable th) {
                    // ignore interruptions.
                }
            }

            ++count;
           
        }
        timercard.closePort();  
    }
}
