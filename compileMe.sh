#!/bin/bash

#===================================================================#
# script to compile this library
# Christoph Goniva - Feb. 2011
#===================================================================#

#define whhere to install (here $CFDEM_LIB_DIR) # define where to find the src (here .)
cmake -DCMAKE_INSTALL_PREFIX=$CFDEM_LIB_DIR/.. [-DSTATIC_LIBSTDCPP=ON] .
make
make install
