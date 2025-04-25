#!/bin/bash

# get dir path of this script
SCRIPT_DIR=$(cd "$(dirname "$0")";pwd)

# get last commit id
LAST_COMMIT=`git log -1 | head -n 1 | awk '{print $2}'`

# put it to file
FILE_PATH=${SCRIPT_DIR}/git_last_commit.txt

# remove existed file
if [ -f "${FILE_PATH}" ]; then
    rm -f ${FILE_PATH}
    echo "${FILE_PATH} removed"
fi

if [ -n "$para1" ]; then
    echo "get git commit log failed"
    echo -n "empty" > ${SCRIPT_DIR}/git_last_commit.txt
else
    echo -n ${LAST_COMMIT} > ${SCRIPT_DIR}/git_last_commit.txt
fi
