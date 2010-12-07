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

/**
 *
 * @author reid
 */
public class Main {

    /**
     * @param args the command line arguments
     */
    public static void main(String[] args) {
        Main application = new Main();
    }
    
    /**
     * Constructs and new Main application object.
     */
    public Main() {
        java.awt.EventQueue.invokeLater(new Runnable() {
            public void run() {
                new DisplayForm().setVisible(true);
            }
        });
    }
}
