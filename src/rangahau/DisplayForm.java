/*
 * Copyright 2007-2010 The Authors (see AUTHORS)
 * This file is part of Rangahau, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

/*
 * DisplayForm.java
 *
 * Created on April 1, 2008, 2:21 AM
 */
package rangahau;

import java.awt.event.ComponentAdapter;
import java.awt.event.ComponentEvent;
import java.io.File;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.TimeZone;
import javax.swing.JDialog;
import javax.swing.JFileChooser;
import javax.swing.SwingUtilities;

/**
 *
 * @author  reid
 */
public class DisplayForm extends javax.swing.JFrame {

    /**
     * Maintains the state of the application.
     */
    Model model;
    
    /**
     * Represents the UTC (Universal Coordinated Time) timezone.
     */
    private TimeZone utcTimeZone = null;
    
    /**
     * The thread used to acquire images from the camera. This may be for
     * measurement or focusing purposes.
     */
    StoppableThread acquisitionThread = null;

    /**
     * Indicates whether acquisition is presently underway or not. True means
     * that acquisitions are occurring.
     */
    boolean acquiring = false;
    
    /**
     * The desired exposure time (in seconds). This desired exposure time will 
     * not become the actual exposure time until the current image acquisition
     * has completed.
     */
    private int desiredExposureTime = 0;
    
    /** Creates new form DisplayForm */
    public DisplayForm() {

        initComponents();
        utcTimeZone = TimeZone.getTimeZone("UTC");
        model = new Model();

        // Load any properties from the rangahau.properties file in the user's
        // home directory. This overwrites any settings in the application's
        // model for any properties that are found.
        model.loadPropertiesFromFile();

        // Set the default exposure time values.
        desiredExposureTime = model.getExposureTime();
        desiredExposureTimeSpinner.setValue(desiredExposureTime);
        setExposureTime(desiredExposureTime);

        // Set the default destination path for saving files to the user's home 
        // directory if the model doesn't have a destination path set already.
        JFileChooser fileChooser = new JFileChooser();
        String defaultDestinationPath = fileChooser.getCurrentDirectory().getPath();
        if (model.getDestinationPath() == null) {
            model.setDestinationPath(defaultDestinationPath);
        }
        destinationDirectoryField.setText(model.getDestinationPath());

        // Set the default frame sequence number.
        frameNumberField.setValue(new Integer(1));

        // If the image type being acquired is a Focus image then disable the
        // save images checkbox (since we shouldn't save focus images).
        String imageType = (String) imageTypeComboBox.getSelectedItem();
        if ((imageType != null) && imageType.equalsIgnoreCase("FOCUS")) {
            saveImagesCheckbox.setEnabled(false);
        }

        // Indicate whether hardware is being simulated or not.
        simulateHardwareButton.setSelected(model.isSimulatingHardware());
    }

    /**
     * Shows intensity data as an image.
     * 
     * @param pixels the intensity data to display. 
     * @param startTime the time when the image acquisition was started (measured in
     *        milliseconds UTC from the Epoch [1 Jan 1970]).
     * @param gpsStartTime the time when the image acquisition was started (measured 
     *        in milliseconds UTC from the Epoch [1 Jan 1970]). This is measured
     *        by a GPS unit.
     */
    public void showImage(int[][] pixels, long startTime, long gpsStartTime) {
        if (pixels == null) {
            return; // There is nothing to display.
        }

        // Save the image to disk
        if (saveImagesCheckbox.isSelected()) {
            final String filename = model.getDestinationPath() + File.separator + getNextImageFilename();

            final long start = startTime;
            final long gpsTime = gpsStartTime;


            final String imageType = (String) imageTypeComboBox.getSelectedItem();
            final String comment = commentField.getText();

            final int[][] pixelValues = pixels;
            SwingUtilities.invokeLater(new Runnable() {

                public void run() {
                    System.out.println("Saving image called " + filename);
                    model.saveImage(filename, pixelValues, start, gpsTime, imageType, comment);
                }
            });
        }
        
        // Display the image in an external viewer
        if (displayImagesCheckbox.isSelected()) {
            final int[][] pixelValues = pixels;
            final long gpsTime = gpsStartTime;
            SwingUtilities.invokeLater(new Runnable() {
                public void run() {
                    System.out.println("Displaying image");
                    model.displayImage(pixelValues, gpsTime);
                }
            });
        }
    }
    
