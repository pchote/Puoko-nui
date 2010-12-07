/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "timercard.h"

int
main(char* argv[], int argc) {
  int portHandle = -1;
  int status = -1;
  int count = 0;
  int cardPresent = 0;
  int found = 0;

  portHandle = xtcOpenPort("/dev/parport0");
  if (portHandle < 0) {
    printf("could not open port.\n");
    return 1;
  }

  xtcPowerOn(portHandle);
  xtcDetectTimerCard(portHandle, &cardPresent);
  xtcEnableInputPulses(portHandle, 1);
  /*xtcSetExposureTime(portHandle, 5);*/
 
  while(count < 20) {
     int signalLevel = 0;
     found = 0;
     while(!found){
      
      unsigned char value = 0;
      int outputBit = 0x20;
      status = xtcReadStatus(portHandle, &value);
        if ((value & 0x20) != 0) {
          printf("Found status port 'output' pulse, %li, value = %i\n", time(0), value);
          found = 1;
        }
      
      /*  
         status = xtcGetOutputSignalLevel(portHandle, &signalLevel);
         if (signalLevel) {
             found = 1;
             printf("Found status port 'output' pulse, %li\n", time(0));
         }
       */  
      }

/*    unsigned char value = 0;
    status = xtcReadStatus(portHandle, &value);
    if (status != 0) {
      printf("There was a problem reading from the status port. %s\n", xtcGetErrorMessage(status));
      return 2;
    }
    printf("C program : parport0 statis port value = %i\n", value);
*/
    sleep(1); // sleep for a second.
  }
    
  if (portHandle >= 0) {
    xtcClosePort(portHandle);
  }

  return 0;
}
