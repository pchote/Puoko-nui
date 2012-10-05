#!/bin/bash


# Load configuration overrides if defined
if [ -f config.sh ]; then source ./config.sh; fi
if [ -z "${REDUCTION_FILENAME}" ]; then REDUCTION_FILENAME="undefined.dat"; fi
if [ -z "${REDUCTION_PLOTOPTIONS}" ]; then REDUCTION_PLOTOPTIONS=""; fi

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

	tsreduce plot ${FILE} ${REDUCTION_PLOTOPTIONS}
	if [ "$?" != '0' ]; then
		echo 'tsreduce plot FAILED.'
	fi
fi