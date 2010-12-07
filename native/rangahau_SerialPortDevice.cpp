/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

/**
 * Implements native methods for the rangahau.SerialPortDevice Java class that manages a RS-232 serial port device. 
 * 
 * @author Mike Reid
 *
 * For more information on programming the serial port under Linux refer to: 
 *   http://digilander.libero.it/robang/rubrica/serial.htm
 *
 */


#define _POSIX_SOURCE 1       /* POSIX compliant source */

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <string.h>

#include <sstream>

#include "rangahau_SerialPortDevice.h"

struct termios newtio;

#ifdef __cplusplus
extern "C" {
#endif


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
 * Class:     rangahau_SerialPortDevice
 * Method:    nativeOpen
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL 
Java_rangahau_SerialPortDevice_nativeOpen(JNIEnv* pEnv, jobject pObject, jstring deviceName) {
  int handle = -1; // File descriptor 'handle' to the device.

  bool released = false;
  const char* pBuffer = pEnv->GetStringUTFChars(deviceName, NULL);
  if (pBuffer == NULL)
  {
    return -1; // OutOfMemoryError has already been thrown.
  }
  int status = open((char*) pBuffer, O_RDWR | O_NOCTTY);
  if (!released) 
  { 
    pEnv->ReleaseStringUTFChars(deviceName, pBuffer);
    released = true;
  }

  if (status < 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem opening the port. Error code from open() was " << status;
    JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str());
  } else {
    handle = status;
  }

  // Configure the serial port for use.
  int baudrate = B9600;
  memset(&newtio, 0, sizeof(newtio));
  newtio.c_cflag = baudrate | CRTSCTS | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  // set input mode (non-canonical, no echo,...)
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME] = 0;  // inter-character timer unused.
  newtio.c_cc[VMIN] = 0;   // no blocking read.

  tcflush(handle, TCIFLUSH);
  tcsetattr(handle, TCSANOW, &newtio);

  // Ensure the port is non-blocking on reads.
  fcntl(handle, F_SETFL, FNDELAY);

  return handle;
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Class:     rangahau_SerialPortDevice
 * Method:    nativeClose
 * Signature: (I)V
 */
JNIEXPORT void JNICALL 
Java_rangahau_SerialPortDevice_nativeClose(JNIEnv* pEnv, jobject pObject, jint handle) {
  if (handle < 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem closing the serial port as the native device handle was invalid, handle = " << handle;
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());
  }

  int status = close(handle);
  if (status != 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem closing the serial port. Error code from close() was " << status;
    JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str());
  }
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Class:     rangahau_SerialPortDevice
 * Method:    nativeBytesAvailable
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL 
Java_rangahau_SerialPortDevice_nativeBytesAvailable(JNIEnv* pEnv, jobject pObject, jint handle) {
  if (handle < 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem determining the bytes available for reading as the native device handle was invalid, handle = " << handle;
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());
  }

  int bytes = 0;
  int status = ioctl(handle, FIONREAD, &bytes);
  if (status != 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem determining the bytes available for reading. Error code from ioctl(FIONREAD) was " << status;
    JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str());
  }

  return bytes;
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Class:     rangahau_SerialPortDevice
 * Method:    nativeReadByte
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL 
Java_rangahau_SerialPortDevice_nativeReadByte(JNIEnv* pEnv, jobject pObject, jint handle) {
  if (handle < 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem reading a byte from the serial port as the native device handle was invalid, handle = " << handle;
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());
  }

  int byte = 0;
  int status = read(handle, &byte, 1);
  if (status < 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem reading a byte from the serial port. Error code from read() was " << status;
    JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str());
  }

  return (byte & 0xFF); // Only the lowest byte of the integer is valid.
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Class:     rangahau_SerialPortDevice
 * Method:    nativeWriteByte
 * Signature: (II)V
 */
JNIEXPORT void JNICALL 
Java_rangahau_SerialPortDevice_nativeWriteByte(JNIEnv* pEnv, jobject pObject, jint handle, jint value) {
  if (handle < 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem writing a byte to the serial port as the native device handle was invalid, handle = " << handle;
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());
  }

  int byte = (value & 0xFF); // Only the lowest byte of the integer is valid.
  int status = write(handle, &byte, 1);
  if (status != 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem writing a byte to the serial port. Error code from write() was " << status;
    JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str());
  }
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Class:     rangahau_SerialPortDevice
 * Method:    nativeFlush
 * Signature: (I)V
 */
JNIEXPORT void JNICALL 
Java_rangahau_SerialPortDevice_nativeFlush(JNIEnv* pEnv, jobject pObject, jint handle) {
  if (handle < 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem flushing data on the serial port as the native device handle was invalid, handle = " << handle;
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());
  }
  int status = tcdrain(handle);
  if (status != 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem flushing data on the serial port. Error code from write() was " << status;
    JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str());
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
