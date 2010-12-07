/*
 * Copyright 2007-2010 The Authors (see AUTHORS)
 * This file is part of Rangahau, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */
package rangahau;

/**
 * A Roper/Princeton Instruments camera that can produce digital images.
 * 
 *  NOTE: The RoperCamera relies on the presence of a suporting native library,
 *       "libropercamera.so" (on Linux) or "videofile.dll" (on Windows). This 
 *       library must be within the Java library path (which is obtained by a 
 *       call to System.getProperty("java.library.path")) otherwise a 
 *       java.lang.UsatisfiedLinkError will be thrown.
 * 
 * @author Mike Reid
 */
public class RoperCamera implements Camera {

    // Load the attached JNI library.
    static {
        System.loadLibrary("ropercamera");
    }

    /**
     * Indicates whether the camera has been opened for use.
     */
    private boolean isOpen = false;

    /**
     * The camera 'handle' used by the underling native library.
     */
    private int handle = -1;

    /**
     * The width (number of columns) of the images produced by this camera 
     * (measured in pixels)
     */
    private int imageWidth = -1;

    /**
     * The height (number of rows) of the images produced by this camera 
     * (measured in pixels).
     */
    private int imageHeight = -1;

    /**
     * Opens the camera and prepares it for use.
     * 
     * @parameter identifier identifies which camera should be opened for use.
     */
    public void open(String identifier) {
        handle = nativeOpen(identifier);
        isOpen = true;
    }

    /**
     * Closes the camera and releases any resources it was using. It is safe to
     * call close() multiple times and even if the camera has not yet been opened
     * with open(). 
     */
    public void close() {
        if (!isOpen) {
            // The camera is not open, so we don't need to do anything.
            return;
        }

        nativeClose(handle);
        isOpen = false;
    }

    /**
     * Gets the width (number of columns) of the images produced by the camera 
     * (in pixels). 
     * 
     * @return the width (in pixels) of the camera's images. 
     * 
     * @throws IllegalStateException if the camera has not been opened with open(). 
     */
    public int getWidth() {
        if (!isOpen) {
            throw new IllegalStateException("Cannot get the camera image width as the camera has not yet been opened.");
        }

        /**
         * If we already know the camera image width we can return it without
         * having to query the underlying library value.
         */
        if (imageWidth > 0) {
            return imageWidth;
        }

        imageWidth = nativeGetWidth(handle);

        return imageWidth;
    }

    /**
     * Gets the height (number of rows) of the images produced by the camera 
     * (in pixels).
     * 
     * @return the height (in pixels) of the camera's images.
     * 
     * @throws IllegalStateException if the camera has not been opened with open(). 
     */
    public int getHeight() {
        if (!isOpen) {
            throw new IllegalStateException("Cannot get the camera image height as the camera has not yet been opened.");
        }

        /**
         * If we already know the camera image height we can return it without
         * having to query the underlying library value.
         */
        if (imageHeight > 0) {
            return imageHeight;
        }

        imageHeight = nativeGetHeight(handle);

        return imageHeight;
    }

    /**
     * Starts the acquisition of an image with the camera. 
     * 
     * @throws IllegalStateException if the camera has not been opened with open(). 
     */
    public void startAcquisition() {
        if (!isOpen) {
            throw new IllegalStateException("Cannot start an image acqusition as the camera has not yet been opened.");
        }

        nativeStartAcquisition(handle);
    }

    /**
     * Stops acquisition images with the camera.
     */
    public void stopAcquisition() {
        if (!isOpen) {
            throw new IllegalStateException("Cannot stop image acqusitions as the camera has not yet been opened.");
        }

        nativeStopAcquisition(handle);
    }

    /**
     * Indicates whether the camera acquisition has produced an image yet.
     * 
     * @return true if an image is ready to be read from the camera, false
     *         if the acquisition is not complete yet.
     * 
     * @throws IllegalStateException if the camera has not been opened with open().
     */
    public boolean isImageReady() {
        if (!isOpen) {
            throw new IllegalStateException("Cannot determine whether an image has been acquired as the camera has not yet been opened.");
        }

        return nativeIsImageReady(handle);
    }

