/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

package rangahau;

import java.awt.geom.Point2D;

import java.io.File;
import java.io.DataOutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

import java.text.SimpleDateFormat;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.List;
import java.util.Properties;
import java.util.TimeZone;
import java.util.zip.GZIPOutputStream;

import nom.tam.fits.BasicHDU;
import nom.tam.fits.Fits;

/**
 * The Model class coordinates the operation of the system. It is the central
 * class of the application. It is used by the DisplayForm class to perform
 * application operations, but can be used (and tested) independently of the
 * DisplayForm class.
 * 
 * @author Mike Reid
 */
public class Model {
    
    /**
     * Represents the UTC (Universal Coordinated Time) timezone.
     */
    private TimeZone utcTimeZone = null;
    
    /**
     * True if the source of images has been initialised.
     */
    private boolean sourceInitialised = false;
                
    /**
     * Indicates whether hardware devices should be simulated or not.
     */
    private boolean simulate = true;

    /**
     * The camera used to acquire images.
     */
    private Camera camera = null;
    
    /**
     * The external timing card used to control frame acquisition start and
     * exposure time.
     */
    private TimerCard timerCard = null;
    
    /**
     * The serial port that provides access to GPS timestamps. 
     */
    private SerialPortDevice serialPort = null;
    
    /**
     * Decodes GPS messages received on the serial port and extracts timestamp
     * information. 
     */
    private GpsDecoder gpsDecoder = null;
    
    /**
     * The exposure time currently being used (in seconds).
     */
    private int exposureTime = 5;

    /**
     * The command to use to synchronize the host's system clock with a
     * network time server.
     */
    //private String synchronizeCommand = "ntpdate cantva.canterbury.ac.nz";
    private String synchronizeCommand = "ntpdate time.nist.gov";
    /**
     * A description of the run. For example, "xcov26".
     */
    private String run = "xcov27";
    
    /**
     * A description of the particpating observers who acquired the data.
     */
    private String observers = "DJS";
    
    /**
     * The target object of the observation.
     */
    private String target = "ec20058";
    
    /**
     * The name of the observatory where the observations were performed.
     */
    private String observatory = "MJUO";
    
    /**
     * The telescope used to acquire the images.
     */
    private String telescope = "MJUO 1-meter";
            
    /**
     * The CCD instrument name.
     */
    private String instrument = "puoko-nui";
    
    /**
     * The destination path where image files will be saved.
     */
    private String destinationPath = null;
    
    /**
     * The location of the target object. This may be null if the target object
     * has not yet been designated.
     */
    private Point2D targetLocation = null;
    
    /**
     * The location of the comparison object. This may be null if the comparison
     * object has not yet been designated.
     */
    private Point2D comparisonLocation = null;
 
    /**
     * File name of the properties file  (excluding the file path) holding 
     * the rangahau settings.
     */
    final static String PROPERTY_FILE_NAME = "rangahau.properties";
    
    /**
     * Name of the property naming the target object of the observation.
     */
    final static String TARGET_PROPERTY_NAME  = "target";
    
    /**
     * Name of the property giving a description of the run. For example, "xcov26".
     */
    final static String RUN_PROPERTY_NAME = "run";
    
    /**
     * Name of the property describing the particpanting observers who acquired 
     * the data. 
     */
    final static String OBSERVERS_PROPERTY_NAME = "observers";
    
    /**
     * Name of the property naming observatory where the observations were performed. 
     */
    final static String OBSERVATORY_PROPERTY_NAME = "observatory";

    /**
     * Name of the property naming the telescope used to acquire the images.
     */
    final static String TELESCOPE_PROPERTY_NAME = "telescope";
    
    /**
     * Name of the property with the CCD instrument name that produced the images.
     */
    final static String INSTRUMENT_PROPERTY_NAME = "instrument";

    /**
     * Name of the property with the destination path where images should be saved.
     */
    final static String DESTINATION_PATH_PROPERTY_NAME = "destinationpath";
    
    /**
     * Measurements made of the target object.
     */
    List<Measurement> targetMeasurements;

    /**
     * Measurements made of the comparison object.
     */
    List<Measurement> comparisonMeasurements;

    /**
     * The GpsClock device (if we have one);
     */
    private GpsClock gpsClock;
    
    /**
     * If the camera is a simulator it supoprts extra operations.
     */
    CameraSimulator cameraSimulator;

