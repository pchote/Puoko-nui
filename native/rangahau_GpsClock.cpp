/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

/**
 * Controls a GpsClock connected via a FTDI FT232 device (USB connectivity).
 *
 * The implementation of this library relies on the libftdi userspace
 * library (which uses the libusb userspace library). The source code
 * to libftdi can be obtained from:
 *   http://www.intra2net.com/en/developer/libftdi/index.php
 *
 * Author: Michael Reid
 */


#include <sstream>
#include <iostream>
#include <map>
#include <vector>
#include <string>

#include <cstring>

#include "ftdi.h"
#include "rangahau_GpsClock.h"



#ifdef __cplusplus
extern "C" {
#endif

/**
 * Holds associations between a FTDI device's identifier 
 * and a pointer to the FTDI context about that open device.
 */
std::map<std::string, struct ftdi_context*> openDevices;


///////////////////////////////////////////////////////////////////////////////
// FUNCTION DECLARATIONS.
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

/**
 * Checks whether a called to an underlying library failed and raises an
 * exception if this happened.
 *
 * @param pointer to the JNI environment object.
 * @param file the name of the source file (usually this is __FILE__).
 * @param line the line number the check was made on (usually this is __LINE__).
 * @param status the status code returned by the library call (negative is assumed to be an error)
 * @param the message to be included in the exception.
 */
void checkLibraryStatus(JNIEnv* pEnv, char* file, int line, int status, const std::string& message);


///////////////////////////////////////////////////////////////////////////////
/*  
 * Opens the native device. This is implemented as a native method.
 * 
 * @param name identifies the device. This name is one of the names 
 *             returned by the findAllDeviceNames() method.
 * 
 * @return the native handle (file descriptor) used to identify the device
 *         by the operating system. This is implemented as a native method.  
 * 
 * @throws IllegalArgumentException if name is null or an empty string.
 * @throws RuntimeException if there was a problem opening the device.
 *
 * Class:     rangahau_GpsClock
 * Method:    nativeOpen
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT void JNICALL 
Java_rangahau_GpsClock_nativeOpen(JNIEnv* pEnv, jobject pObject, jstring name) {
  
  const int maxDeviceNameLength = 256; // maximum number of characters allowed in the device name string.
  char pBuffer[maxDeviceNameLength+1]; // UTF-8 representation of deviceName.
  int actualLength = pEnv->GetStringLength(name);
  if (actualLength <= 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem opening the userspace GPS clock (FTDI FT232) as the device name given was null.";
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());      
  }
  if (actualLength >= maxDeviceNameLength) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem opening the userspace GPS clock (FTDI FT232) as the device name given was " << actualLength
           << " but the maximum length permitted is " << maxDeviceNameLength << " characters.";
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());      

  }
  pEnv->GetStringUTFRegion(name, 0, actualLength, pBuffer); // Note: this buffer does not need to be freed.
  pBuffer[maxDeviceNameLength] = '\0'; // Ensure the buffer is null terminated.
    
  // Check that the device name we have been given is valid (not null and not blank).
  if (pBuffer == 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem opening the userspace GPS clock (FTDI FT232) as the device name given was null.";
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());         
  }
  if (strlen(pBuffer) < 1) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem opening the userspace GPS clock (FTDI FT232) as the device name given was an empty string.";
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());         
  }

  // Get the identifiers of all the FTDI FT232 devices attached to the system.
  // A linked-list containing information about each FTDI device.
  struct ftdi_device_list* pDeviceList = 0;

  const int vendorId = 0x0403;  // The USB vendor identifier for the FTDI company.
  const int productId = 0x6001; // The USB product identifier for the FT232 device.

  // Determine the number of FTDI devices attached (note we don't need a FTDI context
  // to do this, even though that is the first argument of the ftdi_usb_find_all() function).
  const int numDevices = ftdi_usb_find_all(0, &pDeviceList, vendorId, productId);
  checkLibraryStatus(pEnv, __FILE__, __LINE__, numDevices, "ftdi_usb_find_all() returned an error code");

  struct ftdi_device_list* pDevice = pDeviceList; // Used to point to the elements of the device list.
  std::vector<std::string> deviceNames;

  std::string deviceNameToOpen(pBuffer);
  bool alreadyOpen = false;

  // See whether we think the device was already open.
  if (openDevices.find(deviceNameToOpen) != openDevices.end()) {
    alreadyOpen = true;
  }

  bool foundDevice = false;
  struct usb_device* pUsbDevice = 0;

  for (int device = 0; !foundDevice && (device < numDevices); ++device) {
    // Get the bus identifier of the USB device. This is the unique identifier
    // will use to access the FTDI devices (since the bus identifier remains
    // until the device is unplugged, even if the host computer is shut down 
    // and restarted).
    std::string name(pDevice->dev->filename);

    if (name == deviceNameToOpen) {
      pUsbDevice = pDevice->dev;
      foundDevice = true;
    }

    pDevice = pDevice->next;
  }

  // We don't need the list of devices obtained with ftdi_usb_find_all() any more.
  ftdi_list_free(&pDeviceList);

  // If the requested device was not found, and was not open previously then
  // there is a problem (the device cannot be opened).
  if (!foundDevice && !alreadyOpen) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem opening the userspace GPS clock (FTDI FT232) no device with the name of "
           << deviceNameToOpen.c_str() << " was connected to the system.";
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());      
  }

  // If the requested device was previously open, but it is not now then there is a problem.
  // We should report this problem and clean up by closing the device.
  if (!foundDevice && alreadyOpen) {
    // Close and de-register the device as it is no longer present on the system.
    struct ftdi_context* pContext = openDevices[deviceNameToOpen];

    int status = ftdi_usb_close(pContext);
    checkLibraryStatus(pEnv, __FILE__, __LINE__, status, "ftdi_usb_close() returned an error code");

    //ftdi_free(pContext);
    ftdi_deinit(pContext);
    delete pContext;
    pContext = 0;

    openDevices.erase(deviceNameToOpen);

    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem opening the userspace GPS clock (FTDI FT232) no device with the name of "
           << deviceNameToOpen.c_str() << " was connected to the system, although the device is thought to be open (has the device been unplugged?).";
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());
  }

  // If the requested device was found and the device is already open then there is nothing more for us to do.
  if (foundDevice && alreadyOpen) {
    return;
  }

  // If the requested device was found and the device is not open then we should open it and
  // register the device with the list of devices.
  if (foundDevice && !alreadyOpen) {
//    struct ftdi_context* pContext = ftdi_new();
    struct ftdi_context* pContext = new struct ftdi_context;
    int status = ftdi_init(pContext);
    checkLibraryStatus(pEnv, __FILE__, __LINE__, status, "ftdi_init() returned an error code");

    // Prepare the device for use with libftdi library calls.
    status = ftdi_usb_open_dev(pContext, pUsbDevice);
    checkLibraryStatus(pEnv, __FILE__, __LINE__, status, "ftdi_usb_open_dev() returned an error code");

    //ftdi_enable_bitbang(pContext, 0xFF);

    status = ftdi_set_baudrate(pContext, 115200);
    checkLibraryStatus(pEnv, __FILE__, __LINE__, status, "ftdi_set_baudrate() returned an error code");

    status = ftdi_set_line_property(pContext, BITS_8, STOP_BIT_1, NONE);
    checkLibraryStatus(pEnv, __FILE__, __LINE__, status, "ftdi_set_line_property() returned an error code");

    status = ftdi_setflowctrl(pContext, SIO_DISABLE_FLOW_CTRL);
    checkLibraryStatus(pEnv, __FILE__, __LINE__, status, "ftdi_setflowctrl() returned an error code");

    unsigned char latency = 1; // the latency in milliseconds before partially full bit buffers are sent.
    status = ftdi_set_latency_timer(pContext, latency);
    checkLibraryStatus(pEnv, __FILE__, __LINE__, status, "ftdi_set_latency_timer() returned an error code");

    // Register the device as open.
    openDevices[deviceNameToOpen] = pContext;
  }
};

///////////////////////////////////////////////////////////////////////////////
/*  
 * Closes the device.
 * 
 * @param handle the the (non-negative) handle to the open device 
 *        that was obtained from a call to nativeOpen(). 
 * @throws IllegalArgumentException if handle is not a valid handle. 
 *
 * Class:     rangahau_GpsClock
 * Method:    nativeClose
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL 
Java_rangahau_GpsClock_nativeClose(JNIEnv* pEnv, jobject pObject, jstring name) {
  const int maxDeviceNameLength = 256; // maximum number of characters allowed in the device name string.
  char pBuffer[maxDeviceNameLength+1]; // UTF-8 representation of deviceName.
  int actualLength = pEnv->GetStringLength(name);
  if (actualLength <= 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem closing the userspace GPS clock (FTDI FT232) as the device name given was null.";
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());      
  }
  if (actualLength >= maxDeviceNameLength) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem closing the userspace GPS clock (FTDI FT232) as the device name given was " << actualLength
           << " but the maximum length permitted is " << maxDeviceNameLength << " characters.";
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());      

  }
  pEnv->GetStringUTFRegion(name, 0, actualLength, pBuffer); // Note: this buffer does not need to be freed.
  pBuffer[maxDeviceNameLength] = '\0'; // Ensure the buffer is null terminated.
    
  // Check that the device name we have been given is valid (not null and not blank).
  if (pBuffer == 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem opening the userspace GPS clock (FTDI FT232) as the device name given was null.";
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());         
  }
  if (strlen(pBuffer) < 1) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem opening the userspace GPS clock (FTDI FT232) as the device name given was an empty string.";
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());         
  }   

  std::string deviceNameToClose(pBuffer);

  // Close and de-register the device as it is no longer present on the system.
  struct ftdi_context* pContext = openDevices[deviceNameToClose];
  if (pContext != 0) {
    int status = ftdi_usb_close(pContext);
    checkLibraryStatus(pEnv, __FILE__, __LINE__, status, "ftdi_usb_close() returned an error code");

    ftdi_deinit(pContext);
    delete pContext;
    pContext = 0;    
    //ftdi_free(pContext);
  }

  // Register the device as closed.
  openDevices.erase(deviceNameToClose);
};

///////////////////////////////////////////////////////////////////////////////
/* 
 * Performs the writing of a buffer to the device (across the USB to the
 * digital I/O pins). This is implemented as a native method.
 * 
 * @param handle the the (non-negative) handle to the open device 
 *            that was obtained from a call to nativeOpen().
 * @param data the data to be written to the device. Each element of the
 *        buffer represents an 8-bit unsiogned byte value (ranging from 0 
 *        to 255).
 * 
 * @throws IllegalArgumentException if handle is not a valid handle.
 * @throws IllegalArgumentException if data is null.
 * @throws IllegalStateException if the device has not yet been opened with open().
 *
 * Class:     rangahau_GpsClock
 * Method:    nativeWrite
 * Signature: (Ljava/lang/String;[I)V
 */
JNIEXPORT void JNICALL 
Java_rangahau_GpsClock_nativeWrite(JNIEnv* pEnv, jobject pObject, jstring name, jintArray data) {
  const int maxDeviceNameLength = 256; // maximum number of characters allowed in the device name string.
  char pBuffer[maxDeviceNameLength+1]; // UTF-8 representation of deviceName.
  int actualLength = pEnv->GetStringLength(name);
  if (actualLength <= 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem writing data to the userspace GPS clock (FTDI FT232) as the device name given was null.";
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());      
  }
  if (actualLength >= maxDeviceNameLength) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem writing data to the userspace GPS clock (FTDI FT232) as the device name given was " << actualLength
           << " but the maximum length permitted is " << maxDeviceNameLength << " characters.";
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());      

  }
  pEnv->GetStringUTFRegion(name, 0, actualLength, pBuffer); // Note: this buffer does not need to be freed.
  pBuffer[maxDeviceNameLength] = '\0'; // Ensure the buffer is null terminated.
    
  // Check that the device name we have been given is valid (not null and not blank).
  if (pBuffer == 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem writing data to the userspace GPS clock (FTDI FT232) as the device name given was null.";
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());         
  }
  if (strlen(pBuffer) < 1) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem writing data to the userspace GPS clock (FTDI FT232) as the device name given was an empty string.";
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());         
  }   

  std::string deviceNameToWrite(pBuffer);  

  // Check that the device is open and plugged in.
  if (openDevices.find(deviceNameToWrite) == openDevices.end()) {
     // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem writing data to the userspace GPS clock (FTDI FT232) as the device identified by "
           << deviceNameToWrite.c_str() << " is not open for writing.";
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());  
  }

  // Lock the input array (owned by Java) before we grab the elements.
  int* pElements = (int*) pEnv->GetPrimitiveArrayCritical(data, 0);
  if (pElements == NULL) {
    pEnv->ReleasePrimitiveArrayCritical(data, pElements, 0);
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem writing to the GPS clock (FTDI FT232) as the data buffer given was null, device name = " << deviceNameToWrite.c_str();
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());    
    return;
  }

  // Get the number of elements in the data buffer we are to write out.
  int numElements = pEnv->GetArrayLength(data);

  // Convert the elements from ints into unsigned byte values.
  unsigned char* bytes = new unsigned char[numElements];
        
  for (int element = 0; element < numElements; ++element) {
      bytes[element] = (unsigned char) (pElements[element] & 0xFF);
  }
  
  // Release the Java array now that we have made a local copy.
  pEnv->ReleasePrimitiveArrayCritical(data, pElements, 0);

  // Write to the device.
  int status = ftdi_write_data(openDevices[deviceNameToWrite], bytes, numElements);

  if (status != numElements) {
    // Release the local copy of the data.
    if (bytes != 0) {
      delete[] bytes;
      bytes = 0;
    }

//    status = ftdi_usb_purge_tx_buffer(openDevices[deviceNameToWrite]);
//    if (status < 0) {
//    // Release the local copy of the data.
//      if (bytes != 0) {
//        delete[] bytes;
//        bytes = 0;
//      }
//      std::ostringstream buffer;
//      buffer << __FILE__ << " " << __LINE__ << " : There was a problem purging the bytes sending to the GPS clock (FTDI FT232). The return code from ftdi_usb_purge_tx_buffer() was "
//             << status;
//      JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str());
//    }
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem writing " << numElements 
           << " bytes to the GPS clock (FTDI FT232). The return code from ftdi_write_data() was " << status;
    JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str()); 
  }

  // Ensure the data has been transferred to the underlying USB layer.
