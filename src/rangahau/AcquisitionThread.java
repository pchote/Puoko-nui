/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */
package rangahau;

import java.util.TimeZone;
import javax.swing.SwingUtilities;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;

/**
 * The thread that manages coordination of devices when acquiring images. This
 * is a long-running activity so runs in a separate thread to the thread
 * running the user interface (that is, the Swing Event Dispatch Thread).
 * 
 * @author Mike Reid.
 */
public class AcquisitionThread extends StoppableThread {
            
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

    /**
     * Date formatter suitable for GPS times (which always have milliseconds
     * values of 0 since they are precisely controlled to seconds boundaries).
     */
    SimpleDateFormat gpsFormatter = null;

    int imageCount = 0;

    /**
     * A clock which tracks the difference between GPS time and system time.
     */
    Clock clock;

  public AcquisitionThread(DisplayForm form, Model model) {
    setName("Rangahau acquisition thread " + this.getClass().getName()); // Set the name of the thread (useful when debugging).

    this.form = form;
    this.model = model;

    utcTimeZone = TimeZone.getTimeZone("UTC");
    dateFormatter = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS z");
    dateFormatter.setTimeZone(utcTimeZone);
    gpsFormatter = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss z");
    gpsFormatter.setTimeZone(utcTimeZone);

    clock = new Clock();
  }

