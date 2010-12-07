/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

/* 
 * File:   timercard.h
 * Author: sullivan
 *
 * Created on 28 March 2008, 13:34
 */

#ifndef _TIMERCARD_H
#define	_TIMERCARD_H

#ifdef	__cplusplus
extern "C" {
#endif

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
int xtcClosePort(int portHandle);

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
int xtcOpenPort(char* deviceName);

/**
 * Returns a message string describing an error code.
 *
 * @parameter errorCode the error message code.
 *
 * @returns the message string that describes the error code.
 */
const char* xtcGetErrorMessage(int errorCode);

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
int xtcPowerOn(int portHandle);

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
int xtcSetTimingEdgeType(int portHandle, int edgeType);

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
int xtcGetTimingEdgeType(int portHandle, int* pEdgeType);

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
int xtcSetInterrupts(int portHandle, int enable);

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
int xtcEnableInputPulses(int portHandle, int enable);

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
int xtcSetTestSignal(int portHandle, int level);

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
int xtcSendTestPulse(int portHandle);

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
int xtcSetExposureTime(int portHandle, int exposureTime);

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
int xtcGetInputSignalLevel(int portHandle, int* pLevel);

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
int xtcGetOutputSignalLevel(int portHandle, int* pLevel);

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
int xtcDetectTimerCard(int portHandle, int* pCardPresent);


/* Internal functions. */
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
int xtcWriteData(int portHandle, unsigned char value);

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
int xtcReadStatus(int portHandle, unsigned char* pValue);

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
int xtcReadControl(int portHandle, unsigned char* pValue);

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
int xtcWriteControl(int portHandle, unsigned char value);

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
int xtcWriteControlBit(int portHandle, unsigned char bitmask, unsigned char value);

/**
 * Sleeps until an interrupt has been generated (or a time limit has been reached).
 * This should only be used if the xtcUseInterupts() function has been used to
 * enable interrupts.
 *
 * @param timelimit the maximum number of milliseconds to wait for the interrupt.
 *
 * @return 0 if an interrupt occured, or 1 if the time limit was exceeded.
 */
int xtcWaitForInterrupt(int portHandle, long timelimit);


#ifdef	__cplusplus
}
#endif

#endif	/* _TIMERCARD_H */

