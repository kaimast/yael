#! /bin/bash

BUILDTYPE=release
INSTALL_DIR=$HOME/local

meson build -Dbuildtype=$BUILDTYPE --prefix=$INSTALL_DIR -Dbotan_dir=$INSTALL_DIR/include/botan-2
cd build
ninja -v
ninja install -v
