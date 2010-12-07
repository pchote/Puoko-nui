/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/
/* 
 * File:   testtimercard.c
 * Author: sullivan
 *
 * Created on 28 March 2008, 13:34
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "timercard.h"

#define PARALLELPORT_DEVICE "/dev/parport0" /* Name of the parallel port device
                                             * attached to the external timer
                                             * card, eg. "/dev/parport0". */

void runTests();
void testOpenPort();
void testClosePort();
void testPowerOn();
void testDetectTimerCard();
void testWriteData();
void testGetInputSignalLevel();
void testGetOutputSignalLevel();
void testInterrupts();

/*
 * 
 */
int main(int argc, char** argv) {

    runTests();
    
    return (EXIT_SUCCESS);
}

void runTests() {
 
    testOpenPort();
    testClosePort();
    testPowerOn();
    testDetectTimerCard();
    testWriteData();
    testGetInputSignalLevel();
    testGetOutputSignalLevel();
    testInterrupts();
}

void testOpenPort() {
    char* testResult = "OK";
    int portHandle = -1;
    
    printf("Entering testOpenPort() ...\n");
    
    portHandle = xtcOpenPort(PARALLELPORT_DEVICE);
    if (portHandle < 0) {
        testResult = "fail - could not open port.";
    } else {
        xtcClosePort(portHandle);
    }

    printf("Exiting testOpenPort() : test result = %s\n", testResult);
}

void testClosePort() {
    char* testResult = "OK";
    int portHandle = -1;
    printf("Entering testClosePort() ...\n");

    portHandle = xtcOpenPort(PARALLELPORT_DEVICE);
    if (portHandle < 0) {
        testResult = "fail - could not open port.";
    } else {
        xtcClosePort(portHandle);
    }
    
    /* If the port was opened and closed successfully then we try re-oppening
     * again. If the close operation wasn't performed properly then we won't
     * be able to reopen the port (so we are actually testing that the close
     * operation was successful). */
    if (portHandle >= 0) {
        portHandle = -1;
        portHandle = xtcOpenPort(PARALLELPORT_DEVICE);
        if (portHandle < 0) {
            testResult = "fail - could not open port a second time. This probably means it wasn't closed properly.";
        } else {
            xtcClosePort(portHandle);
        }        
    }

    printf("Exiting testClosePort() : test result = %s\n", testResult);
    
}

void testPowerOn() {
    char* testResult = "OK";
    int portHandle = -1;
    int status = -1;
    
    printf("Entering testPowerOn() ...\n");
    
    portHandle = xtcOpenPort(PARALLELPORT_DEVICE);
    if (portHandle < 0) {
        testResult = "fail - could not open port.";
    }
    
    /* If we have a valid open port we should try to switch power on to the
     * device. */
    if (portHandle >= 0) {
      status = xtcPowerOn(portHandle);
      if (status != 0) {
          testResult = "fail - a problem occured when switching power on to the device.";
          testResult = (char*) xtcGetErrorMessage(status);
      }
    }
    
    if (portHandle >= 0) {
        xtcClosePort(portHandle);
    }
    
    printf("Exiting testPowerOn() : test result = %s\n", testResult); 
}

void testDetectTimerCard() {
    char* testResult = "OK";
    int portHandle = -1;
    int status = -1;
    int cardPresent = 0;
    
    printf("Entering testDetectTimerCard() ...\n");
    
    portHandle = xtcOpenPort(PARALLELPORT_DEVICE);
    if (portHandle < 0) {
        testResult = "fail - could not open port.";
    }
    
    /* If we have a valid open port we should try to switch power on to the
     * device. */
    if (portHandle >= 0) {
        status = xtcPowerOn(portHandle);
      if (status != 0) {
          testResult = "fail - a problem occured when switching power on to the device.";
      }
    }
    
    /* If we have a valid open port we should try to detect whether an
     * external timing card is present.
     */
    if ( (portHandle >= 0) && (status == 0) )  {
      status = xtcDetectTimerCard(portHandle, &cardPresent);
      if (status != 0) {
          testResult = "fail - could not detect whether a timing card was present or not.";
      } else {
          if (!cardPresent) {
              testResult = "fail - no timing card was detected.";
          }
      }
    }
    
    if (portHandle >= 0) {
        xtcClosePort(portHandle);
    }
    
    printf("Exiting testDetectTimerCard() : test result = %s\n", testResult);    
}

