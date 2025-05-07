#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
lines=24 #本文件行数,行尾空行不能删

TMP_DIR=$(dirname $(readlink -f "$0"))
while true
do
  RAND_VAL=$RANDOM
  FILE_NMAE=tmp_mf_pkg_${RAND_VAL}
  if [ ! -d "${TMP_DIR}/${FILE_NMAE}" ] && [ ! -f "${TMP_DIR}/${FILE_NMAE}.tar.gz" ]; then
      break
  fi
done

tail -n +$lines $0 > ${TMP_DIR}/${FILE_NMAE}.tar.gz
mkdir ${TMP_DIR}/${FILE_NMAE}
tar -xf ${TMP_DIR}/${FILE_NMAE}.tar.gz -C ${TMP_DIR}/${FILE_NMAE}

bash ${TMP_DIR}/${FILE_NMAE}/mxc-memfabric_hybrid/script/install.sh $*

rm -rf ${TMP_DIR}/${FILE_NMAE}.tar.gz
rm -rf ${TMP_DIR}/${FILE_NMAE}
exit 0
