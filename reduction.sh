#!/bin/bash
# Reduction script called by puokonui
# reduction.sh (true|false) frame-0001.fits.gz [...]
#
# The first argument gives the state of the Reduction button
# The second and subsequent arguments give the frames that
# have been saved since the script last ran

FRAMES=${@:2}
REDUCE=$1

# Load configuration overrides if defined
if [ -f config.sh ]; then source ./config.sh; fi
if [ -z "${REDUCTION_PLOTSIZE}" ]; then REDUCTION_PLOTSIZE="9"; fi
if [ -z "${REDUCTION_FILENAME}" ]; then
	# Extract the run prefix from the first frame argument
	PREFIX=$(basename $2)
	PREFIX=(${PREFIX//-/ })
	unset PREFIX[${#PREFIX[@]}-1]
	PREFIX=$(IFS="-"; echo "${PREFIX[*]}")
	REDUCTION_FILENAME=${PREFIX}".dat";
fi

# Do any post-processing that should be applied to all frames
# gunzip ${FRAMES}

# Reduction is disabled
if [ "${REDUCE}" = "false" ]; then
	exit 0;
fi

# Path to online reduction file to update in the frame directory
DIR=`dirname $2`
FILE=${DIR}/${REDUCTION_FILENAME}

# Reduction file not found
if [ ! -f ${FILE} ]; then
	echo ${FILE} "not found."
	exit 1;
fi

# Update the reduction
tsreduce update ${FILE}
if [ "$?" != '0' ]; then
	echo 'tsreduce update FAILED.'
	exit 1
fi

if [ "${PREFIX}" == "focus" ]; then
	tsreduce focus-plot ${FILE} online_focus_temp.gif/png ${REDUCTION_PLOTSIZE}
	if [ "$?" == '0' ]; then
		mv online_focus_temp.gif online_ts.gif
		if [ -f "online_dft.gif" ]; then
			rm online_dft.gif
		fi
	else
		echo 'tsreduce focus-plot FAILED.'
	fi
else
	tsreduce plot ${FILE} online_ts_temp.gif/png online_dft_temp.gif/png ${REDUCTION_PLOTSIZE}
	if [ "$?" == '0' ]; then
		mv online_ts_temp.gif online_ts.gif
		mv online_dft_temp.gif online_dft.gif
	else
		echo 'tsreduce plot FAILED.'
	fi
fi
