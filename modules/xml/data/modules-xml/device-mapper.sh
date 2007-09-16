#!/bin/bash
if [ $1 = "enable" ]; then
lvm-start.sh;
fi

elif  [ $1 = "disable" ]; then
lvm-stop.sh;
fi

elif [ $1 = "on-shutdown" ]; then
lvm-stop.sh;
fi
