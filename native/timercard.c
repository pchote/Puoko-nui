/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

/**
 * Controls the external timer card (xtc) that is attached to the parallel port.
 * This card counts 1 Hz pulses from a GPS source and after a number of pulses
 * (controlled by the user) it outputs a pulse. This output pulse is sent to the
 * CCD camera and is used to control:
 *   + frame start 
 *   + the exposure time of the new frame, and
 *   + readout of the previous frame (since the camera is a frame-transfer device).
 *
 * The parallel port bits used are:
 *  Data port (byte at parallel port base address):
 *    bits 0-5: Used to set the exposure time (in seconds).
 *    bit 6: Toggled to indicate that exposure time is being set.    
 *    bits 0-7: Turn power on to the card when all bits are set high.
 *
 *  Status port (byte at parallel port base address+1):
 *    bits 0-4: Not used 
 *    bit 5 (0x20): Output pulse level (shows signal from external timer card to camera trigger).
 *    bit 6 (0x40): Input pulse level (shows signal from GPS into external timer card).
 *    bit 7 (0x80): Not used. 
 *
 *  Control port (byte at parallel port bases addres+2):
 *    bit 0 (0x01): Select whether rising or falling voltage transition is used as the start of timing pulse.
 *    bit 1 (0x02): Manipulates the test signal (used in simulation mode). This bit is inverted on the parallel port.
 *    bit 2 (0x04): Enables and disables input pulse counting by the external timer card.
 *    bit 3 (0x08): Not used.
 *    bit 4 (0x10): Enables or disables interrupt generation by the external timer card.
 *    bits 5-7:     Not used. 
 *
 * The implementation of this timer card control functions reply on the Linux
 * user-space parallel port driver called 'ppdev'. The kernel modules required
 * to implement the ppdev functionality are:
 *   + parport 
 *   + partport_pc
 *   + ppdev
 *
 * Note: The Linux kernel module called 'lp' may try and access the port as well.
 *       It is best to remove this kernel when using the external timer card 
 *       (using the command 'rmmod lp' as root).
 *
 * Programming the parallel port using ppdev is described at:
 *   http://people.redhat.com/twaugh/parport/html/parportguide.html
 * This documentation states it is for Linux 2.4 (and Linux 2.2) but it is also
 * applicable to Linux 2.6 as well.  
 *
 * Author: Michael Reid (email: Michael.Reid@paradise.net.nz)
 *
 * Changelog
 * =========
 *
 * 2008-03-27 Initial implementation.
 *
 */

#include <linux/parport.h>
#include <linux/ppdev.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <string.h>
#include <time.h>

#ifndef _XOPEN_SOURCE
  #define _XOPEN_SOURCE 500 /* This define is required to see the usleep() function. */
#endif

#include <unistd.h>
#include <sys/io.h>
#include <poll.h>

#include <stdio.h>

#include <string>

#include "timercard.h"

#define XTC_ERRORCODE_NOERROR                      0  /* Indicates that an error did not occur. */
#define XTC_ERRORCODE_OPEN_NULLNAME             -100  /* The parallel-port device name given was null. */
#define XTC_ERRORCODE_OPEN_BLANKNAME            -101  /* The parallel-port device name given was blank. */
#define XTC_ERRORCODE_OPEN_NODEVICE             -102  /* Cannot access the device file in /dev */        
#define XTC_ERRORCODE_OPEN_CANNOTOPEN           -103  /* Could not successfully open the device file. */
#define XTC_ERRORCODE_OPEN_NOEXCLUSIVE          -104  /* Could not set exclusive port access. */ 
#define XTC_ERRORCODE_OPEN_NOCLAIM              -105  /* Could not claim the parallel port for use. */
#define XTC_ERRORCODE_OPEN_SETMODE              -106  /* Could not set the IEEE1284 mode. */
#define XTC_ERRORCODE_OPEN_DIRECTION            -107  /* Could not set the data port direction. */
#define XTC_ERRORCODE_OPEN_INTERRUPTS           -108  /* Could not disable interupt generation from the port. */
#define XTC_ERRORCODE_CLOSE_BADHANDLE           -110  /* An invalid parallel port file descriptor ('handle') was given. */
#define XTC_ERRORCODE_CLOSE_NOCLAIM             -111  /* Could not successfully release the claim to the port. */
#define XTC_ERRORCODE_CLOSE_NOCLOSE             -112  /* Could not successfully close the device file in /dev. */
#define XTC_ERRORCODE_WRITEDATA_BADHANDLE       -120  /* An invalid parallel port file descriptor was given when writing to the data port. */
#define XTC_ERRORCODE_WRITEDATA_BADWRITE        -121  /* There was a problem when writing a byte to the data port. */   
#define XTC_ERRORCODE_READSTATUS_BADHANDLE      -130  /* An invalid parallel port file descriptor was given when reading from the status port. */
#define XTC_ERRORCODE_READSTATUS_BADVALUE       -131  /* An invalid pointer was given to the destination byte for reading from the status port. */
#define XTC_ERRORCODE_READSTATUS_BADREAD        -132  /* There was a problem when reading a byte from the status port. */        
#define XTC_ERRORCODE_READCONTROL_BADHANDLE     -140  /* An invalid parallel port file descriptor was given when reading from the control port. */
#define XTC_ERRORCODE_READCONTROL_BADVALUE      -141  /* An invalid pointer was given to the destination byte for reading from the control port. */
#define XTC_ERRORCODE_READCONTROL_BADREAD       -142  /* There was a problem when reading a byte from the control port. */ 
#define XTC_ERRORCODE_WRITECONTROL_BADHANDLE    -150  /* An invalid parallel port file descriptor was given when writing to the control port. */
#define XTC_ERRORCODE_WRITECONTROL_BADWRITE     -151  /* There was a problem when writing a byte to the control port. */
#define XTC_ERRORCODE_WRITECONTROLBIT_BADHANDLE -160  /* An invalid parallel port file descriptor was given when writing a bit to the contro port. */ 
#define XTC_ERRORCODE_WRITECONTROLBIT_BADWRITE  -161  /* There was a problem when writing a bit to the control port. */

