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
    echo "-- ${FILE_PATH} removed"
fi

# echo commit id to file
if [ -n "${LAST_COMMIT}" ]; then
    echo -n ${LAST_COMMIT} > ${FILE_PATH}
    FILE_CONTENT=`cat ${FILE_PATH}`
    echo "-- get git commit log successfully, commit id ${FILE_CONTENT}"
else
    echo -n "empty" > ${SCRIPT_DIR}/git_last_commit.txt
    FILE_CONTENT=`cat ${FILE_PATH}`
    echo "-- get git commit log failed, use ${FILE_CONTENT}"
fi
