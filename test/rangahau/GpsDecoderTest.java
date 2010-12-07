/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

package rangahau;

import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;
import java.util.TimeZone;
import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import static org.junit.Assert.*;

/**
 * Unit tests for the GpsDecoder class.
 * 
 * @author Michael Reid
 */
public class GpsDecoderTest {

    public GpsDecoderTest() {
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
     * Test of readGpsTimestamp method, of class GpsDecoder.
     */
    @Test
    public void testReadGpsTimestamp() {
//        System.out.println("readGpsTimestamp");
//        SerialPort serialPort = null;
//        GpsDecoder instance = new GpsDecoder();
//        long expResult = 0L;
//        long result = instance.readGpsTimestamp(serialPort);
//        assertEquals(expResult, result);
//        // TODO review the generated test code and remove the default call to fail.
//        fail("The test case is a prototype.");
    }

    /**
     * Test of parseMessage method, of class GpsDecoder.
     */
    @Test
    public void testParseMessage() {
        GpsDecoder decoder = new GpsDecoder();
        int[] message = decoder.getPrimaryTimingPacketTestData();

        long result = decoder.parseMessage(message);
        //long expectedResult = 1207450143000L;
        long expectedResult = 1207500543000L;
        
        Calendar calendar = Calendar.getInstance();
        calendar.setTimeInMillis(result);
        
        SimpleDateFormat formatter = new SimpleDateFormat("yyyy-MM-dd hh:mm:ss");
        
        String time = formatter.format(calendar.getTime());
        System.out.println("decoded time = " + result + ", expected " + expectedResult 
                           + ", difference = " + (result - expectedResult));
        assertEquals(expectedResult, result);
    }

    /**
     * Test of parseMessage method for each second of the day.
     */
    @Test
    public void testParseMessageForDay() {
        GpsDecoder decoder = new GpsDecoder();

        int year = 2009;
        int month = 5;
        int day = 30;

        int hour = 0;
        int minute = 0;
        int second = 0;

        final int secondsPerMinute = 60;
        final int minutesPerHour = 60;
        final int hoursPerDay = 24;
        final int secondsPerDay = secondsPerMinute*minutesPerHour*hoursPerDay;

        TimeZone utcTimezone = TimeZone.getTimeZone("UTC");
        SimpleDateFormat formatter = new SimpleDateFormat("yyyy-MM-dd hh:mm:ss");
        formatter.setTimeZone(utcTimezone);

        boolean ok = true;

        // Encode times for all seconds of the day.
//        for (int secondInDay = 0; secondInDay < secondsPerDay; ++secondInDay) {
//          second = secondInDay % 60;
//          minute = (secondInDay / 60) % 60;
//          hour = secondInDay / (secondsPerMinute*minutesPerHour);
//
//          int[] encodedMessage = decoder.getPrimaryTimingPacketTestData(year, month, day, hour, minute, second);
//          long result = decoder.parseMessage(encodedMessage);
//
//          Calendar calendar = Calendar.getInstance();
//          calendar.setTimeInMillis(result);
//
//          if ( (year != calendar.get(Calendar.YEAR))
//               || (month != (calendar.get(Calendar.MONTH) +1))
//               || (day != calendar.get(Calendar.DAY_OF_MONTH))
//               || (hour != calendar.get(Calendar.HOUR_OF_DAY))
//               || (minute != calendar.get(Calendar.MINUTE))
//               || (second != calendar.get(Calendar.SECOND)) ) {
//            System.out.println("Time difference :  source time = "
//                    + year + "-" + month + "-" + day + " " + hour + ":"
//                    + minute + ":" + second + ", parsed time = "
//                    + formatter.format(new Date(result)));
//            ok = false;
//          }
//        }
//
//        Assert.assertTrue(ok);
    }
}