#define XTC_ERRORCODE_POWERON_BADHANDLE         -200  /* An invalid parallel port file descriptor was given when powering the device on. */
#define XTC_ERRORCODE_POWERON_BADWRITE          -201  /* There was a problem powering the device on. */

#define XTC_ERRORCODE_SETEDGE_BADHANDLE         -210
#define XTC_ERRORCODE_SETEDGE_BADWRITE          -211
#define XTC_ERRORCODE_GETEDGE_BADHANDLE         -220
#define XTC_ERRORCODE_GETEDGE_BADVALUE          -221
#define XTC_ERRORCODE_GETEDGE_BADREAD           -222
#define XTC_ERRORCODE_SETINT_BADHANDLE          -230
#define XTC_ERRORCODE_SETINT_BADWRITE           -231
#define XTC_ERRORCODE_ENABLEINPULSE_BADHANDLE   -240
#define XTC_ERRORCODE_ENABLEINPULSE_BADWRITE    -241
#define XTC_ERRORCODE_TESTSIGNAL_BADHANDLE      -250
#define XTC_ERRORCODE_TESTSIGNAL_BADWRITE       -251
#define XTC_ERRORCODE_TESTPULSE_BADHANDLE       -260
#define XTC_ERRORCODE_TESTPULSE_BADON           -261
#define XTC_ERRORCODE_TESTPULSE_BADOFF          -262
#define XTC_ERRORCODE_SETEXP_BADHANDLE          -270
#define XTC_ERRORCODE_SETEXP_MINTIME            -271
#define XTC_ERRORCODE_SETEXP_MAXTIME            -272
#define XTC_ERRORCODE_SETEXP_BADWRITE           -273
#define XTC_ERRORCODE_SETEXP_BADTOGGLE          -274
#define XTC_ERRORCODE_SETEXP_BADRESTORE         -275
#define XTC_ERRORCODE_GETINPUT_BADHANDLE        -280
#define XTC_ERRORCODE_GETINPUT_BADVALUE         -281
#define XTC_ERRORCODE_GETINPUT_BADREAD          -282
#define XTC_ERRORCODE_GETOUTPUT_BADHANDLE       -290
#define XTC_ERRORCODE_GETOUTPUT_BADVALUE        -291
#define XTC_ERRORCODE_GETOUTPUT_BADREAD         -292
#define XTC_ERRORCODE_DETECT_BADHANDLE          -300
#define XTC_ERRORCODE_DETECT_BADVALUE           -301


/**
 * Closes a parallel port that was opened with xtcOpenPort().
 *
 * If the port could not be closed successfully (a negative value was returned)
 * then the return value can be given as the argument to the xtcGetErrorMessage()
 * function to obtain a description of the cause of the problem.
 *
 * @parameter portHandle the (non-negative) handle to the open parallel port 
 *            that was obtained from a call to xtcOpenPort().
 *
 * @returns 0 if the parallel port was successfully closed, and a non-zero
 *            value otherwise.
 */
int xtcClosePort(int portHandle) {
    int status = -1; /* Used to hold the return value of function calls made during closing of the port. */
    
    if (portHandle < 0) {
      return XTC_ERRORCODE_CLOSE_BADHANDLE; /* An invalid port handle was given, report the error. */    
    }
    
    /* Release the claim to the port. */
    status = -1;
    status = ioctl(portHandle, PPRELEASE);
    if (status != 0) {
        return XTC_ERRORCODE_CLOSE_NOCLAIM;
    }
    
    /* Close the parallel port device. */
    status = -1;
    status = close(portHandle);
    if (status != 0) {
        return XTC_ERRORCODE_CLOSE_NOCLOSE;
    }
        
    /* If we got here then the port and device was closed successfully. */
    return XTC_ERRORCODE_NOERROR;
}

/**
 * Opens a parallel port for exclusive access. If the port was opened successfully
 * then the return value is a file descriptor that can be used to manipulate the
 * parallel port (and can be passed as the portHandle parameter in other functions
 * within this file). If the parallel port could not be opened for exclusive
 * access then a negative value is returned.
 *
 * A port that is successfully opened with this function must be closed with the 
 * xtcClosePort(portHandle) function. If the port was not successfully opened 
 * then xtcClosePort() does not need to be called.
 *
 * If the port could not be opened successfully (a negative value was returned)
 * then the return value can be given as the argument to the xtcGetErrorMessage()
 * function to obtain a description of the cause of the problem.
 *
 * @parameter deviceName the Linux device name for the parallel port to open,
 *            for example "/dev/parport0".
 *
 * @return a handle to the open parallel port, or a negative value if the port
 *         could not be opened successfully.
 */
