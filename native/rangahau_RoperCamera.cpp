/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <sstream>

#include <master.h>
#include <pvcam.h>

#include "rangahau_RoperCamera.h"
//#include "RoperCamera.h"

#ifdef __cplusplus
extern "C" {
#endif


unsigned short* pPixelBuffer = 0; // Location to place images into (may hold several images at once).

boolean initialised = false; // true if the PVCAM library has been initialised.
boolean cameraOpen = false;  // true if the camera has been opened for .
boolean acquiring = false;   // true if the camera is acquiring images.


// these force proper frame transfer only if pi133b.dat is present.
#define PARAM_FORCE_READOUT_MODE ((CLASS2<<16) + (TYPE_UNS32<<24) + 326)

enum ForceReadOut {
    ALWAYS_CHECK_EXP,
    MAKE_FULL,
    MAKE_FRAME_TRANSFER,
    MAKE_AUTOMATIC
    };

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
 * Checks the error status of the PVCAM library and throws an exception if an
 * exception is found.
 *
 * @param message the mesage describing the PVCAM operation that has just executed.
 * @param line the line number in the source file where the call was made.
 */
void checkPvcamErrorStatus(JNIEnv* pEnv, const char* message, int line);


/**
 * Returns the width (in pixels) of the camera.
 *
 * @param pEnv - pointer to the JNI environment object.
 */
int getWidth(JNIEnv* pEnv, int cameraHandle) {
  unsigned short width = 0;
  pl_ccd_get_ser_size(cameraHandle, &width);
  checkPvcamErrorStatus(pEnv, "Get width (pl_ccd_get_ser_size)", __LINE__);
  
  return width;
}

/**
 * Returns the height (in pixels) of the camera.
 *
 * @param pEnv - pointer to the JNI environment object.
 */
int getHeight(JNIEnv* pEnv, int cameraHandle) {
  unsigned short height = 0;
  pl_ccd_get_par_size(cameraHandle, &height);
  checkPvcamErrorStatus(pEnv,"Get height (pl_ccd_get_par_size)", __LINE__);

  return height;
}


///////////////////////////////////////////////////////////////////////////////
/*
 * Opens the camera and prepares it for use.
 * 
 * @parameter identifier identifies which camera should be opened for use.
 *
 * Class:     rangahau_RoperCamera
 * Method:    nativeOpen
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL 
Java_rangahau_RoperCamera_nativeOpen(JNIEnv* pEnv, jobject pObject, jstring identifier)
{
  bool released = false;
  const char* pBuffer = pEnv->GetStringUTFChars(identifier, NULL);
  if (pBuffer == NULL)
  {
    return -1; // OutOfMemoryError has already been thrown.
  }

  char cameraName[CAM_NAME_LEN]; // camera name rspiusb0 for USB2.0 and
			         // rspipci0 for PCI devices.

  pl_pvcam_init();                // init pvcam libraries.
  checkPvcamErrorStatus(pEnv, "Cannot open the camera as could not initialise the PVCAM library (pl_pvcam_init)", __LINE__);
  initialised = true;

  pl_cam_get_name(0, cameraName); // This should get rspiusb0 for USB2
				  // and rspipci0 for PCI cameras.
  checkPvcamErrorStatus(pEnv, "Cannot open the camera as could not get the camera name (pl_cam_get_name)", __LINE__);

  short handle = -1;
  pl_cam_open(cameraName, &handle, OPEN_EXCLUSIVE); // Try to open the camera.
  checkPvcamErrorStatus(pEnv, "Cannot open the camera (pl_cam_open)", __LINE__);
  cameraOpen = true;

   // Check that the camera is ok.
   boolean ok = pl_cam_get_diags(handle);
   checkPvcamErrorStatus(pEnv, "Cannot open the camera as diagnostics check indicated problem (pl_cam_get_diags)", __LINE__);
   if (!ok) {
    checkPvcamErrorStatus(pEnv, "Cannot open the camera as diagnostics check returned false (pl_cam_get_diags)", __LINE__);
   }
   int cameraHandle = handle;

   // Set parameters for operating the camera.
   long param = 0;
   pl_set_param(cameraHandle, PARAM_SHTR_CLOSE_DELAY, (void*) &param);
   checkPvcamErrorStatus(pEnv, "Cannot open the camera as there was a problem setting the shutter close delay (pl_set_param[PARAM_SHTR_CLOSE_DELAY])", __LINE__);

   param = OUTPUT_NOT_SCAN;
   pl_set_param(cameraHandle, PARAM_LOGIC_OUTPUT, (void*) &param);
   checkPvcamErrorStatus(pEnv, "Cannot open the camera as there was a problem setting the logic output (pl_set_param[OUTPUT_NOT_SCAN])", __LINE__);

   param = EDGE_TRIG_POS;
   pl_set_param(cameraHandle, PARAM_EDGE_TRIGGER, (void*) &param);
   checkPvcamErrorStatus(pEnv, "Cannot open the camera as there was a problem setting the edge trigger (pl_set_param[PARAM_EDGE_TRIGGER])", __LINE__);
   
   param = MAKE_FRAME_TRANSFER;
   pl_set_param(cameraHandle, PARAM_FORCE_READOUT_MODE, (void*) &param);
   checkPvcamErrorStatus(pEnv, "Cannot open the camera as there was a problem setting the readout mode (pl_set_param[MAKE_FRAME_TRANSFER])", __LINE__);

  if (!released) 
  { 
    pEnv->ReleaseStringUTFChars(identifier, pBuffer);
  }

  return cameraHandle;
}


///////////////////////////////////////////////////////////////////////////////
/*
 * Closes the camera and releases any resources it was using. It is safe to
 * call close() multiple times and even if the camera has not yet been opened
 * with open(). 
 *
 * @param cameraHandle the handle that identifies the camera to the 
 *        underlying native library.
 *
 * Class:     rangahau_RoperCamera
 * Method:    nativeClose
 * Signature: (I)V
 */
JNIEXPORT void JNICALL 
Java_rangahau_RoperCamera_nativeClose(JNIEnv* pEnv, jobject pObject, jint handle)
{
  if (acquiring) {
    pl_exp_stop_cont(handle, CCS_CLEAR);
    checkPvcamErrorStatus(pEnv, "Cannot close the camera as there was a problem stopping the exposure (pl_exp_stop_cont)", __LINE__);
    pl_exp_finish_seq(handle, pPixelBuffer, 0);
    checkPvcamErrorStatus(pEnv, "Cannot close the camera as there was a problem finishing the exposure sequence (pl_exp_finish_seq)", __LINE__);
    pl_exp_uninit_seq();
    checkPvcamErrorStatus(pEnv, "Cannot close the camera as there was a problem uninitialising the sequence (pl_exp_uninit_seq)", __LINE__);
  }

  // Aborting is not strictly necessary, but I'm paranoid.
  //pl_exp_abort(handle, CCS_CLEAR);
  //checkPvcamErrorStatus(pEnv, "Cannot close the camera as there was a problem aborting the exposure (pl_exp_abort)", __LINE__);

  if (cameraOpen) {
      cameraOpen = false;
      pl_cam_close(handle);
      checkPvcamErrorStatus(pEnv, "Cannot close the camera as there was a problem closing the camera (pl_cam_close)", __LINE__);
  }

  if (initialised) {
    std::ostringstream buffer;
    buffer << "Cannot close the camera as there was a problem uninitialising the PVCAM library (pl_pvcam_uninit), was initialised = "
           << initialised;

    initialised = false;
    pl_pvcam_uninit();
    checkPvcamErrorStatus(pEnv, buffer.str().c_str(), __LINE__);
  }
  // Release any dynamic memory we allocated.
  if (pPixelBuffer != 0) {
    delete[] pPixelBuffer;
    pPixelBuffer = 0;
  }
}


///////////////////////////////////////////////////////////////////////////////
/*
 * Gets the width (number of columns) of the images produced by the camera 
 * (in pixels). 
 *
 * @param cameraHandle the handle that identifies the camera to the 
 *        underlying native library.
 * 
 * @return the width (in pixels) of the camera's images. 
 *
 * Class:     rangahau_RoperCamera
 * Method:    nativeGetWidth
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL 
Java_rangahau_RoperCamera_nativeGetWidth(JNIEnv* pEnv, jobject pObject, jint handle){
  return getWidth(pEnv, handle);
}


///////////////////////////////////////////////////////////////////////////////
/*
 * @param cameraHandle the handle that identifies the camera to the 
 *        underlying native library.
 *
 * Class:     rangahau_RoperCamera
 * Method:    nativeGetHeight
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL 
Java_rangahau_RoperCamera_nativeGetHeight(JNIEnv* pEnv, jobject pObject, jint handle) {
  return getHeight(pEnv, handle);
};


///////////////////////////////////////////////////////////////////////////////
/*
 * @param cameraHandle the handle that identifies the camera to the 
 *        underlying native library.
 *
 * Class:     rangahau_RoperCamera
 * Method:    nativeStartAcquisition
 * Signature: (I)V
 */
JNIEXPORT void JNICALL 
Java_rangahau_RoperCamera_nativeStartAcquisition(JNIEnv* pEnv, jobject pObject, jint handle) {

  const int imageWidth = getWidth(pEnv, handle);
  const int imageHeight = getHeight(pEnv, handle);

  rgn_type region;
  region.s1 = 0;             // x start ('serial' direction).
  region.s2 = imageWidth-1;  // x end.
  region.sbin = 1;           // x binning (1 = no binning).
  region.p1 = 0;             // y start ('parallel' direction).
  region.p2 = imageHeight-1; // y end.
  region.pbin = 1;           // y binning (1 = no binning).

  pl_exp_init_seq();        // init exposure control libs.

  const long bytesPerPixel = sizeof(unsigned short);  // Number of bytes in a single pixel.
  const long pixelsPerFrame = imageWidth*imageHeight; // Number of pixels in a single frame.
  const long numFramesInBuffer = 1;                           // How many frames we want the pixel buffer to be able to hold. 
  const long pixelsPerBuffer = pixelsPerFrame*numFramesInBuffer; // The size of the continuous pixels buffer (in pixels). 
  long numBytesInPixelBuffer = pixelsPerBuffer*bytesPerPixel;  // The size of the continuous pixels buffer (in bytes). 

  unsigned int exposureTime = 0;  // The exposure time is zero in strobed mode.
  long cameraMode = STROBED_MODE; // camera operating mode.

  bool doBiasFrame = false;

  if (doBiasFrame) {
    cameraMode = TIMED_MODE;
    exposureTime = 0; // Exposure time is zero for bias frames.
  }

  //unsigned int numImagesInSequence = 1;
  unsigned int numRegionDefinitions = 1;
  short circularBufferMode = CIRC_OVERWRITE;

  //unsigned long bytesInSequence = 0; // filled in by the setup function with the number of bytes required for the sequence.
  //pl_exp_setup_seq(handle, numImagesInSequence, numRegionDefinitions, &region, cameraMode, exposureTime, &bytesInSequence); // 0 exposure time always.

  unsigned long bytesInFrame = 0; // filled in by the setup function with the number of bytes in a frame.

  pl_exp_setup_cont(handle, numRegionDefinitions, &region, cameraMode, exposureTime, &bytesInFrame, circularBufferMode);
  //pl_exp_set_cont_mode(handle, circularBufferMode); // CIRC_OVERWRITE mode will not lock the data buffer for each frame.
  checkPvcamErrorStatus(pEnv, "Cannot start acquisition as there was a problem setting up continuous exposure", __LINE__);
  if (pPixelBuffer != 0) {
      delete[] pPixelBuffer;
      pPixelBuffer = 0;
  }

  //pPixelBuffer = new unsigned short[pixelsPerBuffer];
  numBytesInPixelBuffer = bytesInFrame * numFramesInBuffer;
  pPixelBuffer = new unsigned short[numBytesInPixelBuffer];

  pl_exp_start_cont(handle, pPixelBuffer, numBytesInPixelBuffer); // start idling.
  checkPvcamErrorStatus(pEnv, "Cannot start acquisition as there was a problem starting continuous exposure", __LINE__);

  acquiring = true;
};

///////////////////////////////////////////////////////////////////////////////
/*
 * @param cameraHandle the handle that identifies the camera to the
 *        underlying native library.
 *
 * Class:     rangahau_RoperCamera
 * Method:    nativeStopAcquisition
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_rangahau_RoperCamera_nativeStopAcquisition(JNIEnv* pEnv, jobject pObject, jint handle) {
  if (acquiring) {
    pl_exp_stop_cont(handle, CCS_CLEAR);
    checkPvcamErrorStatus(pEnv, "Cannot stop acquisition as there was a problem stopping the exposure (pl_exp_stop_cont)", __LINE__);
    pl_exp_finish_seq(handle, pPixelBuffer, 0);
    checkPvcamErrorStatus(pEnv, "Cannot stop acquisition as there was a problem finishing the exposure sequence (pl_exp_finish_seq)", __LINE__);
    pl_exp_uninit_seq();
    checkPvcamErrorStatus(pEnv, "Cannot stop acquisition as there was a problem uninitialising the sequence (pl_exp_uninit_seq)", __LINE__);
  }
  acquiring = false;

  // Release any dynamic memory we allocated.
  if (pPixelBuffer != 0) {
    delete[] pPixelBuffer;
    pPixelBuffer = 0;
  }
}

///////////////////////////////////////////////////////////////////////////////
/*
 * @param cameraHandle the handle that identifies the camera to the 
 *        underlying native library.
 *
 * Class:     rangahau_RoperCamera
 * Method:    nativeIsImageReady
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL 
Java_rangahau_RoperCamera_nativeIsImageReady(JNIEnv* pEnv, jobject pObject, jint handle) {

  bool imageReady = false;

  short status = READOUT_IN_PROGRESS;  // Indicates whether the a frame is available for readout or not.
  unsigned long bytesStored = 0;       // The number of bytes currently stored in the buffer.
  unsigned long numFilledBuffers = 0;  // The number of times the buffer has been filled.
  pl_exp_check_cont_status(handle, &status, &bytesStored, &numFilledBuffers);
  checkPvcamErrorStatus(pEnv, "Cannot determine whether an image is ready (pl_exp_check_cont_status)", __LINE__);

  if (status == FRAME_AVAILABLE) {
    imageReady = true;
  }

  return imageReady;
};


///////////////////////////////////////////////////////////////////////////////
/*
 * @param cameraHandle the handle that identifies the camera to the 
 *        underlying native library.
 *
 * Class:     rangahau_RoperCamera
 * Method:    nativeGetImage
 * Signature: (I[I)V
 */
JNIEXPORT void JNICALL 
Java_rangahau_RoperCamera_nativeGetImage(JNIEnv* pEnv, jobject pObject, jint handle, jintArray pixels) {

  bool released = false;

  void* pLastImage = 0; // Points to the start of the last image that was acquired.
  pl_exp_get_latest_frame(handle, &pLastImage);
  checkPvcamErrorStatus(pEnv, "Cannot get the latest frame (pl_exp_get_latest_frame)", __LINE__);

  // If the pixel buffer pointer to the last image acquired was not set by PVCam (in the call to 
  // pl_exp_get_oldest_frame()) then there is a problem.  
  if (pLastImage == 0) {
     // Throw an exception for Java to catch.
     std::ostringstream buffer;
     buffer << __FILE__ << " " << __LINE__ 
            << " : Cannot get an image as there are no pixels available (you should have checked with isImageReady()!)";
     JNU_ThrowByName(pEnv, "java/lang/IllegalStateException", buffer.str().c_str());    
  }

  // Lock the input array (owned by Java) before we grab the pixels.
  int* pPixels = (int*) pEnv->GetPrimitiveArrayCritical(pixels, 0);
  if (pPixels == NULL) {
    pEnv->ReleasePrimitiveArrayCritical(pixels, pPixels, 0);

     // Throw an exception for Java to catch.
     std::ostringstream buffer;
     buffer << __FILE__ << " " << __LINE__ 
            << " : Cannot get an image as there was a problem locking the destination pixel array.";
     JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str());
  }

  const int numPixels = getWidth(pEnv, handle)*getHeight(pEnv, handle);

  unsigned short* pImage = (unsigned short*) pLastImage;
  for (int count = 0; count < numPixels; ++count) {
    pPixels[count] = pImage[count];
  }

  if (!released)
  {
    // Release the locked pixel array.
    pEnv->ReleasePrimitiveArrayCritical(pixels, pPixels, 0);
  }
};

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

/**
 * Checks the error status of the PVCAM library and throws an exception if an
 * exception is found.
 *
 * @param pEnv - pointer to the JNI environment object.
 * @param message the mesage describing the PVCAM operation that has just executed.
 * @param line the line number in the source file where the call was made.
 */
void checkPvcamErrorStatus(JNIEnv* pEnv, const char* message, int line) {

    int code = pl_error_code();
    if (code != 0) {
      char errorMessage[ERROR_MSG_LEN];
      errorMessage[0] = '\0';

      pl_error_message(code, errorMessage);
      std::ostringstream buffer;
      buffer << __FILE__ << " " << line
             << " : PVCAM error : error code = " << code
             << " " << errorMessage
             << " : " << message;

      JNU_ThrowByName(pEnv, "java/lang/RuntimeException", buffer.str().c_str());
    }
}


#ifdef __cplusplus
}
#endif
