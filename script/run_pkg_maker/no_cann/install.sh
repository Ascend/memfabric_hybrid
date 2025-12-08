#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
install_flag=y
uninstall_flag=n
install_path_flag=n
install_for_all_flag=n
nocheck=n
script_dir=$(dirname $(readlink -f "$0"))
version1="none"
pkg_arch="none"
os1="none"
default_install_dir="/usr/local/memfabric_hybrid"
echo "USE_CANN is OFF"
function print_help() {
    echo "--install-path=<path>             Install to specific dir"
    echo "--uninstall                       Uninstall product"
    echo "--install-for-all                 Install for all user"
    echo "--no-check                        Skip check during installation"
}

function print()
{
    echo "[${1}] ${2}"
}

function get_version_in_file()
{
    if [ -f ${script_dir}/../version.info ]; then
        version1=`cat ${script_dir}/../version.info | awk -F ':' '$1=="Version" {print $2}'`
        pkg_arch=`cat ${script_dir}/../version.info | awk -F ':' '$1=="Platform" {print $2}'`
        os1=`cat ${script_dir}/../version.info | awk -F ':' '$1=="Kernel" {print $2}'`
    fi
    print "INFO" "memfabric_hybrid version: ${version1} arch: ${pkg_arch} os: ${os1}"
}

function chmod_authority()
{
    chmod_file ${default_install_dir}
    chmod_file ${install_dir}
    local file_rights=$([ "${install_for_all_flag}" == "y" ] && echo 555 || echo 550)
    chmod ${file_rights} ${install_dir}/uninstall.sh
    chmod_dir ${default_install_dir} "550"
    chmod_dir ${install_dir} "550"
    local path_rights=$([ "${install_for_all_flag}" == "y" ] && echo 755 || echo 750)
    chmod ${path_rights} ${default_install_dir}
    chmod ${path_rights} ${install_dir}
}

function chmod_file()
{
    chmod_recursion ${1} "550" "file" "*.sh"
    chmod_recursion ${1} "440" "file" "*.bin"
    chmod_recursion ${1} "440" "file" "*.h"
    chmod_recursion ${1} "440" "file" "*.info"
    chmod_recursion ${1} "440" "file" "*.so"
    chmod_recursion ${1} "440" "file" "*.a"
    chmod_recursion ${1} "640" "file" "*.conf"
}

function chmod_dir()
{
    chmod_recursion ${1} ${2} "dir"
}

function chmod_recursion()
{
    local parameter2=$2
    local rights="$(echo ${parameter2:0:2})""$(echo ${parameter2:1:1})"
    rights=$([ "${install_for_all_flag}" == "y" ] && echo ${rights} || echo $2)
    if [ "$3" = "dir" ]; then
        find $1 -type d -exec chmod ${rights} {} \; 2>/dev/null
    elif [ "$3" = "file" ]; then
        find $1 -type f -name "$4" -exec chmod ${rights} {} \; 2>/dev/null
    fi
}

function parse_script_args()
{
    while true
    do
        case "$1" in
        --install-path=*)
            install_path_flag=y
            target_dir=$(echo $1 | cut -d"=" -f2-)
            target_dir=${target_dir}/memfabric_hybrid
            shift
        ;;
        --uninstall)
            uninstall_flag=y
            shift
        ;;
        --install-for-all)
            install_for_all_flag=y
            shift
        ;;
        --help)
            print_help
            exit 0
        ;;
        --no-check)
            nocheck=y
            shift
        ;;
        --*)
            shift
        ;;
        *)
            break
        ;;
        esac
    done
}

function check_owner()
{
    local cur_owner=$(whoami)

    if [[ "${cur_owner}" != "root" && "${install_flag}" == "y" ]]; then
        default_install_dir="${HOME}/memfabric_hybrid"
    fi

    if [ "${install_path_flag}" == "y" ]; then
        default_install_dir="${target_dir}"
    fi
    print "INFO" "Check owner success, USE_CANN is true."
}

function delete_install_files()
{
    if [ -z "$1" ]; then
        return 0
    fi

    install_dir=$1
    print "INFO" "memfabric_hybrid $(basename $1) delete install files!"
    if [ -d ${install_dir} ]; then
        chmod -R 700 ${install_dir}
        rm -rf ${install_dir}
    elif [ -f ${install_dir} ]; then
        chmod 700 ${install_dir}
        rm -f ${install_dir}
    fi
}

function delete_latest()
{
    cd $1/..
    print "INFO" "memfabric_hybrid delete latest!"
    if [ -d "latest" ]; then
        chmod -R 700 latest
        rm -rf latest
    fi
    if [ -f "set_env.sh" ]; then
        chmod 500 set_env.sh
        rm -rf set_env.sh
    fi
}

function uninstall_process()
{
    if [ ! -d $1 ]; then
        return 0
    fi
    print "INFO" "memfabric_hybrid $(basename $1) uninstall start!"
    mf_dir=$(cd $1/..;pwd)
    delete_latest $1
    delete_install_files $1
    if [ "$2" == "y" -a -z "$(ls $mf_dir)" ]; then
        chmod -R 700 $mf_dir
        rm -rf $mf_dir
    fi
    print "INFO" "memfabric_hybrid $(basename $1) uninstall success!"
}

