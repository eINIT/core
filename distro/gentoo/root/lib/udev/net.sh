#!/bin/sh
#
# net.sh: udev external RUN script
#
# Copyright 2007 Roy Marples <uberlord@gentoo.org>
# Distributed under the terms of the GNU General Public License v2

IFACE="$1"
ACTION="$2"

# ignore interfaces that are registered after being "up" (?)
case ${IFACE} in
    ppp*|ippp*|isdn*|plip*|lo*|irda*|dummy*|ipsec*|tun*|tap*)
    	exit 0 ;;
esac

# sypport for various init systems add your next big init system here
USE_INIT=`grep -Eo '^(init|initng|einit) ' -a /proc/1/cmdline`

if [ -z ${USE_INIT} ]; then
	logger -t udev-net.sh "${IFACE} ${ACTION} failed. Failed to determine init system."
	exit 1
fi

case "${ACTION}" in
	start)
		case "${USE_INIT}" in
			initng)
				EXEC="/sbin/ngc"
				ARGS="-u net/${IFACE}"
				;;
			einit)
				EXEC="/sbin/erc"
				ARGS="net-${IFACE} enable"
				;;
			init)
				EXEC="/etc/init.d/net.${IFACE}"
				ARGS="--quiet start"
				;;
		esac
		;;
	stop)
		case "${USE_INIT}" in
			initng)
				EXEC="/sbin/ngc"
				ARGS="-d net/${IFACE}"
				;;
			einit)
				EXEC="/sbin/erc"
				ARGS="net-${IFACE} disable"
				;;
			init)
				EXEC="/etc/init.d/net.${IFACE}"
				ARGS="--quiet stop"
				;;
		esac
		;;
	*)
		logger -t udev-net.sh "${IFACE} ${ACTION} failed. Improper action requested."
		logger -t udev-net.sh "Action must be start/stop."
		exit 1
		;;
esac


if [ ! -x "${EXEC}" ] ; then
    logger -t udev-net.sh "${IFACE} ${ACTION} failed. ${EXEC}: does not exist or is not executable"
    exit 1
fi

# If we're stopping then sleep for a bit in-case a daemon is monitoring
# the interface. This to try and ensure we stop after they do.
[ "${ACTION}" == "stop" ] && sleep 2

export IN_HOTPLUG=1
"${EXEC}" "${ARGS}"
