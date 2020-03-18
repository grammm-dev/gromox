#!/bin/sh
#
# grid-core:   CORE daemon
#
# chkconfig: 345 98 02
# description:  This is a daemon for bring up grid CORE services
#
# processname: core
#

# Sanity checks.
[ -x /usr/bin/core ] || exit 0

# Source function library.
. /etc/rc.d/init.d/functions

# so we can rearrange this easily
CORE_PATH=/var/grid-core

RETVAL=0

# See how we were called.
case "$1" in
    start)
    	echo -n $"Starting grid-core... "
    	daemon core $CORE_PATH start
		RETVAL=$?
		echo
		[ $RETVAL -eq 0 ]
        ;;
    stop)
    	echo -n $"Stopping grid-core... "
    	daemon core $CORE_PATH stop
    	RETVAL=$?
		echo
		[ $RETVAL -eq 0 ]
        ;;
    status)
        core $CORE_PATH status
        RETVAL=$?
        ;;
    restart)
		echo -n $"Restarting grid-core... "
		daemon core $CORE_PATH stop
		RETVAL=$?
		echo
		[ $RETVAL -eq 0 ]
		daemon core $CORE_PATH start
		RETVAL=$?
		echo
		[ $RETVAL -eq 0 ]
        ;;
    condrestart)
		echo -n $"Restarting grid-core... "
		daemon core $CORE_PATH restart
		RETVAL=$?
		echo
		[ $RETVAL -eq 0 ]
        ;;
    *)
        echo $"Usage: $0 {start|stop|status|restart|condrestart}"
        ;;
esac
exit $RETVAL