void testWriteData() {
    char* testResult = "OK";
    int portHandle = -1;
    int status = -1;
    int cardPresent = 0;

    printf("Entering testWriteData() ...\n");
    
    portHandle = xtcOpenPort(PARALLELPORT_DEVICE);
    if (portHandle < 0) {
        testResult = "fail - could not open port.";
    }
    
    /* If we have a valid open port we should try to switch power on to the
     * device. */
    if (portHandle >= 0) {
        status = xtcPowerOn(portHandle);
      if (status != 0) {
          testResult = "fail - a problem occured when switching power on to the device.";
      }
    }
    
    /* If we have a valid open port we should try to detect whether an
     * external timing card is present.
     */
    if ( (portHandle >= 0) && (status == 0) ) {
      status = xtcDetectTimerCard(portHandle, &cardPresent);
      if (status != 0) {
          testResult = "fail - could not detect whether a timing card was present or not.";
      } else {
          if (!cardPresent) {
              testResult = "fail - no timing card was detected so couldn't perform a write.";
          }
      }
    }

    if ( (portHandle >= 0) && cardPresent && (status == 0) ) {
      int valueToWrite = 0xFF;
      status = xtcWriteData(portHandle, valueToWrite);
      if (status != 0) {
          testResult = "fail - a problem occured when writing data to the port";
          testResult = (char*) xtcGetErrorMessage(status);
      }      
    }

    /* Try writing additional data to the port. */
    if ( (portHandle >= 0) && cardPresent && (status == 0) ) {
      int valueToWrite = 0xCA;
      status = xtcWriteData(portHandle, valueToWrite);
      if (status != 0) {
          testResult = "fail - a problem occured when writing additional data to the port";
      }      
    }
    
    if (portHandle >= 0) {
        xtcClosePort(portHandle);
    }

    printf("Exiting testWriteData() : test result = %s\n", testResult);
}

void testGetInputSignalLevel() {
    char* testResult = "OK";
    int portHandle = -1;
    int status = -1;
    int cardPresent = 0;
    int inputLevel = 0;
    
    printf("Entering testGetInputSignalLevel() ...\n");
    
    portHandle = xtcOpenPort(PARALLELPORT_DEVICE);
    if (portHandle < 0) {
        testResult = "fail - could not open port.";
    }
    
    /* If we have a valid open port we should try to switch power on to the
     * device. */
    if (portHandle >= 0) {
      status = xtcPowerOn(portHandle);
      if (status != 0) {
          testResult = "fail - a problem occured when switching power on to the device.";
      }
    }
    
    /* If we have a valid open port we should try to detect whether an
     * external timing card is present.
     */
    if ( (portHandle >= 0) && (status == 0) )  {
      status = xtcDetectTimerCard(portHandle, &cardPresent);
      if (status != 0) {
          testResult = "fail - could not detect whether a timing card was present or not.";
      } else {
          if (!cardPresent) {
              testResult = "fail - no timing card was detected.";
          }
      }
    }
    
    /* Enable input from the GPS source. */
    if ( (portHandle >= 0) && (status == 0) ) {
      status = xtcEnableInputPulses(portHandle, 1);
      if (status != 0) {
          testResult = "fail - could not enable input pulses.";
      }
    }    
    
    /* Check we can read input pulses from the card. */
    if ( (portHandle >= 0) && (status == 0) ) {
      time_t timeLimit = 2; /* Allow a few seconds to detect an input pulse. */
      time_t endTime = time(0) + timeLimit; /* The pulse must be received by this time or a timeout will occur. */
      int timeout = 0; /* Non-zero when the timeout to read an input pulse has been exceeded. */
      while( (status == 0) && (inputLevel == 0) && (!timeout) ) {
         status = xtcGetInputSignalLevel(portHandle, &inputLevel);
         if (!inputLevel && (time(0) > endTime) ) {
             timeout = 1;
         }
      }
      if (status != 0) {
          testResult = "fail - there was a problem trying to read an input pulse.";
      }      
      if (timeout != 0) {
          testResult = "fail - timeout, could not detect an input pulse during the time allowed.";
      }
      if (inputLevel == 0) {
          testResult = "fail - no input signal was detected.";
      }
    }

    if (portHandle >= 0) {
        xtcClosePort(portHandle);
    }
    
    printf("Exiting testGetInputSignalLevel() : test result = %s\n", testResult); 
}

