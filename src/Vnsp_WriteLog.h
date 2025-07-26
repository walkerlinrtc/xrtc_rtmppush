#ifndef __VNSP_WRITELOG_H__
#define __VNSP_WRITELOG_H__


#define LOG_CHAR_MAX_SIZE 1024  /* 字符数组的最大size */
#define MAX_LOG_FILE_SIZE 300LL*1024*1024   /* 单个日志最大的大小500M */
#define MAX_LOG_SIZE 4*1024*100  /* 开辟的单条日志输出最大size 400k*/
#define LOG_CLEAR_SIZE 60LL*1024*1024*1024  /* 日志文件夹的最大大小80GBytes */
#define LOG_FILE_VALID_DAYS 7   /* 日志保存有效天数 */
#define logfilename(x) strrchr(x,'/')?strrchr(x,'/')+1:x /* 返回当前打印日志所在文件的名称，去除掉前面的目录 */
#define VNSP_LOG( level, deviceId, msg, ...)  Vnsp_WriteLog::GetInstance()->WriteLog(__LINE__, level, logfilename(__FILE__), deviceId, msg, ##__VA_ARGS__)
using namespace std;
// 定义日志等级
typedef enum LogLevel_enum
{
    LOG_DEBUG   =   1,
	LOG_MIDDLE  =   2,
	LOG_INFO    =   3,
    LOG_WARN    =   4,
    LOG_ERROR   =   5,
	LOG_FATAL   =   6
}LogLevel;
// 日志处理类
class Vnsp_WriteLog
{
public:
    Vnsp_WriteLog(/* args */);
    virtual ~Vnsp_WriteLog();
public:
    static Vnsp_WriteLog* GetInstance();
    //  日志写入
    void WriteLog(uint64_t traceId, LogLevel level,string userId,string deviceId, const char* pszfmt, ...);
    //  设置当前服务器的基本信息 暂时未被调用
    void SetServiceBaseInfo(string service_name ,string hostIp);
    //  获取日志所在目录
    string GetStrLogPath();
private:
    //  创建日志文件
    bool CreateLogFile(struct tm pNowTime, struct timeval pnow);
    //  关闭日志文件
    void CloseLogFile();
    //  创建日志文件夹
    void CreateLogFolder(string strDirPath,int nOffset);
    //  清理日志的线程
    static void*  OnCleanLogThread(void* pParam);
    //  清理日志进的线程
	void ProcessCleanLog();
    //  周期检测日志的配置文件
	void CheckLogConf();
private:
    static Vnsp_WriteLog*  m_PWriteLogInstance;     //  日志全局单例对象
    string  m_strDirPath;                           //  定义目录路径   
    string  m_strLogPath;                           //  定义日志路径
    string  m_strLogName;                           //  定义日志名称
    string  m_ServiceName;                          //  定义服务的名称
    string  m_ServiceIP;                            //  定义服务器ip
    uint64_t  m_pLogSize;                           //  定义单个日志大小
    uint64_t  m_pMaxLogSize;                        //  定义单个日志最大文件size
    uint64_t  m_pMaxLogDirSize;                     //  定义日志文件夹最大的size
    int64_t	  m_pLogDay;                            //  定义日志保存天数
    time_t  m_preCheckTime;                         //  定义日志的上次检测时间
	time_t  write_checktimes;                       //  定义上次写入日志的时间
    char  szLogMsg[MAX_LOG_SIZE];                   //  定义保存单条日志的字符数组
    struct tm  m_szLastDate;                        //  定义文件目录的上一个日期，按时间划分目录
    bool  m_bProcessRun;                            //  定义定期删除日志文件的bool值
    int  m_nWriteLog;                               //  
    int  m_nReadLog;                                //  
    pthread_t  m_threadID;                          //  定义线程id
    LogLevel  m_nLogLevel;                          //  定义日志默认等级
    FILE*  m_pFile;                                 //  定义文件句柄   
    LockMutex  m_lockLogFun;                        //  读写日志时使用的线程互斥锁
};



#endif //__VNSP_WRITELOG_H__