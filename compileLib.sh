#!/bin/bash

#===================================================================#
#
# Script to compile this library
#
# (C) 2019 DCS-Computing GmbH
# Authors: Christoph Goniva
#          Arno Mayrhofer
#
#===================================================================#

set -e    # script should always report failures

rm -rf build install
mkdir build install
pushd build
cmake -DCMAKE_INSTALL_PREFIX=$PWD/../install -DSTATIC_LIBSTDCPP=ON -DSTATIC_LIBRARY=ON ..
make clean
make
make install
popd
