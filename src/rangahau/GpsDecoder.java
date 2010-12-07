/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

package rangahau;

import java.io.FileWriter;
import java.io.Writer;
import java.util.Calendar;
import java.util.TimeZone;

/**
 *
 * @author sullivan
 */
public class GpsDecoder {
    /**
     * Represents the UTC (Universal Coordinated Time) timezone.
     */
    private TimeZone utcTimeZone = null;
    
    Writer writer = null; // DEBUG
    Writer rawBytesWriter = null; // DEBUG
    
    public GpsDecoder() {
        utcTimeZone = TimeZone.getTimeZone("UTC");

        if (rawBytesWriter == null) {
            try {
                rawBytesWriter = new FileWriter("rawgpsdata.txt");
            } catch(Throwable th) {
                throw new RuntimeException("Cannot open raw GPS bytes log rawgpsdata.txt for writing.");
            }
        }

        if (writer == null) {
            try {
                writer = new FileWriter("gpsdata.txt");
            } catch(Throwable th) {
                throw new RuntimeException("Cannot open GPS bytes log gpsdata.txt for writing.");
            }
        }
    }
    
    /**
     * Reads the GPS timestamp from the serial port.
     * 
     * @return the GPS timestamp, as milliseconds since the Epoch.
     */
    public long readGpsTimestamp(SerialPort serialPort) {
        int[] buffer = new int[4800];
        int destIndex = 0;
        
        final int delimiterCode = 0x10;  // Indicates the start of a Trimble packet.
        final int endPacketCode = 0x03;  // Indicates the end of a Trimble packet.
        boolean foundStart = false;      // True if we found where the packet starts.
        boolean foundEnd = false;        // True if we found where the packet ends.
        int packetStartIndex = 0;        // Where the packet starts.
        int packetEndIndex = 0;          // Where the packet ends.
        int payloadStartIndex = 0;       // Where the message payload starts.
        int numDelimiters = 0;           // How many delimiters seen after the start of a message.
        
        long startTime = System.currentTimeMillis();
        long endTime = startTime + 1000; // We only have 1 second (1000 ms) to read the timestamp before the next one arrives.
        boolean done = false; // true if we have read the GPS timestamp.
        
        long timestamp = 0L; // The GPS time in milliseconds since the Epoch.
                
        while(!done && (System.currentTimeMillis() < endTime) && (destIndex < buffer.length) )  {
            while(serialPort.bytesAvailable() > 0) {
                int value = serialPort.readByte();
                buffer[destIndex] = value;
                
                try {
                    rawBytesWriter.append("0x").append(Integer.toHexString(value & 0xFF)).append(" ");
                } catch(Throwable th) {
                    throw new RuntimeException("Cannot write byte to the raw GPS log file rawgpsdata.txt.");
                }
                
                // Remember if we have seen the delimiter byte value within
                // a packet.
                if (foundStart && (value == delimiterCode)) {
                  ++numDelimiters;    
                } else {
                    // If we get any other value except for the end packet 
                    // marker we should clear the number of delimiters received.
                    if (value != endPacketCode) {
                        numDelimiters = 0;
                    }
                }
                
                // Look for the start of a packet if we haven't found one
                // already. It will be the first byte that contains the start
                // delimiter byte value.
                if (!foundStart && (value == delimiterCode) ) {
                    foundStart = true;
                    packetStartIndex = destIndex;
                    
                    // The message payload starts two bytes after the message start
                    // since this is the message start delimiter followed by the
                    // message type byte.
                    payloadStartIndex = packetStartIndex+2;
                    
                    numDelimiters = 0;
                }
                
                // See if we have found the end of the message. The end is
                // identified by the end packet value which has been immediately
                // preceded by an odd number of delimiter bytes.
                if (foundStart && (value == endPacketCode) ) {
                    // If there are an odd number of delimiters immediately
                    // before this endPacketByte then we have found the end
                    // of the packet.
                    if ( (numDelimiters > 0) && (numDelimiters % 2 ==1) ) {
                        foundEnd = true;
                        packetEndIndex = destIndex;
                    } else {
                        // It isn't the end of the packet, so clear the number
                        // of delimiters seen so far.
                        numDelimiters = 0;
                    }
                }
                                
                ++destIndex;
                
                // If we found the end of a packet we should get the message
                // from the buffer and send it off for parsing. Then we clear
                // the contents of the buffer so we can accumulate a new packet.
                if (foundEnd) {
                    int numBytes = packetEndIndex - packetStartIndex +1;
                    int[] message =  new int[numBytes];
                    for (int index = packetStartIndex; index <= packetEndIndex; ++index) {
                        message[index - packetStartIndex] = buffer[index];
                    }
                    // Indicate that new messages should accumulate at the start
                    // of the buffer.
                    destIndex = 0;
                    foundStart = false;
                    foundEnd = false;
                    packetStartIndex = 0;
                    packetEndIndex = 0;
                    
                    // Send the message for parsing.
                    timestamp = parseMessage(message);
                    if (timestamp != 0) {
                    
                        // DEBUG
                        try {
                            rawBytesWriter.append("\nGot GPS timestamp of " + timestamp + "\n");
                        } catch(Throwable th) {
                            throw new RuntimeException("Cannot write timestamp info to the raw GPS log file rawgpsdata.txt.");
                        }
                                        
                        // Were able to parse a timestamp.
                        return timestamp;
                    }
                } 
            }

            // No bytes are available for reading so sleep for a short time.
            try {
              Thread.sleep(1);
            } catch(InterruptedException ex) {
              // Ignore interruptions.
            }
        }
        
        return timestamp;
    }
    
