#!/bin/bash
n=`wc -l /etc/fstab | awk '{print $1}'`
echo "<einit prefix=\"configuration-storage-fstab\">"
for ((i=1;i<=$n;i++))
do
 line=`head -$i /etc/fstab | tail -1 | tr -s [:blank:]`
 dev=`echo ${line} | cut -d ' ' -f 1`
 target=`echo ${line} | cut -d ' ' -f 2`
 fs=`echo ${line} | cut -d ' ' -f 3`
 options=`echo ${line} | cut -d ' ' -f 4 | sed s/,/:/g`
 echo " <node mountpoint=\"${target}\" device=\"${dev}\" fs=\"${fs}\" options=\"${options}\" />"
done
echo "</einit>"

