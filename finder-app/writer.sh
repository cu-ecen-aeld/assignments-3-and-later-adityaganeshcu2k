#!/bin/sh
# Writer script for assignment 1 takes 2 arguments :- 1. Full path to a file on filesystem 2. Text string which will be written within this file
# Author :- Aditya Ganesh
# Date :- 01/19/26

# Check argument count
if [ $# -ne 2 ]; then
    echo "ERROR: Invalid number of arguments"
    exit 1
fi

writefile=$1
writestr=$2


dirpath=$(dirname "$writefile")
mkdir -p "$dirpath"


if [ $? -ne 0 ]; then
    echo "ERROR: Could not create directory path"
    exit 1
fi


echo "$writestr" > "$writefile"


if [ $? -ne 0 ]; then
    echo "ERROR: Could not create file"
    exit 1
fi