    /**
     * Parse the timing message received from the GPS.
     * 
     * @param message contains the bytes of the message (in the least significant bytes of each int).
     * 
     * @return the GPS timestamp, as milliseconds since the Epoch.
     */
    public long parseMessage(int[] message) {
        if (message == null) {
            System.out.println("The message is null.");
        }
        int numBytes = message.length;
        
        // Dump the byte values out.
        //for (int index = 0; index < numBytes; ++index) {
        //    System.out.println("message[" + index + "] = 0x" + Integer.toHexString(message[index]));
        //}
                
        final int timingPacketCode = 0x8F;                 // Indicates a timing packet has been received.
        final int primaryTimePacketSubcode = 0xAB;         // Indicates the timing packet is a primary timing packet.
        final int supplementaryTimingPacketSubcode = 0xAC; // Indicates the timing packet is a supplementary timing packet. 

        
        // If the packet is not a timing packet then we ignore it.
        int packetType = message[1];
        if (packetType != timingPacketCode) {
            System.out.println("Ignoring non-timing packet that has a type of 0x" + Integer.toHexString(packetType));
            return 0;
        }

        // If the timing packet is not a primary timing packet then we 
        // ignore it.
        int packetSubtype = message[2];
        if (packetSubtype != primaryTimePacketSubcode) {
            //System.out.println("Ignoring non-primary timing packet that has a type of 0x" + Integer.toHexString(packetSubtype));            
            return 0;
        }
        
        // Ensure the primary timing packet is large enough (note the length is
        // a minimum, but the packet may be larger than this due to 'byte stuffing'
        // escaping occurences of the delimiter value within data bytes).
        int minimumPrimaryTimingPacketSize = 21;
        if (message.length < minimumPrimaryTimingPacketSize) {
            throw new IllegalArgumentException("Primary timing packets must be at least " 
                                               + minimumPrimaryTimingPacketSize
                                               + " bytes in size but the packet given was "
                                               + message.length + " bytes.");
        }
        
        // Unpack the fields of the primary timing packet.
        int delimiterOffset = 2; // two bytes are used for the packet start delimiter and packet type.
        int year = (message[15+delimiterOffset]*256)+message[16+delimiterOffset];
        int month = message[14+delimiterOffset];
        int dayOfMonth = message[13+delimiterOffset];
        int hour = message[12+delimiterOffset];
        int minute = message[11+delimiterOffset];
        int seconds = message[10+delimiterOffset];
        
        int utcOffsetSeconds = (message[7+delimiterOffset]*256)+message[8+delimiterOffset];
        int timingFlags = message[9+delimiterOffset];
        
        Calendar calendar = Calendar.getInstance();
        calendar.setTimeZone(utcTimeZone);
        calendar.set(Calendar.YEAR, year);
        calendar.set(Calendar.MONTH, month -1); // Note: January is month 0 for Calendar.
        calendar.set(Calendar.DAY_OF_MONTH, dayOfMonth);
        calendar.set(Calendar.HOUR_OF_DAY, hour);
        calendar.set(Calendar.MINUTE, minute);
        calendar.set(Calendar.SECOND, seconds);
        calendar.set(Calendar.MILLISECOND, 0);
        
        // If bit 0 is not set on the timingFlags then the time is in GPS
        // time and we need to add the utcOffsetSeconds to the date to convert
     // into UTC.
        String timeType = "UTC";
        if ( (timingFlags & 0x01) == 0) {
            timeType = "GPS, UTC offset = " + utcOffsetSeconds + " seconds";
            calendar.add(Calendar.SECOND, utcOffsetSeconds);
        }        
        
//        System.out.println("GPS sourced time is: " + year  + "-" + month + "-" + dayOfMonth
//                           + " " + hour + ":" + minute + ":" + seconds + " " + timeType);
        
        if ( (year != 2008) && (year != 2009) ) {
            if (writer == null) {
                try {
                    writer = new FileWriter("gpsdata.txt");
                } catch(Throwable th) {
                    throw new RuntimeException("Cannot open GPS log gpsdata.txt for writing.");
                }
            }

            try {
                writer.append("*** GPS sourced time is: " + year  + "-" + month + "-" + dayOfMonth
                               + " " + hour + ":" + minute + ":" + seconds + " " + timeType);

                for (int i = 0; i > message.length; ++i) {
                    writer.append("0x").append(Integer.toHexString(message[i])).append(" ");
                }
                writer.append("\n");
            } catch(Throwable th) {
                    throw new RuntimeException("Cannot write to the GPS log file gpsdata.txt.");
            }
        }
        
        return calendar.getTimeInMillis();
     }
    
