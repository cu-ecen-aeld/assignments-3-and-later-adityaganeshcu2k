#!/bin/sh

DAEMON=/usr/bin/aesdsocket
NAME=aesdsocket
PIDFILE=/var/run/$NAME.pid

case "$1" in
    start)
        echo "Starting $NAME"
        start-stop-daemon \
            --start \
            --background \
            --make-pidfile \
            --pidfile $PIDFILE \
            --exec $DAEMON -- -d
        ;;
    stop)
        echo "Stopping $NAME"
        start-stop-daemon \
            --stop \
            --pidfile $PIDFILE \
            --signal SIGTERM
        ;;
    restart)
        $0 stop
        $0 start
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac

exit 0