int xtcOpenPort(char* deviceName) {
    int portHandle = -1; /* File descriptor of the parallel port device. */
    int status = -1;     /* Used to hold the return value of function calls made during setup. */
    int mode = IEEE1284_MODE_COMPAT; /* The mode for the parallel port to operate in. See <linux/parport.h>
                                      * and 
                                      */
    int direction = 0;   /* The direction the data byte of the parallel port; 
                          *   0 = outward (written by the computer for the external device), and
                          *   1 = inward (written by the external for the computer).
                          */
    
    if (deviceName == 0) {
        return XTC_ERRORCODE_OPEN_NULLNAME;
    }
    if (strlen(deviceName) < 1) {
        return XTC_ERRORCODE_OPEN_BLANKNAME;
    }
    
    /* Check that we are able to access the device given by deviceName. */
    if (access(deviceName, R_OK | W_OK) != 0) {
        return XTC_ERRORCODE_OPEN_NODEVICE;
    }
     
    /* Open the port device. */
    portHandle = open(deviceName, O_RDWR);
    if (portHandle < 0) {
        return XTC_ERRORCODE_OPEN_CANNOTOPEN;
    }
    
    /* Indicate we want exclusive access to the port. */
    status = -1;
    status = ioctl(portHandle, PPEXCL);
    if (status != 0) {
        close(portHandle); /* Release the device since it is open. */
        return XTC_ERRORCODE_OPEN_NOEXCLUSIVE;
    }
    
    /* Claim the port. */
    status = -1;
    status = ioctl(portHandle, PPCLAIM);
    if (status != 0) {
        close(portHandle); /* Release the device since it is open. */
        return XTC_ERRORCODE_OPEN_NOCLAIM;
    }
    
    /* Note: we do not have to set the parallel port mode here. At this stage
     *       it is set to compatibility mode (IEEE1284_MODE_COMPAT). Refer to: 
     *         http://people.redhat.com/twaugh/parport/html/x623.html
     */
    
    /* Set the parallel port mode used when making read and write calls to the
     * data port. We will set it to compatibility mode (IEEE1284_MODE_COMPAT). 
     * Refer to: http://people.redhat.com/twaugh/parport/html/x623.html
     */
    status = -1;
    mode = IEEE1284_MODE_COMPAT;
    status = ioctl(portHandle, PPSETMODE, &mode); 
    if (status != 0) {
        xtcClosePort(portHandle);
        return XTC_ERRORCODE_OPEN_SETMODE;        
    }
    
    /* Set the direction of the data port to the outward direction. Here the
     * direction is 0 for outward and 1 for inward.
     */
    status = -1;
    direction = 0;
    status = ioctl(portHandle, PPDATADIR, &direction);
    if (status != 0) {
        xtcClosePort(portHandle);
        return XTC_ERRORCODE_OPEN_DIRECTION;
    }
        
    /* Disable interrupts. */
    status = -1;
    status = xtcSetInterrupts(portHandle, 0);
    if (status != 0) {
        xtcClosePort(portHandle);
        return XTC_ERRORCODE_OPEN_INTERRUPTS;
    }
    
    /* If we got here then the port was opened and claimed successfully. */
    return portHandle;
}

/**
 * Returns a message string describing an error code.
 *
 * @parameter errorCode the error message code.
 *
 * @returns the message string that describes the error code.
 */
const char* xtcGetErrorMessage(int errorCode) {
    std::string message = "No error";

    switch(errorCode) {
        case XTC_ERRORCODE_NOERROR: {
            message = "No error has occured.";
        } break;
 
        case XTC_ERRORCODE_OPEN_NULLNAME: {
            message = "Could not open the parallel-port as a null pointer was given for the device name whenm something like \"/dev/parport0\" was expected.";
        } break;
        
        case XTC_ERRORCODE_OPEN_BLANKNAME: {
            message = "Could not open the parallel-port as an empty string was given for the device name whenm something like \"/dev/parport0\" was expected.";
        } break;
        
        case XTC_ERRORCODE_OPEN_NODEVICE: {
            message = "Could not open the parallel-port as there was a problem with accessibility of the device file. Please check the correct modules are loaded (partport,parport_pc,ppdev) and the parallel port device in /dev exists and has read and write permissions for this user.";
        } break;

        case XTC_ERRORCODE_OPEN_CANNOTOPEN: {
            message = "Could not open the parallel-port as there was a problem when opening the device file. Please check the correct modules are loaded (partport,parport_pc,ppdev) and the parallel port device in /dev exists and has read and write permissions for this user.";
        } break;
 
        case XTC_ERRORCODE_OPEN_NOEXCLUSIVE: {
            message = "Could not open the parallel-port as there was a problem setting the port for exclusive access.";
        } break;
        
        case XTC_ERRORCODE_OPEN_NOCLAIM: {
            message = "Could not open the parallel-port as there was a problem claiming the port for use.";
        } break;
        
        case XTC_ERRORCODE_OPEN_SETMODE: {
            message = "Could not open the parallel-port as there was a problem setting the IEEE1284 mode of the port. Try changing the port mode in your BIOS to SPP/Standard rather than PS/2, ECP, or EPP";
        } break;

        case XTC_ERRORCODE_OPEN_DIRECTION: {
            message = "Could not open the parallel-port as there was a problem setting the data byte of the port to output mode.";
        } break;
 
        case XTC_ERRORCODE_OPEN_INTERRUPTS: {
            message = "Could not open the parallel-port as there was a problem disabling interrupt generation from the port.";            
        } break;
                
        case XTC_ERRORCODE_CLOSE_BADHANDLE: {
            message = "Could not close the parallel-port as the port handle argument (file descriptor) was invalid.";
        } break;
        
        case XTC_ERRORCODE_CLOSE_NOCLAIM: {
            message = "Could not close the parallel-port properly as there was a problem releasing the claim to the port.";
        } break;
        
        case XTC_ERRORCODE_CLOSE_NOCLOSE: {
            message = "Could not close the parallel-port properly as there was a problem when closing the device file.";
        } break;
 
        case XTC_ERRORCODE_WRITEDATA_BADHANDLE: {
            message = "Could not write to the data port of the parallel-port as the port handle argument (file descriptor) was invalid.";
        } break;
        
        case XTC_ERRORCODE_WRITEDATA_BADWRITE: {
            message = "Could not write to the data port of the parallel-port as the low-level write operation was not successful.";
        } break;
        
        case XTC_ERRORCODE_READSTATUS_BADHANDLE: {
            message = "Could not read from the status port of the parallel-port as the port handle argument (file descriptor) was invalid.";
        } break;

        case XTC_ERRORCODE_READSTATUS_BADVALUE: {
            message = "Could not read from the status port of the parallel-port as the destination byte pointer was invalid.";
        } break;
        
        case XTC_ERRORCODE_READSTATUS_BADREAD: {
            message = "Could not read from the status port of the parallel-port as the low-level read operation was not successful.";
        } break;        

        case XTC_ERRORCODE_READCONTROL_BADHANDLE: {
            message = "Could not read from the control port of the parallel-port as the port handle argument (file descriptor) was invalid.";
        } break; 
 
        case XTC_ERRORCODE_READCONTROL_BADVALUE: {
            message = "Could not read from the status port of the parallel-port as the destination byte pointer was invalid.";
        } break; 
 
        case XTC_ERRORCODE_READCONTROL_BADREAD: {
            message = "Could not read from the control port of the parallel-port as the low-level read operation was not successful.";
        } break; 
 
        case XTC_ERRORCODE_WRITECONTROL_BADHANDLE: {
            message = "Could not write a byte to the control port of the parallel-port as the port handle argument (file descriptor) was invalid.";
        } break; 

        case XTC_ERRORCODE_WRITECONTROL_BADWRITE: {
            message = "Could not write a byte to the control port of the parallel-port as the low-level write operation was not successful.";
        } break; 
 
        case XTC_ERRORCODE_WRITECONTROLBIT_BADHANDLE: {
            message = "Could not write a bit to the control port of the parallel-port as the port handle argument (file descriptor) was invalid.";
        } break; 

        case XTC_ERRORCODE_WRITECONTROLBIT_BADWRITE: {
            message = "Could not write a bit to the control port of the parallel-port as the low-level write operation was not successful.";
        } break; 
        
        case XTC_ERRORCODE_POWERON_BADHANDLE: {
            message = "Could not turn power on to the device properly as the port handle argument (file descriptor) was invalid.";
        } break;

        case XTC_ERRORCODE_POWERON_BADWRITE: {
            message = "Could not turn power on to the device properly as there was a problem writing to the device.";
        } break;
        
        default:
            message = "Unknown error code (possibly not an error)";
    }
    
    return message.c_str();   
}

