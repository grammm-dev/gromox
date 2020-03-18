#!/bin/sh
#
# grid-mra:   MRA daemon
#
# chkconfig: 345 98 02
# description:  This is a daemon for bring up POP and IMAP
#
# processname: mra
#

# Sanity checks.
[ -x /usr/bin/mra ] || exit 0

# Source function library.
. /etc/rc.d/init.d/functions

# so we can rearrange this easily
MRA_PATH=/var/grid-mra

RETVAL=0

# See how we were called.
case "$1" in
    start)
    	echo -n $"Starting grid-mra... "
    	daemon mra $MRA_PATH start
		RETVAL=$?
		echo
		[ $RETVAL -eq 0 ]
        ;;
    stop)
    	echo -n $"Stopping grid-mra... "
    	daemon mra $MRA_PATH stop
    	RETVAL=$?
		echo
		[ $RETVAL -eq 0 ]
        ;;
    status)
        mra $MRA_PATH status
        RETVAL=$?
        ;;
    restart)
		echo -n $"Restarting grid-mra... "
		daemon mra $MRA_PATH stop
		RETVAL=$?
		echo
		[ $RETVAL -eq 0 ]
		daemon mra $MRA_PATH start
		RETVAL=$?
		echo
		[ $RETVAL -eq 0 ]
        ;;
    condrestart)
		echo -n $"Restarting grid-mra... "
		daemon mra $MRA_PATH restart
		RETVAL=$?
		echo
		[ $RETVAL -eq 0 ]
        ;;
    *)
        echo $"Usage: $0 {start|stop|status|restart|condrestart}"
        ;;
esac
exit $RETVAL