    /**
     * Returns an array with byte values (stored in the lowest byte of each integer)
     * that comprise a Trimble GPS primary timing packet. Refer to the  
     * Trimble Standard Interface Protocol for ThunderBolt Section A.10.30 
     * Report Packet 0x8F-AB Primary Timing Packet (pages A-56 to A-58/pages 
     * 112-114) for the definition of the primary timing packet, and Section A.8
     * Packet Structure (page A-9 to A-10/pages 65-66) for more information.
     * 
     * @return an array with the bytes of a sample Trimble Primary Timing Packet.
     */
    public int[] getPrimaryTimingPacketTestData() {
        int[] data = new int[21];
        
        data[0] = 0x10;   // Delimiter
        data[1] = 0x8f;   // Packet Type
        data[2] = 0xab;   // Packet Subtype
        data[3] = 0x00;   // GPS seconds of week (byte 3, Most Significant Byte).
        data[4] = 0x00;   // GPS seconds of week (byte 2).
        data[5] = 0x27;   // GPS seconds of week (byte 1).
        data[6] = 0xad;   // GPS seconds of week (byte 0, least significant Byte).
        data[7] = 0x05;   // GPS Week Number (byte 1, Most Significant Byte).
        data[8] = 0xc2;   // GPS Week Number (byte 0, Least Significant Byte).
        data[9] = 0x00;   // UTC Offset (seconds, byte 1, Most Significant Byte). 
        data[10] = 0x0e;  // UTC Offset (seconds, byte 0, Least Significant Byte). 
        data[11] = 0x03;  // Timing Flags (Indicates UTC or GPS timing).
        data[12] = 0x03;  // Seconds (may be 60 is a leap second event is in progress).
        data[13] = 0x31;  // Minutes of hour.
        //data[14] = 0x02;  // Hour of day.
        data[14] = 0x10;  // Hour of day.
        data[15] = 0x06;  // Day of Month.
        data[16] = 0x04;  // Month of year.
        data[17] = 0x07;  // 4-digit year [eg. 2008] (byte 1, Most Significant Byte).
        data[18] = 0xd8;  // 4-digit year [eg. 2008] (byte 0, Least Significant Byte).
        data[19] = 0x10;  // Delimiter
        data[20] = 0x03;  // Packet end marker (end transmission marker).
        
        return data;
    }
    
