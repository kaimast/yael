#! /bin/bash

BUILDTYPE=release
INSTALL_DIR=$HOME/local

export LIBRARY_PATH=/usr/lib:/usr/local/lib:$INSTALL_DIR/lib
export LD_LIBRARY_PATH=/usr/lib:/usr/local/lib:$INSTALL_DIR/lib

export CC=gcc-8
export CXX=g++-8

meson build -Dbuildtype=$BUILDTYPE --prefix=$INSTALL_DIR -Dbotan_dir=$INSTALL_DIR/include/botan-2
cd build
ninja -v
ninja install -v
