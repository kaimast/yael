#! /bin/bash

INSTALL_DIR=$HOME/local

export LIBRARY_PATH=/usr/lib:/usr/local/lib:$INSTALL_DIR/lib:$INSTALL_DIR/lib/x86_64-linux-gnu
export LD_LIBRARY_PATH=/usr/lib:/usr/local/lib:$INSTALL_DIR/lib:$INSTALL_DIR/lib/x86_64-linux-gnu

cd build

# Without delay
./churn-test listen 14444 &
sleep 1
./churn-test connect localhost 14444 5 0
killall churn-test

# With delay
./churn-test listen 14444 &
sleep 1
./churn-test connect localhost 14444 5 100
killall churn-test
