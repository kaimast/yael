#! /bin/bash

./churn-test listen 14444 &
sleep 1
./churn-test connect localhost 14444 5 0
killall churn-test
