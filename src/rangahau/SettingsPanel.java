/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

package rangahau;

import javax.swing.JDialog;

/**
 * Displays the user-configurable settings. These settings are not changed very 
 * often so are not displayed on the main DisplayForm.
 * 
 * @author  Mike Reid
 */
public class SettingsPanel extends javax.swing.JPanel {
    
    /**
     * The application model.
     */
    private Model model = null;
        
    /**
     * The dialog window that hosts this panel.
     */
    private JDialog dialog = null;
     
    /** Creates new form SettingsPanel */
    public SettingsPanel(Model model, JDialog dialog) {
        if (model == null) {
            throw new IllegalArgumentException("Cannot create a SettingsPanel as the application model given was null.");
        }
        if (dialog == null) {
            throw new IllegalArgumentException("Cannot create a SettingsPanel as the dialog given was null.");
        }
        initComponents();
        
        this.model = model;
        this.dialog = dialog;
        
        // Load any properties from the rangahau.properties file in the user's
        // home directory. This overwrites any settings in the application's
        // model for any properties that are found.
        model.loadPropertiesFromFile();
        
        showSettings();
    }

    /**
     * Shows the application model's current settings in this panel.
     */
    public void showSettings() {
        
        targetField.setText(model.getTarget());
        runField.setText(model.getRun());
        observersField.setText(model.getObservers());
        observatoryField.setText(model.getObservatory());
        telescopeField.setText(model.getTelescope());
        instrumentField.setText(model.getInstrument());
        
        // Show this panel.
        setVisible(true);
    }
    
    /**
     * Update the application model's settings using the value shown in this
     * panel. 
     */
    public void setSettings() {
        model.setTarget(targetField.getText());
        model.setRun(runField.getText());
        model.setObservers(observersField.getText());
        model.setObservatory(observatoryField.getText());
        model.setTelescope(telescopeField.getText());
        model.setInstrument(instrumentField.getText());
    }
    