    public Model() {
      utcTimeZone = TimeZone.getTimeZone("UTC"); 
      gpsDecoder = new GpsDecoder();

      targetMeasurements = new ArrayList<Measurement>();
      comparisonMeasurements = new ArrayList<Measurement>();
    }
    
    /**
     * Returns the timer card being used to control frame start times and
     * exposure control. The timer card is also opened for use.
     * 
     * @return the timer card being used.
     */
    public TimerCard getTimerCard() {
        if (timerCard != null) {
            return timerCard;
        }

        // There is no existing timer card. We must create one.
        if (isSimulatingHardware()) {
            timerCard = new TimerCardSimulator();
        } else {
            timerCard = new ExternalTimerCard();
        }

        // Open the timer card for use.
        timerCard.openPort("/dev/parport0");
        timerCard.powerOn();
        timerCard.setTimingEdgeType(false); // Use the falling edge of the GPS input pulse.
        
        System.out.println("Detecting timer card = " + timerCard.detectTimerCard());
        timerCard.setExposureTime(getExposureTime());
 
        return timerCard;
    }

    /**
     * Returns the GPS clock being used to control frame start times and
     * exposure control. This clock is also opened for use.
     *
     * @return the GPS clock being used.
     */
    public GpsClock getGpsClock() {
        if (gpsClock != null) {
            return gpsClock;
        }

        // There is no existing gpsClock. We must create one.
        if (isSimulatingHardware()) {
            // TODO - finish this
          gpsClock = new GpsClock();
        } else {
            gpsClock = new GpsClock();
        }

        // Open the GPS clock for use.
        List<String> names = GpsClock.getAllDeviceNames();
        String name = names.get(0);

        gpsClock.open(name);

        System.out.println("GPS clock is open = " + gpsClock.isOpen());
        gpsClock.setExposureTime(getExposureTime());

        return gpsClock;
    }

    /**
     * Returns the current exposure time (in seconds).
     * 
     * @return the current exposure time (in seconds).
     */
    public int getExposureTime() {
        return exposureTime;
    }

    /**
     * Set the exposure time that images are currently being acquired with.
     * 
     * @param exposureTime the current exposure time (in seconds).
     * 
     * @throws IllegalArgumentException if exposureTime is 0 or less.
     * @throws IllegalArgumentException if exposureTime is 30 or less.
     */
    public void setExposureTime(int exposureTime) {
        if (exposureTime < 0) {
            throw new IllegalArgumentException("Cannot set the exposure time to a negative value, the value given was " + exposureTime);
        }
//        if (exposureTime > 30) {
//            throw new IllegalArgumentException("Cannot set the exposure time to a value greater than 30 seconds, the value given was " + exposureTime);
//        }
        
        // Change the exposure time on the external timer card.        
        System.out.println("Setting the exposure time to " + exposureTime + " s");

        // Set the timer card to use the specified exposure time.
//        if (timerCard == null) {
//            timerCard = getTimerCard();
//        }
//        timerCard.setExposureTime(exposureTime);

        if (gpsClock == null) {
          gpsClock = getGpsClock();
        }
        gpsClock.setExposureTime(exposureTime);

        if (cameraSimulator != null) {
          System.out.println("Model Setting camera simulator exposure time to " + exposureTime);
          cameraSimulator.setExposureTime(exposureTime);
        }

        this.exposureTime = exposureTime;
    }

    public boolean isSimulatingHardware() {
        return simulate;
    }

    public void setSimulatingHardware(boolean simulate) {
        this.simulate = simulate;
    }

    public Camera getCamera() {
        return camera;
    }

    public void setCamera(Camera camera) {
        this.camera = camera;
    }

    public void setTimerCard(TimerCard timerCard) {
        this.timerCard = timerCard;
    }

    public void setGpsClockCard(GpsClock gpsClock) {
        this.gpsClock = gpsClock;
    }

    /**
     * Releases any resources managed by the model. This is usually performed
     * at application shutdown.
     */
    public void releaseResources() {
        System.out.println("Releasing resources ...");

        if (camera != null) {
            System.out.println("Releasing camera ...");
            camera.close();
            camera = null;
        }
        
        if (timerCard != null) {
            System.out.println("Releasing timercard ...");
            timerCard.closePort();
            timerCard = null;
        }
        
        if (serialPort != null) {
            System.out.println("Releasing serial port ...");
            serialPort.close();
            serialPort = null;
        }

        if (gpsClock != null) {
            System.out.println("Releasing GPS clock ...");
            gpsClock.close();
            gpsClock = null;
        }
    }
    