    /**
     * Generates a filename that is describes the image to be saved to disk as
     * part of a series of images.
     * 
     * @return a generated filename.
     */
    public String getNextImageFilename() {

        // Get the image number within the series. This number should be 
        // 4 digits in length (zeroes are prepended if necessary).
        Integer imageSeriesNumber = (Integer) frameNumberField.getValue();
        String seriesNumber = String.format("%04d", imageSeriesNumber.intValue());
        String filename = null;

        String imageType = (String) imageTypeComboBox.getSelectedItem();
        if (imageType.equalsIgnoreCase("FOCUS")) {
            filename = "focus" + seriesNumber + ".fit.gz";
        }
        if (imageType.equalsIgnoreCase("DARK")) {
            filename = "dark" + seriesNumber + ".fit.gz";
        }
        if (imageType.equalsIgnoreCase("FLAT")) {
            filename = "flat" + seriesNumber + ".fit.gz";
        }
        if (imageType.equalsIgnoreCase("TARGET")) {
            filename = model.getTarget() + "_" + seriesNumber + ".fit.gz";
        }
        // Increment the image series number in preparation for the next observation.
        frameNumberField.setValue(new Integer(imageSeriesNumber.intValue() + 1));

        return filename;
    }

    /** This method is called from within the constructor to
     * initialize the form.
     * WARNING: Do NOT modify this code. The content of this method is
     * always regenerated by the Form Editor.
     */
    // <editor-fold defaultstate="collapsed" desc="Generated Code">//GEN-BEGIN:initComponents
    private void initComponents() {

        exitButton = new javax.swing.JButton();
        startButton = new javax.swing.JButton();
        saveImagesCheckbox = new javax.swing.JCheckBox();
        exposureControlPanel = new javax.swing.JPanel();
        exposureControlLabel = new javax.swing.JLabel();
        currentExposureTimeLabel = new javax.swing.JLabel();
        currentExposureTimeField = new javax.swing.JTextField();
        desiredExposureTimeLabel = new javax.swing.JLabel();
        desiredExposureTimeSpinner = new javax.swing.JSpinner();
        setExposureTimeButton = new javax.swing.JButton();
        hardwarePanel = new javax.swing.JPanel();
        hardwareLabel = new javax.swing.JLabel();
        cameraLabel = new javax.swing.JLabel();
        gpsInputPulseLabel = new javax.swing.JLabel();
        triggerPulseLabel = new javax.swing.JLabel();
        simulateHardwareButton = new javax.swing.JCheckBox();
        resetButton = new javax.swing.JButton();
        gpsTimeField = new javax.swing.JTextField();
        syncTimeField = new javax.swing.JTextField();
        browseDestinationButton = new javax.swing.JButton();
        destinationDirectoryField = new javax.swing.JTextField();
        destinationDirectoryLabel = new javax.swing.JLabel();
        frameNumberField = new javax.swing.JSpinner();
        frameNumberLabel = new javax.swing.JLabel();
        settingsButton = new javax.swing.JButton();
        imageTypeComboBox = new javax.swing.JComboBox();
        imageTypeLabel = new javax.swing.JLabel();
        commentLabel = new javax.swing.JLabel();
        commentField = new javax.swing.JTextField();
        displayImagesCheckbox = new javax.swing.JCheckBox();

        setDefaultCloseOperation(javax.swing.WindowConstants.EXIT_ON_CLOSE);
        setTitle("Rangahau Data Acquisition System");

        exitButton.setText("Exit");
        exitButton.setToolTipText("Exits the application");
        exitButton.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                exitButtonActionPerformed(evt);
            }
        });

        startButton.setText("Start");
        startButton.setToolTipText("Starts acquiring and displaying images from the camera");
        startButton.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                startButtonActionPerformed(evt);
            }
        });

        saveImagesCheckbox.setText("Save images");
        saveImagesCheckbox.setToolTipText("Save images to disk");
        saveImagesCheckbox.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                saveImagesCheckboxActionPerformed(evt);
            }
        });

        exposureControlPanel.setBorder(new javax.swing.border.SoftBevelBorder(javax.swing.border.BevelBorder.RAISED));

        exposureControlLabel.setText("Exposure Control");

        currentExposureTimeLabel.setText("Current");

        currentExposureTimeField.setEditable(false);
        currentExposureTimeField.setToolTipText("The current exposure time (in seconds)");
        currentExposureTimeField.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                currentExposureTimeFieldActionPerformed(evt);
            }
        });

        desiredExposureTimeLabel.setText("Desired");

        desiredExposureTimeSpinner.setModel(new javax.swing.SpinnerNumberModel(1, 1, 30, 1));
        desiredExposureTimeSpinner.setToolTipText("The desired exposure time (in seconds)");

        setExposureTimeButton.setText("Set Exposure Time");
        setExposureTimeButton.setToolTipText("Sets the current exposure time to the value in the desired exposure time field");
        setExposureTimeButton.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                setExposureTimeButtonActionPerformed(evt);
            }
        });

        javax.swing.GroupLayout exposureControlPanelLayout = new javax.swing.GroupLayout(exposureControlPanel);
        exposureControlPanel.setLayout(exposureControlPanelLayout);
        exposureControlPanelLayout.setHorizontalGroup(
            exposureControlPanelLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
            .addGroup(exposureControlPanelLayout.createSequentialGroup()
                .addContainerGap()
                .addGroup(exposureControlPanelLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
                    .addComponent(exposureControlLabel, javax.swing.GroupLayout.DEFAULT_SIZE, 160, Short.MAX_VALUE)
                    .addGroup(exposureControlPanelLayout.createSequentialGroup()
                        .addComponent(currentExposureTimeLabel)
                        .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                        .addComponent(currentExposureTimeField, javax.swing.GroupLayout.DEFAULT_SIZE, 103, Short.MAX_VALUE))
                    .addGroup(exposureControlPanelLayout.createSequentialGroup()
                        .addComponent(desiredExposureTimeLabel)
                        .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                        .addComponent(desiredExposureTimeSpinner, javax.swing.GroupLayout.DEFAULT_SIZE, 102, Short.MAX_VALUE))
                    .addComponent(setExposureTimeButton, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, Short.MAX_VALUE))
                .addContainerGap())
        );
        exposureControlPanelLayout.setVerticalGroup(
            exposureControlPanelLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
            .addGroup(exposureControlPanelLayout.createSequentialGroup()
                .addContainerGap()
                .addComponent(exposureControlLabel)
                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.UNRELATED)
                .addGroup(exposureControlPanelLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
                    .addComponent(currentExposureTimeLabel)
                    .addComponent(currentExposureTimeField, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE))
                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.UNRELATED)
                .addGroup(exposureControlPanelLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
                    .addComponent(desiredExposureTimeLabel)
                    .addComponent(desiredExposureTimeSpinner, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE))
                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                .addComponent(setExposureTimeButton)
                .addContainerGap(javax.swing.GroupLayout.DEFAULT_SIZE, Short.MAX_VALUE))
        );

        hardwarePanel.setBorder(new javax.swing.border.SoftBevelBorder(javax.swing.border.BevelBorder.RAISED));
        hardwarePanel.setEnabled(false);

        hardwareLabel.setText("Hardware");

        cameraLabel.setText("Camera");

        gpsInputPulseLabel.setText("GPS Time");

        triggerPulseLabel.setText("Sync Time");

        simulateHardwareButton.setText("Simulate ");
        simulateHardwareButton.setToolTipText("Use simulated hardware rather than real hardware (good for software development purposes)");
        simulateHardwareButton.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                simulateHardwareButtonActionPerformed(evt);
            }
        });

        resetButton.setText("Reset");
        resetButton.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                resetButtonActionPerformed(evt);
            }
        });

        gpsTimeField.setToolTipText("The UTC time last received from GPS");

        syncTimeField.setToolTipText("The UTC time the last sync pulse was sent");

        javax.swing.GroupLayout hardwarePanelLayout = new javax.swing.GroupLayout(hardwarePanel);
        hardwarePanel.setLayout(hardwarePanelLayout);
        hardwarePanelLayout.setHorizontalGroup(
            hardwarePanelLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
            .addGroup(hardwarePanelLayout.createSequentialGroup()
                .addContainerGap()
                .addGroup(hardwarePanelLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
                    .addGroup(hardwarePanelLayout.createSequentialGroup()
                        .addComponent(simulateHardwareButton)
                        .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED, 59, Short.MAX_VALUE)
                        .addComponent(resetButton, javax.swing.GroupLayout.PREFERRED_SIZE, 112, javax.swing.GroupLayout.PREFERRED_SIZE)
                        .addGap(38, 38, 38))
                    .addGroup(hardwarePanelLayout.createSequentialGroup()
                        .addGroup(hardwarePanelLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
                            .addComponent(hardwareLabel)
                            .addComponent(cameraLabel)
                            .addGroup(hardwarePanelLayout.createSequentialGroup()
                                .addGroup(hardwarePanelLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
                                    .addComponent(gpsInputPulseLabel)
                                    .addComponent(triggerPulseLabel))
                                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                                .addGroup(hardwarePanelLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
                                    .addComponent(syncTimeField, javax.swing.GroupLayout.DEFAULT_SIZE, 205, Short.MAX_VALUE)
                                    .addComponent(gpsTimeField, javax.swing.GroupLayout.DEFAULT_SIZE, 205, Short.MAX_VALUE))))
                        .addContainerGap())))
        );
        hardwarePanelLayout.setVerticalGroup(
            hardwarePanelLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
            .addGroup(hardwarePanelLayout.createSequentialGroup()
                .addContainerGap()
                .addComponent(hardwareLabel)
                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                .addComponent(cameraLabel)
                .addGap(32, 32, 32)
                .addGroup(hardwarePanelLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
                    .addComponent(gpsInputPulseLabel)
                    .addComponent(gpsTimeField, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE))
                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.UNRELATED)
                .addGroup(hardwarePanelLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
                    .addComponent(triggerPulseLabel)
                    .addComponent(syncTimeField, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE))
                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED, javax.swing.GroupLayout.DEFAULT_SIZE, Short.MAX_VALUE)
                .addGroup(hardwarePanelLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
                    .addComponent(simulateHardwareButton)
                    .addComponent(resetButton))
                .addContainerGap())
        );

        browseDestinationButton.setText("Browse");
        browseDestinationButton.setToolTipText("Browse for a destination directory to save images to");
        browseDestinationButton.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                browseDestinationButtonActionPerformed(evt);
            }
        });

        destinationDirectoryField.setEditable(false);
        destinationDirectoryField.setToolTipText("Destination directory where images are saved to");
        destinationDirectoryField.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                destinationDirectoryFieldActionPerformed(evt);
            }
        });

        destinationDirectoryLabel.setText("Destination");

        frameNumberField.setToolTipText("The sequence number of the next frame to be saved");

        frameNumberLabel.setText("Frame Number");

        settingsButton.setText("Settings ...");
        settingsButton.setToolTipText("Change settings that are recorded in the image headers");
        settingsButton.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                settingsButtonActionPerformed(evt);
            }
        });

        imageTypeComboBox.setModel(new javax.swing.DefaultComboBoxModel(new String[] { "Dark", "Flat", "Target" }));
        imageTypeComboBox.setToolTipText("The type of image to be acquired.");
        imageTypeComboBox.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                imageTypeComboBoxActionPerformed(evt);
            }
        });

        imageTypeLabel.setText("Image Type");
        imageTypeLabel.setToolTipText("The type of image to be acquired.");

        commentLabel.setText("Comment");

        commentField.setToolTipText("Comment text to place within FITS header (up to 72 characters)");
        commentField.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                commentFieldActionPerformed(evt);
            }
        });

        displayImagesCheckbox.setSelected(true);
        displayImagesCheckbox.setText("Display images");
        displayImagesCheckbox.setToolTipText("Send images to DS9");
        displayImagesCheckbox.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                displayImagesCheckboxActionPerformed(evt);
            }
        });

        javax.swing.GroupLayout layout = new javax.swing.GroupLayout(getContentPane());
        getContentPane().setLayout(layout);
        layout.setHorizontalGroup(
            layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
            .addGroup(layout.createSequentialGroup()
                .addContainerGap()
                .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
                    .addGroup(layout.createSequentialGroup()
                        .addComponent(commentLabel)
                        .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                        .addComponent(commentField, javax.swing.GroupLayout.DEFAULT_SIZE, 917, Short.MAX_VALUE))
                    .addGroup(layout.createSequentialGroup()
                        .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
                            .addComponent(frameNumberLabel)
                            .addComponent(startButton)
                            .addComponent(imageTypeLabel)
                            .addComponent(settingsButton))
                        .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                        .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
                            .addComponent(saveImagesCheckbox)
                            .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.TRAILING, false)
                                .addComponent(imageTypeComboBox, javax.swing.GroupLayout.Alignment.LEADING, 0, javax.swing.GroupLayout.DEFAULT_SIZE, Short.MAX_VALUE)
                                .addComponent(frameNumberField, javax.swing.GroupLayout.Alignment.LEADING, javax.swing.GroupLayout.DEFAULT_SIZE, 144, Short.MAX_VALUE))
                            .addComponent(displayImagesCheckbox))
                        .addGap(175, 175, 175)
                        .addComponent(exposureControlPanel, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE)
                        .addGap(18, 18, 18)
                        .addComponent(hardwarePanel, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, Short.MAX_VALUE))
                    .addGroup(layout.createSequentialGroup()
                        .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
                            .addComponent(destinationDirectoryLabel)
                            .addComponent(destinationDirectoryField, javax.swing.GroupLayout.DEFAULT_SIZE, 559, Short.MAX_VALUE))
                        .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                        .addComponent(browseDestinationButton)
                        .addGap(222, 222, 222)
                        .addComponent(exitButton)
                        .addGap(35, 35, 35)))
                .addGap(23, 23, 23))
        );
        layout.setVerticalGroup(
            layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
            .addGroup(layout.createSequentialGroup()
                .addContainerGap()
                .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
                    .addComponent(hardwarePanel, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE)
                    .addGroup(layout.createSequentialGroup()
                        .addGap(17, 17, 17)
                        .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
                            .addComponent(exposureControlPanel, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE)
                            .addGroup(layout.createSequentialGroup()
                                .addGap(51, 51, 51)
                                .addComponent(displayImagesCheckbox)
                                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                                .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
                                    .addComponent(startButton)
                                    .addComponent(saveImagesCheckbox))
                                .addGap(12, 12, 12)
                                .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
                                    .addComponent(imageTypeLabel)
                                    .addComponent(imageTypeComboBox, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE))
                                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                                .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
                                    .addGroup(layout.createSequentialGroup()
                                        .addComponent(frameNumberLabel)
                                        .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                                        .addComponent(settingsButton))
                                    .addComponent(frameNumberField, javax.swing.GroupLayout.PREFERRED_SIZE, 28, javax.swing.GroupLayout.PREFERRED_SIZE))))))
                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
                    .addComponent(commentField, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE)
                    .addComponent(commentLabel))
                .addGap(24, 24, 24)
                .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.TRAILING)
                    .addGroup(layout.createSequentialGroup()
                        .addComponent(destinationDirectoryLabel)
                        .addGap(4, 4, 4)
                        .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
                            .addComponent(destinationDirectoryField, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE)
                            .addComponent(browseDestinationButton)))
                    .addComponent(exitButton))
                .addContainerGap(32, Short.MAX_VALUE))
        );

        pack();
    }// </editor-fold>//GEN-END:initComponents
    private void exitButtonActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_exitButtonActionPerformed

        try {
            model.releaseResources();
        } catch (Throwable th) {
            th.printStackTrace(System.err);
            // Ignore shutdown problems.
        }

        setVisible(false);
        dispose();

        System.exit(0);
    }//GEN-LAST:event_exitButtonActionPerformed

    private void startButtonActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_startButtonActionPerformed

        if (!acquiring) {
            // Start acquiring images.
            startAcquisition();

            // Re-label the 'start' button to stop.
            startButton.setText("Stop");
        } else {
            // Stop acquisitions.
            stopAcquisition();

            // Re-label the 'start' button to start.
            startButton.setText("Start");
        }
    }//GEN-LAST:event_startButtonActionPerformed

    private void currentExposureTimeFieldActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_currentExposureTimeFieldActionPerformed
}//GEN-LAST:event_currentExposureTimeFieldActionPerformed

    private void setExposureTimeButtonActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_setExposureTimeButtonActionPerformed
        desiredExposureTime = ((Integer) desiredExposureTimeSpinner.getValue());
        setExposureTime(desiredExposureTime);
    }//GEN-LAST:event_setExposureTimeButtonActionPerformed

