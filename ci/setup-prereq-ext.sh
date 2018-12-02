#!/bin/bash -e

WEAVEDIR=`pwd`
WORKDIR=$HOME/prereq
INSTALL_DIR=$HOME/local

BUILDTYPE=release
export CC=gcc-8
export CXX=g++-8

export LIBRARY_PATH=/usr/lib:/usr/local/lib:$INSTALL_DIR/lib
export LD_LIBRARY_PATH=/usr/lib:/usr/local/lib:$INSTALL_DIR/lib

ssh-keyscan github.com >> ~/.ssh/known_hosts

function clone-repo() {
    dir=$1
    url=$2

    if [ -d $dir ]; then
        return 1
    else
        git clone $url $dir
        return 0
    fi
}

cd $WORKDIR
if clone-repo "botan" "https://github.com/randombit/botan.git"; then
    cd botan
    echo "Building botan"
    git checkout release-2
    python ./configure.py --with-openssl --prefix=$INSTALL_DIR
    make -j10
    make install
fi