    /**
     * 
     * 
     */
    public boolean isImageSourceInitialised() {
        return sourceInitialised;
    }

    public void initialiseImageSource() {
        // Set up the camera that will acquire images.
        if (simulate) {
            System.out.println("Using simulated hardware devices.");
            //timerCard = getTimerCard();
            //serialPort = getSerialPort();
            gpsClock = getGpsClock();
            cameraSimulator = new CameraSimulator();

            camera = cameraSimulator;

        } else {
            System.out.println("Using physical hardware devices.");
            //timerCard = getTimerCard();
            //serialPort = getSerialPort();
            gpsClock = getGpsClock();
            camera = new RoperCamera();
        }

        // Open access to the camera.
        try {
            camera.open("foo");
        } catch (Throwable ex) {
            ex.printStackTrace();
        }
        
        // Ensure we can read output pulses from the camera.
//        boolean foundTimerCard = false;
//        try {
//            foundTimerCard = timerCard.detectTimerCard();
//        } catch(Throwable ex) {
//            ex.printStackTrace();
//        }
//        System.out.println("Timercard detection returned " + foundTimerCard);
        
        // Test we can read a GPS timestamp.
//        long gpsTime = getGpsDecoder().readGpsTimestamp(getSerialPort());
//        if (gpsTime == 0L) {
//            System.out.println("Could not read a GPS timestamp.");
//        } else {
//            SimpleDateFormat formatter = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS z");
//            formatter.setTimeZone(utcTimeZone);
//
//            Calendar calendar = Calendar.getInstance();
//            calendar.setTimeZone(utcTimeZone);
//            calendar.setTimeInMillis(gpsTime);
//
//            String formattedTime = formatter.format(calendar.getTime()) ;
//
//            System.out.println("Successfully read a GPS timestamp, the time read was " + formattedTime);
//        }
         
        sourceInitialised = true;
    }
    