/**
 * Writes to the data port within the parallel port. The data port is the byte
 * located at the base address of the parallel port.
 *
 * If data could not be written successfully (a negative value was returned)
 * then the return value can be given as the argument to the xtcGetErrorMessage()
 * function to obtain a description of the cause of the problem.
 *
 * @parameter portHandle the (non-negative) handle to the open parallel port 
 *            that was obtained from a call to xtcOpenPort().
 *
 * @parameter value the byte of data to be written to the data port within the
 *            parallel port.
 * 
 * @returns 0 if the data was written successfully, and a non-zero value otherwise.
 */
int xtcWriteData(int portHandle, unsigned char value) {
    int status = -1;  /* Used to hold the return value of function calls made during writing data. */
    
    if (portHandle < 0) {
      return XTC_ERRORCODE_WRITEDATA_BADHANDLE; /* An invalid port handle was given, report the error. */    
    }    
     
    status = ioctl(portHandle, PPWDATA, &value);
    if (status != 0) {
        return XTC_ERRORCODE_WRITEDATA_BADWRITE;
    }
            
    /* If we got here then the data was written successfully. */
    return XTC_ERRORCODE_NOERROR; 
}

/**
 * Reads from the status port within the parallel port. The status port is the byte
 * located at one byte past the base address of the parallel port (that is, 
 * base address+1).
 *
 * If data could not be read successfully (a negative value was returned)
 * then the return value can be given as the argument to the xtcGetErrorMessage()
 * function to obtain a description of the cause of the problem.
 *
 * @parameter portHandle the (non-negative) handle to the open parallel port 
 *            that was obtained from a call to xtcOpenPort().
 *
 * @parameter pValue points to the byte where the read value will be placed. This
 *            should not be null.
 * 
 * @returns 0 if the data was read successfully (and placed in the location
 *          pointed to by pValue), and a non-zero value otherwise.
 */
int xtcReadStatus(int portHandle, unsigned char* pValue) {
    int status = -1;  /* Used to hold the return value of function calls made during reading the status. */
    
    if (portHandle < 0) {
      return XTC_ERRORCODE_READSTATUS_BADHANDLE; /* An invalid port handle was given, report the error. */    
    }    
    
    if (pValue == 0) {
        return XTC_ERRORCODE_READSTATUS_BADVALUE; /* An invalid pointer was given, report the error. */
    }
     
    /* Note: the return value of a successful read is either 0 or 1 byte, depending
     * on whether data is available to be read. 
     */
    status = ioctl(portHandle, PPRSTATUS, pValue); /* Read the single byte from the port. */
    if ( (status != 0) && (status != 1) ) {
        return XTC_ERRORCODE_READSTATUS_BADREAD;
    }
            
    /* If we got here then the data was read successfully. */
    return status; 
}

/**
 * Reads from the control port within the parallel port. The control port is the byte
 * located at two bytes past the base address of the parallel port (that is, 
 * base address+2).
 *
 * If data could not be read successfully (a negative value was returned)
 * then the return value can be given as the argument to the xtcGetErrorMessage()
 * function to obtain a description of the cause of the problem.
 *
 * @parameter portHandle the (non-negative) handle to the open parallel port 
 *            that was obtained from a call to xtcOpenPort().
 *
 * @parameter pValue points to the byte where the read value will be placed. This
 *            should not be null.
 * 
 * @returns 0 if the data was read successfully (and placed in the location
 *          pointed to by pValue), and a non-zero value otherwise.
 */
int xtcReadControl(int portHandle, unsigned char* pValue) {
    int status = -1;  /* Used to hold the return value of function calls made during read the control data. */
    
    if (portHandle < 0) {
      return XTC_ERRORCODE_READCONTROL_BADHANDLE; /* An invalid port handle was given, report the error. */    
    }    
    
    if (pValue == 0) {
        return XTC_ERRORCODE_READCONTROL_BADVALUE; /* An invalid pointer was given, report the error. */
    }
     
    /* Note: the return value of a successful read is either 0 or 1 byte, depending
     * on whether data is available to be read. 
     */
    status = ioctl(portHandle, PPRCONTROL, pValue); /* Read the single byte from the port. */
    if ( (status != 0) && (status != 1) ) {
        return XTC_ERRORCODE_READCONTROL_BADREAD;
    }
            
    /* If we got here then the data was read successfully. */
    return status; 
}

/**
 * Writes to the control port within the parallel port. The control port is the 
 * byte located at two bytes past the base address of the parallel port (that is, 
 * base address+2).
 *
 * If data could not be written successfully (a negative value was returned)
 * then the return value can be given as the argument to the xtcGetErrorMessage()
 * function to obtain a description of the cause of the problem.
 *
 * @parameter portHandle the (non-negative) handle to the open parallel port 
 *            that was obtained from a call to xtcOpenPort().
 *
 * @parameter value is the value that should be written to the control port.
 * 
 * @returns 0 if the data was written successfully, and a non-zero value otherwise.
 */