private void destinationDirectoryFieldActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_destinationDirectoryFieldActionPerformed
}//GEN-LAST:event_destinationDirectoryFieldActionPerformed

private void browseDestinationButtonActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_browseDestinationButtonActionPerformed
    // Display a file selection dialog, point it at the current selected destination
    // directory (if any).
    JFileChooser destinationPathChooser = null;
    if ((model != null) && (model.getDestinationPath() != null) && (model.getDestinationPath().trim().length() > 0)) {
        // There is an existing destination directory.
        destinationPathChooser = new JFileChooser(model.getDestinationPath());
    } else {
        // There is no destination currently, so create a file chooser with the
        // default destination directory.
        destinationPathChooser = new JFileChooser();
    }

    destinationPathChooser.setDialogType(JFileChooser.CUSTOM_DIALOG);
    destinationPathChooser.setFileSelectionMode(JFileChooser.DIRECTORIES_ONLY);
    destinationPathChooser.setDialogTitle("Select a destination directory");
    destinationPathChooser.setApproveButtonText("Select");
    destinationPathChooser.setApproveButtonToolTipText("Selects the current directory as the destination to save images to");
    int userChoice = destinationPathChooser.showDialog(this, "Select");

    // If the user chose a directory then put the selected directory in the
    // destination directory field and remember this directory.
    if (userChoice == JFileChooser.APPROVE_OPTION) {
        String path = destinationPathChooser.getSelectedFile().getPath();
        destinationDirectoryField.setText(path);
        if (model != null) {
            model.setDestinationPath(path);

            // Save the application properties so this change is permanent.
            model.savePropertiesToFile();
        }
    }
}//GEN-LAST:event_browseDestinationButtonActionPerformed

