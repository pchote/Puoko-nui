#!/usr/bin/env python

# Copyright 2007-2010 The Authors (see AUTHORS)
# This file is part of Rangahau, which is free software. It is made available
# to you under the terms of version 3 of the GNU General Public License, as
# published by the Free Software Foundation. For more information, see LICENSE.


"""Test plot script"""
import os
import time
import calendar
import fnmatch
import sys
import matplotlib.pyplot as plt
    
def main():
    plt.figure(0)
    if len(sys.argv) >= 1:
        os.chdir(sys.argv[1])
        files = os.listdir('.')
        x = []
        data = []
        
        first = True
        firstt = -1
        for filename in fnmatch.filter(files, "data-*.dat"):
            f = open(filename, "r")
            temp = []
            for line in f.readlines():
                if line[0] is '#':
                    continue
                filename, date, starttime, exptime, star, sky = line.split()
                temp.append(float(star))
                if first is True:
                    t = time.strptime("{0} {1}".format(date,starttime), "%Y-%m-%d %H:%M:%S.%f")
                    if (firstt == -1):
                        firstt = t
                    
                    x.append(calendar.timegm(t) - calendar.timegm(firstt))
            
            data.append(temp)
            first = False
        
        for y in data:
            plt.plot(x[:len(y)], y, 'x')
        
        plt.figure(1)
        
        # Plot the target divided by the average of the comparison
        # Dirty hack
        display = []
        for i in range(0,len(data[0]),1):
            div = 0
            for j in range(1,len(data),1):
                div += data[j][i]
            div /= len(data)-1
            display.append(0 if div < 1 else data[0][i] / div)
        # 
        f = open("output.dat", "w")
        for i in range(0,len(display),1):
            f.write("{0} {1}\n".format(x[i],display[i]))

        f.close()
        plt.plot(x, display, 'x')
        plt.show()

if __name__ == '__main__':
    main()
