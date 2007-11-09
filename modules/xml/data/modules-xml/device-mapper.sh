#!/bin/bash
if [ $1 = "enable" ]; then
/lib/einit/modules-xml/lvm-start.sh;

elif  [ $1 = "disable" ]; then
/lib/einit/modules-xml/lvm-stop.sh;