private void settingsButtonActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_settingsButtonActionPerformed
    // Show the options panel in a dialog. This dialog is application-modal
    // so we don't have to disable the settings button etc when the application
    // is displayed.
    JDialog dialog = new JDialog(this, "Application Settings", true);
    dialog.setDefaultCloseOperation(JDialog.DISPOSE_ON_CLOSE);
    dialog.add(new SettingsPanel(model, dialog));
    dialog.pack();

    // Once the dialog has been hidden we no longer want it so it should be
    // disposed of.
    dialog.addComponentListener(new ComponentAdapter() {

        public void componentHidden(ComponentEvent event) {
            JDialog thisDialog = (JDialog) event.getComponent();
            thisDialog.dispose();
        }
    });

    dialog.setVisible(true);
}//GEN-LAST:event_settingsButtonActionPerformed

private void imageTypeComboBoxActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_imageTypeComboBoxActionPerformed
        // Disable certain controls when the image type is changed.
        String selection = (String) imageTypeComboBox.getSelectedItem();

        // We cannot save images in focus mode.
        if ((selection != null) && selection.equalsIgnoreCase("FOCUS")) {
            saveImagesCheckbox.setEnabled(false);
        } else {
            saveImagesCheckbox.setEnabled(true);
        }
    }
//GEN-LAST:event_imageTypeComboBoxActionPerformed

