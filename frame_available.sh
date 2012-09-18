#!/bin/bash
DIR=`dirname $1`
FILE=${DIR}/ec04sim.dat
tsreduce update ${FILE} &> last_update.log

if [ "$?" -ne "0" ]
then
	echo "tsreduce update FAILED. See last_update.log"
fi

tsreduce plot ${FILE} &> last_plot.log
if [ "$?" -ne "0" ]
then
	echo "tsreduce plot FAILED. See last_plot.log"
fi