int xtcWriteControl(int portHandle, unsigned char value) {
    int status = -1;  /* Used to hold the return value of function calls made during writing of control data. */
    
    if (portHandle < 0) {
      return XTC_ERRORCODE_WRITECONTROL_BADHANDLE; /* An invalid port handle was given, report the error. */    
    }    
         
    status = ioctl(portHandle, PPWCONTROL, value); /* Write the single byte to the port. */
    if (status != 0) {
        return XTC_ERRORCODE_WRITECONTROL_BADWRITE;
    }
            
    /* If we got here then the data was written successfully. */
    return XTC_ERRORCODE_NOERROR; 
}

/**
 * Writes a single bit of the control port within the parallel port. The control 
 * port is the byte located at two bytes past the base address of the parallel 
 * port (that is, base address+2).
 *
 * If the bit could not be written successfully (a negative value was returned)
 * then the return value can be given as the argument to the xtcGetErrorMessage()
 * function to obtain a description of the cause of the problem.
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
 * @returns 0 if the data was written successfully, and a non-zero value otherwise.
 */
int xtcWriteControlBit(int portHandle, unsigned char bitmask, unsigned char value) {
    int status = -1;  /* Used to hold the return value of function calls made during writing of a control bit. */
    
    if (portHandle < 0) {
      return XTC_ERRORCODE_WRITECONTROLBIT_BADHANDLE; /* An invalid port handle was given, report the error. */    
    }    
         
    struct ppdev_frob_struct changes; /* Changes that should be made to the bit. */
    changes.mask = bitmask;
    if (value == 0) {
        changes.val = 0;
    } else {
        changes.val = bitmask;
    }
        
    
    status = ioctl(portHandle, PPFCONTROL, &changes); /* Write the single byte to the port. */
    if (status != 0) {
        return XTC_ERRORCODE_WRITECONTROLBIT_BADWRITE;
    }
            
    /* If we got here then the bit was written successfully. */
    return XTC_ERRORCODE_NOERROR; 
}

/**
 * Switches power on to the timer card on.
 *
 * If the power could not be turned on successfully (a negative value was returned)
 * then the return value can be given as the argument to the xtcGetErrorMessage()
 * function to obtain a description of the cause of the problem.
 *
 * @parameter portHandle the (non-negative) handle to the open parallel port 
 *            that was obtained from a call to xtcOpenPort().
 *
 * @returns 0 if the power was turned on successfully, and a non-zero value otherwise.
 */
int xtcPowerOn(int portHandle) {
    int status = -1; /* Used to hold the return value of function calls made during powering on the card. */
    int powerOnValue = 0xff; /* All data bits on indicate the external timer card should switch its power on. */
    
    if (portHandle < 0) {
      return XTC_ERRORCODE_POWERON_BADHANDLE; /* An invalid port handle was given, report the error. */    
    }

    /* Turn the power on by setting all bits to high on the data port. */
    status = xtcWriteData(portHandle, powerOnValue);
    if (status != XTC_ERRORCODE_NOERROR) {
        return XTC_ERRORCODE_POWERON_BADWRITE;
    }
    usleep(100000);               /* wait for it to come up (4 RC times) */

    /* If we got here then the power was switched on successfully. */
    return XTC_ERRORCODE_NOERROR;
}

/**
 * Sets whether the rising edge (low-to-high voltage transition) or falling edge
 * (high-to-low voltage transition) is used as the start of a timing pulse. Note
 * that GPS 1 Hz timing pulses start with a rising edge (they are low-to-high
 * voltage pulses).
 *
 * @parameter portHandle the (non-negative) handle to the open parallel port 
 *            that was obtained from a call to xtcOpenPort().
 *
 * @parameter edgeType indicates the type of voltage transition to use for the
 *            start of a timing pulse. Use 0 to use rising edges, and non-zero 
 *            to use falling edges.   
 *
 * @returns 0 if the timing edge type was set successfully, and a non-zero value otherwise. 
 */
int xtcSetTimingEdgeType(int portHandle, int edgeType) {
    int status = -1;    /* Used to hold the return value of function calls made during this operation. */    
    int edgeBit = 0x01; /* The lowest bit of the control port determines the timing edge type. */ 
    unsigned char enable = 0; /* Indicates whether the edge type should be rising or falling. */
    
    if (portHandle < 0) {
      return XTC_ERRORCODE_SETEDGE_BADHANDLE; /* An invalid port handle was given, report the error. */    
    }
    
    /* Ensure that the enable flag given to the port is either zero or one 
     * (since the parameter passed in could actually have any integer value).
     */
    if (edgeType != 0) {
        enable = 1;
    }
    
    status = xtcWriteControlBit(portHandle, edgeBit, enable);
    if (status != 0) {
        return XTC_ERRORCODE_SETEDGE_BADWRITE; 
    }
    
    /* If we got here then the edge type was set successfully. */
    return XTC_ERRORCODE_NOERROR;
}

/** Gets whether the rising edge (low-to-high voltage transition) or falling edge
 * (high-to-low voltage transition) is used as the start of a timing pulse. Note
 * that GPS 1 Hz timing pulses start with a rising edge (they are low-to-high
 * voltage pulses).
 *
 * @parameter portHandle the (non-negative) handle to the open parallel port 
 *            that was obtained from a call to xtcOpenPort().
 *
 * @parameter pEdgeType points to the location of an integer that will store the
 *            result of this call. It will indicate the type of voltage transition
 *            being used as the start of a timing pulse. A value of 0 means 
 *            rising edges are being used, and non-zero means falling edges are 
 *            being used.   
 *
 * @returns 0 if the timing edge type was obtained successfully, and a non-zero value otherwise. 
 */
