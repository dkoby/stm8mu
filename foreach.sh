#!/bin/sh

####################################
#
#
####################################

if [ $# -gt 1 ]; then
    ARGS=("$@")
    DIRS=${ARGS[@]:1}

    for dir in ${DIRS}; do
        make -C ${dir} $1
        RET=$?
        if [ $RET -ne 0 ]; then
            exit $RET
        fi
    done
fi

