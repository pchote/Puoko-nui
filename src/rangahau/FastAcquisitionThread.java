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
 * A thread that reads images from the camera and saves them to disk as FITS
 * files. Timestamps obtained from a USB-connected GPS clock are written
 * into the header of the FITS file.
 *
 * @author Mike Reid
 */
public class FastAcquisitionThread extends StoppableThread {

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
     * The GPS clock which timestamps can be obtained from.
     */
    Clock clock;
    
    /**
     * Date formatter that shows milliseconds.
     */
    SimpleDateFormat dateFormatter = null;
    int imageCount = 0;

    public FastAcquisitionThread(DisplayForm form, Model model) {
        setName("Rangahau fast acquisition thread " + this.getClass().getName()); // Set the name of the thread (useful when debugging).

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

        this.clock = model.getClock();

        // Prepare a buffer where image data will be stored.
        pixels = new int[model.getCamera().getHeight() * model.getCamera().getWidth()];
        image = new int[model.getCamera().getHeight()][model.getCamera().getWidth()];

        // Ensure an exposure time has been set.
        int exposureTime = clock.getExposureTime();

        if (exposureTime <= 0) {
            final int defaultExposureTime = 5;
            System.out.println("WARNING: The exposure time was not set, setting to default value of "
                    + defaultExposureTime + " seconds.");

            clock.setExposureTime(defaultExposureTime);
            exposureTime = defaultExposureTime;
        }

        // Ensure all elements have the correct exposure time.
        model.setExposureTime(exposureTime);

        // Starts the camera acquiring images. Note that this only needs to be
        // called once and then images will continue to be acquired by the camera
        // for each sync pulse sent by the external timer card.
        model.getCamera().startAcquisition();

        // Dump any existing images that may have been buffered on the
        // camera as we don't know when they were taken or what their
        // exposure time was.
        model.getCamera().purgeImages();

        // Start acquiring images.
        while (!stopThread) {

            System.out.println("Waiting until camera has downloaded the image ...");

            long lastGpsTimeRequest = 0;
            long gpsTimeRequestIntervalMillis = 200;

            while (!stopThread && !model.getCamera().isImageReady()) {
                // Update the GPS time shown in the display, but not more than a few
                // times a second.
                if (System.currentTimeMillis() > (lastGpsTimeRequest + gpsTimeRequestIntervalMillis)) {
                    final Date lastGpsTime = clock.getLastGpsPulseTime();
                    if (lastGpsTime != null) {
                        lastGpsTimeRequest = System.currentTimeMillis();

                        // Update the GPS time on the GUI.
                        if (SwingUtilities.isEventDispatchThread()) {
                            form.showGpsTime(lastGpsTime);
                        } else {
                            try {

                                SwingUtilities.invokeAndWait(new Runnable() {

                                    public void run() {
                                        form.showGpsTime(lastGpsTime);
                                    }
                                });
                            } catch (Exception ex) {
                                // ignore interruptions.
                                ex.printStackTrace();
                            }
                        }
                    }
                }

                // Sleep for a bit to give the system time to do other tasks.
                try {
                    Thread.sleep(1);
                } catch (InterruptedException ex) {
                    // ignore interruptions.
                }
            }

            // The image is ready, get the GPS sync time at the end of the image.
            // Determine the start time of the exposure (using the end time and
            // exposure start time).
            Date lastSyncTime = clock.getLastSyncPulseTime();
            Date syncStartTime = new Date(lastSyncTime.getTime() - exposureTime * 1000);

            // Get the pixels from the camera.
            final long systemStartTime = System.currentTimeMillis() - exposureTime * 1000;
            if (!stopThread) {
                System.out.println("Image available, getting image which *ended* at GPS sync pulse " + dateFormatter.format(lastSyncTime) + ") ...");
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

            // Display (and possibly save) the image.
            if (!stopThread) {
                final Date syncStart = syncStartTime;
                final Date lastSync = lastSyncTime;
                System.out.println("Displaying image ...");
                if (SwingUtilities.isEventDispatchThread()) {
                    form.showImage(image, systemStartTime, syncStart.getTime());
                    form.showSyncTime(lastSync);
                } else {
                    try {

                        SwingUtilities.invokeAndWait(new Runnable() {

                            public void run() {
                                form.showImage(image, systemStartTime, syncStart.getTime());
                                form.showSyncTime(lastSync);
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
