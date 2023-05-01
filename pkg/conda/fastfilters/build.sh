#!/bin/bash
mkdir build_conda
cd build_conda
cmake \
    -DCMAKE_INSTALL_PREFIX=${PREFIX} \
    -DFF_INSTALL_DIR=${SP_DIR} \
    ..
make -j${CPU_COUNT}
#make fastfilters_py_test
make install
