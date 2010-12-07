/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

package rangahau;

/**
 * Interface of threads that can be stopped.
 *
 * @author Mike Reid
 */
public class StoppableThread extends Thread {

    /**
     * Indicates the thread should stop.
     */
    protected boolean stopThread = false;

    /**
     * True when the thread has completed running.
     */
    protected boolean finished = false;

    /**
     * Indicates that the thread should stop as soon as possible.
     */
    public synchronized void shouldStop() {
      stopThread = true;
      System.out.println("StoppableThread.shouldStop() called, stop thread = " + stopThread + ", thread id = " + Thread.currentThread().getId());

    }

    /**
     * Returns true when the thread has finished running.
     *
     * @return true if the thread has finished running, false if it has not.
     */
    public synchronized boolean finished() {
      return finished;
    }
}