    /**
     * Saves pixels to disk as an image in the Fits format. 
     * 
     * @param pixels the pixel elements of the image.
     * @param the time when the image acquisition was started (measured in 
     *        milliseconds UTC from the Epoch [1 Jan 1970]).
     * @param gpsStartTime the time when the image acquisition was started (measured 
     *        in milliseconds UTC from the Epoch [1 Jan 1970]). This is measured
     *        by a GPS unit.
     * @param imageType the type of image (examples are: FLAT, DARK, TARGET).
     * @param comment text to place in the image's FITS header (up to 72 characters in length).
     */
    public void saveImage(String filename, int[][] pixels, long startTime, long gpsStartTime, String imageType, String comment) {
        if (filename == null) {
            throw new IllegalArgumentException("Cannot save the image as no filename was provided.");
        }
        if (filename.trim().length() < 1) {
            throw new IllegalArgumentException("Cannot save the image as the filename provided was an empty string.");
        }
        if (pixels == null) {
            throw new IllegalArgumentException("Cannot save the image as the pixel array was null.");
        }
        if (imageType == null) {
            throw new IllegalArgumentException("Cannot save the image as the image type given was null.");
        }
        if (imageType.trim().length() < 1) {
            throw new IllegalArgumentException("Cannot save the image as the image type given was a blank string.");
        }
        
        Calendar now = Calendar.getInstance();
        SimpleDateFormat dateFormatter = new SimpleDateFormat("yyyy-MM-dd");

        // Add information to the FITS header.
        try {
            FileOutputStream fos = new FileOutputStream(filename);
            GZIPOutputStream gos = new GZIPOutputStream(fos);
            DataOutputStream dos = new DataOutputStream(gos);

            Fits image = new Fits();
            BasicHDU header = Fits.makeHDU(pixels);
            //header.setBScale(1.0);
            //header.setBZero(0);

            header.addValue("SIMPLE", "T", "File does conform to FITS standard");
            header.addValue("BITPIX", 32, "number of bits per data pixel");
            header.addValue("NAXIS", 2, "number of data axes");
            header.addValue("NAXIS1", pixels.length, "length of data axis 1");
            header.addValue("NAXIS2", pixels[0].length, "length of data axis 2");
            header.addValue("BZERO", 0, "offset data range to that of unsigned short");
            header.addValue("BSCALE", 1, "default scaling factor");


            // The OBJECT property depends on the type of image being acquired,
            // but may be the target object being observed.
            header.addValue("RUN", getRun(), "name of this run");
            
            String object = null;
            if ( (imageType != null) && imageType.equalsIgnoreCase("FOCUS")) {
                object = "FOCUS";
            }
            if ( (imageType != null) && imageType.equalsIgnoreCase("DARK")) {
                object = "DARK";
            }
            if ( (imageType != null) && imageType.equalsIgnoreCase("FLAT")) {
                object = "FLAT";
            }
            if ( (imageType != null) && imageType.equalsIgnoreCase("TARGET")) {
                object = getTarget();
            }            
            header.addValue("OBJECT", object, "Object name");
            
            // Obtained from properties.
            header.addValue("EXPTIME", Integer.toString(getExposureTime()), "Actual integration time (sec)");
            header.addValue("OBSERVER", getObservers(), "Observers");
            header.addValue("OBSERVAT", getObservatory(), "Observatory");
            header.addValue("TELESCOP", getTelescope(), "Telescope name");
            header.addValue("PROGRAM", "rangahau", "Data acquistion program");
            header.addValue("INSTRUME", getInstrument(), "Instrument");
            header.addValue("PSCALE", "0.0", "Image scale, pixels per arcsecond");
            header.addValue("IMAGEDIR", "Normal", "Image normal if prime, reversed if Cass mount");
            header.addValue("CHIPTEMP", "0.0", "CCD operating temperature");
            header.addValue("TEL-RA", "0.0", "Telescope right ascension reading");
            header.addValue("TEL-DEC", "0.0", "Telescope declination reading");

            // Get the observation start time as a Calendar object.
            //final long millisPerSecond = 1000L;
            //long startSeconds = startTime / millisPerSecond; // Start time (rounded to seconds since 1 Jan 1970).
            SimpleDateFormat formatter = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS z");
            formatter.setTimeZone(utcTimeZone);

            SimpleDateFormat gpsFormatter = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");
            gpsFormatter.setTimeZone(utcTimeZone);

            Calendar calendar = Calendar.getInstance();
            calendar.setTimeInMillis(startTime);
            
            // Get the date when the observation was started.
            header.addValue("DATE-OBS", dateFormatter.format(calendar.getTime()), "Date (yyyy-mm-dd) of this exposure");

            // Convert the start time into seconds and into a UTC date string.
            header.addValue("UTC", formatter.format(calendar.getTime()), "System-clock exposure start time.");
            header.addValue("UNIXTIME", Long.toString(startTime), "Start of this exposure (milliseconds since 1 Jan 1970)");
            String haveGPS ="0";
            long difference = 0; // difference in milliseconds between NTP time and GPS time.
            if ( (serialPort != null) && serialPort.isOpen() && (gpsStartTime != 0) ) {
                haveGPS = "1";
                difference = startTime - gpsStartTime;
            }
            header.addValue("GPSCLOCK", haveGPS, "GPS clock status");
            
            // If we have GPS readings then write out the exposure start time as
            // measured by GPS.
            //if (haveGPS.equals("1")) {
                Calendar gpsCalendar = Calendar.getInstance();
                gpsCalendar.setTimeZone(utcTimeZone);
                gpsCalendar.setTimeInMillis(gpsStartTime);
                header.addValue("GPSTIME", gpsFormatter.format(gpsCalendar.getTime()), "GPS time at exposure start.");
            //}
            
            header.addValue("NTP:GPS", Long.toString(difference), "Clock tick difference in ms, NTP vs GPS");
            
            int xTarget = 0;
            int yTarget = 0;
            if (this.getTargetLocation() != null) {
                xTarget = (int) Math.round(getTargetLocation().getX());
                yTarget = (int) Math.round(getTargetLocation().getY());
            }
            header.addValue("TARGET", xTarget + ", " + yTarget, "X,Y Coordinates of target star");

            int xComparison = 0;
            int yComparison = 0;
            if (this.getComparisonLocation() != null) {
                xComparison = (int) Math.round(getComparisonLocation().getX());
                yComparison = (int) Math.round(getComparisonLocation().getY());
            }
            
            header.addValue("COMPSTAR", xComparison + ", " + yComparison, "X,Y Coordinates of primary comparison star");

           if ( (comment != null) && (comment.trim().length() > 0) ) {
             int endIndex = 72;
             if (comment.trim().length() < endIndex) {
               endIndex = comment.trim().length();
             }
              header.addValue("COMMENT", comment.trim().substring(0, endIndex), null);
            }

            image.addHDU(header);
            image.write(dos);

            dos.close();
            gos.close();
            fos.close();
            System.out.println("Finished writing image " + filename);
        } catch (Exception ex) {
            ex.printStackTrace();
            System.exit(3);
        }
    }