void testGetOutputSignalLevel() {
    char* testResult = "OK";
    int portHandle = -1;
    int status = -1;
    int cardPresent = 0;
    int signalLevel = 0;
    
    printf("Entering testGetOutputSignalLevel() ...\n");
    
    portHandle = xtcOpenPort(PARALLELPORT_DEVICE);
    if (portHandle < 0) {
        testResult = "fail - could not open port.";
    }
    
    /* If we have a valid open port we should try to switch power on to the
     * device. */
    if (portHandle >= 0) {;
      status = xtcPowerOn(portHandle);
      if (status != 0) {
          testResult = "fail - a problem occured when switching power on to the device.";
      }
    }
    
    /* If we have a valid open port we should try to detect whether an
     * external timing card is present.
     */
    if ( (portHandle >= 0) && (status == 0) )  {
      status = xtcDetectTimerCard(portHandle, &cardPresent);
      if (status != 0) {
          testResult = "fail - could not detect whether a timing card was present or not.";
      } else {
          if (!cardPresent) {
              testResult = "fail - no timing card was detected.";
          }
      }
    }
    
    /* Enable input from the GPS source. */
    if ( (portHandle >= 0) && (status == 0) ) {
      status = xtcEnableInputPulses(portHandle, 1);
      if (status != 0) {
          testResult = "fail - could not enable input pulses (required so that output pulses are generated).";
      }
    }    
    
    /* Set the exposure time. */
    if ( (portHandle >= 0) && (status == 0) ) {
      int exposureTime = 10; /* The desired exposure time (in seconds). */
      status = xtcSetExposureTime(portHandle, exposureTime);
      if (status != 0) {
          testResult = "fail - could not set the exposure time.";
      }
    }     
    
    /* Check we can read output pulses from the card. */
    if ( (portHandle >= 0) && (status == 0) ) {
      time_t timeLimit = 32; /* Allow the maximum exposure time (plus a second) to detect an output pulse. */
      time_t startTime = time(0);
      time_t endTime = startTime + timeLimit; /* The pulse must be received by this time or a timeout will occur. */
      int timeout = 0; /* Non-zero when the timeout to read an output pulse has been exceeded. */
      while( (status == 0) && (signalLevel == 0) && (!timeout) ) {
         status = xtcGetOutputSignalLevel(portHandle, &signalLevel);
         if (!signalLevel && (time(0) > endTime) ) {
             timeout = 1;
         }
      }
      if (status != 0) {
          testResult = "fail - there was a problem trying to read an output pulse.";
      }      
      if (timeout != 0) {
          testResult = "fail - timeout, could not detect an output pulse during the time allowed.";
      }
      if (signalLevel == 0) {
          testResult = "fail - no output signal was detected.";
      } else {
          printf("The time taken to detect an output pulse was %li seconds\n", time(0)-startTime);
      }
    }

    if (portHandle >= 0) {
        xtcClosePort(portHandle);
    }
    
    printf("Exiting testGetOutputSignalLevel() : test result = %s\n", testResult); 
}

void testInterrupts() {
    char* testResult = "OK";
    int portHandle = -1;
    int status = -1;
    int cardPresent = 0;
    int signalLevel = 0;

    printf("Entering testInterrupts() ...\n");

    portHandle = xtcOpenPort(PARALLELPORT_DEVICE);
    if (portHandle < 0) {
        testResult = "fail - could not open port.";
    }

    /* If we have a valid open port we should try to switch power on to the
     * device. */
    if (portHandle >= 0) {;
      status = xtcPowerOn(portHandle);
      if (status != 0) {
          testResult = "fail - a problem occured when switching power on to the device.";
      }
    }

    /* If we have a valid open port we should try to detect whether an
     * external timing card is present.
     */
    if ( (portHandle >= 0) && (status == 0) )  {
      status = xtcDetectTimerCard(portHandle, &cardPresent);
      if (status != 0) {
          testResult = "fail - could not detect whether a timing card was present or not.";
      } else {
          if (!cardPresent) {
              testResult = "fail - no timing card was detected.";
          }
      }
    }

    /* Enable input from the GPS source. */
    if ( (portHandle >= 0) && (status == 0) ) {
      status = xtcEnableInputPulses(portHandle, 1);
      if (status != 0) {
          testResult = "fail - could not enable input pulses (required so that output pulses are generated).";
      }
    }

    /* Set the exposure time. */
    if ( (portHandle >= 0) && (status == 0) ) {
      int exposureTime = 10; /* The desired exposure time (in seconds). */
      status = xtcSetExposureTime(portHandle, exposureTime);
      if (status != 0) {
          testResult = "fail - could not set the exposure time.";
      }
    }

    /* Enable interrupts. */
    if ( (portHandle >= 0) && (status == 0) ) {
      status = xtcSetInterrupts(portHandle, 1);
      if (status != 0) {
          testResult = "fail - could not enable interrupts.";
      }
    }

    /* Check we can read output pulses from the card. */
    if ( (portHandle >= 0) && (status == 0) ) {
      time_t timeLimit = 32; /* Allow the maximum exposure time (plus a second) to detect an output pulse. */
      int timeout = 0; /* Non-zero when the timeout to read an output pulse has been exceeded. */
      int count = 0;
      time_t startTime = 0;
      time(&startTime);

      while( (status == 0) && (!timeout) && (count < 5) ) {
         timeout = xtcWaitForInterrupt(portHandle, 1100);
         time_t now = 0;
         time(&now);
         printf("Received interrupt: %lf s\n", difftime(startTime, now));
         ++count;
      }
      if (status != 0) {
          testResult = "fail - there was a problem trying to read an output pulse via interrupts.";
      }
      if (timeout != 0) {
          testResult = "fail - timeout, could not detect an interrupt during the time allowed.";
      }
    }

    if (portHandle >= 0) {
        xtcClosePort(portHandle);
    }

    printf("Exiting testInterrupts() : test result = %s\n", testResult);
}




