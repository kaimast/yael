#! /bin/bash

# For g++-9
sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y

# We need a more recent meson too
sudo add-apt-repository ppa:jonathonf/meson -y

sudo apt-get update

sudo apt-get install meson build-essential git g++-9 libgtest-dev libgflags-dev libboost-program-options-dev libpython3-dev libboost-python-dev pkg-config python3-pytest cmake clang-6.0 libgmp-dev libssl-dev libgoogle-glog-dev -y