int xtcGetTimingEdgeType(int portHandle, int* pEdgeType) {
    int status = -1;    /* Used to hold the return value of function calls made during this operation. */    
    int edgeBit = 0x01; /* The lowest bit of the control port determines the timing edge type. */ 
    unsigned char value = 0; /* Used to obtain the control port's value. */
    
    if (portHandle < 0) {
      return XTC_ERRORCODE_GETEDGE_BADHANDLE; /* An invalid port handle was given, report the error. */    
    }
    
    if (pEdgeType == 0) {
        return XTC_ERRORCODE_GETEDGE_BADVALUE; /* An invalid destination was given, report the error. */
    }
        
    status = xtcReadControl(portHandle, &value);
    if (status != 0) {
        return XTC_ERRORCODE_GETEDGE_BADREAD; 
    }
    
    /* Determine whether the edge bit was set and return the value accordingly. */
    if ( (value & edgeBit) != 0) {
        *pEdgeType = 1;
    } else {
        *pEdgeType = 0;
    }
    
    /* If we got here then the edge type was set successfully. */
    return XTC_ERRORCODE_NOERROR;   
}

/**
 * Sets whether the external timing card will generate interrupts or not whenever
 * an output pulse (check this, is it for input pulses instead?) is generated 
 * by the card.
 *
 * @parameter portHandle the (non-negative) handle to the open parallel port 
 *            that was obtained from a call to xtcOpenPort().
 *
 * @parameter enable indicates whether interrupts should be generated or not. 
 *            Use 0 to disable interrupts, and 1 to enable them.   
 *
 * @returns 0 if the interrupt mode was set successfully, and a non-zero value otherwise. 
 */
int xtcSetInterrupts(int portHandle, int enable) {
    int status = -1;         /* Used to hold the return value of function calls made during this operation. */    
    int interruptBit = 0x10; /* The bit 4 of the control port determines whether interrupts are generated. */ 
    unsigned char useInterrupts = 0; /* Indicates whether the interrupts should be enabled or disabled. */
    
    if (portHandle < 0) {
      return XTC_ERRORCODE_SETINT_BADHANDLE; /* An invalid port handle was given, report the error. */    
    }
    
    /* Ensure that the useInterrupts flag written to the port is either zero or 
     * one (since enable could actually be passed in with any integer value).
     */
    if (enable != 0) {
        useInterrupts = 1;
    }
    
    status = xtcWriteControlBit(portHandle, interruptBit, useInterrupts);
    if (status != 0) {
        return XTC_ERRORCODE_SETINT_BADWRITE; 
    }
    
    /* If we got here then the interrupt mode was set successfully. */
    return XTC_ERRORCODE_NOERROR;    
}

/**
 * Enables whether GPS input pulses will be read by the external timer card.
 * Disabling input pulses from the GPS allows input pulses to be simulated using
 * the xtcSimulateTestSignal() function.
 *
 * @parameter portHandle the (non-negative) handle to the open parallel port 
 *            that was obtained from a call to xtcOpenPort().
 *
 * @parameter enable indicates whether input pulses should be obtained from the
 *            GPS input or not. Use 0 to disable GPS input pulses, and 1 to enable them.   
 *
 * @returns 0 if the GPS input pulse mode was set successfully, and a non-zero value otherwise. 
 */
int xtcEnableInputPulses(int portHandle, int enable) {
    int status = -1;             /* Used to hold the return value of function calls made during this operation. */    
    int enableBit = 0x04;        /* Bit 2 of the control port determines whether input pulses are enabled. */ 
    unsigned char usePulses = 0; /* Indicates whether GPS input pulses should be enabled. */
    
    if (portHandle < 0) {
      return XTC_ERRORCODE_ENABLEINPULSE_BADHANDLE; /* An invalid port handle was given, report the error. */    
    }
    
    /* Ensure that the usePulses flag written to the port is either zero or one 
     * (since the parameter passed in could actually have any integer value).
     */
    if (enable != 0) {
        usePulses = 1;
    }
    
    status = xtcWriteControlBit(portHandle, enableBit, usePulses);
    if (status != 0) {
        return XTC_ERRORCODE_ENABLEINPULSE_BADWRITE; 
    }
    
    /* If we got here then the edge type was set successfully. */
    return XTC_ERRORCODE_NOERROR;   
}

/**
 * Instructs the external timer card to set the value of the input signal line.
 * This can be used for testing the input pulse operation (and for detecting
 * the presence of the external timer card itself). 
 *
 * This function should only be called when the timer card is in simulation mode,
 * which is entered by disabling GPS input pulses with xtcEnableInputPulses().
 * Once in simulation mode this xtcSetTestSignal() can be used to set the logic
 * level that can be read by the xtcGetInputPulseLevel() function.
 *
 * @parameter portHandle the (non-negative) handle to the open parallel port 
 *            that was obtained from a call to xtcOpenPort().
 *
 * @parameter level indicates the logic level that should be used for the
 *            (simulated) input signal level.   
 *
 * @returns 0 if the test signal logic level was set successfully, and a non-zero value otherwise.
 */
int xtcSetTestSignal(int portHandle, int level) {
    int status = -1;             /* Used to hold the return value of function calls made during this operation. */    
    int testBit = 0x02;          /* Bit 1 of the control port sets the test input signal level. Note: this bit has inverted voltage level, but this is handled by the timer card. */ 
    unsigned char testLevel = 0; /* The logic level of the test input signal. */
    
    if (portHandle < 0) {
      return XTC_ERRORCODE_TESTSIGNAL_BADHANDLE; /* An invalid port handle was given, report the error. */    
    }
    
    /* Ensure that the testLevel flag written to the port is either zero or one 
     * (since the parameter passed in could actually have any integer value).
     */
    if (level != 0) {
        testLevel = 1; 
    }
    
    status = xtcWriteControlBit(portHandle, testBit, testLevel);
    if (status != 0) {
        return XTC_ERRORCODE_TESTSIGNAL_BADWRITE; 
    }
    
    /* If we got here then the test input signal level was set successfully. */
    return XTC_ERRORCODE_NOERROR;     
}