private void commentFieldActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_commentFieldActionPerformed
}//GEN-LAST:event_commentFieldActionPerformed

private void simulateHardwareButtonActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_simulateHardwareButtonActionPerformed
    boolean simulating = simulateHardwareButton.isSelected();
    model.setSimulatingHardware(simulating);

    // Close and re-initialise the hardware (or simulated hardware) to reflect
    // whether simulation mode was selected or not.
    model.releaseResources();

    final long sleepEndTime = System.currentTimeMillis() + 1000;
    while (System.currentTimeMillis() < sleepEndTime) {
        try {
            Thread.sleep(10);
        } catch (Throwable th) {
            // Ignore interruptions.
        }
    }

    model.initialiseImageSource();
}//GEN-LAST:event_simulateHardwareButtonActionPerformed

private void resetButtonActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_resetButtonActionPerformed
    // Close and re-initialise the hardware.
    model.releaseResources();

    final long sleepEndTime = System.currentTimeMillis() + 1000;
    while (System.currentTimeMillis() < sleepEndTime) {
        try {
            Thread.sleep(10);
        } catch (Throwable th) {
            // Ignore interruptions.
        }
    }

    model.initialiseImageSource();

}//GEN-LAST:event_resetButtonActionPerformed

private void displayImagesCheckboxActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_displayImagesCheckboxActionPerformed
}//GEN-LAST:event_displayImagesCheckboxActionPerformed

