#!/bin/sh
# eINIT udev script
# original script (before breaking it out of udev.xml) written by Ryan Hope

# pexec-options don't work here, so i had to make sure all udev apps get
# something as their stdin. /dev/null should be appropriate.

if [ $1 = "enable" ]; then
 echo 'feedback|notice|Using udev to manage /dev'
 udev=""
 if [ -z "${udev}" ] ; then
  touch /dev/.einit
  if [ "${configuration_services_udev_device_tarball}" = 'yes' ] ; then
   echo 'feedback|notice|Populating /dev with saved device nodes'
   tar -jxpf /lib/udev/state/devices.tar.bz2 -C /dev
   sleep 2
  fi

  echo 'feedback|notice|Seeding /dev with needed nodes'
  if [ ! -c /dev/console ] ; then mknod /dev/console c 5 1 ; fi
  if [ ! -c /dev/tty1 ] ; then mknod /dev/tty1 c 4 1 ; fi
  if [ ! -c /dev/null ] ; then mknod /dev/null c 1 3 ; fi
  if [ -d /lib/udev/devices ] ; then cp --preserve=all --recursive --update /lib/udev/devices/* /dev 2>/dev/null ; fi
  if [ -e /proc/kcore ] ; then ln -snf /proc/kcore /dev/core ; fi
  if [ -e /proc/sys/kernel/hotplug ] ; then
   echo 'feedback|notice|Setting up proper hotplug agent'
   if [ $(uname -r | cut -f 3 -d . | cut -f 1 -d -) -gt 14 ] ; then
    echo 'feedback|notice|Using netlink for hotplug events...'
    echo '' > /proc/sys/kernel/hotplug
   else
    echo 'feedback|notice|Setting /sbin/udevsend as hotplug agent ...'
    echo '/sbin/udevsend' > /proc/sys/kernel/hotplug
   fi
  fi
  echo 'feedback|notice|Generating udev rules to generate /dev/root symlink'
  /lib/einit/scripts/write_devroot_rules
  echo 'feedback|notice|Starting udevd'
  udevd --daemon </dev/null
  touch /dev/.udev_populate
  if [ $(uname -r | cut -f 3 -d . | cut -f 1 -d -) -gt 14 ] ; then
   echo 'feedback|notice|Populating /dev with existing devices through uevents'
   opts=""
   if [ '${configuration_services_udev_coldplug}' != 'yes' ] ; then opts='--attr-match=dev' ; fi
   udevtrigger ${opts} </dev/null
  else
   echo 'feedback|notice|Populating /dev with existing devices with udevstart'
   udevstart </dev/null
  fi
  echo 'feedback|notice|Letting udev process events'
  udevsettle --timeout=60 </dev/null
  rm -f /dev/.udev_populate
  if [ -x /sbin/lvm ] ; then /sbin/lvm vgscan -P --mknodes --ignorelockingfailure &>/dev/null ; fi
  if [ -x /sbin/evms_activate ] ; then /sbin/evms_activate -q &>/dev/null ; fi
 else
  udevd --daemon </dev/null
 fi
elif [ $1 = "disable" ]; then
 killall -9 udevd;
elif [ $1 = "on-shutdown" ]; then
 if [ "${configuration_services_udev_device_tarball}" = 'yes' ] ; then
  echo 'feedback|notice|Saving device nodes'
  save_tmp_base=/tmp/udev.savedevices.'$$'
  devices_udev='${save_tmp_base}'/devices.udev
  devices_real='${save_tmp_base}'/devices.real
  devices_totar='${save_tmp_base}'/devices.totar
  device_tarball='${save_tmp_base}'/devices
  rm -rf '${save_tmp_base}'
  mkdir '${save_tmp_base}'
  touch '${devices_udev}' '${devices_real}' '${devices_totar}' '${device_tarball}'
  if [ -f ${devices_udev} -a -f ${devices_real} -a -f ${devices_totar} -a -f ${device_tarball} ] ; then
   cd /dev
   find . -xdev -type b -or -type c -or -type l | cut -d/ -f2- > '${devices_real}'
   udevinfo=$(udevinfo --export-db)
   echo '${udevinfo}' | gawk '
    /^(N|S):.+/ {
     sub(/^(N|S):/, &quot;&quot;)
     split($0, nodes)
     for (x in nodes)
     print nodes[x];
    }' > '${devices_udev}'
   for x in MAKEDEV core fd initctl pts shm stderr stdin stdout ; do
    echo '${x}' >> '${devices_udev}'
   done
   if [ -d /lib/udev/devices ] ; then
    cd /lib/udev/devices
    find . -xdev -type b -or -type c -or -type l | cut -d/ -f2- >> '${devices_udev}'
   fi
   cd /dev
   fgrep -x -v -f '${devices_udev}' < '${devices_real}' | grep -v ^\\.udev > '${devices_totar}'
   if [ -s ${devices_totar} ] ; then
    tar --one-file-system --numeric-owner -jcpf '${device_tarball}' -T '${devices_totar}'
    mv -f '${device_tarball}' /lib/udev/state/devices.tar.bz2
   else
    rm -f /lib/udev/state/devices.tar.bz2
   fi
   echo 'feedback|warning|Could not create temporary files'
  fi
  rm -rf '${save_tmp_base}'
 else
  echo 'feedback|notice|Not saving device nodes'
 fi
fi