function uninstall()
{
    install_dir=${default_install_dir}/${version1}
    uninstall_process ${install_dir} y
}

function check_arch()
{
    # get arch
    if [ $( uname -m | grep -c -i "x86_64" ) -ne 0 ]; then
        local_arch="x86_64"
    elif [ $( uname -m | grep -c -i "aarch64" ) -ne 0 ]; then
        local_arch="aarch64"
    else
        print "ERROR" "it is not system of x86_64 or aarch64"
        exit 1
    fi

    if [ "${local_arch}" != "${pkg_arch}" ]; then
        print "ERROR" "Install failed, pkg_arch: ${pkg_arch}, os arch: ${local_arch}"
        exit 1
    fi

    local_os=$(uname -s | awk '{print tolower($0)}')
    if [ "${local_os}" != "${os1}" ]; then
        print "ERROR" "Install failed, pkg_os: ${os1}, os arch: ${local_os}"
        exit 1
    fi
}

function check_path()
{
    parentPath=$(dirname $(dirname ${default_install_dir}))
    if [ ! -d "${parentPath}" ];then
        print "ERROR" "install path ${parentPath} not exists, runpackage only support create one level of directory,need create $parentPath."
        exit 1
    fi
    username=$(whoami)
    # Run permission check as current user: $(whoami)
    if [ ! -x "${parentPath}" ]; then
        print "ERROR" "The ${username} do not have the permission to access ${parentPath}, please reset the directory to a right permission."
        exit 1
    fi

    install_dir=$1
    if [ ! -d ${install_dir} ]; then
        mkdir -p ${install_dir}
        if [ ! -d ${install_dir} ]; then
            print "ERROR" "Install failed, create ${install_dir} failed"
            exit 1
        fi
    fi
}

function install_wheel_package() {
    wheel_dir="$1"
    wheel_name="$2"
    python_version="$3"
    if [ -z ${wheel_dir} ]; then
        print "ERROR" "invalid wheel package directory, skip install wheel."
        return
    fi
    if [ -z "${wheel_name}" ]; then
        print "ERROR" "empty wheel package name, skip install wheel."
        return
    fi
    if [ -z "${python_version}" ]; then
        print "ERROR" "empty python version, skip install wheel."
        return
    fi

    wheel_package=$(find "${wheel_dir}" -name "${wheel_name}-${version1}-cp${python_version}*" -print -quit)
    if [ -z "${wheel_package}" ]; then
        print "WARNING" "not found wheel package ${wheel_name} for python-${python_version}, skip install wheel."
        return
    fi

    pip3 install "${wheel_package}" --force-reinstall
}

function install_to_path()
{
    install_dir=${default_install_dir}/${version1}
    if [ -d ${install_dir} ]; then
        print "INFO" "The installation directory exists, uninstall first"
    fi
    uninstall_process ${install_dir}
    check_path ${install_dir}

    cd ${install_dir}
    cp -r ${script_dir}/../${pkg_arch}-${os1} ${install_dir}/
    cp -r ${script_dir}/uninstall.sh ${install_dir}/
    cp -r ${script_dir}/../version.info ${install_dir}/

    cd ${default_install_dir}
    ln -snf ${version1} latest

    pip_path=$(which pip3 2>/dev/null)
    if [ -z "$pip_path" ]; then
        print "WARNING" "pip3 Not Found, skip install wheel package."
        return
    fi

    wheel_dir="${install_dir}"/"${pkg_arch}"-"${os1}"/wheel
    python_version=$(python3 -c "import sys; print(''.join(map(str, sys.version_info[:2])))")

    install_wheel_package "${wheel_dir}" memfabric_hybrid "${python_version}"
}

function generate_set_env()
{
    touch ${default_install_dir}/set_env.sh
    cat>>${default_install_dir}/set_env.sh<<EOF
export MEMFABRIC_HYBRID_HOME_PATH=${default_install_dir}/latest
export LD_LIBRARY_PATH=${default_install_dir}/latest/${pkg_arch}-${os1}/lib64:\$LD_LIBRARY_PATH
export PATH=${default_install_dir}/latest/${pkg_arch}-${os1}/bin:\$PATH
EOF
}

function install_process()
{
    if [ -n "${target_dir}" ]; then
        if [[ ! "${target_dir}" = /* ]]; then
            print "ERROR" "Install failed, [ERROR] use absolute path for --install-path argument"
            exit 1
        fi
    fi

    print "INFO" "memfabric_hybrid start install into ${default_install_dir}"
    install_to_path
    generate_set_env
}

function main()
{
    parse_script_args $*
    get_version_in_file
    if [ "$uninstall_flag" == "y" ]; then
        uninstall
    elif [ "$install_flag" == "y" ] || [ "$install_path_flag" == "y" ]; then
        if [ "$nocheck" == "y" ]; then
            print "INFO" "skip check arch and owner."
        else
            check_arch
            check_owner
        fi

        install_process
        chmod_authority
        print "INFO" "memfabric_hybrid install success"
    fi
}

main $*
exit 0