    /**
     * Obtains the image pixels obtained from the last acquisition and 
     * places them in the array pointed to by pixels. Each element of the
     * pixels array contains a single pixel.
     * 
     * @param pixels the destination to place the pixels of the last image 
     *        acquired by the camera.
     * 
     * @throws IllegalArgumentException if pixels is null.
     * @throws IllegalArgumentException if pixels does not have a number of 
     *         elements which is the same as getWidth()*getHeight() or greater.
     * 
     * @throws IllegalArgumentException if pixels is null.
     * @throws IllegalArgumentException if pixels has too few pixels for the
     *         images produced by this camera (it should have at least as many
     *         pixels as the product of getWidth() and getHeight()).
     * @throws IllegalStateException if the camera has not been opened with open().
     */
    public void getImage(int[] pixels) {
        if (pixels == null) {
            throw new IllegalArgumentException("Cannot get the image as the destination pixels array was null.");
        }
        int pixelsRequired = getWidth() * getHeight();
        if (pixels.length < pixelsRequired) {
            throw new IllegalArgumentException("Cannot get the image as the destination pixels array is too small (it has "
                    + pixels.length + " elements when this camera has images with " + pixelsRequired + " pixels).");
        }
        if (!isOpen) {
            throw new IllegalStateException("Cannot determine whether an image has been acquired as the camera has not yet been opened.");
        }

        nativeGetImage(handle, pixels);
    }

    /**
     * Removes all images that may be buffered on the camera. The next call to
     * isImageReady() should return false, although this not strictly true if
     * a camera is acquiring images in the background.
     */
    public void purgeImages() {
        final int numPixels = getWidth() * getHeight();
        int[] pixels = new int[numPixels];

        // Grab all images but don't do anything with them (just dump them).
        while (isImageReady()) {
            getImage(pixels);
        }

        // There should be no images left in the camera's buffers.
    }

    /**
     * Implements the open() method by calling a function in the underlying 
     * native library.
     *  
     * @param identifier identifies which camera should be opened for use.
     *
     * @return a 'handle' used by the underlying native library that identifies
     *         the camera that was opened.
     */
    native int nativeOpen(String identifier);

    /**
     * Implements the close() method by calling a function in the underlying
     * native library.
     * 
     * @param cameraHandle the handle that identifies the camera to the 
     *        underlying native library.
     */
    native void nativeClose(int cameraHandle);

    /**
     * Implements the getWidth() method by calling a function in the underlying
     * native library.
     * 
     * @param cameraHandle the handle that identifies the camera to the 
     *        underlying native library.
     * 
     * @return the width (number of columns) of the images produced by the camera
     *         (measured in pixels).
     */
    native int nativeGetWidth(int cameraHandle);

    /**
     * Implements the getHeight() method by calling a function in the underlying
     * native library.
     *
     *@param cameraHandle the handle that identifies the camera to the 
     *        underlying native library.
     * 
     * @return the height (number of rows) of the images produced by the camera
     *         (measured in pixels).
     */
    native int nativeGetHeight(int cameraHandle);

    /**
     * Implements the startAcquisition() method by calling a function in the
     * underlying native library.
     * 
     * @param cameraHandle the handle that identifies the camera to the 
     *        underlying native library. 
     */
    native void nativeStartAcquisition(int cameraHandle);

    /**
     * Implements the stopAcquisition() method by calling a function in the
     * underlying native library.
     *
     * @param cameraHandle the handle that identifies the camera to the
     *        underlying native library.
     */
    native void nativeStopAcquisition(int cameraHandle);

    /**
     * Implements the isImageReady() method by calling a function in the 
     * underlying native library.
     * 
     * @return true if an image is ready to be read from the camera, false
     *         if the acquisition is not complete yet.
     * 
     * @param cameraHandle the handle that identifies the camera to the 
     *        underlying native library.
     */
    native boolean nativeIsImageReady(int cameraHandle);

    /**
     * Implements the getImage() method by calling a function in the underlying
     * native library.
     * 
    @param cameraHandle the handle that identifies the camera to the
     *        underlying native library. 
     * 
     * @param pixels the destination to place the image pixels in.
     */
    native void nativeGetImage(int cameraHandle, int[] pixels);
}
