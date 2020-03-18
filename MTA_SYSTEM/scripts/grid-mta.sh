#!/bin/sh
#
# grid-mta:   MTA daemon
#
# chkconfig: 345 98 02
# description:  This is a daemon for bring up SMTP and DELIVERY
#
# processname: mta
#

# Sanity checks.
[ -x /usr/bin/mta ] || exit 0

# Source function library.
. /etc/rc.d/init.d/functions

# so we can rearrange this easily
MTA_PATH=/var/grid-mta

RETVAL=0

# See how we were called.
case "$1" in
    start)
    	echo -n $"Starting grid-mta... "
    	daemon mta $MTA_PATH start
		RETVAL=$?
		echo
		[ $RETVAL -eq 0 ]
        ;;
    stop)
    	echo -n $"Stopping grid-mta... "
    	daemon mta $MTA_PATH stop
    	RETVAL=$?
		echo
		[ $RETVAL -eq 0 ]
        ;;
    status)
        mta $MTA_PATH status
        RETVAL=$?
        ;;
    restart)
		echo -n $"Restarting grid-mta... "
		daemon mta $MTA_PATH stop
		RETVAL=$?
		echo
		[ $RETVAL -eq 0 ]
		daemon mta $MTA_PATH start
		RETVAL=$?
		echo
		[ $RETVAL -eq 0 ]
        ;;
    condrestart)
		echo -n $"Restarting grid-mta... "
		daemon mta $MTA_PATH restart
		RETVAL=$?
		echo
		[ $RETVAL -eq 0 ]
        ;;
    *)
        echo $"Usage: $0 {start|stop|status|restart|condrestart}"
        ;;
esac
exit $RETVAL
