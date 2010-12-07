/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

package rangahau;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.TimeZone;
import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import static org.junit.Assert.*;

/**
 *
 * @author reid
 */
public class GpsClockTest {

    SimpleDateFormat dateFormatter;

    TimeZone utcTimeZone;

    public GpsClockTest() {
      utcTimeZone = TimeZone.getTimeZone("UTC");
      dateFormatter = new SimpleDateFormat("yyyy:MM:dd:HH:mm:ss:SSS");
      dateFormatter.setTimeZone(utcTimeZone);
    }

  @BeforeClass
  public static void setUpClass() throws Exception {
  }

  @AfterClass
  public static void tearDownClass() throws Exception {
  }

    @Before
    public void setUp() {
    }

    @After
    public void tearDown() {
    }

  /**
   * Test of getAllDeviceNames method, of class GpsClock.
   */
  @Test
  public void testGetAllDeviceNames() {
    List<String> result = GpsClock.getAllDeviceNames();

    assertNotNull(result);
    assertTrue(result.size() > 0);

    for (String name : result) {
      assertNotNull(name);
      assertTrue(name.trim().length() > 0);
    }
  }

  /**
   * Test of open method, of class GpsClock.
   */
  @Test
  public void testOpen() {
    List<String> names = GpsClock.getAllDeviceNames();

    assertNotNull(names);
    assertTrue(names.size() > 0);
    String name = names.get(0);

    GpsClock clock = new GpsClock();
    try {
      clock.open(name);
    } finally {
      clock.close();
    }

    // Test can re-open.
    try {
      clock.open(name);
    } finally {
      clock.close();
    }
  }

  /**
   * Test of isOpen method, of class GpsClock.
   */
  @Test
  public void testIsOpen() {
    List<String> names = GpsClock.getAllDeviceNames();

    assertNotNull(names);
    assertTrue(names.size() > 0);
    String name = names.get(0);

    GpsClock clock = new GpsClock();

    try {
      clock.open(name);
      boolean open = clock.isOpen();
      Assert.assertTrue(open);
    } finally {
      clock.close();
    }
    Assert.assertFalse(clock.isOpen());

    // Test can re-open and open status recorded properly.
    try {
      clock.open(name);
      boolean open = clock.isOpen();
      Assert.assertTrue(open);
    } finally {
      clock.close();
    }
    Assert.assertFalse(clock.isOpen());
  }

  /**
   * Test of close method, of class GpsClock.
   */
  @Test
  public void testClose() {
    List<String> names = GpsClock.getAllDeviceNames();

    assertNotNull(names);
    assertTrue(names.size() > 0);
    String name = names.get(0);

    GpsClock clock = new GpsClock();

    try {
      clock.open(name);
    } finally {
      clock.close();
    }

    // Test can re-open.
    try {
      clock.open(name);
    } finally {
      clock.close();
    }
  }

  /**
   * Test of getDeviceName method, of class GpsClock.
   */
  @Test
  public void testGetDeviceName() {

    List<String> names = GpsClock.getAllDeviceNames();

    for (String name : names) {
      GpsClock clock = new GpsClock();
      try {
        clock.open(name);

        String result = clock.getDeviceName();

        assertNotNull(result);
        assertEquals(name, result);
      } finally {
        clock.close();
      }
    }
  }

  /**
   * Test of write method, of class GpsClock.
   */
  @Test
  public void testWrite() {
    List<String> names = GpsClock.getAllDeviceNames();

    assertNotNull(names);
    assertTrue(names.size() > 0);
    String name = names.get(0);

    GpsClock clock = new GpsClock();

    try {
      clock.open(name);
      int[] request = clock.getSetExposureTimeRequest(10);

      assertNotNull(request);
      final int expectedLength = 9;
      assertEquals(expectedLength, request.length);
      clock.write(request);

      int timeoutMillis = 20;
      final int expectedResponseLength = 9;
      int[] response = clock.read(timeoutMillis, expectedResponseLength);

      assertNotNull(response);
      assertEquals(expectedResponseLength, response.length);
    } finally {
      clock.close();
    }
  }

