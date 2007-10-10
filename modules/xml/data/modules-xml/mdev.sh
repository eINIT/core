#!/bin/sh
# eINIT udev script
# original script (before breaking it out of udev.xml) written by Ryan Hope

# pexec-options don't work here, so i had to make sure all udev apps get
# something as their stdin. /dev/null should be appropriate.

if [ $1 = "enable" ]; then
 mount -n -t tmpfs -o exec,nosuid,mode=0755 mdev /dev
 mdev -s
 chmod 777 /dev/null
 chmod 666 /dev/zero
 chmod 660 /dev/console
 chmod 777 /dev/ptmx
fi
