/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

package rangahau.plot;

import java.awt.AlphaComposite;
import java.awt.BasicStroke;
import java.awt.Color;
import java.awt.Font;
import java.awt.FontMetrics;
import java.awt.Graphics2D;
import java.awt.GradientPaint;
import java.awt.Paint;
import java.awt.Shape;
import java.awt.Stroke;
import java.awt.geom.Rectangle2D;
import java.awt.RenderingHints;
import java.awt.image.BufferedImage;

import java.awt.geom.AffineTransform;

import java.text.SimpleDateFormat;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;
import java.util.Comparator;
import java.util.Date;
import java.util.GregorianCalendar;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

/**
 * The ScatterPlot class produces images which contain x-y plots (ie. graphs).
 */
public class ScatterPlot
{
    /**
     * The area occupied by the associated plot.
     */
    private Rectangle2D plotArea;
       
    /**
     * The span of the xAxis.
     */
    double xAxisSpan;
      
    /**
     * The span of the date axis (in units of milliseconds).
     */
    double dateAxisSpan;
    
    /**
     * The start of the date axis (in milliseconds since the Epoch [1 Jan 1970]).
     */
    double dateAxisSpanOffset;

    /**
     * The clipping endpoint (maximum date, in milliseconds since the Epoch) along the date axis.
     * Used so that even if tick marks past the end of the month are shown on the graph then
     * data points past the end of the month won't be shown.
     */
    double dateAxisClippingEndpoint;
    
    /**
     * The span of the route displacement axis (in units of km).
     */
    double displacementAxisSpan;
    
    /**
     * The start of the displacement axis (in km).
     */
    double displacementAxisSpanOffset;
    

    /**
     * The span of the year axis of the traffic annual plot (in milliseconds).
     */
    double annualAxisSpan;
    
