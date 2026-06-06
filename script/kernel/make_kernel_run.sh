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
#
# make_kernel_run.sh - Produce a self-extracting .run package for AICPU kernel artifacts.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
OUTPUT_DIR="${PROJECT_ROOT}/output/hybm/aicpu_kernel"
RUN_OUTPUT_DIR="${PROJECT_ROOT}/output"
PACKAGE_NAME="cann-hybm-compat.tar.gz"
JSON_NAME="libcann_hybm_kernel.json"
INSTALL_SCRIPT="install.sh"
VERSION_FILE="cann_hybm_kernel_version"
RUN_NAME="memfabric_hybrid_aicpu_kernel.run"

if [[ ! -f "${OUTPUT_DIR}/${PACKAGE_NAME}" || ! -f "${OUTPUT_DIR}/${JSON_NAME}" || ! -f "${OUTPUT_DIR}/${INSTALL_SCRIPT}" || ! -f "${OUTPUT_DIR}/${VERSION_FILE}" ]]; then
    echo "Error: AICPU kernel artifacts not found in ${OUTPUT_DIR}" >&2
    echo "Required files: ${PACKAGE_NAME}, ${JSON_NAME}, ${INSTALL_SCRIPT}, ${VERSION_FILE}" >&2
    exit 1
fi

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "${WORK_DIR}"' EXIT

# Copy artifacts into work directory
cp "${OUTPUT_DIR}/${PACKAGE_NAME}" "${WORK_DIR}/"
cp "${OUTPUT_DIR}/${JSON_NAME}" "${WORK_DIR}/"
cp "${OUTPUT_DIR}/${INSTALL_SCRIPT}" "${WORK_DIR}/"
cp "${OUTPUT_DIR}/${VERSION_FILE}" "${WORK_DIR}/"

# Build the self-extracting header script.
# The placeholder "lines=XX" is replaced below with the actual header line count.
HEADER="${WORK_DIR}/header.sh"
cat > "${HEADER}" << 'HEADER_EOF'
#!/bin/bash
set -euo pipefail
lines=XX

SCRIPT_PATH=$(readlink -f "$0") 2>/dev/null || { echo "readlink failed" >&2; exit 1; }

# ---- Argument parsing ----
EXTRACT_DIR=""
NOEXEC=0
PASSTHROUGH=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --extract=*)
            EXTRACT_DIR="${1#*=}"
            shift
            ;;
        --noexec)
            NOEXEC=1
            shift
            ;;
        --help|-h|--version|--check|--list|--uninstall|--install|--install-for-all|--force)
            PASSTHROUGH+=("$1")
            shift
            ;;
        --extract)
            echo "Error: --extract requires a value (use --extract=<path>)" >&2
            exit 1
            ;;
        *)
            PASSTHROUGH+=("$1")
            shift
            ;;
    esac
done

# ---- Create temporary working directory under TMPDIR or /tmp ----
WORKDIR=$(mktemp -d "${TMPDIR:-/tmp}/tmp_kernel_pkg_$$_XXXXXX") || {
    echo "Error: mktemp failed" >&2
    exit 1
}
ARCHIVE_PATH="${WORKDIR}/payload.tar.gz"

# ---- Cleanup handler: removes temp dir on any exit ----
cleanup() {
    rm -rf "$WORKDIR"
}
trap cleanup EXIT

# ---- Extract payload archive from the end of this script ----
tail -n +$lines "$0" > "$ARCHIVE_PATH"
tar -xf "$ARCHIVE_PATH" -C "$WORKDIR"

# ---- Detect inquiry commands (need extraction before delegation) ----
IS_INQUIRY=0
for arg in "${PASSTHROUGH[@]}"; do
    if [[ "$arg" == "--help" || "$arg" == "-h" || "$arg" == "--version" || "$arg" == "--list" || "$arg" == "--check" ]]; then
        IS_INQUIRY=1
        break
    fi
done

# ---- Handle inquiry commands by delegating to install.sh ----
if [[ $IS_INQUIRY -eq 1 ]]; then
    if [[ "${PASSTHROUGH[0]}" == "--help" || "${PASSTHROUGH[0]}" == "-h" ]]; then
        bash "${WORKDIR}/install.sh" --help 2>&1 | sed '1s/^Usage: install.sh/Usage: memfabric_hybrid_aicpu_kernel.run/'
    else
        bash "${WORKDIR}/install.sh" "${PASSTHROUGH[@]}"
    fi
    rc=$?
    exit $rc
fi

# ---- --noexec must be used with --extract=<path> ----
if [[ $NOEXEC -eq 1 && -z "$EXTRACT_DIR" ]]; then
    echo "Error: --noexec must be used with --extract=<path>" >&2
    exit 1
fi

# ---- --extract=<path> [--noexec] ----
if [[ -n "$EXTRACT_DIR" ]]; then
    mkdir -p "$EXTRACT_DIR"
    tar -xf "$ARCHIVE_PATH" -C "$EXTRACT_DIR"
    echo "Extracted payload to: ${EXTRACT_DIR}"
    if [[ $NOEXEC -eq 1 ]]; then
        exit 0
    fi
    # Run install.sh from the extracted directory, record exit code, cleanup via trap
    bash "${EXTRACT_DIR}/install.sh" "${PASSTHROUGH[@]}"
    rc=$?
    exit $rc
fi

# ---- Default: run install.sh from temporary workdir ----
bash "${WORKDIR}/install.sh" "${PASSTHROUGH[@]}"
rc=$?
exit $rc
HEADER_EOF

# Determine the number of lines in the header and update the placeholder
HEADER_LINES=$(wc -l < "${HEADER}")
sed -i "s/^lines=XX$/lines=$(( HEADER_LINES + 1 ))/" "${HEADER}"

# Create the payload archive (includes VERSION metadata)
PAYLOAD_ARCHIVE="${WORK_DIR}/payload.tar.gz"
tar -czf "${PAYLOAD_ARCHIVE}" -C "${WORK_DIR}" \
    "${PACKAGE_NAME}" "${JSON_NAME}" "${INSTALL_SCRIPT}" "${VERSION_FILE}"

# Ensure output directory exists
mkdir -p "${RUN_OUTPUT_DIR}"

# Assemble the .run file: header script concatenated with the tar.gz payload
RUN_FILE="${RUN_OUTPUT_DIR}/${RUN_NAME}"
cp "${HEADER}" "${RUN_FILE}"
cat "${PAYLOAD_ARCHIVE}" >> "${RUN_FILE}"
chmod +x "${RUN_FILE}"

echo "Created kernel run package: ${RUN_FILE}"
