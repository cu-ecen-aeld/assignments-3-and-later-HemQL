#!/bin/sh

if [ $# -lt 2 ]
then
	echo "Not enough arguments"
	exit 1
fi
FILESDIR=$1
SEARCHSTR=$2
if [ -d "$FILESDIR" ]
then
	:
else
	echo "Failed to find directory $FILESDIR"
	exit 1
fi


NUMFILES=$( grep -c "$SEARCHSTR" ${FILESDIR}/*.txt | grep -c ".txt")
NUMLINES=$( cat ${FILESDIR}/*.txt | grep -c "$SEARCHSTR")

echo "The number of files are ${NUMFILES} and the number of matching lines are ${NUMLINES}\n"
exit 0