    /**
     * Returns an array with byte values (stored in the lowest byte of each integer)
     * that comprise a Trimble GPS primary timing packet. Refer to the  
     * Trimble Standard Interface Protocol for ThunderBolt Section A.10.30 
     * Report Packet 0x8F-AC Supplementary Timing Packet (pages A-59 to A-63/pages 
     * 115-119) for the definition of the primary timing packet, and Section A.8
     * Packet Structure (page A-9 to A-10/pages 65-66) for more information.
     * 
     * @return an array with the bytes of a sample Trimble Primary Timing Packet.
     */    
    public int[] getSupplementaryTimingPacketTestData() {
        int[] data = new int[71];
        
        data[0] = 0x10;   // Delimiter
        data[1] = 0x8f;   // Packet Type
        data[2] = 0xac;   // Packet Subtype        
        data[3] = 0x07;   // Receiver Mode flags.
        data[4] = 0x02;   // Disciplinig Mode flags.
        data[5] = 0x64;   // Self-Survey Progress (0-100%). 
        data[6] = 0x00;   // Holdover duration (seconds, byte 3, Most Significant Byte). 
        data[7] = 0x00;   // Holdover duration (seconds, byte 2).   
        data[8] = 0x03;   // Holdover duration (seconds, byte 1).
        data[9] = 0xbd;   // Holdover duration (seconds, byte 0, Least Significant Byte).
        
        data[10] = 0x00;  // Critical Alarms Flags (byte 1, Most Significant Byte).
        data[11] = 0x00;  // Critical Alarms Flags (byte 0, Least Significant Byte).
        data[12] = 0x00;  // Minor Alarms Flags (byte 1, Most Significant Byte).    
        data[13] = 0x58;  // Minor Alarms Flags (byte 0, Least Significant Byte).
        data[14] = 0x08;  // GPS Decoding Status flags.
        data[15] = 0x06;  // Disciplining Activity Flags.
        data[16] = 0x00;  // Spare Status 1.
        data[17] = 0x00;  // Spare Status 2.
        data[18] = 0x3e;  // Pulse-per-Second Offset (ns) (Float-32 byte 3, Most Significant Byte).
        data[19] = 0xdb;  // Pulse-per-Second Offset (ns) (Float-32 byte 2).
        
        data[20] = 0x2c;  // Pulse-per-Second Offset (ns) (Float-32 byte 1).
        data[21] = 0xc0;  // Pulse-per-Second Offset (ns) (Float-32 byte 0, Least Significant Byte).
        data[22] = 0x3c;  // 10 MHz Offset (ppb) (Float-32 byte 3, Most Significant Byte).      
        data[23] = 0x27;  // 10 MHz Offset (ppb) (Float-32 byte 2).
        data[24] = 0xc1;  // 10 MHz Offset (ppb) (Float-32 byte 1).
        data[25] = 0x7a;  // 10 MHz Offset (ppb) (Float-32 byte 0, Least Significant Byte).
        data[26] = 0x00;  // DAC Value (byte 3, Most Significant Byte).
        data[27] = 0x08;  // DAC Value (byte 2).      
        data[28] = 0x2a;  // DAC Value (byte 1).
        data[29] = 0x85;  // DAC Value (byte 0, Least Significant Byte).
        
        data[30] = 0x3d;  // DAC Voltage (Volts, Float-32 byte 3, Most Significant Byte).
        data[31] = 0xd4;  // DAC Voltage (Volts, Float-32 byte 2).
        data[32] = 0x99;  // DAC Voltage (Volts, Float-32 byte 1).      
        data[33] = 0x00;  // DAC Voltage (Volts, Float-32 byte 0, Least Significant Byte).
        data[34] = 0x42;  // Temperature (Centigrade,  Float-32 byte 3, Most Significant Byte).
        data[35] = 0x0e;  // Temperature (Centigrade,  Float-32 byte 2).
        data[36] = 0xff;  // Temperature (Centigrade,  Float-32 byte 1).
        data[37] = 0xfb;  // Temperature (Centigrade,  Float-32 byte 0, Least Significant Byte).       
        data[38] = 0xbf;  // Latitude (radians, Double-64 byte 7, Most Significant Byte).
        data[39] = 0xe8;  // Latitude (radians, Double-64 byte 6).
        
        data[40] = 0x91;  // Latitude (radians, Double-64 byte 5).
        data[41] = 0x2a;  // Latitude (radians, Double-64 byte 4).
        data[42] = 0x00;  // Latitude (radians, Double-64 byte 3).       
        data[43] = 0x22;  // Latitude (radians, Double-64 byte 2).
        data[44] = 0xeb;  // Latitude (radians, Double-64 byte 1).
        data[45] = 0x4e;  // Latitude (radians, Double-64 byte 0, Least Significant Byte).
        data[46] = 0x40;  // Longitude (radians, Double-64 byte 7, Most Significant Byte).
        data[47] = 0x07;  // Longitude (radians, Double-64 byte 6).       
        data[48] = 0xcd;  // Longitude (radians, Double-64 byte 5).
        data[49] = 0x1f;  // Longitude (radians, Double-64 byte 4).
        
        data[50] = 0x42;  // Longitude (radians, Double-64 byte 3).
        data[51] = 0xf9;  // Longitude (radians, Double-64 byte 2).
        data[52] = 0x92;  // Longitude (radians, Double-64 byte 1).       
        data[53] = 0xac;  // Longitude (radians, Double-64 byte 0, Least Significant Byte).
        data[54] = 0x40;  // Altitude (meters, Double-64 byte 7, Most Significant Byte).
        data[55] = 0x8f;  // Altitude (meters, Double-64 byte 6).
        data[56] = 0xf2;  // Altitude (meters, Double-64 byte 5).
        data[57] = 0xc1;  // Altitude (meters, Double-64 byte 4).        
        data[58] = 0xb9;  // Altitude (meters, Double-64 byte 3).
        data[59] = 0xcf;  // Altitude (meters, Double-64 byte 2).
        
        data[60] = 0x00;  // Altitude (meters, Double-64 byte 1).
        data[61] = 0x00;  // Altitude (meters, Double-64 byte 0, Least Significant Byte).
        data[62] = 0x00;  // Spare (for future expansion, byte 7)      
        data[63] = 0x00;  // Spare (for future expansion, byte 6) 
        data[64] = 0x00;  // Spare (for future expansion, byte 5) 
        data[65] = 0x00;  // Spare (for future expansion, byte 4) 
        data[66] = 0x00;  // Spare (for future expansion, byte 3) 
        data[67] = 0x00;  // Spare (for future expansion, byte 2)        
        data[68] = 0x00;  // Spare (for future expansion, byte 1) 
        data[69] = 0x01;  // Spare (for future expansion, byte 0) 

        data[70] = 0x10;  // Delimiter
        data[71] = 0x03;  // Packe end marker (end transmission marker).        
        
        return data;
    }