    /**
     * The lowest date of the year axis of the traffic annual plot (in milliseconds since 1 Jan 1970).
     */
    double annualAxisSpanOffset;
    
      
    /**
     * Creates a TrafficPlot object.
     */
    public ScatterPlot()
    {
        
    }
    
//    /**
//     * Produces an image which shows the yearly traffic flow. The traffic
//     * data is obtained from a List of TrafficData objects.
//     *
//     * @param data - the TrafficData objects which are to be plotted.
//     * @param width - the width of the plot image to be produced (in pixels).
//     * @param height - the height of the plot image to be produced (in pixels).
//     * @param averageTrafficFlow - the average daily traffic flow, to be drawn as a line on the graph.
//     * @param month - the month to plot: 0 = all year (12 months), 1 = January only, 2 = February only, etc.
//     *
//     * @return an image containing a plot of the traffic flow data.
//     *
//     * @throws IllegalArgumentException - if data is null or contains no elements.
//     * @throws IllegalArgumentException - if width is zero or less.
//     * @throws IllegalArgumentException - if height is zero or less.
//     * @throws IllegalArgumentException - if averageTrafficFlow is less than zero.
//     * @throws IllegalArgumentException - if month is less than zero or greater than 12.
//     */
//    public BufferedImage getTrafficFlowPlot(List data, int width, int height, double averageTrafficFlow, int month) throws IllegalArgumentException
//    {
//       if (data == null)
//       {
//         throw new IllegalArgumentException("Cannot create a traffic flow plot as the list of traffic data in null.");
//       }
//
//       if (data.size() < 1)
//       {
//         throw new IllegalArgumentException("Cannot create a traffic flow plot as the list of traffic data has no observations.");
//       }
//
//       if (width < 1)
//       {
//           throw new IllegalArgumentException("Cannot create a  traffic flow plot as the width of the ouput image must be greater than zero but was specified as " + width);
//       }
//
//       if (height < 1)
//       {
//           throw new IllegalArgumentException("Cannot create a traffic flow plot as the height of the ouput image must be greater than zero but was specified as " + height);
//       }
//
//       if (averageTrafficFlow < 0)
//       {
//           throw new IllegalArgumentException("Cannot create a traffic flow plot as the average daily traffic flow must be zero or greater but was specified as " + averageTrafficFlow);
//       }
//
//       if ( (month < 0) || (month > 12) ) {
//            throw new IllegalArgumentException("Canno create a traffic flow plot as valid month values are between 0 and 12 but the month given was " + month);
//        }
//
//       BufferedImage image = new BufferedImage(width, height, BufferedImage.TYPE_3BYTE_BGR);
//
//       computePlotArea(image);
//
//       Graphics2D gc = (Graphics2D) image.getGraphics();
//
//       gc.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
//       gc.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
//       gc.setRenderingHint(RenderingHints.KEY_RENDERING, RenderingHints.VALUE_RENDER_SPEED);
//       //gc.setRenderingHint(RenderingHints.KEY_RENDERING, RenderingHints.VALUE_RENDER_QUALITY);
//
//       drawBackground(image);
//       drawPlotBackground(gc);
//       drawAxes(gc, data, month);
//
//       gc.setColor(Color.BLUE);
//       drawTraces(gc, data);
//
//       // Draw the daily average total line.
//       gc.setColor(Color.RED);
//       double y = trafficAxisPosition(averageTrafficFlow);
//       gc.drawLine((int) (plotArea.getMinX() -1), (int) y, (int) (plotArea.getMaxX() +1), (int) y);
//
//       return image;
//    }
//
//    /**
//     * Compute the space available for drawing the graph given space needs
//     * to be reserved for the axis, labels, and legend.
//     *
//     * @param image - the image that the plot will be drawn on.
//     *
//     * @throws IllegalArgumentException - if image is null.
//     */
//    public void computePlotArea(BufferedImage image) throws IllegalArgumentException
//    {
//
//        if (image == null)
//        {
//            throw new IllegalArgumentException("Cannot compute the plot area as the image in null.");
//        }
//
//        double topInset = 30;
//        double bottomInset = 75;
//        double leftInset = 125;
//        double rightInset = 65;
//
//        plotArea = new Rectangle2D.Double(leftInset, topInset, image.getWidth() - leftInset - rightInset, image.getHeight() - topInset - bottomInset);
//    }
//
//        /**
//     * Compute the space available for drawing the graph given space needs
//     * to be reserved for the axis, labels, and legend.
//     *
//     * @param image - the image that the plot will be drawn on.
//     *
//     * @throws IllegalArgumentException - if image is null.
//     */
//    public void computeTrafficRoutePlotArea(BufferedImage image) throws IllegalArgumentException
//    {
//
//        if (image == null)
//        {
//            throw new IllegalArgumentException("Cannot compute the plot area as the image in null.");
//        }
//
//        double topInset = 60;
//        double bottomInset = 75;
//        double leftInset = 125;
//        double rightInset = 65;
//
//        plotArea = new Rectangle2D.Double(leftInset, topInset, image.getWidth() - leftInset - rightInset, image.getHeight() - topInset - bottomInset);
//    }
//
//    /**
//     * Draw the axes surrounding the plot.
//     *
//     * @param gc - the graphics context to draw on.
//     * @param data - a list of TrafficData objects to be plotted.
//     * @param month - the month to plot: 0 = all year (12 months), 1 = January only, 2 = February only, etc.
//     *
//     * @throws IllegalArgumentException - if gc is null.
//     * @throws IllegalArgumentException - if data is null.
//     * @throws IllegalArgumentException - if month is less than zero or greater than 12.
//     */
//    public void drawAxes(Graphics2D gc, List data, int month) throws IllegalArgumentException {
//
//        if (gc == null) {
//            throw new IllegalArgumentException("Cannot draw the plot axes as the graphics object to draw on is null.");
//        }
//
//        if (data == null) {
//            throw new IllegalArgumentException("Cannot draw the plot axes as the list of data to plot is null.");
//        }
//
//        if ( (month < 0) || (month > 12) ) {
//            throw new IllegalArgumentException("Cannot draw the plot axes as valid month values are between 0 and 12 but the month given was " + month);
//        }
//
//        // Draw the top axis. Note that this axis lies just outside the plotting area
//        // (hence the y value is plotArea.getMin() -1).
//        gc.setPaint(Color.BLACK);
//        gc.drawLine((int) plotArea.getMinX()-1, (int) plotArea.getMinY() -1, (int) plotArea.getMaxX()+1, (int) plotArea.getMinY()-1);
//
//        // Draw the right axis. Note that this axis lies just outside the plotting area
//        // (hence the x value is plotArea.getMax() +1)
//        gc.drawLine((int) plotArea.getMaxX()+1, (int) plotArea.getMinY() -1, (int) plotArea.getMaxX()+1, (int) plotArea.getMaxY()-1);
//
//        // Draw the Traffic Flow axis (left vertical axis).
//        drawTrafficFlowAxis(gc, data);
//
//        // Now draw the date axis.
//        Date oldest = findMinimumDate(data);
//        final int year = oldest.getYear() + 1900;
//
//        if (month == 0) {
//            // Draw an axis for an entire year.
//            drawYearAxis(gc, data, year);
//        } else {
//            // Draw an axis for a single month.
//            drawMonthAxis(gc, data, year, month);
//        }
//    }
//
//    /**
//     * Draws and marks the traffic flow axis (left vertical axis) of the graph.
//     *
//     * @param gc - the graphics context to draw on.
//     * @param data - a list of TrafficData objects to be plotted.
//     */
//    public void drawTrafficFlowAxis(Graphics2D gc, List data) {
//        // Draw the left (traffic flow) axis.
//        // Determine the maximum number of labels that will fit within the axis size.
//        FontMetrics metrics = gc.getFontMetrics(gc.getFont());
//        double minimumLabelSpacing = 1.5;      // minimum spacing between labels as fraction of label height.
//        int labelHeight = (int) (metrics.getHeight() * minimumLabelSpacing);
//
//        // Determine how many tick marks (with their associated labels) will fit
//        // in the axis length.
//        final double xAxisLength = plotArea.getMaxX() - plotArea.getMinX() +1;
//        final double yAxisLength = plotArea.getMaxY() - plotArea.getMinY() +1;
//        int numLabelsInLength = (int) Math.floor(yAxisLength / labelHeight);
//
//        // We always need a minimum of two tick marks (and therefore two labels)
//        // at the beginning and end of the axis.
//        if (numLabelsInLength < 2) {
//            numLabelsInLength = 2;
//        }
//
//        // Find the smallest step interval which can fit the specified number
//        // of labels (at each tick mark) and still span the entire data range.
//        int[] multipliers = { 1, 2, 4, 5 };
//        boolean foundInterval = false; // true when have found a good interval to display.
//        double interval = 0;           // size of the traffic-flow interval between tick marks.
//
//        // Find the maximum and minimum traffic values.
//        int lowestTraffic = findMinimumTrafficFlow(data);
//        int highestTraffic = findMaximumTrafficFlow(data);
//        //final int trafficRange = highestTraffic - lowestTraffic +1;
//        final int trafficRange = highestTraffic; // make the range from 0 to highestTraffic
//
//        for (int order = 1; !foundInterval; order *= 10) {
//            for (int index = 0; !foundInterval && (index < multipliers.length); ++index) {
//                interval = multipliers[index]*order;
//
//                // If this interval can span the range of data then
//                // we have found the interval we wish to use.
//                if ( (interval*(numLabelsInLength -1)) >= trafficRange) {
//                    foundInterval = true;
//                }
//            }
//        }
//
//        // See how many intervals will actually be needed for this traffic range.
//        int numIntervals = (numLabelsInLength -1);
//
//        if (numIntervals < 1) {
//            numIntervals = 1;
//        }
//
//        for (int numTestIntervals = numIntervals; ((interval*numTestIntervals) >= trafficRange) && (numTestIntervals > 0); --numTestIntervals)  {
//            numIntervals = numTestIntervals;
//        }
//
//        int numTicks = numIntervals +1;
//
//
//
//        // Set the range of the traffic axis. Note that this is determined by the interval
//        // size and the number of intervals.
//        trafficFlowAxisSpan = interval*numIntervals;
//
//        double axisUpperLimit = trafficFlowAxisSpan;
//        if (axisUpperLimit < highestTraffic) {
//            // Moving the lower limit to an interval boundary may have moved the upper limit to below
//            // the value of the maximum observed data. We may need to add one more interval.
//            ++numIntervals;
//
//            numTicks = numIntervals +1;
//            trafficFlowAxisSpan = interval*numIntervals;
//        }
//
//        // Draw the axis line. Note that this line is just to the left of the plotting area
//        // (hence the x position is plotArea.getMinX() -1). Also, the axis extends vertically
//        // above and below the plotting area by one pixel (so join with other bordering axes
//        // around the plotting area).
//        gc.setPaint(Color.BLACK);
//        gc.drawLine((int) plotArea.getMinX()-1, (int) plotArea.getMinY() -1, (int) plotArea.getMinX() -1, (int) plotArea.getMaxY()+1);
//
//        double tickLength = 7; //xAxisLength / 50.0;
//        double labelSpace = tickLength; // space between tick and the associated label.
//
//        double xMinLabel = Double.MAX_VALUE;
//
//        for (int tick = 0; tick < numTicks; ++tick) {
//            double y = trafficAxisPosition(tick*interval);
//
//            gc.drawLine((int) plotArea.getMinX() -1, (int) y, (int) (plotArea.getMinX() - 1 - tickLength), (int) y);
//
//            // Draw the label.
//            int tickValue = (int) (tick*interval);
//            String label = Integer.toString(tickValue);
//            double labelWidth = metrics.getStringBounds(label, gc).getWidth();
//
//            double x = plotArea.getMinX() -1 - tickLength - labelSpace - labelWidth;
//            gc.drawString(label, (float) x, (float) (y + metrics.getHeight()/2.0 - metrics.getDescent()));
//
//            // Remember the minimum x value of all the tickmark labels.
//            if (x < xMinLabel) {
//                xMinLabel = x;
//            }
//        }
//
//        // Draw the label axis. The x position of the label will be half-way between the edge
//        // of the graph and the tickmark label closest to the left edge.
//        String axisLabel = "Vehicles per day";
//        double axisLabelLength = metrics.getStringBounds(axisLabel, gc).getWidth();
//
//
//        final AffineTransform saved = (AffineTransform) gc.getTransform().clone(); // save the current affine transform.
//        final int rotationAngle = -90;
//        AffineTransform transform = (AffineTransform) gc.getTransform().clone();
//
//        //double xLabel = xMinLabel - labelSpace - labelHeight;
//        double xLabel = xMinLabel/2.0; // - labelSpace - labelHeight;
//        double yLabel = (plotArea.getMaxY()+plotArea.getMinY())/2.0 + axisLabelLength/2.0;
//        transform.translate(xLabel, yLabel);
//        transform.rotate(Math.toRadians(rotationAngle));
//        gc.setTransform(transform);
//        gc.drawString(axisLabel, 0, 0);
//        gc.setTransform(saved); // restore the original affine transform.
//    }
//
//    /**
//     * Draws and marks the year axis (bottom horizontal axis) of the graph.
//     *
//     * @param gc - the graphics context to draw on.
//     * @param data - a list of TrafficData objects to be plotted.
//     * @param year - the year to be displayed (eg. 1975).
//     */
//    public void drawYearAxis(Graphics2D gc, List data, int year) {
//        // Draw the bottom (date) axis.
//
//        // Determine the maximum number of labels that will fit within the axis size.
//        FontMetrics metrics = gc.getFontMetrics(gc.getFont());
//        double minimumLabelSpacing = 1.5;      // minimum spacing between labels as fraction of label height.
//        int labelHeight = (int) (metrics.getHeight() * minimumLabelSpacing);
//
//        String[] monthInitials = { "J", "F", "M", "A", "M", "J", "J", "A", "S", "O", "N", "D" };
//        String[] monthShortNames = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
//        String[] monthNames = { "January", "February", "March", "April", "May", "June",
//                                "July", "August", "September", "October", "November", "December" };
//
//        // Compute the width of each of the three ways of showing the date.
//        float monthInitialsWidth = 0;
//        float monthShortNamesWidth = 0;
//        float monthNamesWidth = 0;
//
//        for (int month = 0; month < monthInitials.length; ++month) {
//            monthInitialsWidth += metrics.getStringBounds(monthInitials[month], gc).getWidth();
//            monthShortNamesWidth += metrics.getStringBounds(monthShortNames[month], gc).getWidth();
//            monthNamesWidth += metrics.getStringBounds(monthNames[month], gc).getWidth();
//        }
//
//        // We use one of the three ways of displaying month names by choosing the largest name type
//        // which still has sufficient space between the names.
//        final double spacingFactor = 1.5;
//        final double xAxisLength = plotArea.getMaxX() - plotArea.getMinX() +1;
//
//        String[] months = monthInitials;
//
//        if ( (spacingFactor*monthShortNamesWidth) < xAxisLength) {
//            months = monthShortNames;
//        }
//
//        if ( (spacingFactor*monthNamesWidth) < xAxisLength) {
//            months = monthNames;
//        }
//
//        // There will always be 13 major tick marks on this axis, which separate the months.
//        // Note that the tick marks will occur at uneven intervals as the number of days in
//        // each month is uneven.
//        int numTicks = 13;
//
//        GregorianCalendar yearStart = new GregorianCalendar(year, 0, 1, 0, 0); // The date corresponding to 00:00 on 1 January of the year being plotted.
//        int numDays = 365; // number of days in the year.
//
//        if (yearStart.isLeapYear(year)) {
//            numDays += 1;
//        }
//
//        // Draw the week boundaries in grey, only do this if the graph is over
//        // 200 pixels in size (otherwise everything gets too cramped).
//        GregorianCalendar date = new GregorianCalendar(year, 0, 1, 0, 0); // The date corresponding to 00:00 on 1 January of the year being plotted.
//
//        // Remember the span and offset of the axis (used when plotting the traces).
//        dateAxisSpanOffset = date.getTimeInMillis();
//        final double millisPerDay = 24*60*60*1000; // the number of milliseconds in one day.
//        dateAxisSpan = (numDays -1)*millisPerDay;  // the number of days (exclusing the first day).
//        dateAxisClippingEndpoint = dateAxisSpanOffset + dateAxisSpan;
//
//        if (xAxisLength > 200) {
//            for (int day = 0; day < numDays; ++day) {
//                // Check if this is the first day of the new week, if it is we should draw a
//                // vertical line to indicate this.
//                if (date.getFirstDayOfWeek() == date.get(GregorianCalendar.DAY_OF_WEEK)) {
//                    gc.setColor(Color.LIGHT_GRAY);
//
//                    double x = this.dateAxisPosition(date.getTime());
//
//                    // Note that these lines are clipped to be within the plotting area.
//                    gc.drawLine((int) x, (int) (plotArea.getMinY()), (int) x, (int) (plotArea.getMaxY()) );
//                }
//
//                // Increment the date by one day.
//                date.add(GregorianCalendar.DAY_OF_YEAR, 1);
//            }
//        }
//
//        gc.setColor(Color.BLACK);
//        gc.drawLine((int) (plotArea.getMinX() -1), (int) plotArea.getMaxY(), (int) (plotArea.getMaxX() +1), (int) plotArea.getMaxY());
//        double tickLength = 7; //xAxisLength / 50.0;
//        double labelSpace = tickLength; // space between tick and the associated label.
//        double previousX = 0.0;
//        double yLowestTickLabel = -Double.MAX_VALUE;
//
//        for (int tick = 0; tick < numTicks; ++tick) {
//            int month = tick; // the month number with the start of January being month 0, note: the month with index 12 will be 1 January of the next year.
//            GregorianCalendar monthStart = new GregorianCalendar(year, month, 1, 0,0); // the start of each month of this year.
//
//            // Get date as proportion of days in year.
//            double dateFraction = (monthStart.getTimeInMillis() - yearStart.getTimeInMillis())/(double) (millisPerDay*numDays);
//
//            // Draw the tick mark.
//            double x = plotArea.getMinX() + dateFraction*xAxisLength;
//
//            gc.setColor(Color.BLACK);
//            gc.drawLine((int) x, (int) plotArea.getMaxY(), (int) x, (int) (plotArea.getMaxY() + tickLength));
//
//            // Draw the tick label halfway between this tick and the previous tick.
//            if (tick > 0) {
//                double center = (x + previousX)/2.0;
//                double labelWidth = metrics.getStringBounds(months[tick-1], gc).getWidth();
//
//                double labelLeft = center - labelWidth/2.0;
//                double y = plotArea.getMaxY() + tickLength + labelSpace + metrics.getHeight();
//                gc.drawString(months[tick-1], (float) labelLeft, (float) y);
//
//                // Remember the lowest tick label position (actually since the screen corrdinates
//                // increase in the 'down' direction, we want the largest numerical value).
//                if (y > yLowestTickLabel) {
//                    yLowestTickLabel = y;
//                }
//            }
//
//            previousX = x;
//        }
//
//        // Draw the axis label. This will be halfway along the axis and below the bottom of
//        // the graph's lower label.
//        double center = (plotArea.getMaxX() + plotArea.getMinX())/2.0;
//
//        String axisLabel = "Month [" + year + "]";
//        double axisLabelWidth = metrics.getStringBounds(axisLabel, gc).getWidth();
//        double left = center - axisLabelWidth/2.0;
//
//        double y = yLowestTickLabel + 2.0*metrics.getHeight();
//
//        gc.drawString(axisLabel, (float) left, (float) y);
//    }
//
//    /**
//     * Draws and marks a single month on the date axis (bottom horizontal axis) of the graph.
//     *
//     * @param gc - the graphics context to draw on.
//     * @param data - a list of TrafficData objects to be plotted.
//     * @param year - the year to be displayed (eg. 1975).
//     * @param month - the month to be drawn (1 is January, 2 is February, etc).
//     *
//     * @throws IllegalArgumentException - if gc is null.
//     * @throws IllegalArgumentException - if data is null.
//     * @throws IllegalArgumentException - if year is less than 0.
//     * @throws IllegalArgumentException - if month is not in the range 1 to 12.
//     */
//    public void drawMonthAxis(Graphics2D gc, List data, int year, int month) {
//        // Draw the bottom (date) axis.
//
//        if (gc == null) {
//            throw new IllegalArgumentException("Cannot draw the month axis as the Graphics2D object is null.");
//        }
//
//        if (data == null) {
//            throw new IllegalArgumentException("Cannot draw the month axis as the list of data to be drawn is null.");
//        }
//
//        if (year < 0) {
//            throw new IllegalArgumentException("Cannot draw the month axis as the year is less than zero, the year specified was " + year);
//        }
//
//        if (month < 0) {
//            throw new IllegalArgumentException("Cannot draw the month axis as the month is not in the range 1 to 12, the value was " + month);
//        }
//
//        // Determine the maximum number of labels that will fit within the axis size.
//        FontMetrics metrics = gc.getFontMetrics(gc.getFont());
//        double minimumLabelSpacing = 1.5;      // minimum spacing between labels as fraction of label height.
//        int labelHeight = (int) (metrics.getHeight() * minimumLabelSpacing);
//
//        // Determine the number of days in the month.
//        GregorianCalendar monthStart = new GregorianCalendar(year, month -1, 1);
//        final int numDays = monthStart.getActualMaximum(GregorianCalendar.DAY_OF_MONTH);
//
//        // Determine how many day labels could fit across this axis (assuming all
//        // the labels have the same width as the number "00").
//        double labelWidth = metrics.getStringBounds("00", gc).getWidth();
//        final double spacingFactor = 1.5;  // the fraction of space required between the labels.
//        final double xAxisLength = plotArea.getMaxX() - plotArea.getMinX() +1;
//
//        int maxNumLabels = (int) Math.floor(xAxisLength/(labelWidth*spacingFactor));
//
//        // Always label the day at the beginning and end of the month.
//        if (maxNumLabels < 2) {
//            maxNumLabels = 2;
//        }
//
//        // Determine the number of tick marks and the interval between tick marks.
//        // The number of tick marks cannot exceed the maximum number of labels
//        // which can be displayed. Use the smallest interval possible which will
//        // cover the number of days in the month.
//        int[] intervals = { 1, 2, 4, 5, 10, numDays };
//
//        boolean foundInterval = false;
//        int interval = 1;
//
//        for(int index = 0; !foundInterval && (index < intervals.length); ++index) {
//            int span = 1 + intervals[index]*(maxNumLabels -1);
//
//            if (span >= numDays) {
//                foundInterval = true;
//                interval = intervals[index];
//            }
//        }
//
//        // Determine the number of intervals we will actually need.
//        int numIntervals = 1 + (int) Math.ceil( (numDays -1) / (double) interval);
//
//        // We always need at least one interval.
//        if (numIntervals < 1) {
//            numIntervals = 1;
//        }
//
//        final int numTicks = numIntervals;
//
//        // Remember the span and offset of the axis and its span (used when plotting the traces).
//        dateAxisSpanOffset = monthStart.getTimeInMillis();
//        final double millisPerDay = 24*60*60*1000; // the number of milliseconds in one day.
//        final int span = (numTicks -1)*interval; // number of days that are actually plotted by the graph, excluding the first point.
//        dateAxisSpan = span*millisPerDay;
//        dateAxisClippingEndpoint = dateAxisSpanOffset + numDays*millisPerDay; // clip at the end of the month, rather than at the end of the axis (which may extend past the end of the month).
//
//        // Draw the week boundaries.
//        GregorianCalendar currentDay = new GregorianCalendar(year, month -1, 1);
//        for (int day = 1; day <= numDays; ++day) {
//            // Check if this is the first day of the new week, if it is we should draw a
//            // vertical line to indicate this.
//            if (currentDay.getFirstDayOfWeek() == currentDay.get(GregorianCalendar.DAY_OF_WEEK)) {
//                gc.setColor(Color.LIGHT_GRAY);
//
//                double x = this.dateAxisPosition(currentDay.getTime());
//
//                // Note that these lines are clipped to be within the plotting area.
//                gc.drawLine((int) x, (int) (plotArea.getMinY()), (int) x, (int) (plotArea.getMaxY()) );
//            }
//
//            // Increment the date by one day.
//            currentDay.add(GregorianCalendar.DAY_OF_YEAR, 1);
//        }
//
//        double tickLength = 7; //xAxisLength / 50.0;
//        double labelSpace = tickLength; // space between tick and the associated label.
//
//        // Draw the axis bar.
//        gc.setColor(Color.BLACK);
//        gc.drawLine((int) (plotArea.getMinX() -1), (int) (plotArea.getMaxY()), (int) (plotArea.getMaxX() +1), (int) (plotArea.getMaxY()));
//
//        // Draw the tick marks and their associated day label.
//
//        for (int tick = 0; tick < numTicks; ++tick) {
//            double x = plotArea.getMinX() + xAxisLength*tick/(double)(numTicks -1);
//            double y = plotArea.getMaxY() +1;
//
//            // Draw the tick.
//            gc.drawLine((int) x, (int) y, (int) x, (int) (y + tickLength));
//
//            // Draw the label.
//            int day = 1 + tick*interval;
//            String label = Integer.toString(day);
//            labelWidth = metrics.getStringBounds(label, gc).getWidth();
//            double xLabel = x - labelWidth/2.0;
//            double yLabel = y + labelSpace + labelHeight;
//
//            // Only draw labels for days in the month (an extra tick may
//            // be needed at the end of the month but this should not be
//            // labeled if it is past the end day of the month).
//            if (day <= numDays) {
//              gc.drawString(label, (float) xLabel, (float) yLabel);
//            }
//        }
//
//        // Draw the axis label.
//        SimpleDateFormat monthFormatter = new SimpleDateFormat("MMMMM  [yyyy]");
//        StringBuffer buffer = new StringBuffer();
//        String axisLabel = monthFormatter.format(monthStart.getTime());
//
//        double axisLabelWidth = metrics.getStringBounds(axisLabel, gc).getWidth();
//        gc.drawString(axisLabel, (float) (plotArea.getMinX() + xAxisLength/2.0 - axisLabelWidth/2.0), (float) (plotArea.getMaxY() +1 + labelSpace + labelHeight + labelSpace + labelHeight));
//    }
//
//    /**
//     * The background colour of the entire plot (the background inside the
//     * plotting are is drawn in the drawPlotBackground() method).
//     *
//     * @param image - the image to draw onto.
//     */
//    public void drawBackground(BufferedImage image) {
//        Graphics2D gc = (Graphics2D) image.getGraphics();
//        gc.setPaint(Color.WHITE);
//        gc.fillRect(0, 0, image.getWidth(), image.getHeight());
//    }
//
//    /**
//     * The background colour of the plotting area.
//     *
//     * @param gc - the graphics object to draw into.
//     */
//    public void drawPlotBackground(Graphics2D gc) {
//        gc.setPaint(Color.WHITE);
//        gc.fillRect((int) plotArea.getMinX(), (int) plotArea.getMinY(), (int) plotArea.getWidth(), (int) plotArea.getHeight());
//    }
//
//    /**
//     * Draw the traces on the plot.
//     *
//     * @param gc - the graphics context to draw on.
//     * @param data - a list of TrafficData objects to be plotted.
//     *
//     * @throws IllegalArgumentException - if data is null or has no elements.
//    */
//    public void drawTraces(Graphics2D gc, List data) throws IllegalArgumentException {
//
//        if (data == null) {
//            throw new IllegalArgumentException("Cannnot draw the trace of traffic data as the list of traffic data is null.");
//        }
//
//        if (data.size() < 1) {
//            throw new IllegalArgumentException("Cannnot draw the trace of traffic data as the list of traffic data has no elements.");
//        }
//
//        // Ensure the plotting data is in ascending time order.
//        Collections.sort(data);
//
//        Shape oldClippingShape = gc.getClip();
//        gc.setClip(plotArea);
//
//        final double width = plotArea.getWidth();
//        final double height = plotArea.getHeight();
//
//        double oldX = 0.0;
//        double oldY = 0.0;
//        double x = 0.0;
//        double y = 0.0;
//        double phase = 0.0;
//
//        // Find the number of days spanned by the data. We do this by finding
//        // whether the data spans more than a month or not. If it spans more
//        // than a month then we determine the number of days in the year
//        // (checking whether it is a leap year or not).
//        int year = ((TrafficData) data.get(0)).date.getYear() + 1900;
//        GregorianCalendar gregorian = new GregorianCalendar(year, 1, 1);
//
//        int oldestMonth = ((TrafficData) data.get(0)).date.getMonth();
//        int newestMonth = ((TrafficData) data.get(data.size() -1)).date.getMonth();
//
//        int numDays = 0; // number of days the graph will span.
//
//        // If the traffic data is not from the same month then it must span a
//        if (oldestMonth != newestMonth) {
//            // Determine whether the year is a leap year or not.
//            numDays = 365;
//            if (gregorian.isLeapYear(year)) {
//                numDays += 1;
//            }
//        } else {
//            // The data is within a single month. We should get the number of days within this month.
//            // On leap years Februrary has 29 days instead of 28.
//            gregorian = new GregorianCalendar(year, oldestMonth, 1);
//            numDays = gregorian.getActualMaximum(Calendar.DAY_OF_MONTH);
//        }
//
//        GregorianCalendar yearStart = new GregorianCalendar(year, 0, 1, 0, 0); // The date corresponding to 00:00 on 1 January of the year being plotted.
//
//        // Plot each data point. While doing this we need to check whether any
//        // days are missing from the data.
//        TrafficData current = null;
//        TrafficData previous = null;
//
//        // Find the maximum and minimum traffic values.
//        int lowestTraffic = findMinimumTrafficFlow(data);
//        int highestTraffic = findMaximumTrafficFlow(data);
//
//        for (int count = 0; count < data.size(); ++count)
//        {
//            current = (TrafficData) data.get(count);
//
//            if (current == null || current.date == null || current.value == null) {
//                // Don't do anything, there isn't a data point to plot at this date.
//                //System.out.println("current(" + count + ") : current.date = " + current.date + ", current.value = " + current.value);
//            } else {
//                x = dateAxisPosition(current.date);
//                y = trafficAxisPosition(current.value.doubleValue());
//
//                if ( (previous == null) || (numDaysDifference(current.date, previous.date) > 1)
//                    || (current.date.getTime() < this.dateAxisSpanOffset) || (current.date.getTime() > dateAxisClippingEndpoint) )
//                {
//                    // There is no data for the previous day so move to the starting
//                    // point.
//                }
//                else
//                {
//                    // Draw a line from the current location to the current position.
//                    gc.drawLine((int) oldX, (int) oldY, (int) x, (int) y);
//                }
//
//                previous = current;
//                oldX = x;
//                oldY = y;
//            }
//        }
//
//        gc.setClip(oldClippingShape);
//    }
//
//
//    /**
//     * Find the minimum (oldest) date present in a list of TrafficData.
//     *
//     * @param data - the list of TrafficData objects to find the minimum date of.
//     *
//     * @throws IllegalArgumentException - if data is null.
//     */
//    public Date findMinimumDate(List data) throws IllegalArgumentException
//    {
//        if (data == null)
//        {
//            throw new IllegalArgumentException("Cannot find the minimum date for a list of data which is null.");
//        }
//
//        Iterator iterator = data.iterator();
//        Date oldest = null;
//
//        while(iterator.hasNext())
//        {
//            TrafficData traffic = (TrafficData) iterator.next();
//            if ( (oldest == null) || (traffic.date.before(oldest)) )
//            {
//                oldest = (Date) traffic.date.clone();
//            }
//        }
//
//        return oldest;
//    }
//
//    /**
//     * Find the maximum (newest) date present in a list of TrafficData.
//     *
//     * @param data - the list of TrafficData objects to find the maximum date of.
//     *
//     * @throws IllegalArgumentException - if data is null.
//     */
//    public Date findMaximumDate(List data) throws IllegalArgumentException
//    {
//        if (data == null)
//        {
//            throw new IllegalArgumentException("Cannot find the maximum date for a list of data which is null.");
//        }
//
//        Iterator iterator = data.iterator();
//        Date latest = null;
//
//        while(iterator.hasNext())
//        {
//            TrafficData traffic = (TrafficData) iterator.next();
//            if ( (latest == null) || (traffic.date.after(latest)) )
//            {
//                latest = (Date) traffic.date.clone();
//            }
//        }
//
//        return latest;
//    }
//
//    /**
//     * Find the minimum (lowest) traffic flow present in a list of TrafficData.
//     *
//     * @param data - the list of TrafficData objects to find the minimum value of.
//     *
//     * @return the minimum traffic flow seen.
//     *
//     * @throws IllegalArgumentException - if data is null.
//     */
//    public int findMinimumTrafficFlow(List data) throws IllegalArgumentException
//    {
//        if (data == null)
//        {
//            throw new IllegalArgumentException("Cannot find the minimum traffic flow for a list of data which is null.");
//        }
//
//        Iterator iterator = data.iterator();
//        int lowest = Integer.MAX_VALUE;
//
//        while(iterator.hasNext())
//        {
//            TrafficData traffic = (TrafficData) iterator.next();
//            if ( (traffic.value != null) && (traffic.value.intValue() < lowest) )
//            {
//                lowest = traffic.value.intValue();
//            }
//        }
//
//        return lowest;
//    }
//
//    /**
//     * Find the maximum (lowest) traffic flow present in a list of TrafficData.
//     *
//     * @param data - the list of TrafficData objects to find the maximum value of.
//     *
//     * @return the maximum traffic flow seen.
//     *
//     * @throws IllegalArgumentException - if data is null.
//     */
//    public int findMaximumTrafficFlow(List data) throws IllegalArgumentException
//    {
//        if (data == null)
//        {
//            throw new IllegalArgumentException("Cannot find the maximum traffic flow for a list of data which is null.");
//        }
//
//        Iterator iterator = data.iterator();
//        int highest = Integer.MIN_VALUE;
//
//        while(iterator.hasNext())
//        {
//            TrafficData traffic = (TrafficData) iterator.next();
//            if ( (traffic.value != null) && (traffic.value.intValue() > highest) )
//            {
//                highest = traffic.value.intValue();
//            }
//        }
//
//        return highest;
//    }
//
//    /**
//     * Returns the number of days difference between two dates. The difference
//     * is rounded to the closest number of days if they don't differ by an exact
//     * number of days, and a postive number of days is always returned (ie. the
//     * absolute number of days difference is computed).
//     *
//     * @param first - the first date.
//     * @param second - the second date.
//     *
//     * @return the absolute time difference between the two dates, rounded to the
//     * closest number of days.
//     *
//     * @throws IllegalArgumentException - if first or second are null.
//     */
//    public long numDaysDifference(Date first, Date second)
//    {
//        if (first == null)
//        {
//            throw new IllegalArgumentException("Cannot compute the number of days difference between two dates as the first date is null.");
//        }
//
//        if (second == null)
//        {
//            throw new IllegalArgumentException("Cannot compute the number of days difference between two dates as the second date is null.");
//        }
//
//        final double millisPerDay = 24.0*60.0*60.0*1000.0; // number of milliseconds in one day.
//        final double milliDifference = first.getTime() - second.getTime();
//
//        final int numDays = (int) Math.abs(milliDifference/millisPerDay);
//
//        return Math.round(numDays);
//    }
//
//    /**
//     * Computes the position of a traffic flow datum relative to the Traffic Flow (left vertical) axis.
//     *
//     * @param value - the traffic flow value (in vehicles per day).
//     * @return position - the position of the traffic data relative to the Traffic Flow axis (in pixels).
//     *
//     * @throws IllegalArgumentException - if value is less than zero.
//     */
//    public double trafficAxisPosition(double value) throws IllegalArgumentException {
//        if (value < 0) {
//            throw new IllegalArgumentException("Cannot compute the position of the traffic data relative to the Traffic Flow axis as the value argument was less than zero (was " + value + ")");
//        }
//
//        final double trafficFlowAxisLength = plotArea.getMaxY() - plotArea.getMinY() +1;
//        return plotArea.getMaxY() - value*trafficFlowAxisLength/trafficFlowAxisSpan;
//    }
//
//    /**
//     * Computes the position of a date relative to the Date (bottom horizontal) axis.
//     *
//     * @param date - the date.
//     * @return position - the position of the date relative to the date axis (in pixels).
//     *
//     * @throws IllegalArgumentException - if date is null.
//     */
//    public double dateAxisPosition(Date date) throws IllegalArgumentException {
//        if (date == null) {
//            throw new IllegalArgumentException("Cannot compute the position of the traffic data relative to the date axis as the date argument was null.");
//        }
//
//        final double dateAxisLength = plotArea.getMaxX() - plotArea.getMinX() +1;
//
//        return plotArea.getMinX() + (date.getTime() - dateAxisSpanOffset)*dateAxisLength/dateAxisSpan;
//    }
//
//    /**
//     * Creates an image which contains a plot of traffic flow versus year for
//     * one or more tarffic measurement sites.
//     *
//     * @param data - a List containing one or more Lists of TrafficData objects.
//     *               each List contained within data is a single data series to
//     *               be plotted. Each data series corresponds to a different
//     *               measurement site.
//     * @param seriesNames - a List containing the names of the data series contained
//     *                      in the data list. This is used for building the graph
//     *                      legend. Each data series corresponds to a different
//     *                      measurement site.
//     * @param graphTitle - the title of the entire graph.
//     * @param xAxisLabel - the label of the graph's x-axis.
//     * @param yAxisLabel - the label of the graph's y-axis.
//     * @param width - the width of the output graph image.
//     * @param height - the height of the output graph image.
//     *
//     * @returns an image containing a plot which shows the data series contained in data.
//     *
//     * @throws IllegalArgumentException - if data is null or contains no lists or
//     *                                    they contain no elements.
//     * @throws IllegalArgumentException - if seriesNames doesn't contain the same number
//     *                                    of elements as there are lists in the data List.
//     * @throws IllegalArgumentException - if width is zero or less.
//     * @throws IllegalArgumentException - if height is zero or less.
//     */
//    public BufferedImage getTrafficAnnualPlot(List data, List seriesNames, String graphTitle, String xAxisLabel, String yAxisLabel, int width, int height) throws IllegalArgumentException {
//        // Check the arguments are valid.
//        if (data == null) {
//            throw new IllegalArgumentException("Cannot create a traffic total plot as the data list was null");
//        }
//        if (data.size() < 1) {
//            throw new IllegalArgumentException("Cannot create a traffic total plot as the data list has no elements");
//        }
//        if (seriesNames == null) {
//            throw new IllegalArgumentException("Cannot create a traffic total plot as the list of series names was null");
//        }
//        if (seriesNames.size() < 1) {
//            throw new IllegalArgumentException("Cannot create a traffic total plot as the list of series names has no elements");
//        }
//        if (seriesNames.size() != data.size()) {
//            throw new IllegalArgumentException("Cannot create a traffic total plot as the list of series names is not the same size as the liust of data series.");
//        }
//        if (width < 1) {
//            throw new IllegalArgumentException("Cannot create a traffic total plot as the specified width was " + width);
//        }
//        if (height < 1) {
//            throw new IllegalArgumentException("Cannot create a traffic total plot as the specified height was " + height);
//        }
//
//        // Get the limits of all the graphs.
//        Date earliestDate = null;
//        Date latestDate = null;
//        int lowestTraffic = Integer.MAX_VALUE;
//        int highestTraffic = Integer.MIN_VALUE;
//
//        for (int count = 0; count < data.size(); ++count) {
//            List series = (List) data.get(count);
//
//            if ( (series != null) && (series.size() > 0) ) {
//                Date maxDate = findMaximumDate(series);
//                Date minDate = findMinimumDate(series);
//                int maxTraffic = findMaximumTrafficFlow(series);
//                int minTraffic = findMinimumTrafficFlow(series);
//
//                if ( (earliestDate == null) || ((minDate != null) && minDate.before(earliestDate)) ) {
//                    earliestDate = minDate;
//                }
//
//                if ( (latestDate == null) || ((maxDate != null) && maxDate.after(latestDate)) ) {
//                    latestDate = maxDate;
//                }
//
//                if (minTraffic < lowestTraffic) {
//                    lowestTraffic = minTraffic;
//                }
//
//                if (maxTraffic > highestTraffic) {
//                    highestTraffic = maxTraffic;
//                }
//            }
//        }
//
//        // Find the earliest and latest years plotted on the graph.
//        final int januaryMonth = 0;      // The month code for January.
//        final int firstDayInJanuary = 1; // The first day in January.
//        final int decemberMonth = 11;     // The month code for December (January is 0).
//        final int lastDayInDecember = 31; // The last day in December.
//
//        Date minDate = new Date(earliestDate.getYear(), januaryMonth, firstDayInJanuary);
//        Date maxDate = new Date(latestDate.getYear(), decemberMonth, lastDayInDecember);
//
//        // Draw the plot.
//        final int plotHeight = (9*height)/10; // TODO - finish this.
//        BufferedImage plot = new BufferedImage(width, plotHeight, BufferedImage.TYPE_INT_ARGB);
//        Graphics2D gc = plot.createGraphics();
//
//        gc.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
//        gc.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
//        gc.setRenderingHint(RenderingHints.KEY_RENDERING, RenderingHints.VALUE_RENDER_SPEED);
//        //gc.setRenderingHint(RenderingHints.KEY_RENDERING, RenderingHints.VALUE_RENDER_QUALITY);
//
//        computePlotArea(plot);
//
//        drawPlotBackground(gc);
//        drawTrafficAnnualAxes(gc, minDate, maxDate, lowestTraffic, highestTraffic, xAxisLabel, yAxisLabel);
//
//        // Label the graph.
//        Font defaultFont = gc.getFont();
//        Font graphTitleFont = gc.getFont().deriveFont(Font.BOLD, gc.getFont().getSize() +3.0f);
//        gc.setFont(graphTitleFont);
//        if ( (graphTitle != null) && (graphTitle.trim().length() > 0) ) {
//            gc.drawString(graphTitle, (float) ((plotArea.getMinX() + plotArea.getMaxX())/2.0f - gc.getFontMetrics().getStringBounds(graphTitle, gc).getWidth()/2.0f), (float) (plotArea.getMinY()/2.0f /*- gc.getFontMetrics().getHeight()/2.0f */));
//        }
//        gc.setFont(defaultFont);
//
//        // Paint the traces. Note: they are painted in reverse order so that
//        // the newer traces overwrite the older ones.
//        List tracePaints = new ArrayList();
//        Paint tracePaint1 = new Color(1.0f, 0.0f, 0.0f, 1.0f);
//        tracePaints.add(tracePaint1);
//
//        Paint tracePaint2 = new Color(0.0f, 1.0f, 0.0f, 0.8f);
//        tracePaints.add(tracePaint2);
//
//        Paint tracePaint3 = new Color(0.0f, 0.0f, 1.0f, 0.6f);
//        tracePaints.add(tracePaint3);
//
//        List dash = new ArrayList();
//        float[] traceDash1 = { 1 };
//        float[] traceDash2 = {20, 5};
//        float[] traceDash3 = {20, 5, 5, 5};
//
//        dash.add(traceDash1);
//        dash.add(traceDash2);
//        dash.add(traceDash3);
//
//        String[] symbols = { "+", "x", "*" };
//
//        List strokes = new ArrayList();
//        for (int count = 0; count < data.size(); ++count) {
//            float strokeWidth = 2.0f - count/2.0f;
//            Stroke stroke = new BasicStroke(strokeWidth, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND, 0.0f, (float[]) dash.get(count), 0.0f);
//            strokes.add(stroke);
//        }
//
//        for (int count = data.size() -1; count >= 0; --count) {
//            List dataSeries = (List) data.get(count);
//
//            gc.setPaint((Paint) tracePaints.get(count));
//            gc.setStroke((Stroke) strokes.get(count));
//
//            drawTrafficAnnualTraces(gc, dataSeries, minDate, maxDate, symbols[count]);
//        }
//
//
//        //plotGraphics.setColor(Color.ORANGE);
//        //plotGraphics.fillRect(0, 0, width, height);
//
//        // Draw the legend.
//        BufferedImage legend = drawLegend("Site", seriesNames, tracePaints, strokes, width);
//
//        // Combine the plot and legend.
//        int totalHeight = plot.getHeight() + legend.getHeight();
//
//        BufferedImage combinedPlot = new BufferedImage(width, totalHeight, BufferedImage.TYPE_INT_ARGB);
//        Graphics2D combinedPlotGraphics = combinedPlot.createGraphics();
//        Paint defaultPaint = combinedPlotGraphics.getPaint();
//        combinedPlotGraphics.setPaint(Color.WHITE);
//        combinedPlotGraphics.fillRect(0, 0, combinedPlot.getWidth(), combinedPlot.getHeight());
//        combinedPlotGraphics.setPaint(defaultPaint);
//        combinedPlotGraphics.drawImage(plot, 0, 0, width, plot.getHeight(), null);
//        combinedPlotGraphics.drawImage(legend, 0, plot.getHeight(), width, legend.getHeight(), null);
//
//        return combinedPlot;
//    }
//
//    /**
//     * Draws the axis for the traffic annual plot.
//     *
//     * @param gc - the graphics to draw the axes on.
//     * @param earliestDate - the earliest date that is to be displayed on the graph.
//     * @param latestDate - the latest date that is to be displayed on the graph.
//     * @param minTrafficFlow - the minimum traffic flow measured on the highway.
//     * @param maxTrafficFlow - the maximum traffic flow measured on the highway.
//     * @param xAxisLabel - the label for the x-axis.
//     * @param yAxisLabel - the label for the y-axis.
//     *
//     * @throws IllegalArgumentException - if gc is null.
//     */
//    public void drawTrafficAnnualAxes(Graphics2D gc, Date earliestDate, Date latestDate, int minTrafficFlow, int maxTrafficFlow, String xAxisLabel, String yAxisLabel) throws IllegalArgumentException {
//        if (gc == null) {
//            throw new IllegalArgumentException("Cannot create a traffic annual plot as the graphics was null.");
//        }
//
//        drawTrafficRouteFlowAxis(gc, minTrafficFlow, maxTrafficFlow, yAxisLabel);
//        drawTrafficAnnualYearAxis(gc, earliestDate, latestDate, xAxisLabel);
//
//        // Draw the top and right borders of the plot.
//        gc.drawLine( (int) (plotArea.getMinX() -1), (int) (plotArea.getMinY() -1), (int) plotArea.getMaxX() +1, (int) (plotArea.getMinY() -1) ); // draw the top line.
//        gc.drawLine( (int) (plotArea.getMaxX() +1), (int) (plotArea.getMinY() -1), (int) plotArea.getMaxX() +1, (int) (plotArea.getMaxY() +1) ); // draw the top line.
//    }
//
//    /**
//     * Draws the year (horizontal) axis for the traffic annual plot.
//     *
//     * @param gc - the graphics to draw the axes on.
//     * @param minDate - the earliest date that is to be displayed on the graph.
//     * @param maxDate - the latest date that is to be displayed on the graph.
//     * @param axisLabel - the label for this axis.
//     *
//     * @throws IllegalArgumentException - if gc is null.
//     */
//    public void drawTrafficAnnualYearAxis(Graphics2D gc, Date minDate, Date maxDate, String axisLabel) throws IllegalArgumentException {
//        if (gc == null) {
//            throw new IllegalArgumentException("Cannot create the year axis of the traffic annual plot as the graphics was null.");
//        }
//
//        annualAxisSpan = maxDate.getTime() - minDate.getTime() +1;
//        annualAxisSpanOffset = minDate.getTime();
//
//        // Determine the number of years covered between the minimum and maximum dates (inclusive).
//        final int numYears = maxDate.getYear() - minDate.getYear() +1;
//
//        int minYear = minDate.getYear() + 1900;
//        int maxYear = maxDate.getYear() + 1900;
//        double yearLabelWidth = gc.getFontMetrics().getStringBounds(Integer.toString(maxYear), gc).getWidth();
//
//        double axisLength = plotArea.getMaxX() - plotArea.getMinX() +1; // the length of the axis, in pixels.
//
//        int numLabelsInLength = (int) Math.floor(axisLength / yearLabelWidth);
//
//        int interval = 0;
//        boolean foundInterval = false;
//
//
//        // Draw the axis and tick marks.
//        gc.setPaint(Color.BLACK);
//        gc.drawLine((int) (plotArea.getMinX() -1), (int) (plotArea.getMaxY() +1), (int) (plotArea.getMaxX() +1), (int) (plotArea.getMaxY() +1) );
//
//        double tickLength = 7; //xAxisLength / 50.0;
//        double labelSpace = tickLength; // space between tick and the associated label.
//
//        int numIntervals = numYears;
//        int numTicks = numIntervals +1;
//        final int decemberMonth = 11;     // The month code for December (January is 0).
//        final int lastDayInDecember = 31; // The last day in December.
//        double lastX = 0.0;
//
//        for (int tick = 0; tick < numTicks; ++tick) {
//            double x = plotArea.getMinX();
//            double y = plotArea.getMaxY() +1;
//
//            if (tick > 0) {
//                Date tickDate = new Date(minDate.getYear() + (tick-1), decemberMonth, lastDayInDecember);
//
//                x = trafficAnnualYearPosition(tickDate);
//
//                int year = 1900 + minDate.getYear() + (tick-1);
//                String yearString = Integer.toString(year);
//                double yearStringWidth = gc.getFontMetrics().getStringBounds(yearString, gc).getWidth();
//                double yearStringHeight = gc.getFontMetrics().getHeight();
//
//                gc.drawString(yearString, (float) ((lastX + x) /2.0 - yearStringWidth/2.0), (float) (y + labelSpace + yearStringHeight));
//            }
//
//            gc.drawLine((int) x, (int) y, (int) x, (int) (y + tickLength));
//            lastX = x;
//        }
//
//        // Draw the axis label. The y position of the label will be half-way between the edge
//        // of the graph and the tickmark label closest to the bottom edge.
//        Font defaultFont = gc.getFont();
//        Font graphAxisFont = gc.getFont().deriveFont(Font.BOLD, gc.getFont().getSize() +1.0f);
//        gc.setFont(graphAxisFont);
//
//        double axisLabelLength = gc.getFontMetrics().getStringBounds(axisLabel, gc).getWidth();
//        double labelHeight = gc.getFontMetrics().getHeight();
//        double axisLabelBottom = (float) (plotArea.getMaxY() + labelSpace + labelHeight + labelSpace + labelHeight);
//        gc.drawString(axisLabel, (float) (plotArea.getMinX() + (plotArea.getMaxX() - plotArea.getMinX() +1)/2), (float) axisLabelBottom);
//        gc.setFont(defaultFont);
//
//    }
//
//    /**
//     * Draws a single trace onto gc using the current paint and stroke as given by gc.
//     *
//     * @param symbol - the character to be plotted.
//     */
//    public void drawTrafficAnnualTraces(Graphics2D gc, List dataSeries, Date minDate, Date maxDate, String symbol) {
//        if (gc == null) {
//            throw new IllegalArgumentException("Cannot draw a traffic flow vs. year trace as the graphics is null.");
//        }
//
//        Stroke lineStroke = gc.getStroke();
//
//        // Sort the measurements by date.
//        Collections.sort(dataSeries);
//
//        int symbolHalfLength = 4;
//
//        for (int count = 0; count < dataSeries.size(); ++count) {
//            TrafficData currentData = (TrafficData) dataSeries.get(count);
//
//
//            // Plot the measured data points.
//            double x = trafficAnnualYearPosition(currentData.date);
//            double y = trafficAxisPosition(currentData.value.doubleValue());
//
//            gc.setStroke(new BasicStroke());
//            if (symbol.equals("x") || symbol.equals("*")) {
//                gc.drawLine((int) (x -symbolHalfLength), (int) (y -symbolHalfLength), (int) (x+symbolHalfLength), (int) (y+symbolHalfLength));
//                gc.drawLine((int) (x -symbolHalfLength), (int) (y +symbolHalfLength), (int) (x+symbolHalfLength), (int) (y-symbolHalfLength));
//            }
//
//            if (symbol.equals("+") || symbol.equals("*")) {
//                gc.drawLine((int) (x -symbolHalfLength), (int) y, (int) (x+symbolHalfLength), (int) y);
//                gc.drawLine((int) x, (int) (y -symbolHalfLength), (int) x, (int) (y+symbolHalfLength));
//            }
//
//            // Plot the connecting lines bewteen points.
//            if (count > 0) {
//                TrafficData previousData = (TrafficData) dataSeries.get(count -1);
//
//                int currentYear = currentData.date.getYear() + 1900;
//                int previousYear = previousData.date.getYear() + 1900;
//
//                if (currentYear == (previousYear+1)) {
//                    int previousX = (int) trafficAnnualYearPosition(previousData.date);
//                    int previousY = (int) trafficAxisPosition(previousData.value.doubleValue());
//                    int currentX = (int) trafficAnnualYearPosition(currentData.date);
//                    int currentY = (int) trafficAxisPosition(currentData.value.doubleValue());
//
//                    gc.setStroke(lineStroke);
//                    gc.drawLine(previousX, previousY, currentX, currentY);
//                }
//            }
//        }
//    }
//
//    /**
//     * Returns the horizontal pixel (along the horizontal axis of the traffic
//     * annual plot) for a given date.
//     *
//     * @param date - the date to find the x-axis position for.
//     * @return the pixel position alomg the x-axis corresponding to the specified date.
//     *
//     * @throws IllegalArgumentException - if date is null.
//     */
//    public double trafficAnnualYearPosition(Date date) throws IllegalArgumentException {
//        if (date == null) {
//            throw new IllegalArgumentException("Cannot determine the horizontal position within the traffic annual plot as the date is null.");
//        }
//
//        final double axisLength = plotArea.getMaxX() - plotArea.getMinX() +1;
//        return plotArea.getMinX() + (date.getTime() - annualAxisSpanOffset)*axisLength/annualAxisSpan;
//    }
//
//    /**
//     * Plots the traffic flow along a highway. The flow along the highway is
//     * recorded at each measurement site by a single TrafficSiteData object.
//     * More than one data series can be displayed where each data series
//     * corresponds to a traffic flow measurement at a different time.
//     *
//     * @param data - a List containing one or more Lists of TrafficSiteData
//     *               objects. Each List contained within data is a single data
//     *               series to be plotted. Each data series corresponds to a different
//     *               time that traffic flow measurements were made along a
//     *               highway.
//     * @param seriesNames - a List containing the names of the data series contained
//     *                      in the data list. This is used for building the graph
//     *                      legend. Each data series corresponds to a different
//     *                      time of measurement.
//     * @param xLabels - a List containing TrafficSite objects which are to be
//     *                  used to label the x-axis.
//     *
//     * @param graphTitle - the title of the entire graph.
//     * @param xAxisLabel - the label of the graph's x-axis.
//     * @param yAxisLabel - the label of the graph's y-axis.
//     * @param width - the width of the output graph image.
//     * @param height - the height of the output graph image.
//     *
//     * @returns an image containing a plot which shows the data series contained in data.
//     *
//     * @throws IllegalArgumentException - if data is null or contains no lists or
//     *                                    they contain no elements.
//     * @throws IllegalArgumentException - if seriesNames doesn't contain the same number
//     *                                    of elements as there are lists in the data List.
//     * @throws IllegalArgumentException - if width is zero or less.
//     * @throws IllegalArgumentException - if height is zero or less.
//
//     */
//    public BufferedImage getTrafficRoutePlot(List data, List seriesNames, List xLabels, String graphTitle, String xAxisLabel, String yAxisLabel, int width, int height) throws IllegalArgumentException {
//        // Check the arguments are valid.
//        if (data == null) {
//            throw new IllegalArgumentException("Cannot create a traffic route plot as the data list was null");
//        }
//        if (data.size() < 1) {
//            throw new IllegalArgumentException("Cannot create a traffic route plot as the data list has no elements");
//        }
//        if (seriesNames == null) {
//            throw new IllegalArgumentException("Cannot create a traffic total route as the list of series names was null");
//        }
//        if (seriesNames.size() < 1) {
//            throw new IllegalArgumentException("Cannot create a traffic route plot as the list of series names has no elements");
//        }
//        if (seriesNames.size() != data.size()) {
//            throw new IllegalArgumentException("Cannot create a traffic route plot as the list of series names is not the same size as the list of data series.");
//        }
//        if (xLabels == null) {
//            throw new IllegalArgumentException("Cannot create a traffic route plot as the list of route displacement (x-axis) labels series names is null.");
//        }
//        if (xLabels.size() < 1) {
//            throw new IllegalArgumentException("Cannot create a traffic route plot as the list of route displacement (x-axis) labels series has no elements.");
//        }
//        if (width < 1) {
//            throw new IllegalArgumentException("Cannot create a traffic route plot as the specified width was " + width);
//        }
//        if (height < 1) {
//            throw new IllegalArgumentException("Cannot create a traffic route plot as the specified height was " + height);
//        }
//
//        // Get all the displacements from all the graphs. We use a Set as this means duplicate values will be merged.
//        Set allDisplacements = new HashSet();
//
//        Iterator displacementIterator = xLabels.iterator();
//        while(displacementIterator.hasNext()) {
//            TrafficSite site = (TrafficSite) displacementIterator.next();
//            allDisplacements.add(new Double(site.displacement));
//        }
//
//        Integer minTrafficFlow = null;
//        Integer maxTrafficFlow = null;
//
//        Iterator dataSeriesIterator = data.iterator();
//        while (dataSeriesIterator.hasNext()) {
//            List dataSeries = (List) dataSeriesIterator.next();
//
//            Iterator measurementsIterator = dataSeries.iterator();
//            while (measurementsIterator.hasNext()) {
//                TrafficSiteData siteData = (TrafficSiteData) measurementsIterator.next();
//                allDisplacements.add(new Double(siteData.displacement));
//
//                if ( (minTrafficFlow == null) || ((siteData.value != null) && (siteData.value.intValue() < minTrafficFlow.intValue())) ) {
//                    minTrafficFlow = siteData.value;
//                }
//
//                if ( (maxTrafficFlow == null) || ((siteData.value != null) && (siteData.value.intValue() > maxTrafficFlow.intValue())) ) {
//                    maxTrafficFlow = siteData.value;
//                }
//            }
//        }
//
//        // Sort the measurement site displacements into increasing order.
//        Comparator displacementComparator = new Comparator() {
//            public int compare(Object object1, Object object2) {
//                Double displacement1 = null;
//                Double displacement2 = null;
//
//                if (object1 != null) {
//                    displacement1 = (Double) object1;
//                }
//
//                if (object2 != null) {
//                    displacement2 = (Double) object2;
//                }
//
//                if (displacement1 != null) {
//                    return displacement1.compareTo(displacement2);
//                } else {
//                    if (displacement2 != null) {
//                        return displacement2.compareTo(displacement1);
//                    } else {
//                        // Both objects were null.
//                        return 0;
//                    }
//                }
//           } };
//
//        List uniqueDisplacements = new ArrayList(allDisplacements);
//        Collections.sort(uniqueDisplacements);
//
//        Double minDisplacement = (Double) uniqueDisplacements.get(0);
//        Double maxDisplacement = (Double) uniqueDisplacements.get(uniqueDisplacements.size() -1);
//
//        // Now do the actual drawing.
//        final int plotHeight = (9*height)/10; // TODO - finish this.
//        BufferedImage plot = new BufferedImage(width, plotHeight, BufferedImage.TYPE_INT_ARGB);
//        Graphics2D gc = (Graphics2D) plot.getGraphics();
//
//        gc.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
//        gc.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
//        gc.setRenderingHint(RenderingHints.KEY_RENDERING, RenderingHints.VALUE_RENDER_SPEED);
//        //gc.setRenderingHint(RenderingHints.KEY_RENDERING, RenderingHints.VALUE_RENDER_QUALITY);
//
//        computeTrafficRoutePlotArea(plot);
//
//        // Find the length of the longest site label which will be drawn below
//        // the x-axis. Use this to adjust the plot area.
//        double longestSiteLabel = Double.MIN_VALUE;
//        Iterator siteIterator = xLabels.iterator();
//        while(siteIterator.hasNext()) {
//            TrafficSite site = (TrafficSite) siteIterator.next();
//            String siteLabel = site.name;
//            if (siteLabel != null) {
//                double labelLength = gc.getFontMetrics().getStringBounds(siteLabel, gc).getWidth();
//
//                if (labelLength > longestSiteLabel) {
//                    longestSiteLabel = labelLength;
//                }
//            }
//        }
//        plotArea = new Rectangle2D.Double(plotArea.getMinX(), plotArea.getMinY(), (plotArea.getMaxX() - plotArea.getMinX() +1), (plotArea.getMaxY() - plotArea.getMinY() +1) - 1.5*longestSiteLabel);
//
//
//        drawPlotBackground(gc);
//        drawTrafficRouteAxes(gc, minDisplacement.doubleValue(), maxDisplacement.doubleValue(), minTrafficFlow.intValue(), maxTrafficFlow.intValue(), xAxisLabel, yAxisLabel, xLabels);
//
//        // Label the graph.
//        Font defaultFont = gc.getFont();
//        Font graphTitleFont = gc.getFont().deriveFont(Font.BOLD, gc.getFont().getSize() +3.0f);
//        gc.setFont(graphTitleFont);
//        if ( (graphTitle != null) && (graphTitle.trim().length() > 0) ) {
//            gc.drawString(graphTitle, (float) ((plotArea.getMinX() + plotArea.getMaxX())/2.0f - gc.getFontMetrics().getStringBounds(graphTitle, gc).getWidth()/2.0f), (float) (plotArea.getMinY()/2.0f /*- gc.getFontMetrics().getHeight()/2.0f */));
//        }
//        gc.setFont(defaultFont);
//
//        // Paint the traces. Note: they are painted in reverse order so that
//        // the newer traces overwrite the older ones.
//        List tracePaints = new ArrayList();
//        Paint tracePaint1 = new Color(1.0f, 0.0f, 0.0f, 1.0f);
//        tracePaints.add(tracePaint1);
//
//        Paint tracePaint2 = new Color(0.0f, 1.0f, 0.0f, 0.8f);
//        tracePaints.add(tracePaint2);
//
//        Paint tracePaint3 = new Color(0.0f, 0.0f, 1.0f, 0.6f);
//        tracePaints.add(tracePaint3);
//
//        List dash = new ArrayList();
//        float[] traceDash1 = { 1 };
//        float[] traceDash2 = {20, 5};
//        float[] traceDash3 = {20, 5, 5, 5};
//
//        dash.add(traceDash1);
//        dash.add(traceDash2);
//        dash.add(traceDash3);
//
//        List strokes = new ArrayList();
//        for (int count = 0; count < data.size(); ++count) {
//            float strokeWidth = 2.0f - count/2.0f;
//            Stroke stroke = new BasicStroke(strokeWidth, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND, 0.0f, (float[]) dash.get(count), 0.0f);
//            strokes.add(stroke);
//        }
//
//        for (int count = data.size() -1; count >= 0; --count) {
//            List dataSeries = (List) data.get(count);
//
//            gc.setPaint((Paint) tracePaints.get(count));
//            gc.setStroke((Stroke) strokes.get(count));
//            drawTrafficRouteTraces(gc, dataSeries, uniqueDisplacements);
//        }
//
//        // Draw the legend.
//        BufferedImage legend = drawLegend("Year", seriesNames, tracePaints, strokes, width);
//
//        // Combine the plot and legend.
//        int totalHeight = plot.getHeight() + legend.getHeight();
//
//        BufferedImage combinedPlot = new BufferedImage(width, totalHeight, BufferedImage.TYPE_INT_ARGB);
//        Graphics2D combinedPlotGraphics = combinedPlot.createGraphics();
//        Paint defaultPaint = combinedPlotGraphics.getPaint();
//        combinedPlotGraphics.setPaint(Color.WHITE);
//        combinedPlotGraphics.fillRect(0, 0, combinedPlot.getWidth(), combinedPlot.getHeight());
//        combinedPlotGraphics.setPaint(defaultPaint);
//        combinedPlotGraphics.drawImage(plot, 0, 0, width, plot.getHeight(), null);
//        combinedPlotGraphics.drawImage(legend, 0, plot.getHeight(), width, legend.getHeight(), null);
//
//        return combinedPlot;
//    }
//
//    /**
//     * Draws the axis for the traffic route plot.
//     *
//     * @param gc - the graphics to draw the axes on.
//     * @param minDisplacement - the minimum displacement measured on the highway.
//     * @param maxDisplacement - the maximum displacement measured on the highway.
//     * @param minTrafficFlow - the minimum traffic flow measured on the highway.
//     * @param maxTrafficFlow - the maximum traffic flow measured on the highway.
//     * @param xLabels - a List of TrafficSite object to label the x-axis with.
//     *
//     * @throws IllegalArgumentException - if gc is null.
//     */
//    public void drawTrafficRouteAxes(Graphics2D gc, double minDisplacement, double maxDisplacement, int minTrafficFlow, int maxTrafficFlow, String xAxisLabel, String yAxisLabel, List xLabels) throws IllegalArgumentException {
//        if (gc == null) {
//            throw new IllegalArgumentException("Cannot draw the axes of the traffic route plot as the graphics to draw on was null.");
//        }
//
//        drawTrafficRouteFlowAxis(gc, minTrafficFlow, maxTrafficFlow, yAxisLabel);
//        drawTrafficRouteDisplacementAxis(gc, minDisplacement, maxDisplacement, xAxisLabel, xLabels);
//
//        // Draw the top and right borders of the plot.
//        gc.drawLine( (int) (plotArea.getMinX() -1), (int) (plotArea.getMinY() -1), (int) plotArea.getMaxX() +1, (int) (plotArea.getMinY() -1) ); // draw the top line.
//        gc.drawLine( (int) (plotArea.getMaxX() +1), (int) (plotArea.getMinY() -1), (int) plotArea.getMaxX() +1, (int) (plotArea.getMaxY() +1) ); // draw the top line.
//    }
//
//
//    public void drawTrafficRouteFlowAxis(Graphics2D gc, int minTrafficFlow, int maxTrafficFlow, String axisLabel) throws IllegalArgumentException {
//        if (gc == null) {
//            throw new IllegalArgumentException("Cannot draw the traffic flow (vertical) axis of the traffic route plot as the graphics to draw on was null.");
//        }
//
//        // Draw the left (traffic flow) axis.
//        // Determine the maximum number of labels that will fit within the axis size.
//        FontMetrics metrics = gc.getFontMetrics(gc.getFont());
//        double minimumLabelSpacing = 1.5;      // minimum spacing between labels as fraction of label height.
//        int labelHeight = (int) (metrics.getHeight() * minimumLabelSpacing);
//
//        // Determine how many tick marks (with their associated labels) will fit
//        // in the axis length.
//        final double xAxisLength = plotArea.getMaxX() - plotArea.getMinX() +1;
//        final double yAxisLength = plotArea.getMaxY() - plotArea.getMinY() +1;
//        int numLabelsInLength = (int) Math.floor(yAxisLength / labelHeight);
//
//        // We always need a minimum of two tick marks (and therefore two labels)
//        // at the beginning and end of the axis.
//        if (numLabelsInLength < 2) {
//            numLabelsInLength = 2;
//        }
//
//        // Find the smallest step interval which can fit the specified number
//        // of labels (at each tick mark) and still span the entire data range.
//        int[] multipliers = { 1, 2, 4, 5 };
//        boolean foundInterval = false; // true when have found a good interval to display.
//        double interval = 0;           // size of the traffic-flow interval between tick marks.
//
//        // Find the maximum and minimum traffic values.
//        int lowestTraffic = minTrafficFlow;
//        int highestTraffic = maxTrafficFlow;
//        //final int trafficRange = highestTraffic - lowestTraffic +1;
//        final int trafficRange = highestTraffic; // make the range from 0 to highestTraffic
//
//        for (int order = 1; !foundInterval; order *= 10) {
//            for (int index = 0; !foundInterval && (index < multipliers.length); ++index) {
//                interval = multipliers[index]*order;
//
//                // If this interval can span the range of data then
//                // we have found the interval we wish to use.
//                if ( (interval*(numLabelsInLength -1)) >= trafficRange) {
//                    foundInterval = true;
//                }
//            }
//        }
//
//        // See how many intervals will actually be needed for this traffic range.
//        int numIntervals = (numLabelsInLength -1);
//
//        if (numIntervals < 1) {
//            numIntervals = 1;
//        }
//
//        for (int numTestIntervals = numIntervals; ((interval*numTestIntervals) >= trafficRange) && (numTestIntervals > 0); --numTestIntervals)  {
//            numIntervals = numTestIntervals;
//        }
//
//        int numTicks = numIntervals +1;
//
//
//
//        // Set the range of the traffic axis. Note that this is determined by the interval
//        // size and the number of intervals.
//        trafficFlowAxisSpan = interval*numIntervals;
//
//        double axisUpperLimit = trafficFlowAxisSpan;
//        if (axisUpperLimit < maxTrafficFlow) {
//            // Moving the lower limit to an interval boundary may have moved the upper limit to below
//            // the value of the maximum observed data. We may need to add one more interval.
//            ++numIntervals;
//
//            numTicks = numIntervals +1;
//            trafficFlowAxisSpan = interval*numIntervals;
//        }
//
//        // Draw the axis line. Note that this line is just to the left of the plotting area
//        // (hence the x position is plotArea.getMinX() -1). Also, the axis extends vertically
//        // above and below the plotting area by one pixel (so join with other bordering axes
//        // around the plotting area).
//        gc.setPaint(Color.BLACK);
//        gc.drawLine((int) plotArea.getMinX()-1, (int) plotArea.getMinY() -1, (int) plotArea.getMinX() -1, (int) plotArea.getMaxY()+1);
//
//        double tickLength = 7; //xAxisLength / 50.0;
//        double labelSpace = tickLength; // space between tick and the associated label.
//
//        double xMinLabel = Double.MAX_VALUE;
//
//        for (int tick = 0; tick < numTicks; ++tick) {
//            double y = trafficAxisPosition(tick*interval);
//
//            gc.drawLine((int) plotArea.getMinX() -1, (int) y, (int) (plotArea.getMinX() - 1 - tickLength), (int) y);
//
//            // Draw the label.
//            int tickValue = (int) (tick*interval);
//            String label = Integer.toString(tickValue);
//            double labelWidth = metrics.getStringBounds(label, gc).getWidth();
//
//            double x = plotArea.getMinX() -1 - tickLength - labelSpace - labelWidth;
//            gc.drawString(label, (float) x, (float) (y + metrics.getHeight()/2.0 - metrics.getDescent()));
//
//            // Remember the minimum x value of all the tickmark labels.
//            if (x < xMinLabel) {
//                xMinLabel = x;
//            }
//        }
//
//        // Draw the label axis. The x position of the label will be half-way between the edge
//        // of the graph and the tickmark label closest to the left edge.
//        Font defaultFont = gc.getFont();
//        Font graphAxisFont = gc.getFont().deriveFont(Font.BOLD, gc.getFont().getSize() +1.0f);
//        gc.setFont(graphAxisFont);
//        double axisLabelLength = metrics.getStringBounds(axisLabel, gc).getWidth();
//
//
//        final AffineTransform saved = (AffineTransform) gc.getTransform().clone(); // save the current affine transform.
//        final int rotationAngle = -90;
//        AffineTransform transform = (AffineTransform) gc.getTransform().clone();
//
//        //double xLabel = xMinLabel - labelSpace - labelHeight;
//        double xLabel = xMinLabel/2.0; // - labelSpace - labelHeight;
//        double yLabel = (plotArea.getMaxY()+plotArea.getMinY())/2.0 + axisLabelLength/2.0;
//        transform.translate(xLabel, yLabel);
//        transform.rotate(Math.toRadians(rotationAngle));
//        gc.setTransform(transform);
//        gc.drawString(axisLabel, 0, 0);
//        gc.setTransform(saved); // restore the original affine transform.
//        gc.setFont(defaultFont);
//    }
//
//    /**
//     * Draws the displacement (horizontal) axis of the traffic route plot.
//     *
//     * @param gc - the graphics to draw on.
//     * @param minDisplacement - the minimum measured displacement value along the highway.
//     * @param maxDisaplcement - the maximum measured displacement value along the highway.
//     * @param axisLabel - the axis label.
//     * @param xAxisLabels - a List of TrafficSite objects to use to label this axis.
//     */
//    public void drawTrafficRouteDisplacementAxis(Graphics2D gc, double minDisplacement, double maxDisplacement, String axisLabel, List xLabels) throws IllegalArgumentException {
//        if (gc == null) {
//            throw new IllegalArgumentException("Cannot draw the site displacement (horizontal) axis of the traffic route plot as the graphics to draw on was null.");
//        }
//
//        // Find the maximum and minimum displacements.
//        int lowestDisplacement = (int) Math.floor(minDisplacement);
//        int highestDisplacement = (int) Math.ceil(maxDisplacement);
//
//        final int range = highestDisplacement - lowestDisplacement +1; // make the range from 0 to highestTraffic
//
//        // Draw the bottom (site displacement) axis.
//        // Determine the maximum number of labels that will fit within the axis size.
//        FontMetrics metrics = gc.getFontMetrics(gc.getFont());
//        double minimumLabelSpacing = 1.5;      // minimum spacing between labels as fraction of the maximum label width.
//        int maxLabelHeight = (int) (metrics.getHeight() * minimumLabelSpacing);
//        int maxLabelWidth = (int) (metrics.getStringBounds(Integer.toString(highestDisplacement), gc).getWidth() * minimumLabelSpacing);
//
//        // Determine how many tick marks (with their associated labels) will fit
//        // in the axis length.
//        final double xAxisLength = plotArea.getMaxX() - plotArea.getMinX() +1;
//        final double yAxisLength = plotArea.getMaxY() - plotArea.getMinY() +1;
//        int numLabelsInLength = (int) Math.floor(xAxisLength / maxLabelWidth);
//
//        // We always need a minimum of two tick marks (and therefore two labels)
//        // at the beginning and end of the axis.
//        if (numLabelsInLength < 2) {
//            numLabelsInLength = 2;
//        }
//
//        // Find the smallest step interval which can fit the specified number
//        // of labels (at each tick mark) and still span the entire data range.
//        int[] multipliers = { 1, 2, 4, 5 };
//        boolean foundInterval = false; // true when have found a good interval to display.
//        double interval = 0;           // size of the traffic-flow interval between tick marks.
//
//
//        for (int order = 1; !foundInterval; order *= 10) {
//            for (int index = 0; !foundInterval && (index < multipliers.length); ++index) {
//                interval = multipliers[index]*order;
//
//                // If this interval can span the range of data then
//                // we have found the interval we wish to use.
//                if ( (interval*(numLabelsInLength -1)) >= range) {
//                    foundInterval = true;
//                }
//            }
//        }
//
//        // See how many intervals will actually be needed for this range.
//        int numIntervals = (numLabelsInLength -1);
//
//        if (numIntervals < 1) {
//            numIntervals = 1;
//        }
//
//        boolean foundNumIntervals = false;
//        for (int numTestIntervals = numIntervals; !foundNumIntervals && (numTestIntervals > 1); --numTestIntervals)  {
//            if ((interval*numTestIntervals) >= range) {
//                numIntervals = numTestIntervals;
//            } else {
//                foundNumIntervals = true;
//            }
//        }
//
//        int numTicks = numIntervals +1;
//
//        // Set the lower limit of the axis. The lower limit is the lowest tick value
//        // which encompasses the lowest value seen.
//        int numIncrementsBeforeLowest = (int) (minDisplacement / interval);
//        displacementAxisSpanOffset = numIncrementsBeforeLowest*interval;
//
//        // Set the range of the traffic axis. Note that this is determined by the interval
//        // size and the number of intervals.
//        displacementAxisSpan = interval*numIntervals;
//
//        double displacementAxisUpperLimit = displacementAxisSpanOffset + displacementAxisSpan;
//        if (displacementAxisUpperLimit < maxDisplacement) {
//            // Moving the lower limit to an interval boundary may have moved the upper limit to below
//            // the value of the maximum observed data. We may need to add one more interval.
//            ++numIntervals;
//
//            numTicks = numIntervals +1;
//            displacementAxisSpan = interval*numIntervals;
//        }
//
//        // Draw the axis line. Note that this line is just to the bottom of the plotting area
//        // (hence the x position is plotArea.getMinX() -1). Also, the axis extends horizontally
//        // to the left and right of the plotting area by one pixel (so join with other bordering axes
//        // around the plotting area).
//        gc.setPaint(Color.BLACK);
//        gc.drawLine((int) plotArea.getMinX()-1, (int) plotArea.getMaxY() +1, (int) plotArea.getMaxX() +1, (int) plotArea.getMaxY()+1);
//
//        double tickLength = 7; //xAxisLength / 50.0;
//        double labelSpace = tickLength; // space between tick and the associated label.
//
//        double yTickBase = plotArea.getMaxY() +1;
//
//        final double displacementAxisLength = plotArea.getMaxX() - plotArea.getMinX() +1; // axis length in pixels.
//
//        double yMaxLabel = Double.MIN_VALUE; // The greatest y value of any label (closest to bottom of the graph).
//
//        for (int tick = 0; tick < numTicks; ++tick) {
//            double displacement = displacementAxisSpanOffset + tick*interval; // the value of the displacement at this tick.
//            double x = plotArea.getMinX() + (tick*interval)*displacementAxisLength/displacementAxisSpan;
//
//            // Draw the tick itself.
//            gc.drawLine((int) x, (int) yTickBase, (int) x, (int) (yTickBase + tickLength));
//
//            // Draw the label.
//            String label = Integer.toString((int) displacement);
//            double labelWidth = metrics.getStringBounds(label, gc).getWidth();
//            double labelHeight = metrics.getHeight();
//
//            double xLabel = x - labelWidth/2;
//            double yLabel = yTickBase + tickLength + labelSpace + labelHeight;
//
//            gc.drawString(label, (float) xLabel, (float) yLabel);
//
//            // Remember the minimum x value of all the tickmark labels.
//            if (yLabel > yMaxLabel) {
//                yMaxLabel = yLabel;
//            }
//        }
//
//        // Draw the label axis. The y position of the label will be half-way between the edge
//        // of the graph and the tickmark label closest to the bottom edge.
//        Font defaultFont = gc.getFont();
//        Font graphAxisFont = gc.getFont().deriveFont(Font.BOLD, gc.getFont().getSize() +1.0f);
//        gc.setFont(graphAxisFont);
//
//        double axisLabelLength = metrics.getStringBounds(axisLabel, gc).getWidth();
//
//        double labelHeight = metrics.getHeight();
//        double axisLabelBottom = (float) (yMaxLabel + labelSpace + labelHeight);
//        gc.drawString(axisLabel, (float) (plotArea.getMinX() + (plotArea.getMaxX() - plotArea.getMinX() +1)/2), (float) axisLabelBottom);
//        gc.setFont(defaultFont);
//
//        // Now show the site labels below the axis.
//        double xMinLabel = Double.MIN_VALUE;
//        double xMaxLabel = Double.MIN_VALUE;
//        double xLastMinLabel = Double.MIN_VALUE;
//        double xLastMaxLabel = Double.MIN_VALUE;
//
//        Iterator labelIterator = xLabels.iterator();
//        while(labelIterator.hasNext()) {
//            TrafficSite site = (TrafficSite) labelIterator.next();
//
//           labelHeight = gc.getFontMetrics().getHeight();
//           double labelWidth = gc.getFontMetrics().getStringBounds(site.name, gc).getWidth();
//
//           double displacement = site.displacement;
//
//           double x = plotArea.getMinX() + (displacement - displacementAxisSpanOffset)*(plotArea.getMaxX() - plotArea.getMinX() +1)/displacementAxisSpan;
//           double y = axisLabelBottom + 2*labelSpace + labelWidth;
//
//           xMinLabel = x - labelHeight/2.0;
//           xMaxLabel = x + labelHeight/2.0;
//
//           // Only plot the label if it doesn't overlap the previous label.
//           if (xMinLabel > xLastMaxLabel) {
//               final AffineTransform saved = (AffineTransform) gc.getTransform().clone(); // save the current affine transform.
//               final int rotationAngle = -90;
//               AffineTransform transform = (AffineTransform) gc.getTransform().clone();
//               transform.setToTranslation(x, y);
//               transform.rotate(Math.toRadians(rotationAngle));
//               gc.setTransform(transform);
//               gc.drawString(site.name, 0, 0);
//               gc.setTransform(saved);
//
//               xLastMinLabel = xMinLabel;
//               xLastMaxLabel = xMaxLabel;
//           }
//        }
//
//    }
//
//    /**
//     * Draws a single trace on the specified plot using the current paint and stroke.
//     *
//     * @param gc - the graphics to draw on.
//     * @param dataSeries - a list of TrafficSiteData objects to plot.
//     * @param uniqueDisplacements - a list of Double objects giving the displacements of all measurement sites.
//     *
//     * @throws IllegalArgumentException - if gc is null.
//     * @throws IllegalArgumentExceptin - if dataSeries is null.
//     * @throws IllegalArgumentException - if uniqueDispacements is null.
//     */
//    public void drawTrafficRouteTraces(Graphics2D gc, List dataSeries, List uniqueDisplacements) throws IllegalArgumentException {
//        if (gc == null) {
//            throw new IllegalArgumentException("Cannnot draw a traffic route trace as the graphics to draw on was null.");
//        }
//        if (dataSeries == null) {
//            throw new IllegalArgumentException("Cannot draw a traffic route trace as the data series was null.");
//        }
//
//        // Ensure the unique displacements are sorted into ascending displacement order.
//        Collections.sort(uniqueDisplacements);
//
//        // Ensure the data is sorted into ascending displacement order.
//        Comparator dataDisplacementComparator = new Comparator() {
//            public int compare(Object object1, Object object2) {
//                Double displacement1 = null;
//                Double displacement2 = null;
//
//                if (object1 != null) {
//                    displacement1 = new Double(((TrafficSiteData) object1).displacement);
//                }
//
//                if (object2 != null) {
//                    displacement2 = new Double(((TrafficSiteData) object2).displacement);
//                }
//
//                if (displacement1 != null) {
//                    return displacement1.compareTo(displacement2);
//                } else {
//                    if (displacement2 != null) {
//                        return displacement2.compareTo(displacement1);
//                    } else {
//                        // Both objects were null.
//                        return 0;
//                    }
//                }
//           } };
//
//        Collections.sort(dataSeries, dataDisplacementComparator);
//
//        // Move through the measurement sites (unique displacements) and find the
//        // corresponding traffic flow value measured at that site. If there is
//        // no measurement at that site we skip to the next site.
//        boolean havePreviousValue = false; // true if we have a measurement at a previous displacement.
//        double lastX = 0.0;
//        double lastY = 0.0;
//
//        for (int count = 0; count < uniqueDisplacements.size(); ++count) {
//            double displacement = ((Double) uniqueDisplacements.get(count)).doubleValue();
//
//            // Find the corresponding measurement value at this location.
//            Integer trafficFlow = null;
//
//            boolean found = false;
//            for (int measurement = 0; !found && (measurement < dataSeries.size()); ++measurement) {
//                TrafficSiteData datum = (TrafficSiteData) dataSeries.get(measurement);
//
//                if (datum.displacement == displacement) {
//                    trafficFlow = datum.value;
//                    found = true;
//                }
//            }
//
//            if (trafficFlow != null) {
//                // We have a location.
//                double x = plotArea.getMinX() + (displacement - displacementAxisSpanOffset)*(plotArea.getMaxX() - plotArea.getMinX() +1.0)/displacementAxisSpan;
//                double flow = trafficFlow.doubleValue();
//                double y = trafficAxisPosition(flow);
//
//                // If we have a previous location then draw a line to this location, otherwise just move to this location.
//                if (havePreviousValue) {
//                    gc.drawLine((int) lastX, (int) lastY, (int) x, (int) y);
//                }
//
//                havePreviousValue = true; // remember this measurement was found.
//                lastX = x;
//                lastY = y;
//            } else {
//                havePreviousValue = false;
//            }
//        }
//    }
//
//    /**
//     * Draws a legend which describes the series displayed in a plot.
//     *
//     * @param legendTitle - the title of the legend.
//     * @param seriesNames - a List containing strings.
//     * @param seriesPaint - a List containing the Paint objects that were used to draw the series.
//     * @param seriesStrokes - a List containing the line Strokes that were used to draw the series.
//     * @param width - the width (in pixels) that the legend is to cover.
//     *
//     * @return a BufferedImage containing the legend.
//     */
//    public BufferedImage drawLegend(String legendTitle, List seriesNames, List seriesPaint, List seriesStrokes, int width) {
//        // Determine the height of a string in the default Font.
//        BufferedImage dummy = new BufferedImage(width, 100, BufferedImage.TYPE_INT_ARGB);
//        Graphics2D dummyGC = dummy.createGraphics();
//
//        final double graphTitleHeight = dummyGC.getFontMetrics().getStringBounds(legendTitle, dummyGC).getHeight();
//        final double graphTitleWidth = dummyGC.getFontMetrics().getStringBounds(legendTitle, dummyGC).getWidth();
//
//        // Create the image to draw on.
//        int height = (int) (5*graphTitleHeight);
//        BufferedImage image = new BufferedImage(width, height, BufferedImage.TYPE_INT_ARGB);
//        Graphics2D gc = image.createGraphics();
//
//        Paint  defaultPaint = gc.getPaint();
//        Stroke defaultStroke = gc.getStroke();
//
//        // Draw the series bars.
//        double seriesBorderSpacing = 20;   // The spacing between the border and series.
//        double seriesSpacing = 40;         // The spacing between series.
//        double seriesBarLength = 30;       // Length of the line segment to show,
//        double seriesBarLabelSpacing = 10; // Spacing between the series bar and its label.
//        double xTotal = plotArea.getMinX() + seriesBorderSpacing;
//
//
//
//        for (int count = 0; count < seriesNames.size(); ++count) {
//            String name = (String) seriesNames.get(count);
//            Paint paint = (Paint) seriesPaint.get(count);
//            Stroke stroke = (Stroke) seriesStrokes.get(count);
//
//            // Draw a line in the colour and dash style.
//            gc.setPaint(paint);
//            gc.setStroke(stroke);
//            gc.drawLine((int) xTotal, (int) (height/2.0f), (int) (xTotal + seriesBarLength), (int) (height/2.0f));
//            xTotal += seriesBarLength;
//
//            // Draw the series label.
//            gc.setPaint(Color.BLACK);
//            gc.setStroke(defaultStroke);
//
//            double labelHeight = gc.getFontMetrics().getStringBounds(name, gc).getHeight();
//            double labelWidth = gc.getFontMetrics().getStringBounds(name, gc).getWidth();
//            xTotal += seriesBarLabelSpacing;
//            gc.drawString(name, (float) xTotal, (float) (height/2.0f + labelHeight/2.0f));
//            xTotal += labelWidth;
//
//            if (count != (seriesNames.size() -1)) {
//                xTotal += seriesSpacing;
//            }
//        }
//        // Restore the default Stroke and Paint.
//        gc.setPaint(defaultPaint);
//        gc.setStroke(defaultStroke);
//
//        xTotal += seriesBorderSpacing;
//
//        // Draw the border around the image.
//        final double xMinBorder = plotArea.getMinX();
//        final double xMaxBorder = xTotal;
//        final double yMinBorder = graphTitleHeight;
//        final double yMaxBorder = 4*graphTitleHeight;
//
//        gc.setPaint(Color.LIGHT_GRAY);
//        gc.drawLine((int) xMinBorder, (int) yMaxBorder, (int) xMaxBorder, (int) yMaxBorder);
//        gc.drawLine((int) xMinBorder, (int) yMinBorder, (int) xMinBorder, (int) yMaxBorder);
//        gc.drawLine((int) xMaxBorder, (int) yMinBorder, (int) xMaxBorder, (int) yMaxBorder);
//
//        gc.drawLine((int) xMinBorder, (int) yMinBorder, (int) (xMinBorder + seriesBorderSpacing), (int) yMinBorder);
//        gc.drawLine((int) (xMinBorder + 2*seriesBorderSpacing + graphTitleWidth), (int) yMinBorder, (int) xMaxBorder, (int) yMinBorder);
//
//        // Draw the legend title.
//        gc.setPaint(Color.BLACK);
//        gc.drawString(legendTitle, (float) (xMinBorder + 1.5*seriesBorderSpacing), (float) (yMinBorder + graphTitleHeight/2.0) -1);
//
//        // Draw the background.
//        //BufferedImage backgroundImage = new BufferedImage(width, height, BufferedImage.TYPE_INT_ARGB);
//        gc.setComposite(AlphaComposite.getInstance(AlphaComposite.DST_OVER));
//        gc.setPaint(new GradientPaint((float)plotArea.getMinX(), (float) graphTitleHeight, new Color(1.0f, 1.0f, 1.0f, 0.0f), (float) plotArea.getMinX(), (float)(3*graphTitleHeight), new Color(1.0f, 1.0f, 1.0f, 1.0f)));
//        gc.fillRect((int) plotArea.getMinX(), (int) graphTitleHeight, (int) (xTotal - plotArea.getMinX()), (int) (3*graphTitleHeight));
//
//        return image;
//    }
}
