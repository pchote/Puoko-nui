#!/bin/bash
DIR=`dirname $1`
FILE=${DIR}/undefined.dat
tsreduce update ${FILE} 1> /dev/null 2> /dev/null
tsreduce plot ${FILE} 1> /dev/null 2> /dev/null