    /**
     * Gets the directory path that is the destination where acquired images 
     * should be saved.
     * 
     * @return the acquisition destination directory path.
     */
    public String getDestinationPath() {
        return destinationPath;
    }

    /**
     * Sets the directory path that is the destination where acquired images 
     * should be saved.
     * 
     * @param destinationPath the acquisition destination directory path.
     */
    public void setDestinationPath(String destinationPath) {
        this.destinationPath = destinationPath;
    }

    public SerialPortDevice getSerialPort() {
        if (serialPort != null) {
            return serialPort;
        }

        // There is no existing timer card. We must create one.
        if (isSimulatingHardware()) {
            serialPort = new SerialPortDevice(); // TODO - Need a serial port simulator.
        } else {
            serialPort = new SerialPortDevice();
        }

        // Open the serial port for use.
        serialPort.open("/dev/ttyS0");
 
        return serialPort;
    }

    public void setSerialPort(SerialPortDevice serialPort) {
        this.serialPort = serialPort;
    }

    public String getRun() {
        return run;
    }

    public void setRun(String run) {
        this.run = run;
    }

    public String getObservers() {
        return observers;
    }

    public void setObservers(String observers) {
        this.observers = observers;
    }

    public String getTarget() {
        return target;
    }

    public void setTarget(String target) {
        this.target = target;
    }

    public String getObservatory() {
        return observatory;
    }

    public void setObservatory(String observatory) {
        this.observatory = observatory;
    }

    public String getTelescope() {
        return telescope;
    }

    public void setTelescope(String telescope) {
        this.telescope = telescope;
    }

    public String getInstrument() {
        return instrument;
    }

    public void setInstrument(String instrument) {
        this.instrument = instrument;
    }

    public

    /**
     * Decodes GPS messages received on the serial port and extracts timestamp
     * information.
     */
    GpsDecoder getGpsDecoder() {
        return gpsDecoder;
    }

    public void setGpsDecoder(GpsDecoder gpsDecoder) {
        this.gpsDecoder = gpsDecoder;
    }
    
    /**
     * Loads the rangahau.properties file from the user's home directory. If this
     * file is found any recognised properties within it are stored within the 
     * application's model (which will also be displayed on the SettingsPanel).
     */
    public void loadPropertiesFromFile() {
        String propertiesFileName = System.getProperty("user.dir") + File.separator + PROPERTY_FILE_NAME;

        try {
            FileInputStream stream = new FileInputStream(propertiesFileName);
            Properties properties = new Properties();
            properties.load(stream);

            String targetProperty = properties.getProperty(TARGET_PROPERTY_NAME);
            String runProperty = properties.getProperty(RUN_PROPERTY_NAME);
            String observersProperty = properties.getProperty(OBSERVERS_PROPERTY_NAME);
            String observatoryProperty = properties.getProperty(OBSERVATORY_PROPERTY_NAME);
            String telescopeProperty = properties.getProperty(TELESCOPE_PROPERTY_NAME);
            String instrumentProperty = properties.getProperty(INSTRUMENT_PROPERTY_NAME);
            String destinationProperty = properties.getProperty(DESTINATION_PATH_PROPERTY_NAME);

            // Set the properties on the model, but only for those that were read
            // successfully from the properties file (those that are not null).
            if (targetProperty != null) {
                setTarget(targetProperty);
            }

            if (runProperty != null) {
                setRun(runProperty);
            }

            if (observersProperty != null) {
                setObservers(observersProperty);
            }

            if (observatoryProperty != null) {
                setObservatory(observatoryProperty);
            }

            if (telescopeProperty != null) {
                setTelescope(telescopeProperty);
            }

            if (instrumentProperty != null) {
                setInstrument(instrumentProperty);
            }

            if (destinationProperty != null) {
                setDestinationPath(destinationProperty);
            }

            System.out.println("Successfully loaded settings from the properties file called " + propertiesFileName);
        } catch (IOException ex) {
            // There was a problem lmading the properties file. This is ok as the
            // user may not have created a properties file.
            System.out.println("Could not load settings from the properties file called " + propertiesFileName + ", (properties file may not exist?).");
            return;
        }
    }