    /** This method is called from within the constructor to
     * initialize the form.
     * WARNING: Do NOT modify this code. The content of this method is
     * always regenerated by the Form Editor.
     */
    @SuppressWarnings("unchecked")
    // <editor-fold defaultstate="collapsed" desc="Generated Code">//GEN-BEGIN:initComponents
    private void initComponents() {

        targetLabel = new javax.swing.JLabel();
        runLabel = new javax.swing.JLabel();
        observersLabel = new javax.swing.JLabel();
        observatoryLabel = new javax.swing.JLabel();
        targetField = new javax.swing.JTextField();
        closeButton = new javax.swing.JButton();
        acceptButton = new javax.swing.JButton();
        runField = new javax.swing.JTextField();
        observersField = new javax.swing.JTextField();
        observatoryField = new javax.swing.JTextField();
        telescopeLabel = new javax.swing.JLabel();
        telescopeField = new javax.swing.JTextField();
        instrumentLabel = new javax.swing.JLabel();
        instrumentField = new javax.swing.JTextField();
        titleLabel = new javax.swing.JLabel();

        targetLabel.setText("Target");

        runLabel.setText("Run");

        observersLabel.setText("Observers");

        observatoryLabel.setText("Observatory");

        targetField.setToolTipText("The name of the target object being observed, eg. \"L19-2\"");

        closeButton.setText("Close");
        closeButton.setToolTipText("Close without accepting any changes");
        closeButton.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                closeButtonActionPerformed(evt);
            }
        });

        acceptButton.setText("Accept");
        acceptButton.setToolTipText("Accept changes to the settings");
        acceptButton.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                acceptButtonActionPerformed(evt);
            }
        });

        runField.setToolTipText("The observing run, eg. xcov26");

        observersField.setToolTipText("The observers, eg. DJS");

        observatoryField.setToolTipText("The observatory where the measurements were made, eg. MJUO");

        telescopeLabel.setText("Telescope");

        telescopeField.setToolTipText("The name of the telescope used, eg.  MJUO 1m");

        instrumentLabel.setText("Instrument");

        instrumentField.setToolTipText("Name of the instrument system, eg. puoko-nui");

        titleLabel.setText("Settings");

        javax.swing.GroupLayout layout = new javax.swing.GroupLayout(this);
        this.setLayout(layout);
        layout.setHorizontalGroup(
            layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
            .addGroup(layout.createSequentialGroup()
                .addContainerGap()
                .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
                    .addGroup(javax.swing.GroupLayout.Alignment.TRAILING, layout.createSequentialGroup()
                        .addComponent(acceptButton)
                        .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                        .addComponent(closeButton))
                    .addGroup(layout.createSequentialGroup()
                        .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
                            .addComponent(targetLabel)
                            .addComponent(runLabel)
                            .addComponent(observatoryLabel)
                            .addComponent(telescopeLabel)
                            .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.TRAILING, false)
                                .addComponent(titleLabel, javax.swing.GroupLayout.Alignment.LEADING, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, Short.MAX_VALUE)
                                .addComponent(observersLabel, javax.swing.GroupLayout.Alignment.LEADING, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, Short.MAX_VALUE))
                            .addComponent(instrumentLabel))
                        .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                        .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
                            .addComponent(instrumentField, javax.swing.GroupLayout.DEFAULT_SIZE, 286, Short.MAX_VALUE)
                            .addComponent(observatoryField, javax.swing.GroupLayout.DEFAULT_SIZE, 286, Short.MAX_VALUE)
                            .addComponent(observersField, javax.swing.GroupLayout.DEFAULT_SIZE, 286, Short.MAX_VALUE)
                            .addComponent(runField, javax.swing.GroupLayout.DEFAULT_SIZE, 286, Short.MAX_VALUE)
                            .addComponent(targetField, javax.swing.GroupLayout.DEFAULT_SIZE, 286, Short.MAX_VALUE)
                            .addComponent(telescopeField, javax.swing.GroupLayout.DEFAULT_SIZE, 286, Short.MAX_VALUE))))
                .addContainerGap())
        );
        layout.setVerticalGroup(
            layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
            .addGroup(javax.swing.GroupLayout.Alignment.TRAILING, layout.createSequentialGroup()
                .addContainerGap()
                .addComponent(titleLabel)
                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED, 26, Short.MAX_VALUE)
                .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
                    .addComponent(targetLabel)
                    .addComponent(targetField, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE))
                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
                    .addComponent(runLabel)
                    .addComponent(runField, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE))
                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
                    .addComponent(observersField, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE)
                    .addComponent(observersLabel))
                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
                    .addComponent(observatoryField, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE)
                    .addComponent(observatoryLabel))
                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
                    .addComponent(telescopeField, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE)
                    .addComponent(telescopeLabel))
                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
                    .addComponent(instrumentLabel)
                    .addComponent(instrumentField, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE))
                .addGap(12, 12, 12)
                .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
                    .addComponent(closeButton)
                    .addComponent(acceptButton))
                .addContainerGap())
        );
    }// </editor-fold>//GEN-END:initComponents

private void acceptButtonActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_acceptButtonActionPerformed
    // The user wishes to close the settings panel and use the edits they have//GEN-LAST:event_acceptButtonActionPerformed
    // make.
    dialog.setVisible(false);
    setSettings();
    
    // Save the current settings permanently to disk.
    model.savePropertiesToFile();
}

private void closeButtonActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_closeButtonActionPerformed

    // The user wishes to close the settings panel and discard the edits they have
    // make.
    dialog.setVisible(false);
    showSettings(); // Discard the settings by reloading the settings from the model.
}//GEN-LAST:event_closeButtonActionPerformed


    // Variables declaration - do not modify//GEN-BEGIN:variables
    private javax.swing.JButton acceptButton;
    private javax.swing.JButton closeButton;
    private javax.swing.JTextField instrumentField;
    private javax.swing.JLabel instrumentLabel;
    private javax.swing.JTextField observatoryField;
    private javax.swing.JLabel observatoryLabel;
    private javax.swing.JTextField observersField;
    private javax.swing.JLabel observersLabel;
    private javax.swing.JTextField runField;
    private javax.swing.JLabel runLabel;
    private javax.swing.JTextField targetField;
    private javax.swing.JLabel targetLabel;
    private javax.swing.JTextField telescopeField;
    private javax.swing.JLabel telescopeLabel;
    private javax.swing.JLabel titleLabel;
    // End of variables declaration//GEN-END:variables

}
