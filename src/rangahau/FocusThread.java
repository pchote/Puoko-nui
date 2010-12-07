/*
 * Copyright 2007-2010 The Authors (see AUTHORS)
 * This file is part of Rangahau, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */
package rangahau;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.TimeZone;
import javax.swing.SwingUtilities;

/**
 * A thread that reads images from the camera as quickly as it can (subject to
 * the frame exposure time). Used when focussing camera images,
 *
 * @author Mike Reid
 */
public class FocusThread extends StoppableThread {

    /**
     * The form that will display the images that have been acquired.
     */
    DisplayForm form = null;

    /**
     * The model manages the internal state of the application. It manages
     * resources and the application operations.
     */
    private Model model = null;

    /**
     * The pixel data containing intensities to be displayed, both as an
     * array and a two-dimensional image array.
     */
    int[] pixels = null;
    int[][] image = null;

    /**
     * Represents the UTC (Universal Coordinated Time) timezone.
     */
    private TimeZone utcTimeZone = null;

    /**
     * Date formatter that shows milliseconds.
     */
    SimpleDateFormat dateFormatter = null;
    int imageCount = 0;

    public FocusThread(DisplayForm form, Model model) {
        setName("Rangahau focus thread " + this.getClass().getName()); // Set the name of the thread (useful when debugging).

        this.form = form;
        this.model = model;

        utcTimeZone = TimeZone.getTimeZone("UTC");
        dateFormatter = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS z");
        dateFormatter.setTimeZone(utcTimeZone);
    }

    /**
     * Performs the acquisition of images. This method will run indefinitely
     * until the shouldStop() method is called (usually by another thread)
     */
    public void run() {

        System.out.println("Starting focus acquisition with an exposure time of " + model.getExposureTime() + " ms.");

        if (!model.isImageSourceInitialised()) {
            model.initialiseImageSource();
        }

        // Prepare a buffer where image data will be stored.
        pixels = new int[model.getCamera().getHeight() * model.getCamera().getWidth()];
        image = new int[model.getCamera().getHeight()][model.getCamera().getWidth()];

        // Starts the camera acquiring images. Note that this only needs to be
        // called once and then images will continue to be acquired by the camera
        // for each sync pulse sent by the external timer card.
        model.getCamera().startAcquisition();

        // Dump any existing images that may have been buffered on the
        // camera as we don't know when they were taken or what their
        // exposure time was.
        model.getCamera().purgeImages();

        //final int exposureTime = model.getExposureTime();

        while (!stopThread) {

            System.out.println("Waiting until camera has downloaded the image ...");

            while (!stopThread && !model.getCamera().isImageReady()) {
                try {
                    Thread.sleep(1);
                } catch (InterruptedException ex) {
                    // ignore interruptions.
                }
            }
            // Get the pixels from the camera.
            final long now = System.currentTimeMillis();
            if (!stopThread) {
                System.out.println("Image available, getting image from camera  at " + dateFormatter.format(new Date()) + ") ...");
                model.getCamera().getImage(pixels);
            }

            // Convert the one-dimensional list of pixels into a
            // two-dimensional image.
            int height = model.getCamera().getHeight();
            int width = model.getCamera().getWidth();
            if (!stopThread) {
                int sourceIndex = 0;
                for (int row = 0; row < height; ++row) {
                    for (int column = 0; column < width; ++column) {
                        image[row][column] = pixels[sourceIndex];
                        ++sourceIndex;
                    }
                }
            }

            // Display the image.
            if (!stopThread) {
                System.out.println("Displaying image ...");
                if (SwingUtilities.isEventDispatchThread()) {
                    form.showImage(image, now, now);
                } else {
                    try {
                        SwingUtilities.invokeAndWait(new Runnable() {

                            public void run() {
                                form.showImage(image, now, now);
                            }
                        });
                    } catch (Exception ex) {
                        // ignore interruptions.
                        ex.printStackTrace();
                    }
                }
            }
        }

        finished = true;
    }
}
