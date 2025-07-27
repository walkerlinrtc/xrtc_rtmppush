#!/bin/bash
#if ! [ -x "$(command -v killall)" ]; then
  #sudo yum install psmisc
#fi
PIDS=`ps -ef|grep -w xrtc_rtmppush|grep -v grep|awk '{print $2}'`
if [ "$PIDS" != "" ]; then
    echo "kill xrtc_rtmppush"
    kill -9 $PIDS
fi
