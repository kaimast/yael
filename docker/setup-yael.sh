#!/bin/bash
# keep sudo as this may be useful in non-docker environment
set -e
set -x

export LD_LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
mkdir -p $HOME/local/{bin,include,lib}

# packages
apt-get update > /dev/null 2>&1
apt-get install -y --no-install-recommends \
    g++-7 sudo ca-certificates git pkg-config \
    build-essential libtool wget python libssl-dev \
    libgflags-dev libgoogle-glog-dev google-mock googletest libgtest-dev \
    pkg-config libboost-program-options-dev libboost-python-dev unzip ninja-build \
    clang-6.0 clang-tidy-6.0 clang-format \
    python3-pip python3-dev python3 python3-setuptools python3-wheel psmisc doxygen graphviz > /dev/null 2>&1
sudo pip3 install meson > /dev/null 2>&1

# use gcc-7
export CC=gcc-7
export CXX=g++-7

git clone https://github.com/kaimast/yael.git
cd yael
meson build --prefix=$HOME/local/
cd build
meson configure -Dbuildtype=release
ninja
ninja install