  /**
   * Test of read method, of class GpsClock.
   */
  @Test
  public void testRead() {
    List<String> names = GpsClock.getAllDeviceNames();

    assertNotNull(names);
    assertTrue(names.size() > 0);
    String name = names.get(0);

    GpsClock clock = new GpsClock();

    try {
      clock.open(name);
      int[] request = clock.getEchoRequest();
      clock.write(request);

      int timeoutMillis = 20;
      final int expectedLength = 5;
      int[] response = clock.read(timeoutMillis, expectedLength);
      assertNotNull(response);

      assertEquals(expectedLength, response.length);
    } finally {
      clock.close();
    }
  }

  /**
   * Tests setting and getting the exposure time
   */
  @Test
  public void testExposureTime() {
    List<String> names = GpsClock.getAllDeviceNames();

    assertNotNull(names);
    assertTrue(names.size() > 0);
    String name = names.get(0);

    GpsClock clock = new GpsClock();

    try {
      clock.open(name);

      int exposureTime = 10;
      clock.setExposureTime(exposureTime);

      // Ensure an exposure time is set and give the clock time to align with
      // the next 10-second boundary.
      long endTime = System.currentTimeMillis() + 10000;

      while (System.currentTimeMillis() < endTime) {
        try {
          Thread.sleep(100);
        } catch(InterruptedException ex) {
          // Ignore interruptions.
        }
      }

      int exposureTime2 = clock.getExposureTime();
      Assert.assertEquals(exposureTime, exposureTime2);

      // Try setting another exposure time.
      exposureTime = 5;
      clock.setExposureTime(exposureTime);

      // Ensure an exposure time is set and give the clock time to align with
      // the next 10-second boundary.
      endTime = System.currentTimeMillis() + 10000;

      while (System.currentTimeMillis() < endTime) {
        try {
          Thread.sleep(100);
        } catch(InterruptedException ex) {
          // Ignore interruptions.
        }
      }

      int exposureTime3 = clock.getExposureTime();
      Assert.assertEquals(exposureTime, exposureTime3);
    } finally {
      clock.close();
    }
  }

  /**
   * Tests getting the time of the last sync/exposure pulse (since we set the
   * exposure time in a previous test this should be safe to do).
   */
  @Test
  public void testGetLastSyncPulseTime() {
    List<String> names = GpsClock.getAllDeviceNames();

    assertNotNull(names);
    assertTrue(names.size() > 0);
    String name = names.get(0);

    GpsClock clock = new GpsClock();

    try {
      clock.open(name);

      // Ensure an exposure time is set and give the clock time to align with
      // the next 10-second boundary.
      int exposureTime = 5;
      clock.setExposureTime(exposureTime);

      // Wait for the exposure time to count down.
      long waitEndTime = System.currentTimeMillis() + 14000 + exposureTime;

      while (System.currentTimeMillis() < waitEndTime) {
        try {
          Thread.sleep(100);
        } catch(InterruptedException ex) {
          // Ignore interruptions.
        }
      }

      Date startTime = new Date();
      Date lastSyncTime = clock.getLastSyncPulseTime();
      Date endTime = new Date();

      assertNotNull(lastSyncTime);


      long timeDifference = startTime.getTime() - lastSyncTime.getTime();
      System.out.println("Clock start time     = " + dateFormatter.format(startTime));
      System.out.println("Sync time            = " + dateFormatter.format(lastSyncTime));
      System.out.println("Clock end time       = " + dateFormatter.format(endTime));
      System.out.println("Sync time difference = " + timeDifference);

      long exposureTimeMillis = clock.getExposureTime()*1000;
      assertTrue(timeDifference < exposureTimeMillis);
    } finally {
      clock.close();
    }
  }

  /**
   * Tests getting the time of the last GPS time received.
   */
  @Test
  public void testGetLastGpsPulseTime() {
    List<String> names = GpsClock.getAllDeviceNames();

    assertNotNull(names);
    assertTrue(names.size() > 0);
    String name = names.get(0);

    GpsClock clock = new GpsClock();

    try {
      clock.open(name);

      Date startTime = new Date();
      Date lastGpsTime = clock.getLastGpsPulseTime();

      Date endTime = new Date();

      assertNotNull(lastGpsTime);

      long timeDifference = startTime.getTime() - lastGpsTime.getTime();
      System.out.println("Clock start time    = " + dateFormatter.format(startTime));
      System.out.println("GPS time            = " + dateFormatter.format(lastGpsTime));
      System.out.println("Clock end time      = " + dateFormatter.format(endTime));
      System.out.println("Gps time difference = " + timeDifference);
      
      assertTrue(timeDifference < 1100);

    } finally {
      clock.close();
    }
  }
}