/*
 * Copyright 2007-2010 The Authors (see AUTHORS)
 * This file is part of Rangahau, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */
package rangahau;

/**
 * The Camera interface provides operations on digital cameras that can be used
 * to acquire images.
 * 
 * @author Mike Reid
 */
public interface Camera {

    /**
     * Opens the camera and prepares it for use.
     * 
     * @parameter identifier identifies which camera should be opened for use.
     */
    public void open(String identifier);

    /**
     * Closes the camera and releases any resources it was using.
     */
    public void close();

    /**
     * Gets the width (number of columns) of the images produced by the camera 
     * (in pixels). 
     * 
     * @return the width (in pixels) of the camera's images. 
     */
    public int getWidth();

    /**
     * Gets the height (number of rows) of the images produced by the camera 
     * (in pixels).
     * 
     * @return the height (in pixels) of the camera's images.
     */
    public int getHeight();

    /**
     * Starts the acquisition of an image with the camera. 
     */
    public void startAcquisition();

    /**
     * Stops acquisition images with the camera.
     */
    public void stopAcquisition();

    /**
     * Indicates whether the camera acquisition has produced an image yet.
     * 
     * @return true if an image is ready to be read from the camera, false
     *         if the acquisition is not complete yet.
     */
    public boolean isImageReady();

    /**
     * Obtains the image pixels obtained from the last acquisition and 
     * places them in the array pointed to by pixels. Each element of the
     * pixels array contains a single pixel.
     * 
     * @param pixels the destination of the last image acquired by the camera.
     * 
     * @throws IllegalArgumentException if pixels is null.
     * @throws IllegalArgumentException if pixels does not have a number of 
     *         elements which is the same as getWidth()*getHeight() or greater.
     */
    public void getImage(int[] pixels);

    /**
     * Removes all images that may be buffered on the camera. The next call to
     * isImageReady() should return false, although this not strictly true if
     * a camera is acquiring images in the background.
     */
    public void purgeImages();
}