    /**
     * Saves the current settings to the rangahau.properties file from the user's 
     * home directory.
     */
    public void savePropertiesToFile() {
        String propertiesFileName = System.getProperty("user.dir") + File.separator + PROPERTY_FILE_NAME;
        try {
            String comment = " Settings for the rangahau astronomical data acquisition application\n" + "#\n" + "# Recognised properties are:\n" + "#   " + TARGET_PROPERTY_NAME + " : The target object of the observation. For example, ec20058.\n" + "#   " + RUN_PROPERTY_NAME + " : A description of the run. For example, xcov26.\n" + "#   " + OBSERVERS_PROPERTY_NAME + " : The observers who acquired the data. For example DJS.\n" + "#   " + OBSERVATORY_PROPERTY_NAME + " : The name of the observatory. For example, MJUO.\n" + "#   " + TELESCOPE_PROPERTY_NAME + " : The telescope used to observe. For example, MJUO 1-meter.\n" + "#   " + INSTRUMENT_PROPERTY_NAME + " : The CCD instrument being used. For example, puoko-nui.\n" + "#   " + DESTINATION_PATH_PROPERTY_NAME + " : The destination directory for saving images. For example, /home/sullivan/data";

            Properties properties = new Properties();
            properties.setProperty(TARGET_PROPERTY_NAME, getTarget());
            properties.setProperty(RUN_PROPERTY_NAME, getRun());
            properties.setProperty(OBSERVERS_PROPERTY_NAME, getObservers());
            properties.setProperty(OBSERVATORY_PROPERTY_NAME, getObservatory());
            properties.setProperty(TELESCOPE_PROPERTY_NAME, getTelescope());
            properties.setProperty(INSTRUMENT_PROPERTY_NAME, getInstrument());
            properties.setProperty(DESTINATION_PATH_PROPERTY_NAME, getDestinationPath());


            FileOutputStream stream = new FileOutputStream(propertiesFileName);
            properties.store(stream, comment);
            stream.close();
            System.out.println("Successfully saved settings to the properties file called " + propertiesFileName);
        } catch (IOException ex) {
            // There was a problem saving the properties file.
            System.err.println("Could not save settings to the properties file called " + propertiesFileName + ". Stack trace was:");
            ex.printStackTrace(System.err);
            return;
        }
    }

    /**
     * Gets the location of the target object on images. This may be null if no 
     * target object is designated.
     * 
     * @return the location of the target object on images.
     */
    public synchronized Point2D getTargetLocation() {
        return targetLocation;
    }

    /**
     * Sets the location of the target object on images. This may be null if no 
     * target object is designated.
     * 
     * @param targetLocation the location of the target object on images. This may be null.
     */
    public synchronized void setTargetLocation(Point2D targetLocation) {
        this.targetLocation = targetLocation;
    }

    /**
     * Gets the location of the comparison object on images. This may be null if
     * no comparison object is designated.
     * 
     * @return the location of the comparison object on images.
     */
    public synchronized Point2D getComparisonLocation() {
        return comparisonLocation;
    }

    /**
     * Sets the location of the comparison object on images. This may be null 
     * if no comparison object is designated.
     * 
     * @param comparisonLocation the location of the comparison object on images. 
     *        This may be null.
     */
    public synchronized void setComparisonLocation(Point2D comparisonLocation) {
        this.comparisonLocation = comparisonLocation;
    }
    
    /**
     * Corrects the hosts system clock by synchronizing with an NTP time server.
     * This relies on the 'ntpdate' program being present on the host system
     * and the user having sufficient privilege to execute it.
     */
    public void correctSystemClock() {
      try {
        Runtime.getRuntime().exec(synchronizeCommand);
      } catch(IOException ex) {
        System.err.println("A problem occured when synchronizing the system clock with the command '"
                + synchronizeCommand + "'");
        ex.printStackTrace(System.err);
      }
    }

  /**
   * The command to use to synchronize the host's system clock with a
   * network time server.
   * @return the synchronizeCommand
   */
  public String getSynchronizeCommand() {
    return synchronizeCommand;
  }

  /**
   * The command to use to synchronize the host's system clock with a
   * network time server.
   * @param synchronizeCommand the synchronizeCommand to set
   */
  public void setSynchronizeCommand(String synchronizeCommand) {
    this.synchronizeCommand = synchronizeCommand;
  }

