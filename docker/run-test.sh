#!/bin/bash

export LD_LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export PYTHONPATH=${HOME}/local/lib/python3.6/site-packages:${HOME}/local/lib/python3.6/dist-packages


set -e
set -x

case $run_test in
    unit_test)
        ./yael-test
        ;;
    *)
        echo unknown run_test=$run_test
        exit 1
        ;;
esac
