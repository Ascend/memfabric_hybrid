#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
CUR_DIR=$(dirname $(readlink -f $0))

function print()
{
    echo "[${1}] ${2}"
}

function delete_install_files()
{
    if [ -z "$1" ]; then
        return 0
    fi

    install_dir=$1
    print "INFO" "memfabric_hybrid $(basename $1) delete install files!"
    if [ -f ${install_dir} ]; then
        chmod 700 ${install_dir}
        rm -f ${install_dir}
    elif [ -d ${install_dir} ]; then
        chmod -R 700 ${install_dir}
        rm -rf ${install_dir}
    fi
}

function delete_latest()
{
    print "INFO" "memfabric_hybrid delete latest!"
    cd $1/..
    if [ -f "set_env.sh" ]; then
        chmod 700 set_env.sh
        rm -rf set_env.sh
    fi
    if [ -d "latest" ]; then
        chmod -R 700 latest
        rm -rf latest
    fi
}

function uninstall_process()
{
    if [ ! -d $1 ]; then
        return 0
    fi
    print "INFO" "memfabric_hybrid $(basename $1) uninstall start.."
    delete_latest $1
    delete_install_files $1
    mf_dir=$(cd $1/..;pwd)
    if [ -z "$(ls $mf_dir)" ]; then
        chmod -R 700 $mf_dir
        rm -rf $mf_dir
    fi

    pip_path=$(which pip3 2>/dev/null)
    if [ -z "$pip_path" ]; then
        print "WARNING" "memfabric_hybrid  pip3 Not Found, skip uninstall wheel package."
    else
        pip3 uninstall -y mf_smem
    fi
    print "INFO" "memfabric_hybrid $(basename $1) uninstall success!"
}

install_dir=${CUR_DIR}
uninstall_process ${install_dir}