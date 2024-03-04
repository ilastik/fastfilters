#!/bin/bash
mkdir build_conda
cd build_conda

EXTRA_CMAKE_ARGS=""
if [[ `uname` == 'Darwin' ]] && [[ ${ARCH} == 'arm64' ]]; then
        EXTRA_CMAKE_ARGS="-DUSE_SIMDE_ON_ARM=ON"
fi

cmake \
    -DCMAKE_INSTALL_PREFIX=${PREFIX} \
    -DFF_INSTALL_DIR=${SP_DIR} \
    ${EXTRA_CMAKE_ARGS} \
    ..

make -j${CPU_COUNT}
#make fastfilters_py_test
make install
