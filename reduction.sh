#!/bin/bash

# Load configuration overrides if defined
if [ -f config.sh ]; then source ./config.sh; fi
if [ -z "${REDUCTION_FILENAME}" ]; then REDUCTION_FILENAME="undefined.dat"; fi
if [ -z "${REDUCTION_PLOTSIZE}" ]; then REDUCTION_PLOTSIZE="9"; fi

# Path to online reduction file to update
# By default, look for the given name in the frame directory
DIR=`dirname $1`
FILE=${DIR}/${REDUCTION_FILENAME}

# Do the reduction if the file is found
if [ -f ${FILE} ]; then
	tsreduce update ${FILE}
	if [ "$?" != '0' ]; then
		echo 'tsreduce update FAILED.'
		exit 1
	fi

	# Save to a temporary file then move to make the save look like an atomic operation wrt the preview html
	tsreduce plot ${FILE} online_ts_temp.gif/gif ${REDUCTION_PLOTSIZE} online_dft_temp.gif/gif ${REDUCTION_PLOTSIZE}
	if [ "$?" == '0' ]; then
		mv online_ts_temp.gif online_ts.gif
		mv online_dft_temp.gif online_dft.gif
	else
		echo 'tsreduce plot FAILED.'
	fi
fi