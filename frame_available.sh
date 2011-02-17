#!/bin/bash
DIR=`dirname $1`
python ./reduction/reduction.py ${DIR:1}/
python ./reduction/plot.py ${DIR:1}/
