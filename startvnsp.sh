#指定shell环境Bash
#!/bin/bash
#链接当前目录，保存到动态链接库的路径中
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./
#设置用户当前进程核心转储最大1g文件
ulimit -c 1073741824
#设置用户打开文件描述符数量60000
ulimit -HSn 60000
#sysctl -w kernel.core_uses_pid=1
PIDS=`ps -ef|grep -w xrtc_rtmppush|grep -v grep|awk '{print $2}'`
if [ "$PIDS" == "" ]; then
	cd bin
     ./xrtc_rtmppush -D
else
    echo "xrtc_rtmppush already exist and ReStart!"
    kill -9 $PIDS
    bin/xrtc_rtmppush -D
fi
