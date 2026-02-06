#!/bin/sh
# Tester script for assignment 1 and assignment 2
# Author: Siddhant Jajoo

set -e
set -u

NUMFILES=10
WRITESTR=AELD_IS_FUN
WRITEDIR=/tmp/aeld-data

USERNAME=$(cat /etc/finder-app/conf/username.txt)
ASSIGNMENT=$(cat /etc/finder-app/conf/assignment.txt)

if [ $# -lt 3 ]
then
	echo "Using default value ${WRITESTR} for string to write"
	if [ $# -lt 1 ]
	then
		echo "Using default value ${NUMFILES} for number of files to write"
	else
		NUMFILES=$1
	fi
else
	NUMFILES=$1
	WRITESTR=$2
	WRITEDIR=/tmp/aeld-data/$3
fi

MATCHSTR="The number of files are ${NUMFILES} and the number of matching lines are ${NUMFILES}"

echo "Writing ${NUMFILES} files containing string ${WRITESTR} to ${WRITEDIR}"

rm -rf "${WRITEDIR}"

if [ "$ASSIGNMENT" != "assignment1" ]
then
	mkdir -p "$WRITEDIR"
	if [ ! -d "$WRITEDIR" ]; then
		exit 1
	fi
fi

# writer is expected to be in PATH (/usr/bin)
for i in $(seq 1 $NUMFILES)
do
	writer "$WRITEDIR/${USERNAME}$i.txt" "$WRITESTR"
done

# finder.sh is expected to be in PATH (/usr/bin)
OUTPUTSTRING=$(finder.sh "$WRITEDIR" "$WRITESTR")

# Write output for assignment 4 requirement
echo "$OUTPUTSTRING" > /tmp/assignment4-result.txt

# Cleanup
rm -rf /tmp/aeld-data

echo "$OUTPUTSTRING" | grep "${MATCHSTR}" >/dev/null
if [ $? -eq 0 ]; then
	echo "success"
	exit 0
else
	echo "failed: expected ${MATCHSTR} in ${OUTPUTSTRING}"
	exit 1
fi
