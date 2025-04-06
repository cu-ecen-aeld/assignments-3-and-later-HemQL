#!/bin/bash

if [ $# -lt 2 ]
then
	echo "Not enough arguments"
	exit 1;
else
	WRITEFILE=$1
	WRITESTR=$2
	mkdir -p "${WRITEFILE%/*}"
	echo "${WRITESTR}" > ${WRITEFILE}
	exit 0
	
        
fi

