#!/bin/bash

export LD_LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export PYTHONPATH=${HOME}/local/lib/python3.6/site-packages:${HOME}/local/lib/python3.6/dist-packages


set -e
set -x

multi_client_test() {
    ./multi-client-test listen 44444  &
    sleep 1
    ./multi-client-test connect localhost 44444 20 0
    killall -9 multi-client-test
}

multi_client_delayed_test() {
    ./multi-client-test listen 44444 > /dev/null &
    sleep 1
    ./multi-client-test connect localhost 44444 20 2000
    killall -9 multi-client-test
}

churn_test() {
    ./churn-test listen 44444  &
    sleep 1
    ./churn-test connect localhost 44444 20 0
    killall -9 churn-test
}


case $run_test in
    unit_test)
        ./yael-test --gtest_repeat=100
        ;;
    churn)
        churn_test
        ;;
    multi_client)
        multi_client_test
        ;;
    multi_client_delayed)
        multi_client_delayed_test
        ;;
        *)
        echo unknown run_test=$run_test
        exit 1
        ;;
esac
