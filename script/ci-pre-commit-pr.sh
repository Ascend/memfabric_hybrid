#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

set -euo pipefail

# 配置参数
TARGET_BRANCH=${TARGET_BRANCH:-develop}

# 脚本标题
echo "================================================"
echo "          Pre-Commit CI 增量检查"
echo "================================================"

# 输出目标分支信息
echo "[INFO] 目标分支: ${TARGET_BRANCH}"

# 配置Git中文文件名支持
echo "[INFO] 配置 Git 中文文件名支持"
git config core.quotePath false

# 拉取远程目标分支
echo -e "\n[INFO] 拉取远程分支"
echo "[COMMAND] git fetch origin ${TARGET_BRANCH}"
git fetch origin "${TARGET_BRANCH}"

# 获取变更文件列表
echo -e "\n[INFO] 获取变更文件列表"
echo "[COMMAND] git diff --name-only --diff-filter=ACMR origin/${TARGET_BRANCH} HEAD"
FILES_ARR=($(git diff --name-only --diff-filter=ACMR origin/${TARGET_BRANCH} HEAD | sort -u))

# 无变更文件直接退出
if [ ${#FILES_ARR[@]} -eq 0 ]; then
  echo "[INFO] 无变更文件，检查通过"
  exit 0
fi

# 输出变更文件信息
echo -e "\n[INFO] 变更文件数量: ${#FILES_ARR[@]}"
echo "[INFO] 变更文件列表:"
for f in "${FILES_ARR[@]}"; do echo "  $f"; done

# 安装pre-commit工具
echo -e "\n[INFO] 安装 pre-commit"
echo "[COMMAND] pip install pre-commit"
pip install pre-commit

# 执行pre-commit检查
echo -e "\n[INFO] 开始 pre-commit 检查"
echo "[COMMAND] pre-commit run --files ${FILES_ARR[*]}"
set +e
pre-commit run --files "${FILES_ARR[@]}"
CODE=$?
set -e

# 输出检查结果
echo -e "\n================================================================"
if [ ${CODE} -eq 0 ]; then
  echo "[INFO] pre-commit 检查全部通过"
else
  echo "[ERROR] pre-commit 检查失败"
  echo "[INFO] 请在本地执行以下命令修复后重新提交:"
  echo ""
  echo "1. 安装/初始化环境"
  echo "pip install pre-commit && pre-commit install --install-hooks"
  echo ""
  echo "2. 检查并修复变更文件"
  echo "pre-commit run --files ${FILES_ARR[*]}"
fi
echo "================================================================"

exit ${CODE}
