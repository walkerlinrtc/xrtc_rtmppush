#ifndef __VNSP_STANDARD_H__
#define __VNSP_STANDARD_H__

#include <iostream>
#include <stdint.h> 
#include <cstring>
#include <string>
#include <map>
#include <list>			
#include <sstream>
#include <fcntl.h>			//	文件控制的函数和常量的定义
#include <inttypes.h>		//	处理整数类型的宏和函数 跨平台的方式
#include <stdio.h>			//	标准输入输出函数的声明
#include <stdlib.h>			//	标准输入输出函数的声明
#include <stdarg.h>			//	可变参数函数
#include <curl/curl.h>		//	libcurl头文件
#include <fstream>			//	文件操作输入输出
#include <unistd.h>         //  检查文件目录的访问权限
#include <iostream>         //  标准输入输出头文件
#include <sys/time.h>       //  获取系统时间戳的头文件
#include <sys/select.h>     //  多路复用 IO 操作的函数和宏的定义
#include <sys/socket.h>		//	套接字操作相关的函数和常量的声明
#include <sys/wait.h>		//	进程等待相关的函数和常量的声明
#include <sys/prctl.h>      //  进程控制的函数和宏的定义
#include <sys/stat.h>       //  定义了一些用于文件和目录操作的函数和宏  
#include <netinet/in.h>		//	网络相关的结构体和常量
#include <arpa/inet.h>		//	IP地址转换的函数
#include <dirent.h>         //  目录流打开文件目录
#include "AutoLock.h"		//	锁管理服务对象
#include "Markup.h"			//	xml格式库
#include "Vnsp_WriteLog.h"	//	服务内日志

using namespace std;        // 	std标准声明

// 线程阻塞时间设置。利用select实现线程阻塞
inline int threadDelay(const long lTimeSec, const long lTimeMSec)
{
	struct timeval timeOut;
	timeOut.tv_sec = lTimeSec;				// 秒值
	timeOut.tv_usec = lTimeMSec * 1000;		// 微秒值
	if (0 != select(0, NULL, NULL, NULL, &timeOut))
	{
		//	超时时间内至少一个文件描述符已准备好进行读取、写入或发生了异常情况
		return 1;
	}
	return 0;
}

#endif // __VNSP_STANDARD_H__