//  std::cerr << __FILE__ << " " << __LINE__ << " : purging transmit data on FTDI device "
//            << deviceNameToWrite << " to ensure bytes were written ..."<< std::endl;
//
//  status = ftdi_usb_purge_tx_buffer(openDevices[deviceNameToWrite]);
//  if (status < 0) {
//    // Release the local copy of the data.
//    if (bytes != 0) {
//      delete[] bytes;
//      bytes = 0;
//    }
//    std::ostringstream buffer;
//    buffer << __FILE__ << " " << __LINE__ << " : There was a problem purging the bytes sending to the GPS clock (FTDI FT232). The return code from ftdi_usb_purge_tx_buffer() was "
//           << status;
//    JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str());
//  }

  // Release the local copy of the data, if we haven't done so already.
  if (bytes != 0) {
      delete[] bytes;
      bytes = 0;
  }
};

/*
 * Performs reading from the device (across the USB). This is implemented
 * as a native method.
 *
 * @param name identifies the device. This name is one of the names
 *             returned by the findAllDeviceNames() method.
 * @param timeoutMillis the maximum number of milliseconds to wait for
 *        a message to appear from the device.
 * @param data where data read from the device is to be put. Each element of
 *        the buffer represents an 8-bit unsigned byte value (ranging from 0
 *        to 255).
 *
 * @return the number of bytes read from the device.
 *
 * @throws IllegalArgumentException if name is not a valid device name.
 * @throws IllegalArgumentException if data is null.
 * @throws IllegalStateException if the device has not yet been opened with open().
 * @throws RuntimeException if a message was not read from the device within the timeout limit.
 *
 * Class:     rangahau_GpsClock
 * Method:    nativeRead
 * Signature: (Ljava/lang/String;J[I)I
 */