    /**
     * Returns an array with byte values (stored in the lowest byte of each integer)
     * that comprise a Trimble GPS primary timing packet. Refer to the
     * Trimble Standard Interface Protocol for ThunderBolt Section A.10.30
     * Report Packet 0x8F-AB Primary Timing Packet (pages A-56 to A-58/pages
     * 112-114) for the definition of the primary timing packet, and Section A.8
     * Packet Structure (page A-9 to A-10/pages 65-66) for more information.
     *
     * @return an array with the bytes of a sample Trimble Primary Timing Packet.
     */
    public int[] getPrimaryTimingPacketTestData(int year, int month, int day,
                                                int hour, int minute, int second) {
        int[] data = new int[128];

        int element = 0;

        data[element] = 0x10;   // Delimiter
        ++element;
        data[element] = 0x8f;   // Packet Type
        ++element;

        data[element] = 0xab;   // Packet Subtype
        ++element;

        data[element] = 0x00;   // GPS seconds of week (byte 3, Most Significant Byte).
        ++element;
        data[element] = 0x00;   // GPS seconds of week (byte 2).
        ++element;
        data[element] = 0x27;   // GPS seconds of week (byte 1).
        ++element;
        data[element] = 0xad;   // GPS seconds of week (byte 0, least significant Byte).
        ++element;

        data[element] = 0x05;   // GPS Week Number (byte 1, Most Significant Byte).
        ++element;
        data[element] = 0xc2;   // GPS Week Number (byte 0, Least Significant Byte).
        ++element;

        data[element] = 0x00;   // UTC Offset (seconds, byte 1, Most Significant Byte).
        ++element;
        data[element] = 0x0e;  // UTC Offset (seconds, byte 0, Least Significant Byte).
        ++element;


        data[element] = 0x03;  // Timing Flags (Indicates UTC or GPS timing).
        ++element;
        //if () {
          data[element] = 0x03;  // Duplicate packet to escape it.
          ++element;
        //}

        data[element] = second;  // Seconds (may be 60 is a leap second event is in progress).
        ++element;
        if (second == 3) {
          data[element] = second;  // Duplicate packet to escape it.
          ++element;
        }

        data[element] = minute;  // Minutes of hour.
        ++element;
        if (minute == 3) {
          data[element] = minute; // Duplicate packet to escape it.
          ++element;
        }

        data[element] = hour;  // Hour of day.
        ++element;
        if (hour == 3) {
          data[element] = hour;  // Duplicate packet to escape it.
          ++element;
        }

        data[element] = day;  // Day of Month.
        ++element;
        if (day == 3) {
          data[element] = day;  // Duplicate packet to escape it.
          ++element;
        }

        data[element] = month;  // Month of year.
        ++element;
        if (month == 3) {
          data[element] = month;  // Duplicate packet to escape it.
          ++element;
        }

        data[element] = (year / 256);  // 4-digit year [eg. 2008] (byte 1, Most Significant Byte).
        ++element;
        data[element] = (year % 256);  // 4-digit year [eg. 2008] (byte 0, Least Significant Byte).
        ++element;

        data[element] = 0x10;  // Delimiter
        ++element;
        data[element] = 0x03;  // Packet end marker (end transmission marker).
        ++element;

        int[] outputData = new int[element];
        System.arraycopy(data, 0, outputData, 0, element);

        return outputData;
    }

}
