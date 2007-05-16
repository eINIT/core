#!/bin/bash
for conf in `ls /etc/einit/conf.d`; do 
 differ=`diff -q /etc/einit/conf.d/${conf} ../conf.d/${conf} | grep -c differ`;
 if [ ${differ} != 0 ]; then 
  diff -u /etc/einit/conf.d/${conf} ../conf.d/${conf} > /tmp/.update-${conf}.diff;
 fi;
done