private void saveImagesCheckboxActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_saveImagesCheckboxActionPerformed
}//GEN-LAST:event_saveImagesCheckboxActionPerformed

    /**
     * Sets the exposure time. 
     * 
     * @param exposure the exposure time to use (in seconds).
     * 
     * @throws IllegalArgumentException if exposureTime is 0 or less.
     * @throws IllegalArgumentException if exposureTime is 30 or less.
     */
    public void setExposureTime(int exposure) {
        if (exposure < 0) {
            throw new IllegalArgumentException("Cannot set the exposure time to a negative value, the value given was " + exposure);
        }
        if (exposure > 30) {
            throw new IllegalArgumentException("Cannot set the exposure time to a value greater than 30 seconds, the value given was " + exposure);
        }

        System.out.println("Setting the exposure time to " + exposure + " s");

        // We cannot set the exposure time while a frame acquisition is taking
        // place, so disable frame acquisition.
        if (acquiring) {
            stopAcquisition();
        }

        model.setExposureTime(exposure);

        // Indicate that the exposure time has been changed.
        currentExposureTimeField.setText(Integer.toString(exposure));

        // If we were acquiring we start new acquisition thread that will use 
        // the new exposure time value.
        if (acquiring) {
            startAcquisition();
        }
    }

    private void startAcquisition() {
        // Disable user interface elements that cannot be used when acquiring.
        browseDestinationButton.setEnabled(false);
        setExposureTimeButton.setEnabled(false);
        settingsButton.setEnabled(false);
        imageTypeComboBox.setEnabled(false);
        simulateHardwareButton.setEnabled(false);
        resetButton.setEnabled(false);
        exitButton.setEnabled(false);

        // Ensure the hardware is set up for acquisitions.
        if (!model.isImageSourceInitialised()) {
            model.initialiseImageSource();
        }

        // If an acquisition thread is already running we don't need to do anything.
        if (acquisitionThread == null) {
            acquisitionThread = new FastAcquisitionThread(this, model);
            acquisitionThread.start();
        }
        acquiring = true;
    }

    private void stopAcquisition() {
        System.out.println("Stopping acquisitions :)");

        // Enable user interface elements that were disabled while acquiring.
        browseDestinationButton.setEnabled(true);
        setExposureTimeButton.setEnabled(true);
        settingsButton.setEnabled(true);
        imageTypeComboBox.setEnabled(true);
        simulateHardwareButton.setEnabled(true);
        resetButton.setEnabled(true);
        exitButton.setEnabled(true);

        if (acquisitionThread != null) {
            acquisitionThread.shouldStop();

            // Wait until the thread has actually stopped.
            System.out.println("Waiting for acquisition thread to finish ... : main thread Id = " + Thread.currentThread().getId());
            long endTime = System.currentTimeMillis() + 3000;
            while (!acquisitionThread.finished() && (System.currentTimeMillis() < endTime)) {
                Thread.yield();
                //try {
                //     Thread.sleep(10);
                //} catch (InterruptedException ex) {
                //    // Ignore interruptions.
                //}
            }

            System.out.println("Acquisition thread has finished.");
        }

        // Stop acquisitions.
        if ((model != null) && (model.getCamera() != null)) {
            model.getCamera().stopAcquisition();
        }

        acquisitionThread = null; // We no longer own the acquisition thread.

        acquiring = false;
    }

    /**
     * @param args the command line arguments
     */
    public static void main(String args[]) {
        java.awt.EventQueue.invokeLater(new Runnable() {

            public void run() {
                new DisplayForm().setVisible(true);
            }
        });
    }
    // Variables declaration - do not modify//GEN-BEGIN:variables
    private javax.swing.JButton browseDestinationButton;
    private javax.swing.JLabel cameraLabel;
    private javax.swing.JTextField commentField;
    private javax.swing.JLabel commentLabel;
    private javax.swing.JTextField currentExposureTimeField;
    private javax.swing.JLabel currentExposureTimeLabel;
    private javax.swing.JLabel desiredExposureTimeLabel;
    private javax.swing.JSpinner desiredExposureTimeSpinner;
    private javax.swing.JTextField destinationDirectoryField;
    private javax.swing.JLabel destinationDirectoryLabel;
    private javax.swing.JCheckBox displayImagesCheckbox;
    private javax.swing.JButton exitButton;
    private javax.swing.JLabel exposureControlLabel;
    private javax.swing.JPanel exposureControlPanel;
    private javax.swing.JSpinner frameNumberField;
    private javax.swing.JLabel frameNumberLabel;
    private javax.swing.JLabel gpsInputPulseLabel;
    private javax.swing.JTextField gpsTimeField;
    private javax.swing.JLabel hardwareLabel;
    private javax.swing.JPanel hardwarePanel;
    private javax.swing.JComboBox imageTypeComboBox;
    private javax.swing.JLabel imageTypeLabel;
    private javax.swing.JButton resetButton;
    private javax.swing.JCheckBox saveImagesCheckbox;
    private javax.swing.JButton setExposureTimeButton;
    private javax.swing.JButton settingsButton;
    private javax.swing.JCheckBox simulateHardwareButton;
    private javax.swing.JButton startButton;
    private javax.swing.JTextField syncTimeField;
    private javax.swing.JLabel triggerPulseLabel;
    // End of variables declaration//GEN-END:variables

    /**
     * The desired exposure time (in seconds). This desired exposure time will
     * not become the actual exposure time until the current image acquisition
     * has completed.
     */
    public int getDesiredExposureTime() {
        return desiredExposureTime;
    }

    public void setDesiredExposureTime(int desiredExposureTime) {
        this.desiredExposureTime = desiredExposureTime;
    }

    /**
     * Shows the time of the last sync pulse from the timing unit.
     *
     * @param syncTime the time of the last sync pulse from the timing unit.
     */
    public void showSyncTime(Date syncTime) {
        SimpleDateFormat formatter = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");
        formatter.setTimeZone(utcTimeZone);

        String time = formatter.format(syncTime);
        syncTimeField.setText(time);
    }

    /**
     * Shows the time of the last GPS pulse from the timing unit.
     *
     * @param gpsTime the time of the last gps pulse from the timing unit.
     */
    public void showGpsTime(Date gpsTime) {
        SimpleDateFormat formatter = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");
        formatter.setTimeZone(utcTimeZone);

        String time = formatter.format(gpsTime);
        gpsTimeField.setText(time);
    }
}
