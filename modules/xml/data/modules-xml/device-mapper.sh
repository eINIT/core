#!/bin/bash
if [ $1 = "enable" ]; then
./lvm-start.sh;
 
elif  [ $1 = "disable" ]; then
./lvm-stop.sh;
 
elif [ $1 = "on-shutdown" ]; then
./lvm-stop.sh;
fi