JNIEXPORT jint
JNICALL Java_rangahau_GpsClock_nativeRead(JNIEnv* pEnv, jobject pObject, jstring name, jintArray data) {
  const int maxDeviceNameLength = 256; // maximum number of characters allowed in the device name string.
  char pBuffer[maxDeviceNameLength+1]; // UTF-8 representation of deviceName.
  int actualLength = pEnv->GetStringLength(name);
  if (actualLength <= 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem reading data from the userspace GPS clock (FTDI FT232) as the device name given was null.";
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());
  }
  if (actualLength >= maxDeviceNameLength) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem reading data from the userspace GPS clock (FTDI FT232) as the device name given was " << actualLength
           << " but the maximum length permitted is " << maxDeviceNameLength << " characters.";
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());

  }
  pEnv->GetStringUTFRegion(name, 0, actualLength, pBuffer); // Note: this buffer does not need to be freed.
  pBuffer[maxDeviceNameLength] = '\0'; // Ensure the buffer is null terminated.

  // Check that the device name we have been given is valid (not null and not blank).
  if (pBuffer == 0) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem reading data from the userspace GPS clock (FTDI FT232) as the device name given was null.";
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());
  }
  if (strlen(pBuffer) < 1) {
    // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem reading data from the userspace GPS clock (FTDI FT232) as the device name given was an empty string.";
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());
  }

  std::string deviceNameToRead(pBuffer);

  // Check that the device is open and plugged in.
  if (openDevices.find(deviceNameToRead) == openDevices.end()) {
     // Throw an exception for Java to catch.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem reading data from the userspace GPS clock (FTDI FT232) as the device identified by "
           << deviceNameToRead.c_str() << " is not open for reading.";
    JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());
  }

  // Convert the elements from ints into unsigned byte values.
  const int bufferLength = 4096; // the FTDI library can transfer with packet sizes pof 64 or 512 bytes.
  unsigned char bytes[bufferLength];

  // Read from the device.
  int status = ftdi_read_data(openDevices[deviceNameToRead], bytes, bufferLength);

  if (status < 0) {
    // There was an error reported by the underlying library.
    std::ostringstream buffer;
    buffer << __FILE__ << " " << __LINE__ << " : There was a problem reading "
            << "bytes from the GPS clock (FTDI FT232). The return code from ftdi_read_data() was " << status;

    JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str());
  } else {
      // There were zero or more bytes read. Convert these bytes to integer
      // values and place in the destination array.
      if (status > 0) {
        // Lock the destination array (owned by Java) before we assign the elements.
        int* pElements = (int*) pEnv->GetPrimitiveArrayCritical(data, 0);

        // Get the number of elements in the data buffer we can write into.
        jint numElements = pEnv->GetArrayLength(data); // initialise to maximum destination length.

        if (status < numElements) { // actual amount of data to transfer.
          numElements = status;
        }

        if (pElements == NULL) {
          pEnv->ReleasePrimitiveArrayCritical(data, pElements, 0);
          // Throw an exception for Java to catch.
          std::ostringstream buffer;
          buffer << __FILE__ << " " << __LINE__ << " : There was a problem reading from the GPS clock (FTDI FT232) as could not lock the destination array, device name = " << deviceNameToRead.c_str();
          JNU_ThrowByName(pEnv, "java/lang/IllegalArgumentException", buffer.str().c_str());
          return -1;
        } else {
          // Copy the data over.
          for (int element = 0; element < numElements; ++element) {
            pElements[element] = bytes[element];
          }

          // Release the Java array now that we assigned the elements.
          pEnv->ReleasePrimitiveArrayCritical(data, pElements, 0);
        }
      }
  }

  return status;
};