  /**
   * Adds a measurement of the target object to the list of target measurements.
   * 
   * @param measurement the measurement to add.
   */
  public void addTargetMeasurement(Measurement measurement) {
    if (measurement != null) {
      targetMeasurements.add(measurement);
    }
  }

  /**
   * Adds a measurement of the comparison object to the list of comparison
   * measurements.
   *
   * @param measurement the measurement to add.
   */
  public void addComparisonMeasurement(Measurement measurement) {
    if (measurement != null) {
      comparisonMeasurements.add(measurement);
    }
  }

  /**
   * Makes a measurement of the image at a specific location (usually
   * corresponding to an object).
   *
   * @param pixels the pixel elements of the image.
   * @param xCenter the x (column) pixel location of the center of the measurement.
   * @param yCenter the y (row) pixel location of the center of the measurement.
   * @param startTime the time when the image acquisition was started (measured in
   *        milliseconds UTC from the Epoch [1 Jan 1970]).
   * @param endTime the time when the image acquisition finished (in milliseconds
   *        since the Epoch).
   *
   * @return the measurement centered at (xCenter, yCenter).
   */
  public Measurement measure(String filename, int[][] pixels, int xCenter, int yCenter, long startTime, long endTime) {
    if (pixels == null) {
      return null;
    }

    // The maximum radius of the measurement aperture.
    final int maxRadius = 20;
    
    Measurement measurement = new Measurement();
    
    measurement.setFrame(filename);
    measurement.setCentralFlux(pixels[yCenter][xCenter]);
    measurement.setStartTimeMillis(startTime);
    measurement.setEndTimeMillis(endTime);
    measurement.setMaxRadius(maxRadius);
    measurement.setX(xCenter);
    measurement.setY(yCenter);

    double[] rawCounts = new double[1 + maxRadius];
    double[] numPixels = new double[1 + maxRadius];
    double[] backgroundCounts = new double[1 + maxRadius];
    
    
    final int width = pixels[0].length;
    final int height = pixels.length;
    
    // Iterate through aperture sizes from 0 to the maximum.
    for (int radius = 0; radius <= maxRadius; ++radius) {
      int counts = 0;
      int pixelCount = 0;
      int radiusSquared = radius * radius;
      int deltaX = 0;
      int deltaY = 0;
      int deltaSquared = 0;

      for (int x = xCenter - radius; x <= (xCenter + radius); ++x) {
        if ( (x >= 0) && (x < width) ) {
          for (int y = yCenter - radius; y <= (yCenter + radius); ++y) {
            if ( (y >= 0) && (y < height) ) {
              // The pixels is onscreen, now determine whether it is within
              // the aperture radius.
              deltaX = x - xCenter;
              deltaY = y - yCenter;
              
              deltaSquared = deltaX*deltaX + deltaY*deltaY;
              if (deltaSquared <= radiusSquared) {
                counts += pixels[y][x];
                pixelCount++;
              }
            }
          }
        }
      }

      rawCounts[radius] = counts;
      numPixels[radius] = pixelCount;
    }

    // Determine the background count per pixel.
    double countsInAnnulus = rawCounts[maxRadius];
    double pixelsInAnnulus = numPixels[maxRadius];

    if (maxRadius > 0) {
      countsInAnnulus = rawCounts[maxRadius] - rawCounts[maxRadius - 1];
      pixelsInAnnulus = numPixels[maxRadius] - numPixels[maxRadius -1];
    }

    double background = 0.0;
    if (pixelsInAnnulus > 0) {
      background = countsInAnnulus / pixelsInAnnulus;
    }
    
    double totalFlux = 0.0;
    for (int radius = 0; radius <= maxRadius; ++radius) {
      backgroundCounts[radius] = background * numPixels[radius];

      totalFlux += rawCounts[radius] - backgroundCounts[radius];
    }

    measurement.setRawCounts(rawCounts);
    measurement.setNumPixels(numPixels);
    measurement.setBackgroundCounts(backgroundCounts);
    measurement.setBackground(background);
    measurement.setFlux(totalFlux);
    
    return measurement;
  }

  /**
   * The GpsClock device (if we have one)
   * @param gpsClock the gpsClock to set
   */
  public void setGpsClock(GpsClock gpsClock) {
    this.gpsClock = gpsClock;
  }
 }
