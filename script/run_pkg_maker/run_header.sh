#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# This file is a part of the CANN Open Software.
# Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
lines=50  # 本文件行数，行尾空行不能删

SCRIPT_PATH=$(readlink -f "$0") || { echo "readlink failed"; exit 1; }
TMP_DIR=$(dirname "$SCRIPT_PATH")

while true; do
    RAND_VAL=$RANDOM
    FILE_NAME="tmp_mf_pkg_${RAND_VAL}"
    TARGET_DIR="${TMP_DIR}/${FILE_NAME}"
    ARCHIVE_PATH="${TARGET_DIR}.tar.gz"

    if [ ! -e "$TARGET_DIR" ] && [ ! -e "$ARCHIVE_PATH" ]; then
        break
    fi

    sleep 0.1
done

tail -n +$lines "$0" > "$ARCHIVE_PATH"

mkdir -p "$TARGET_DIR"
if [ $? -ne 0 ]; then
    echo "Failed to create directory: $TARGET_DIR"
    rm -f "$ARCHIVE_PATH"
    exit 1
fi

tar -xf "$ARCHIVE_PATH" -C "$TARGET_DIR"
if [ $? -ne 0 ]; then
    echo "Failed to extract archive: $ARCHIVE_PATH"
    rm -rf "$TARGET_DIR" "$ARCHIVE_PATH"
    exit 1
fi

bash ${TARGET_DIR}/memfabric_hybrid/script/install.sh $*

EXIT_CODE=$?

rm -rf "$TARGET_DIR" "$ARCHIVE_PATH"

exit $EXIT_CODE