  /**
   * Performs the acquisition of images. This method will run indefinitely
   * until the shouldStop() method is called (usually by another thread)
   */
  public void run() {

    System.out.println("Starting image acquisitions with an exposure time of " + model.getExposureTime() + " ms.");

    if (!model.isImageSourceInitialised()) {
      model.initialiseImageSource();
    }

    boolean firstImage = true; // True if we have not yet read out an image.

//        long startTime = 0;        // The system clock timestamp when the frame to be readout was started.
//        long gpsStartTime = 0;     // The GPS timestamp when the frame to be readout was started.
//
//        long nextStartTime = 0;    // The system clock timestamp when the next frame was started.
//        long nextGpsStartTime = 0; // The GPS timestamp when the next frame was started.

    // The timestamp (according to the computer's system clock) when the
    // next output (sync) pulse is expected to arrive. This is predicted
    // from the frame start time and the exposure time setting.
    long predictedEndTime = 0;

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

    final int exposureTime = model.getExposureTime();

    Exposure finishingExposure = null; // The exposure that the next sync pulse will finish.
    Exposure startingExposure = null;  // The exposure that the next sync pulse will start.

    // The time we last synchronized with the network time server.
    long lastSynchronizationTime = System.currentTimeMillis();

    System.out.println("Issuing command to synchronise system clock with time server ...");
    model.correctSystemClock();

    // Remove any bytes from the serial port's buffer.
    while (model.getSerialPort().bytesAvailable() > 0) {
      model.getSerialPort().readByte();
    }


    while (!stopThread) {
      System.out.println("StopThread = " + stopThread + ", thread id = " + Thread.currentThread().getId());

      // If this is the first image that is being acquired then we wait
      // for the synchronisation pulse that starts the frame. The reason
      // we must wait for this pulse is we don't know what the exposure
      // time of any previous frames could have been. Note, we will wait
      // up to 31 seconds for this first pulse as that is the maximum
      // possible delay.
      if (!stopThread && firstImage) {
        // Do special processing for the first image we are to acquire.
        System.out.println("=> Waiting for initial sync pulse ...");

        // Block here until an output signal is detected.
        final long timelimit = 31 * 1000; // The maximum time to wait (in milliseconds).

        long firstPulseTimeLimit = System.currentTimeMillis() + timelimit;
        boolean gotSyncPulse = false;
        while (!gotSyncPulse && (System.currentTimeMillis() < firstPulseTimeLimit)) {
          // Remove any bytes from the serial port's buffer.
          while (model.getSerialPort().bytesAvailable() > 0) {
            model.getSerialPort().readByte();
          }

          // Wait 1 second to see whether the initial sync pulse
          // has been received.
          gotSyncPulse = model.getTimerCard().waitForOutputSignal(1000);
        }

        long startTime = System.currentTimeMillis();

        long gpsStartTime = 0;
        if (!stopThread) {
          gpsStartTime = model.getGpsDecoder().readGpsTimestamp(model.getSerialPort());
        }

        finishingExposure = new Exposure();
        finishingExposure.setSystemStartTimeMillis(startTime);
        finishingExposure.setGpsStartTimeMillis(gpsStartTime);
        finishingExposure.setDurationMillis(model.getExposureTime() * 1000);

        // Determine the timestamp when the next sync pulse is expected
        // to be received. Note: The predicted end time should be on a
        // GPS second mark, so we dont expect any fractional seconds
        // (apart from a small amount due to system clock drift - which
        // should be kept under control with NTP clock synchronisation),
        // except when the system clock is slightly after the GPS time
        // (negative time difference) which has a large milliseconds
        // residual.
        predictedEndTime = startTime + exposureTime * 1000;
        if ((predictedEndTime % 1000) < 800) {
          predictedEndTime -= (predictedEndTime % 1000); // Remove any milliseconds component.
        }

        if (gotSyncPulse) {
          if (Math.abs(startTime - gpsStartTime) < 50) {
            clock.addMeasurement(new ClockMeasurement(startTime, gpsStartTime));
          }
          Calendar calendar = Calendar.getInstance(utcTimeZone);
          calendar.setTimeInMillis(startTime);
          System.out.println("Got initial synchronisation pulse at " + startTime + " (" + dateFormatter.format(calendar.getTime()) + ")");
          calendar = Calendar.getInstance(utcTimeZone);
          calendar.setTimeInMillis(gpsStartTime);
          System.out.println("GPS time of initial synchronisation pulse is " + gpsStartTime + " (" + dateFormatter.format(calendar.getTime()) + ")");
          System.out.println("System time - GPS time = " + (startTime - gpsStartTime) + " ms");
        } else {
          System.err.println("Did not get initial syncronisation pulse!");
          stopThread = true;
        }

        // Dump any existing images that may have been buffered on the
        // camera as we don't know when they were taken or what their
        // exposure time was.
        model.getCamera().purgeImages();
      }

      // A frame is currently being acquired. Wait until an output (sync)
      // pulse is received. Since we know the predicted time that the
      // next sync pulse is to be received we can simply sleep untilpredictedEndTime
      // near this time.
      System.out.println("*****************************************************************");
      System.out.println("Collecting GPS timstamps while waiting for output/camera sync pulse ...");
      System.out.println("Frame should end at system time " + predictedEndTime + " (" + dateFormatter.format(new Date(predictedEndTime)) + ")");
      final long sleepDurationMillis = 10;  // How long to sleep for (in milliseconds).
      final long wakeUpTime = predictedEndTime - 10 * sleepDurationMillis; // The timestamp to stop sleeping and start looking for sync pulses.

      int numGpsTimesCollected = 0;

      long lastGpsTime = 0; // Time we last got a GPS timestamp.

      while (!stopThread && (System.currentTimeMillis() < wakeUpTime)) {
        // Only look for GPS timestamps if the time available is not too
        // close to the 'wake-up time' where we start looking for output
        // pulses. Also, only look for GPS times when we are near the
        // boundary of a second.
        long currentMillis = System.currentTimeMillis();
        final long millisSinceLastGps = Math.abs(currentMillis - lastGpsTime);

        final long timeLimit = 20; // The number of milliseconds to look for the pulse.

        boolean nearSecondBoundary = false;
        if (((currentMillis % 1000) > (1000 - timeLimit)) || ((currentMillis % 1000) < timeLimit)) {
          nearSecondBoundary = true;
        }

        if (((currentMillis + timeLimit) < wakeUpTime) && (millisSinceLastGps > (1000 - timeLimit)) && nearSecondBoundary) {
          boolean gotSyncPulse = model.getTimerCard().waitForInputSignal(timeLimit);
          // We detected an input pulse, a GPS timestamp should follow.
          if (gotSyncPulse) {
            long systemTime = System.currentTimeMillis();

            // Read GPS information from the serial port buffer.
            long gpsTime = model.getGpsDecoder().readGpsTimestamp(model.getSerialPort());

            // Update the clock difference between system and GPS time
            // but only do this if they were close together. If they
            // differ by too much then it is very likely that once of
            // the clock readings was bad.
            if (Math.abs(systemTime - gpsTime) < 50) {
              clock.addMeasurement(new ClockMeasurement(systemTime, gpsTime));
              ++numGpsTimesCollected;
              lastGpsTime = currentMillis;
            //System.out.println("Received input sync pulse, system time - gps time = " + (systemTime - gpsTime));
            }
          }
        }

        try {
          Thread.sleep(sleepDurationMillis);
        } catch (InterruptedException ex) { /* Ignore interruptions */ }
      }

      System.out.println("Obtained " + numGpsTimesCollected + " timestamps while waiting.");

      // Remove any extra bytes from the serial port's buffer.
      while (model.getSerialPort().bytesAvailable() > 0) {
        model.getSerialPort().readByte();
      }

      // Look for the output (sync) pulse that signals the end of one
      // frame and the start of another. If the sync pulse doesn't arrive
      // within a few seconds then we have missed it.
      final long timelimit = 200; // Wait for this many milliseconds for a sync pulse.
      long seekFrameEndTime = System.currentTimeMillis();
      System.out.println("Started looking for output pulse at at " + seekFrameEndTime + " (" + dateFormatter.format(new Date(seekFrameEndTime)) + ") ...");

      if (!stopThread) {
        boolean gotSyncPulse = model.getTimerCard().waitForOutputSignal(timelimit);

        // Get the system clock time and the GPS clock time that the
        // sync pulse was detected.
        long systemEndTime = System.currentTimeMillis();

        long gpsEndTime = systemEndTime - clock.getSystemToGpsCorrection(); // estimate initially.

        if (!stopThread) {
          if (gotSyncPulse) {
            gpsEndTime = model.getGpsDecoder().readGpsTimestamp(model.getSerialPort());
          } else {
            // We didn't find the sync pulse, use the estimated GPS time
            // and discard GPS time-string information from the serial port.
            while (model.getSerialPort().bytesAvailable() > 0) {
              model.getSerialPort().readByte();
            }
          }
        }

        // If the GPS time we got was more than some time limit different
        // from the system clock time then there is a problem. The cause
        // could be a bad GPS reading (for example by noise on the serial
        // line between the GPS unit and the host), or because the host
        // system's clock is far from GPS.
        boolean validGpsTime = true;
        final long systemGpsToleranceMillis = 500;

        if (Math.abs(systemEndTime - gpsEndTime) > systemGpsToleranceMillis) {
          long badGpsTime = gpsEndTime;
          validGpsTime = false;

          System.err.println("WARNING: GPS time differs greatly from the system time, either time could be bad.");
          gpsEndTime = systemEndTime - clock.getSystemToGpsCorrection(); // use estimated GPS time.
          System.err.println("Using estimated GPS time of " + gpsEndTime + " (" + dateFormatter.format(new Date(gpsEndTime)) + ")");
          System.err.println("Discarded bad GPS time of " + badGpsTime + " (" + dateFormatter.format(new Date(badGpsTime)) + ")");

        }

        // If the system time has drifted from GPS we should synchronize
        // with a network time server.
        final long driftToleranceMillis = 20;
        final long synchronizationTimeLimitMillis = 120000; // Don't attempt to synchronize more than once every two minutes
        if ((Math.abs(systemEndTime - gpsEndTime) > driftToleranceMillis) && ((lastSynchronizationTime + synchronizationTimeLimitMillis) < System.currentTimeMillis())) {
          System.out.println("Issuing command to synchronise system clock with time server ...");
          model.correctSystemClock();
          lastSynchronizationTime = System.currentTimeMillis();
        }

        // Set information about the exposure that just ended and the
        // new exposure just beginning.
        finishingExposure.setSystemEndTimeMillis(systemEndTime);
        finishingExposure.setGpsEndTimeMillis(gpsEndTime);

        startingExposure = new Exposure();
        startingExposure.setSystemStartTimeMillis(systemEndTime);
        startingExposure.setGpsStartTimeMillis(gpsEndTime);
        ++imageCount;

        if (!gotSyncPulse || !validGpsTime) {
          System.err.println("WARNING: Failed to detect camera sync pulse within the time limit!");
        } else {
          System.out.println("Found output/camera sync pulse");

          System.out.println("System time of sync pulse " + imageCount + " = " + dateFormatter.format(new Date(startingExposure.getSystemStartTimeMillis())) + " (" + startingExposure.getSystemStartTimeMillis() + ")");
          System.out.println("GPS time of sync pulse " + imageCount + "    = " + dateFormatter.format(new Date(startingExposure.getGpsStartTimeMillis())) + " (" + startingExposure.getGpsStartTimeMillis() + ")");
          System.out.println("System time - GPS time = " + (startingExposure.getSystemStartTimeMillis() - startingExposure.getGpsStartTimeMillis()) + " ms");
          System.out.println("Ending exposure started at GPS time   : " + gpsFormatter.format(new Date(finishingExposure.getGpsStartTimeMillis())));
          System.out.println("Starting exposure started at GPS time : " + gpsFormatter.format(new Date(startingExposure.getGpsStartTimeMillis())));
        }

        // Determine the timestamp when the next sync pulse is expected
        // to be received. Note: The predicted end time should be on a
        // GPS second mark, so we dont expect any fractional seconds
        // (apart from a small amount due to system clock drift - which
        // should be kept under control with NTP clock synchronisation),
        // except when the system clock is slightly after the GPS time
        // (negative time difference) which has a large milliseconds
        // residual.
        predictedEndTime = systemEndTime + exposureTime * 1000;
        if ((predictedEndTime % 1000) < 800) {
          predictedEndTime -= (predictedEndTime % 1000); // Remove any milliseconds component.
        }


        // Wait until the next image is available. This image was read-out
        // when the timercard output pulse was received, so we expect this
        // readout to have completed within a few seconds of the output
        // pulse being received.
        System.out.println("Waiting until camera has downloaded the image ...");
//               long cameraWaitFinishTime = System.currentTimeMillis() + 2000;
//               while (System.currentTimeMillis() < cameraWaitFinishTime) {
//                 try {
//                   Thread.sleep(100);
//                 } catch(InterruptedException ex) {
//                   // Ignore interruptions.
//                 }
//               }

        while (!stopThread && !model.getCamera().isImageReady()) {
          try {
            Thread.sleep(200);
          } catch (InterruptedException ex) {
            // ignore interruptions.
          }
        }
        // Get the pixels from the camera.
        if (!stopThread) {
          System.out.println("Getting image from camera ...");
          model.getCamera().getImage(pixels);
        }

        // Convert the one-dimensional list of pixels into a
        // two-dimensional image.
        int[][] exposurePixels = new int[model.getCamera().getHeight()][model.getCamera().getWidth()];
        if (!stopThread) {
          int sourceIndex = 0;
          for (int row = 0; row < model.getCamera().getHeight(); ++row) {
            for (int column = 0; column < model.getCamera().getWidth(); ++column) {
              image[row][column] = pixels[sourceIndex];
              exposurePixels[row][column] = pixels[sourceIndex];
              ++sourceIndex;
            }
          }
        }

        finishingExposure.setImage(exposurePixels);
        Calendar calendar = Calendar.getInstance(utcTimeZone);
        calendar.setTimeInMillis(finishingExposure.getSystemStartTimeMillis());
        System.out.println("Assigning pixels to exposure started at " + gpsFormatter.format(new Date(finishingExposure.getGpsStartTimeMillis())));


        // Display the image (and save it at the same time).
        if (!stopThread) {
          System.out.println("Displaying image ...");
          if (SwingUtilities.isEventDispatchThread()) {
            form.showImage(image, finishingExposure.getSystemStartTimeMillis(), finishingExposure.getGpsStartTimeMillis());
          } else {
            final long startTimestamp = finishingExposure.getSystemStartTimeMillis();
            final long gpsTimestamp = finishingExposure.getGpsStartTimeMillis();
            try {
              SwingUtilities.invokeAndWait(new Runnable() {

                public void run() {
                  form.showImage(image, startTimestamp, gpsTimestamp);
                }
              });
            } catch (Exception ex) {
              // ignore interruptions.
              ex.printStackTrace();
            }
          }
        }
      }

      // The start time for the next frame is known.
      firstImage = false;
      finishingExposure = startingExposure;
      startingExposure = null;

      // Discard any GPS information received while we were getting the
      // image.
      while (model.getSerialPort().bytesAvailable() > 0) {
        model.getSerialPort().readByte();
      }
    }

    finished = true; // The acquisition thread has finished running.
    System.out.println("AcquisitionThread.run(), finished = " + finished);
  }
}
