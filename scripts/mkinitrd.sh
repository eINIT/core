#!/bin/bash
WORKDIR=/tmp/work
INITRD=${WORKDIR}/initrd
IMG=${WORKDIR}/initrd-eINIT-$(einit -v | cut -d " " -f 2).img
rm -rf ${WORKDIR}
mkdir -p ${INITRD}
for lib in $(ldd /sbin/einit | grep "=>" | cut -d ">" -f 2 | sed 's/^[ \t]*//;s/[ \t]*$//' | sed '/^\//!d' | cut -d " " -f 1); do
 dir=$(dirname $lib)
 file=$(basename $lib)
 mkdir -p ${INITRD}${dir}
 cp $lib ${INITRD}${dir}/
done
mkdir ${INITRD}/etc
cp -rP /etc/einit ${INITRD}/etc/
rm -rf ${INITRD}/etc/einit/init.d
SIZE=$(( $(du -s ${INITRD} | cut -f 1) + $(du -s /sbin/einit | cut -f 1) + $(du -s /lib/einit | cut -f 1) + 50 ))
rm -rf ${IMG}
dd if=/dev/zero of=${IMG} bs=1024 count=${SIZE} &> /dev/null
yes|mkfs.ext2 ${IMG} &> /dev/null
mkdir ${WORKDIR}/mntloop
mount ${IMG} ${WORKDIR}/mntloop -o loop
mkdir -p ${WORKDIR}/mntloop/sbin
cp /sbin/einit ${WORKDIR}/mntloop/sbin/
cp -r ${INITRD}/* ${WORKDIR}/mntloop/
cp -r /lib/einit ${WORKDIR}/mntloop/lib/
umount ${WORKDIR}/mntloop
rm -rf ${WORKDIR}/{initrd,mntloop}
gzip ${IMG}
mv ${IMG}.gz ${IMG}
