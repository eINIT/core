#!/bin/sh
#
# net.sh: udev external RUN script
#

IFACE="$1"
ACTION="$2"

if grep -q initng /proc/1/cmdline 
then
    EXEC="/sbin/ngc"
    INITNG="yes"
    EINIT="no"
elif grep -q einit /proc/1/cmdline
then
    EXEC="/sbin/einit-control"
    INITNG="no"
    EINIT="yes"
else
    EXEC="/etc/init.d/net.${IFACE}"
    INITNG="no"
    EINIT="no"
fi

case "${ACTION}" in
    start)
	if [ "${INITNG}" = "yes" ]
	then
	    ARGS="-u net/${IFACE}"
	elif [ "${EINIT}" = "yes" ]
	then
	    ARGS="rc net-${IFACE} enable"
	else
	    ARGS="--quiet start"
	fi
	;;
    stop)
	if [ "${INITNG}" = "yes" ]
	then
	    ARGS="-d net/${IFACE}"
	elif [ "${EINIT}" = "yes" ]
	then
	    ARGS="rc net-${IFACE} disable"
	else
	    ARGS="--quiet stop"
	fi
	;;
    *)
	echo "$0: wrong arguments" >&2
	echo "Call with <interface> <start|stop>" >&2
	exit 1
	;;
esac

export IN_HOTPLUG=1

if [ -x "${EXEC}" ]
then
    ${EXEC} ${ARGS}
    exit 0
else
    logger -t netplug "Error: Couldn't configure ${IFACE}, no ${EXEC} !"
    exit 1
fi

# vim: set ts=4
