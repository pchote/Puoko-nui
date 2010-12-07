/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <sstream>
#include <iostream>

#include "rangahau_ExternalTimerCard.h"
#include "timercard.h"

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jboolean JNICALL 
Java_rangahau_ExternalTimerCard_nativeGetOutputSignalLevel(JNIEnv* pEnv, jobject pObject, jint handle);

/**
* A utility method that can throw an exception from native code which will
* be received in Java.
*
* @param pEnv - pointer to the JNI environment object.
* @param name - a string describing the Java exception class to throw,
*               eg. "java/lang/IllegalArgumentException"
* @param msg - the message the exception is to display.
*/
void JNU_ThrowByName(JNIEnv* pEnv, const char* name, const char* msg);


///////////////////////////////////////////////////////////////////////////////
/*
 * Closes a parallel port that was opened with nativeOpenPort().
 *
 * @parameter portHandle the (non-negative) handle to the open parallel port 
 *            that was obtained from a call to nativeOpenPort().
 * 
 * @throws IllegalArgumentException if portHandle is not a valid handle.
 *
 * Class:     rangahau_ExternalTimerCard
 * Method:    nativeClosePort
 * Signature: (I)V
 */
JNIEXPORT void JNICALL 
Java_rangahau_ExternalTimerCard_nativeClosePort(JNIEnv* pEnv, jobject pObject, jint handle) {
  if (handle < 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem closing the port as the native device handle was invalid, handle = " << handle;
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());
  }

  int status = xtcClosePort(handle);
  if (status != 0) {
    const char* pMessage = xtcGetErrorMessage(status);
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem closing the port, " << pMessage;
    JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str());
  }
}

///////////////////////////////////////////////////////////////////////////////
/*
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
 *
 * Class:     rangahau_ExternalTimerCard
 * Method:    nativeOpenPort
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL 
Java_rangahau_ExternalTimerCard_nativeOpenPort(JNIEnv* pEnv, jobject pObject, jstring deviceName) {

  bool released = false;
  const char* pBuffer = pEnv->GetStringUTFChars(deviceName, NULL);
  if (pBuffer == NULL)
  {
    return -1; // OutOfMemoryError has already been thrown.
  }
  int status = xtcOpenPort((char*) pBuffer);
  if (!released) 
  { 
    pEnv->ReleaseStringUTFChars(deviceName, pBuffer);
    released = true;
  }

  if (status < 0) {
    const char* pMessage = xtcGetErrorMessage(status);
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem opening the serial port. " << pMessage;
    JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str());
  }

  // The return value of open is the device handle (if non-negative), so return this. 
  return status;
}

///////////////////////////////////////////////////////////////////////////////
/*
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
 *
 * Class:     rangahau_ExternalTimerCard
 * Method:    nativeWriteData
 * Signature: (II)V
 */
JNIEXPORT void JNICALL 
Java_rangahau_ExternalTimerCard_nativeWriteData(JNIEnv* pEnv, jobject pObject, jint handle, jint value) {
 
  if (handle < 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem writing to the data port as the native device handle was invalid, handle = " << handle;
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());
  }

  unsigned char byte = (unsigned char) (value & 0xFF); // Get the lowest byte of the value argument.
 
  int status = xtcWriteData(handle, byte);
  if (status < 0) {
    const char* pMessage = xtcGetErrorMessage(status);
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem writing to the data byte of the device. " << pMessage;
    JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str());
  }
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Reads from the status port within the parallel port. The status port is the byte
 * located at one byte past the base address of the parallel port (that is, 
 * base address+1).
 *
 * @parameter portHandle the (non-negative) handle to the open parallel port 
 *            that was obtained from a call to nativeOpenPort().
 *
 * @returns the value read from the 'status' byte of the parallel port.
 *
 * Class:     rangahau_ExternalTimerCard
 * Method:    nativeReadStatus
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL 
Java_rangahau_ExternalTimerCard_nativeReadStatus(JNIEnv* pEnv, jobject pObject, jint handle) {

  if (handle < 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem reading from the status port as the native device handle was invalid, handle = " << handle;
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());
  }

  unsigned char byte = 0;
  int status = xtcReadStatus(handle, &byte);
  if (status < 0) {
    const char* pMessage = xtcGetErrorMessage(status);
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem reading the status byte from the device. xtcReadStatus() returned "
           << status << ", message = " << pMessage;
    JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str());
  }

  int statusValue = (byte & 0xFF);
  return statusValue;
}

