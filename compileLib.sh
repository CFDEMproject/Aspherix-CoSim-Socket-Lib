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

mkdir build
mkdir install
pushd build
cmake -DCMAKE_INSTALL_PREFIX=$PWD/../install -DSTATIC_LIBSTDCPP=ON -DSTATIC_LIBRARY=ON ..
make
make install
popd