/**
 * Instructs the external timer card to send a short low-to-high pulse along
 * the input signal line. This can be used for testing the input pulse operation
 * (and for detecting the presence of the external timer card itself). 
 *
 * This function should only be called when the timer card is in simulation mode,
 * which is entered by disabling GPS input pulses with xtcEnableInputPulses().
 * Once in simulation mode this xtcSendTestPulse() can be used to simulate 
 * incoming GPS timing pulses. After enough of these pulses have been received 
 * the output signal level should change (which can be detected using 
 * xtcGetOutputSignalLevel()). 
 *
 * @parameter portHandle the (non-negative) handle to the open parallel port 
 *            that was obtained from a call to xtcOpenPort().  
 *
 * @returns 0 if the test input pulse was generated successfully, and a non-zero value otherwise.
 */
int xtcSendTestPulse(int portHandle) {
    int status = -1; /* Used to hold the return value of function calls made during this operation. */    

    if (portHandle < 0) {
      return XTC_ERRORCODE_TESTPULSE_BADHANDLE; /* An invalid port handle was given, report the error. */    
    }   

    /* Generate the pulse by simulating the transition of the input logic level
       from low-to-high and back to low again. */
    status = xtcSetTestSignal(portHandle, 1); /* Input signal high. */
    if (status != 0) {
        return XTC_ERRORCODE_TESTPULSE_BADON;
    }
    
    usleep(100); /* Wait a bit so the pulse can be detected. */

    status = xtcSetTestSignal(portHandle, 0); /* Input signal low. */
    if (status != 0) {
        return XTC_ERRORCODE_TESTPULSE_BADOFF;
    }
    
    /* If we got here then the test pulse was generated successfully. */
    return XTC_ERRORCODE_NOERROR;    
}

/**
 * Sets the exposure time (in seconds). The exposure time is the number of input
 * pulses that must occur before the external timer card will output a pulse
 * on its output signal line.
 *
 * At present the exposure time must be a value between 0 and 30 seconds. An
 * exposure time of 0 stops the external timer card from producing output pulses.
 *
 * @parameter portHandle the (non-negative) handle to the open parallel port 
 *            that was obtained from a call to xtcOpenPort().  
 *
 * @parameter exposureTime the number of seconds that should elapse between
 *            pulses on the output signal line of the external timer card 
 *            (which defines the exposure time of the connected camera). A value
 *            of zero stops output signals from being emitted by the external
 *            timer card.
 *
 * @returns 0 if the exposure time was set successfully, and a non-zero value otherwise.
 */
int xtcSetExposureTime(int portHandle, int exposureTime) {
    int status = -1;                       /* Used to hold the return value of function calls made during this operation. */    
    int minExposureTime = 0;               /* The minimum valid exposure time (in seconds). */
    int maxExposureTime = 30;              /* The maximum valid exposure time (in seconds). */
    unsigned char setExposureBit = 0x20;   /* Bit 5 of the data port is used to indicate the exposure time is being set. */
    unsigned char exposureTimeMask = 0x1F; /* Bits 0-4 of the data port holds the exposure time (in seconds). */   
    unsigned char byte = 0;                /* Used to compose the value to be written to the data port. */
    unsigned char upperBits = 0xC0;        /* Bits 6-7 of the byte switched on, the others switched off. */
    unsigned char allBits = 0xFF;          /* All bits of the byte turned on. */
    
    if (portHandle < 0) {
      return XTC_ERRORCODE_SETEXP_BADHANDLE; /* An invalid port handle was given, report the error. */    
    }
    
    /* Check that the requested exposure time is within the range of valid
     * exposure times. At present this range is 0-31 seconds as only 5 bits are 
     * accepted by the external timer card for the exposure time).
     */
    if (exposureTime < minExposureTime) {
        return XTC_ERRORCODE_SETEXP_MINTIME;
    }

    if (exposureTime > maxExposureTime) {
        return XTC_ERRORCODE_SETEXP_MAXTIME;
    }
    
    /* Write the exposure time into the data port. Ensure the set exposure bit
     * (bit 5) is low.
     */
    byte = upperBits | (unsigned char) (exposureTime & exposureTimeMask);
    status = xtcWriteData(portHandle, byte);
    if (status != 0) {
        return XTC_ERRORCODE_SETEXP_BADWRITE; 
    }    
    usleep(1); /* Allow a little bit of time for the external timer card to register the change. */
    
    /* Toggle the set exposure time bit high and write it into the data port. */
    byte |= setExposureBit;
    status = xtcWriteData(portHandle, byte);
    if (status != 0) {
        return XTC_ERRORCODE_SETEXP_BADTOGGLE; 
    }
    usleep(1); /* Allow a little bit of time for the external timer card to register the change. */
    
    /* Restore all bits to 1 on the data byte. */
    status = xtcWriteData(portHandle, allBits);
    if (status != 0) {
        return XTC_ERRORCODE_SETEXP_BADRESTORE; 
    }
    
    /* If we got here then the exposure time was set successfully. */
    return XTC_ERRORCODE_NOERROR; 
}

/**
 * Reads the input signal line from the external timer card. This line shows
 * pulses from the GPS, or simulated pulses if xtcSendTestPulse() is being used
 * to generate pulses for testing purposes.
 *
 * @parameter portHandle the (non-negative) handle to the open parallel port 
 *            that was obtained from a call to xtcOpenPort().  
 * 
 * @parameter pLevel points to the integer that is the destination for the 
 *            input signal level that was read. The destination integer will 
 *            be set to 0 if the input signal line has low voltage, and 1 if 
 *            the input signal line has high logic voltage. This pointer should 
 *            not be null.
 *
 * @returns 0 if the input signal line was read successfully, and a non-zero value otherwise.
 */
int xtcGetInputSignalLevel(int portHandle, int* pLevel) {
    int status = -1;         /* Used to hold the return value of function calls made during this operation. */    
    int inputBit = 0x40;     /* Bit 6 of the status port is used to read the input signal level. */ 
    unsigned char value = 0; /* Used to obtain the status port's value. */
    
    if (portHandle < 0) {
      return XTC_ERRORCODE_GETINPUT_BADHANDLE; /* An invalid port handle was given, report the error. */    
    }
    
    if (pLevel == 0) {
        return XTC_ERRORCODE_GETINPUT_BADVALUE; /* An invalid destination was given, report the error. */
    }
        
    status = xtcReadStatus(portHandle, &value);
    if (status != 0) {
        return XTC_ERRORCODE_GETINPUT_BADREAD; 
    }
    
    /* Determine whether the input signal bit was set and return the value accordingly. */
    if ( (value & inputBit) != 0) {
        *pLevel = 1;
    } else {
        *pLevel = 0;
    }
     
    /* If we got here then the input signal level was read successfully. */
    return XTC_ERRORCODE_NOERROR; 
}