///////////////////////////////////////////////////////////////////////////////
/*
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
 *
 * Class:     rangahau_ExternalTimerCard
 * Method:    nativeReadControl
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL 
Java_rangahau_ExternalTimerCard_nativeReadControl(JNIEnv* pEnv, jobject pObject, jint handle) {

  if (handle < 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem reading from the control port as the native device handle was invalid, handle = " << handle;
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());
  }

  unsigned char byte = 0;
  int status = xtcReadControl(handle, &byte);
  if (status < 0) {
    const char* pMessage = xtcGetErrorMessage(status);
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem reading the control byte from the device. " << pMessage;
    JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str());
  }

  return byte;
}

///////////////////////////////////////////////////////////////////////////////
/*
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
 *
 * Class:     rangahau_ExternalTimerCard
 * Method:    nativeWriteControl
 * Signature: (II)V
 */
JNIEXPORT void JNICALL 
Java_rangahau_ExternalTimerCard_nativeWriteControl(JNIEnv* pEnv, jobject pObject, jint handle, jint value) {

  if (handle < 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem writing to the control port as the native device handle was invalid, handle = " << handle;
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());
  }

  unsigned char byte = (unsigned char) (value & 0xFF); // Get the lowest byte of the value argument.
  int status = xtcWriteControl(handle, byte);
  if (status < 0) {
    const char* pMessage = xtcGetErrorMessage(status);
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem writing to the control byte of the device. " << pMessage;
    JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str());
  }
}

///////////////////////////////////////////////////////////////////////////////
/*
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
 *
 * Class:     rangahau_ExternalTimerCard
 * Method:    nativeWriteControlBit
 * Signature: (III)V
 */
JNIEXPORT void JNICALL 
Java_rangahau_ExternalTimerCard_nativeWriteControlBit(JNIEnv* pEnv, jobject pObject, jint handle, jint bitMask, jint value) {
   unsigned char mask = (unsigned char) (bitMask & 0xFF); // Get the lowest byte of the bitMask argument.
   unsigned char byte = (unsigned char) (value & 0xFF);   // Get the lowest byte of the value argument.

  if (handle < 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem writing a masked bit to the control port as the native device handle was invalid, handle = " << handle;
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());
  }

  int status = xtcWriteControlBit(handle, mask, byte);
  if (status < 0) {
    const char* pMessage = xtcGetErrorMessage(status);
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem writing a control bit to the device. " << pMessage;
    JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str());
  }  
}

/*
 * Class:     rangahau_ExternalTimerCard
 * Method:    nativeGetOutputSignalLevel
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL 
Java_rangahau_ExternalTimerCard_nativeGetOutputSignalLevel(JNIEnv* pEnv, jobject pObject, jint handle) {
  if (handle < 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem getting the output signal level as the native device handle was invalid, handle = " << handle;
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());
  }

  int level = 0;
  int status = xtcGetOutputSignalLevel(handle, &level);
  if (status < 0) {
    const char* pMessage = xtcGetErrorMessage(status);
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem getting the output signal level. " << pMessage;
    JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str());
  } 

  if (level == 0) {
    return 0;
  } else {
    //std::cout << __FILE__ << " " << __LINE__ << " nativeGetOutputSignalLevel() = true" << std::endl; 
    return 1;
  }
}

///////////////////////////////////////////////////////////////////////////////
/**
* A utility method that can throw an exception from native code which will
* be received in Java.
*
* @param pEnv - pointer to the JNI environment object.
* @param name - a string describing the Java exception class to throw,
*               eg. "java/lang/IllegalArgumentException"
* @param msg - the message the exception is to display.
*/
void
JNU_ThrowByName(JNIEnv* pEnv, const char* name, const char* msg)
{
  jclass clazz = pEnv->FindClass(name);
  // If clazz is NULL, an exception has already been thrown.
  if (clazz != NULL) {
    pEnv->ThrowNew(clazz, msg);
  }
  // Free the local reference to the class.
  pEnv->DeleteLocalRef(clazz);
}

#ifdef __cplusplus
}
#endif
