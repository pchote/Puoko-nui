#!/bin/bash

# Path to online reduction file to update
# By default, look for the given name in the frame directory
DIR=`dirname $1`
FILE=${DIR}/undefined.dat

# Do the reduction if the file is found
if [ -f ${FILE} ]; then
	tsreduce update ${FILE} &> last_update.log
	if [ "$?" != '0' ]; then
		echo 'tsreduce update FAILED. See last_update.log'
		exit 1
	fi

	# Windows does not offer a persistent plot device,
	# so plot images which are then displayed in a web page
	PLOTDEVICE=''
	if [[ "${OSTYPE}" == 'msys' ]]; then
		PLOTDEVICE='online_ts.gif/gif online_dft.gif/gif'
	fi

	tsreduce plot ${FILE} ${PLOTDEVICE} &> last_plot.log
	if [ "$?" != '0' ]; then
		echo 'tsreduce plot FAILED. See last_plot.log'
	fi
fi