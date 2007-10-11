#!/bin/sh
# eINIT mdev script

if [ $1 = "enable" ]; then
 echo '/lib/einit/scripts/einit-hotplug' > /proc/sys/kernel/hotplug
 mount -n -t tmpfs -o exec,nosuid,mode=0755 mdev /dev
 mdev -s
 chmod 777 /dev/null
 chmod 666 /dev/zero
 chmod 660 /dev/console
 chmod 777 /dev/ptmx
fi