/**
 * Reads the output signal line from the external timer card. This line shows
 * pulses from the external timer card that are intended to trigger the camera,
 * or simulated pulses if xtcSendTestPulse() is being used to generate pulses 
 * for testing purposes.
 *
 * @parameter portHandle the (non-negative) handle to the open parallel port 
 *            that was obtained from a call to xtcOpenPort().  
 * 
 * @parameter pLevel points to the integer that is the destination for the 
 *            output signal level that was read. The destination integer will 
 *            be set to 0 if the output signal line has low voltage, and 1 if 
 *            the output signal line has high logic voltage. This pointer should 
 *            not be null.
 *
 * @returns 0 if the output signal line was read successfully, and a non-zero value otherwise.
 */
int xtcGetOutputSignalLevel(int portHandle, int* pLevel) {
    int status = -1;         /* Used to hold the return value of function calls made during this operation. */    
    int outputBit = 0x20;    /* Bit 5 of the status port is used to read the output signal level. */ 
    unsigned char value = 0; /* Used to obtain the status port's value. */
    
    if (portHandle < 0) {
      return XTC_ERRORCODE_GETOUTPUT_BADHANDLE; /* An invalid port handle was given, report the error. */    
    }
    
    if (pLevel == 0) {
        return XTC_ERRORCODE_GETOUTPUT_BADVALUE; /* An invalid destination was given, report the error. */
    }
        
    status = xtcReadStatus(portHandle, &value);
    if (status != 0) {
        return XTC_ERRORCODE_GETOUTPUT_BADREAD; 
    }
    
    /* Determine whether the output signal bit was set and return the value accordingly. */
    if ( (value & outputBit) != 0) {
        *pLevel = 1;
    } else {
        *pLevel = 0;
    }
    
    /* If we got here then the output signal level was read successfully. */
    return XTC_ERRORCODE_NOERROR;     
}


/**
 * Determines whether there is a external timer card connected to the parallel
 * port or not.
 *
 * @parameter portHandle the (non-negative) handle to the open parallel port 
 *            that was obtained from a call to xtcOpenPort(). 
 *
 * @parameter pCardPresent points to a location which is the destination of the
 *            detection. The integer pointed to will be set to 0 if an external
 *            timer card was not detected, and set to 1 if a card was detected.
 *            This pointer should not be null.
 *
 * @returns 0 if the detection process completed successfully, and a non-zero value otherwise. 
 */
int xtcDetectTimerCard(int portHandle, int* pCardPresent) {
    /*int status = -1; */        /* Used to hold the return value of function calls made during this operation. */ 
    int inputLevel = 0;      /* The logic level of the input signal line. */
    
    if (portHandle < 0) {
      return XTC_ERRORCODE_DETECT_BADHANDLE; /* An invalid port handle was given, report the error. */    
    }
    
    if (pCardPresent == 0) {
        return XTC_ERRORCODE_DETECT_BADVALUE; /* An invalid destination was given, report the error. */
    }
    
    /* TODO - check return values of all called functions. */
    
    /* Turn the signal input off and the test/simulation signal off. We expect
       no signals to be received. */
    xtcEnableInputPulses(portHandle, 0);
    xtcSetTestSignal(portHandle, 0);
    
    inputLevel = 1; /* Set to a value that indicates no card unless this variable is set by the next call. */
    xtcGetInputSignalLevel(portHandle, &inputLevel);
    if (inputLevel) {
        /* A pulse was received, therefore we don't have a working external timer card. */
        *pCardPresent = 0;
        return XTC_ERRORCODE_NOERROR;
    }

    /* Turn the signal input on and the test/simulation signal on. We expect
       a signal to be received. */
    xtcEnableInputPulses(portHandle, 1);
    xtcSetTestSignal(portHandle, 1);
    
    inputLevel = 0;  /* Set to a value that indicates no card unless this variable is set by the next call. */
    xtcGetInputSignalLevel(portHandle, &inputLevel);
    if (inputLevel == 0) {
        /* A pulse was received, therefore we don't have a working external timer card. */
        *pCardPresent = 0;
        return XTC_ERRORCODE_NOERROR;
    }
    
    /* Clear the test signal level, but leave the input signal line enabled. */
    xtcSetTestSignal(portHandle, 0);
    
    /* If we got here then a working external timing card is present. */
    *pCardPresent = 1;
       
    /* If we got here then the timer card detection test was completed. */
    return XTC_ERRORCODE_NOERROR; 
}

/**
 * Sleeps until an interrupt has been generated (or a time limit has been reached).
 * This should only be used if the xtcUseInterupts() function has been used to
 * enable interrupts.
 *
 * @param timelimit the maximum number of milliseconds to wait for the interrupt.
 *
 * @return 0 if an interrupt occured, or 1 if the time limit was exceeded.
 */
int xtcWaitForInterrupt(int portHandle, long timelimit) {
    
    struct pollfd fileDescriptor;
    fileDescriptor.fd = portHandle;
    fileDescriptor.events = POLLIN;
    fileDescriptor.revents = 0;

    struct pollfd pollFileDescriptors[1];
    pollFileDescriptors[0] = fileDescriptor;
    
    nfds_t numFileDescriptors = 1; // Number of file descriptors in pollFileDescriptors.
    int timeoutMillis = (int) timelimit;
    
    int status = poll(pollFileDescriptors, numFileDescriptors, timeoutMillis);

    printf("poll file descriptior revents = %i (POLLIN = %i, timelimit = %i, status = %i)\n", fileDescriptor.revents, POLLIN, timeoutMillis, status);
    
    // The return value of poll is the number of file descriptors that had
    // events occur.
    if (status == 1) {
        return 0;
    }

    // No events occured, we must have reached the time limit.
    return 1;
}