///////////////////////////////////////////////////////////////////////////////
/*  
 * Finds the identifiers for all FTDI 232 devices connected to the system.
 * These identifiers can be given to the nativeOpen() method to select
 * which FTDI 232 device should be opened for use.
 * 
 * Note: These identifiers are valid until a device is added or removed 
 *        from the system (to clarify further, are still valid if the host 
 *        computer is restarted, but not if a FTDI 232 is added or removed).
 * 
 * @return identifiers for all FTDI 232 connected to the system.
 *
 * Class:     rangahau_GpsClock
 * Method:    nativeFindAllDeviceIdentifiers
 * Signature: ()[Ljava/lang/String;
 */
JNIEXPORT jobjectArray JNICALL 
Java_rangahau_GpsClock_nativeFindAllDeviceIdentifiers(JNIEnv* pEnv, jclass pClass) {

  // A linked-list containing information about each FTDI device.
  struct ftdi_device_list* pDeviceList = 0;

  const int vendorId = 0x0403;  // The USB vendor identifier for the FTDI company.
  const int productId = 0x6001; // The USB product identifier for the FT232 device.

  // Determine the number of FTDI devices attached (note we don't need a FTDI context
  // to do this, even though that is the first argument of the ftdi_usb_find_all() function).
  const int numDevices = ftdi_usb_find_all(0, &pDeviceList, vendorId, productId);
  checkLibraryStatus(pEnv, __FILE__, __LINE__, numDevices, "ftdi_usb_find_all() returned an error code");

  struct ftdi_device_list* pDevice = pDeviceList; // Used to point to the elements of the device list.
  std::vector<std::string> deviceNames;

  for (int device = 0; device < numDevices; ++device) {
    // Get the bus identifier of the USB device. This is the unique identifier
    // will use to access the FTDI devices (since the bus identifier remains
    // until the device is unplugged, even if the host computer is shut down 
    // and restarted).
    std::string name(pDevice->dev->filename);
    deviceNames.push_back(name);

    pDevice = pDevice->next;
  }

  // We don't need the list of devices obtained with ftdi_usb_find_all() any more.
  ftdi_list_free(&pDeviceList);

  // Copy the list of names into a form that can be returned to Java.
  jclass stringArrayClass = pEnv->FindClass("java/lang/String");
  jobjectArray names = pEnv->NewObjectArray(deviceNames.size(), stringArrayClass, 0);

  for (unsigned int count = 0; count < deviceNames.size(); ++count) {
    jstring name = pEnv->NewStringUTF(deviceNames[count].c_str()); // create a Java string to be returned to the Java program.
    pEnv->SetObjectArrayElement(names, count, name);               // add the string to the array we will return.
    pEnv->DeleteLocalRef(name);                                    // indicate we no longer need a local reference to the string.     
  }

  return names;
};

/**
 * Checks whether a called to an underlying library failed and raises an 
 * exception if this happened.
 *
 * @param pointer to the JNI environment object.
 * @param file the name of the source file (usually this is __FILE__).
 * @param line the line number the check was made on (usually this is __LINE__).
 * @param status the status code returned by the library call (negative is assumed to be an error)
 * @param the message to be included in the exception. 
 */
void checkLibraryStatus(JNIEnv* pEnv, char* file, int line, int status, const std::string& message) {
    if (status >= 0) {
        return;
    }

    // There was a problem, so throw an exception.
    std::ostringstream buffer;
    buffer << file << " line " << line << " : " << message << ", status = "
           << status;
    JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str());
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

