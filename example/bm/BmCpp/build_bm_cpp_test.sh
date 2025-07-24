BM_CPP_EXAMPLE_DIR=$(dirname $(readlink -f "$0"))
PROJ_DIR=$(BM_CPP_EXAMPLE_DIR)../../../
bash ${PROJ_DIR}/script/build.sh DEBUG
mkdir ${PROJ_DIR}/output/tmp_mf/aarch64-linux -p
cp -rf ${PROJ_DIR}/output/hybm/* ${PROJ_DIR}/output/tmp_mf/aarch64-linux/
cp -rf ${PROJ_DIR}/output/smem/* ${PROJ_DIR}/output/tmp_mf/aarch64-linux/

export MEMFABRIC_HYBRID_HOME_PATH=${PROJ_DIR}/output/tmp_mf

cd ${BM_CPP_EXAMPLE_DIR}
mkdir build
cmake . -B build
make -C build
mkdir ${PROJ_DIR}/output/bm_example -p
cp -rf ${PROJ_DIR}/output/tmp_mf/aarch64-linux/lib64/* ${PROJ_DIR}/output/bm_example
cp -rf ${PROJ_DIR}/output/tmp_mf/aarch64-linux/lib/* ${PROJ_DIR}/output/bm_example
cp -rf ${BM_CPP_EXAMPLE_DIR}/bm_example ${PROJ_DIR}/output/bm_example

rm -rf ${BM_CPP_EXAMPLE_DIR}/bm_example
rm -rf ${BM_CPP_EXAMPLE_DIR}/build
rm -rf ${PROJ_DIR}/output/tmp_mf/