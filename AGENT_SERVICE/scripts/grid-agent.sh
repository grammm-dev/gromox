#!/bin/sh
#
# grid-agent:   AGENT daemon
# chkconfig: 345 98 02
#
# description:  This is a daemon for bring up domain socket services
#
# processname: agent
#

# Sanity checks.
[ -x /usr/bin/agent ] || exit 0

# Source function library.
. /etc/rc.d/init.d/functions

# so we can rearrange this easily
AGENT_PATH=/var/grid-agent

RETVAL=0

# See how we were called.
case "$1" in
    start)
    	echo -n $"Starting grid-agent... "
    	daemon agent $AGENT_PATH start
		RETVAL=$?
		echo
		[ $RETVAL -eq 0 ]
        ;;
    stop)
    	echo -n $"Stopping grid-agent... "
    	daemon agent $AGENT_PATH stop
    	RETVAL=$?
		echo
		[ $RETVAL -eq 0 ]
        ;;
    status)
        agent $AGENT_PATH status
        RETVAL=$?
        ;;
    restart)
		echo -n $"Restarting grid-agent... "
		daemon agent $AGENT_PATH stop
		RETVAL=$?
		echo
		[ $RETVAL -eq 0 ]
		daemon agent $AGENT_PATH start
		RETVAL=$?
		echo
		[ $RETVAL -eq 0 ]
        ;;
    condrestart)
		echo -n $"Restarting grid-agent... "
		daemon agent $AGENT_PATH restart
		RETVAL=$?
		echo
		[ $RETVAL -eq 0 ]
        ;;
    *)
        echo $"Usage: $0 {start|stop|status|restart|condrestart}"
        ;;
esac
exit $RETVAL
