#include "Vnsp_WriteLog.h"
// 从服务器上的config文件拿到的字符串与日志等级做对比
static const char *LOG_levels[] =
    {
        "TEST",
        "DEBUG",
        "MIDDLE",
        "INFO",
        "WARN",
        "ERROR",
        "FATAL"
    };
//  判断日志目录中是否含有特殊字符
static int is_special_dir(const char *path)
{
    return strcmp(path, ".") == 0 || strcmp(path, "..") == 0;
}
//  获取指定日志目录path的大小(包括子目录)
static int64_t get_dirs_size(const char *path)
{
    uint64_t dirs_size = 0;
    struct stat statbuf;
    if (lstat(path, &statbuf) != 0)
        return dirs_size;
    // 判断给定的路径是目录还是文件，如果是文件，则直接返回文件的大小
    if (S_ISREG(statbuf.st_mode))
        dirs_size = statbuf.st_size;
    else if (S_ISDIR(statbuf.st_mode))
    {
        char tmp[LOG_CHAR_MAX_SIZE];
        DIR *dirptr = NULL;
        struct dirent *entry = NULL;
        //  打开目录后返回的新的目录，子目录
        if ((dirptr = opendir(path)) != NULL)
        {
            //  读取子目录
            while ((entry = readdir(dirptr)) != NULL)
            {
                //  如果是特殊目录则直接跳过处理
                if (is_special_dir(entry->d_name))
                    continue;
                snprintf(tmp, LOG_CHAR_MAX_SIZE, "%s/%s", path, entry->d_name);
                dirs_size += get_dirs_size(tmp);
            }
            closedir(dirptr);
        }
    }
    return dirs_size;
}
//  对目录进行压缩处理
static void tar_dirs(char *path)
{
    struct stat statbuf;
    if (lstat(path, &statbuf) != 0)
        return;
    if (S_ISREG(statbuf.st_mode) || S_ISDIR(statbuf.st_mode))
    {
        char cmd[LOG_CHAR_MAX_SIZE];
        //  合成目录的控制命令
        snprintf(cmd, LOG_CHAR_MAX_SIZE, "tar -czvf %s.tar.gz %s --remove-files", path, path);
        system(cmd);
        return;
    }
}
//  删除目录下的文件
static void rm_dirs(char *path)
{
    struct stat statbuf;
    if (lstat(path, &statbuf) != 0)
        return;
    if (S_ISREG(statbuf.st_mode))
    {
        remove(path);
        return;
    }
    if (S_ISDIR(statbuf.st_mode))
    {
        char tmp[LOG_CHAR_MAX_SIZE];
        DIR *dirptr = NULL;
        struct dirent *entry = NULL;
        if ((dirptr = opendir(path)) != NULL)
        {
            while ((entry = readdir(dirptr)) != NULL)
            {
                if (is_special_dir(entry->d_name))
                    continue;
                snprintf(tmp, LOG_CHAR_MAX_SIZE, "%s/%s", path, entry->d_name);
                rm_dirs(tmp);
            }
            closedir(dirptr);
        }
        remove(path);
    }
}
//  将path路径下的文件转入目标路径下
static void mv_dirs(const char *path, char *directPath)
{
    struct stat statbuf;
    if (lstat(path, &statbuf) != 0)
        return;
    //  判断是否是普通文件还是普通目录
    if (S_ISREG(statbuf.st_mode) || S_ISDIR(statbuf.st_mode))
    {
        //  通过system执行移动操作
        char cmd[LOG_CHAR_MAX_SIZE];
        snprintf(cmd, LOG_CHAR_MAX_SIZE, "mv %s %s", path, directPath);
        int nTimes = 0;
        while (nTimes++ < 3)
        {
            if (-1 != system(cmd))
            {
                //  移动命令执行成功
                break;
            }
        }
    }
}
//  编译时将日志单例对象置为空
Vnsp_WriteLog *Vnsp_WriteLog::m_PWriteLogInstance = NULL;
//  日志对象构造函数
Vnsp_WriteLog::Vnsp_WriteLog()
{
    m_nLogLevel = LOG_INFO;
    m_pMaxLogSize = MAX_LOG_FILE_SIZE;
    m_pLogDay = LOG_FILE_VALID_DAYS;
    m_pMaxLogDirSize = LOG_CLEAR_SIZE;
    m_strLogName = "vnspserver"; // 设置保存的日志文件名称
    CMarkup xml;
    // 获取日志配置文件
    if (xml.Load("./logConfig.xml"))
    {
        // 获取日志路径
        if (xml.FindChildElem("LogFileDir"))
        {
            m_strDirPath = xml.GetChildData();
            // 判断日志路径最后是否携带/，没有携带就加上，然后拼接路径使用
            if (m_strDirPath.substr(m_strDirPath.size() - 1, 1) != "/")
            {
                m_strDirPath.append("/");
            }
        }
        // 如果设置了日志等级，那就采用config文件中的
        if (xml.FindChildElem("LogLevel"))
        {
            string tmpLevel = xml.GetChildData();
            const char *level = tmpLevel.c_str();
            if (level != NULL)
            {
                for (uint8_t cur_level = LOG_DEBUG; cur_level <= LOG_FATAL; cur_level++)
                {
                    // 比较字符串，如果config中的日志等级不一致，则采用config中的设置
                    if (strlen(level) >= strlen(LOG_levels[cur_level]) && strncasecmp(level, LOG_levels[cur_level], strlen(LOG_levels[cur_level])) == 0)
                    {
                        m_nLogLevel = (LogLevel)cur_level;
                        break;
                    }
                }
            }
        }
        // 获取config中是否配置了单个日志的最大size
        if (xml.FindChildElem("LogFileMaxSize"))
        {
            // 将输入的字符串的大小转换为longlong
            m_pMaxLogSize = atoll(xml.GetChildData().c_str());
            if (m_pMaxLogSize < 1)
            {
                m_pMaxLogSize = 1;
            }
            else if (m_pMaxLogSize > 1024)
            {
                m_pMaxLogSize = 1024;
            }
            m_pMaxLogSize *= 1024 * 1024;
        }
        // 获取日志的有效天数，只有小于0时才使用
        if (xml.FindChildElem("LogFileValidDays"))
        {
            m_pLogDay = atoll(xml.GetChildData().c_str());
            if (m_pLogDay < 0)
            {
                m_pLogDay = 0;
            }
        }
        // 获取日志文件夹的大小，以实际配置为主
        if (xml.FindChildElem("LogClearSize"))
        {
            m_pMaxLogDirSize = atoll(xml.GetChildData().c_str());
            if (m_pMaxLogDirSize < 1)
            {
                m_pMaxLogDirSize = 1;
            }
            m_pMaxLogDirSize *= 1024 * 1024 * 1024;
        }
    }
    //  m_strDirPath= /xxx/xxx/vnspserver_log
    m_strDirPath = m_strDirPath + "vnspserver_log";
    // 创建日志目录的文件夹
    CreateLogFolder(m_strDirPath, 0);
    m_strLogPath = m_strDirPath + "/" + m_strLogName + ".log";
    m_pFile = NULL;
    memset(&m_szLastDate, 0, sizeof(m_szLastDate));
    m_bProcessRun = true;
    m_nWriteLog = 0;
    m_nReadLog = 0;
    m_pLogSize = 0; //  当前日志文件大小初始化为0
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    // 定期日志删除线程，循环调度包括周期处理过期日志文件的清理以及检查日志文件的配置
    pthread_create(&m_threadID, &attr, OnCleanLogThread, this);
    // 日志输出
    WriteLog(__LINE__, LOG_INFO, logfilename(__FILE__), "", "LogFileDir:%s, LogLevel: %s, LogFileMaxSize:%llu LogDay:%lld MaxDirSize:%llu", m_strDirPath.c_str(), LOG_levels[m_nLogLevel], m_pMaxLogSize, m_pLogDay, m_pMaxLogDirSize);
}
// 日志对象析构函数
Vnsp_WriteLog::~Vnsp_WriteLog()
{
    m_bProcessRun = false;
    CloseLogFile();
}
//  获取日志单例的对象
Vnsp_WriteLog *Vnsp_WriteLog::GetInstance()
{
    while (m_PWriteLogInstance == NULL)
    {
        m_PWriteLogInstance = new (std::nothrow) Vnsp_WriteLog();
    }
    return m_PWriteLogInstance;
}
//  日志输出函数
void Vnsp_WriteLog::WriteLog(uint64_t traceId, LogLevel level, string userId, string deviceId, const char *pszfmt, ...)
{
    //  过滤掉低于设置的目标等级的日志。只打印当前等级和更高等级的日志
    if (level < m_nLogLevel)
    {
        return;
    }
    char szTime[32] = {0};
    struct timeval stTime;
    struct tm pTmTime;
    // 获取当前系统时间，最后输出精确到毫秒
    if (0 == gettimeofday(&stTime, NULL))
    {
        localtime_r(&stTime.tv_sec, &pTmTime);
        snprintf(szTime, sizeof(szTime), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                 1900 + pTmTime.tm_year, 1 + pTmTime.tm_mon, pTmTime.tm_mday,
                 pTmTime.tm_hour, pTmTime.tm_min, pTmTime.tm_sec, (int)(stTime.tv_usec / 1000));
    }
    string tmptime = szTime;
    //  互斥锁
    AutoMutex lock(&m_lockLogFun);
    string str_level = "";
    if ("" == deviceId)
    {
        deviceId = "NULL";
    }
    str_level = LOG_levels[level];
    //  格式化日志信息
    memset(szLogMsg, 0, MAX_LOG_SIZE);
    va_list argptr;
    va_start(argptr, pszfmt);
    //  使用vsnprintf 防止缓冲区溢出
    vsnprintf(szLogMsg, MAX_LOG_SIZE - 1, pszfmt, argptr);
    //  清理可变参数列表
    va_end(argptr);
    string strTraceId = to_string(traceId);
    string strLog = tmptime + " [" + str_level + "][" + m_ServiceName + "]["+ userId +":"+ strTraceId +"][" + deviceId + "]service_ip:" + m_ServiceIP + " " + szLogMsg;
    //  写入到文件中,先判断是否需要创建对应的file
    if (CreateLogFile(pTmTime, stTime))
    {
        if (NULL != m_pFile)
        {
            fwrite(strLog.c_str(), strLog.length(), 1, m_pFile);
            fwrite("\xd\xa", 2, 1, m_pFile);    // 写入换行符
            fflush(m_pFile);    // 刷新缓冲区，立即写入文件
            m_pLogSize += strLog.length() + 2;
        }
    }
}
//  创建日志文件夹
void Vnsp_WriteLog::CreateLogFolder(string strDirPath, int nOffset)
{
    if (!strDirPath.empty())
    {
        int nPos = strDirPath.find("/", nOffset);
        string strTemp = (nPos != -1) ? strDirPath.substr(0, nPos) : strDirPath;
        if (!strTemp.empty())
        {
            DIR *dp = opendir(strTemp.c_str());
            if (NULL == dp)
            {
                //  文件所有者具有读、写和执行权限 (S_IRWXU) 
                mkdir(strTemp.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            }
            else
            {
                closedir(dp);
            }
        }
        //  递归按照路径逐个打开,直到没有找到/
        if (nPos != -1)
        {
            CreateLogFolder(strDirPath, nPos + 1);
        }
    }
}
//  创建日志文件
bool Vnsp_WriteLog::CreateLogFile(struct tm pNowTime, struct timeval pnow)
{
    if (m_pFile)
    {
        //  按时间和文件大小来判断是否写入文件，若有差异，需要将当前文件移入目标文件夹
        if (m_szLastDate.tm_year != pNowTime.tm_year || m_szLastDate.tm_mon != pNowTime.tm_mon ||
            m_szLastDate.tm_mday != pNowTime.tm_mday || m_pLogSize > m_pMaxLogSize)
        {
            CloseLogFile();
            char tmpLogpath[100] = {0};
            snprintf(tmpLogpath, sizeof(tmpLogpath), "%s/%s_%04d%02d%02d[%02d.%02d.%02d.%03d].log", m_strDirPath.c_str(), m_strLogName.c_str(),
                     1900 + pNowTime.tm_year, 1 + pNowTime.tm_mon, pNowTime.tm_mday,
                     pNowTime.tm_hour, pNowTime.tm_min, pNowTime.tm_sec, (int)(pnow.tv_usec / 1000));
            //  转存文件
            mv_dirs(m_strLogPath.c_str(), tmpLogpath);
        }
        //  检测时间超过5s，则检查下文件的访问权限
        else if (m_preCheckTime + 5 < pnow.tv_sec)
        {
            //  检查日志目录文件的访问权限
            if (access(m_strLogPath.c_str(), 0) < 0)
            {
                //  没有权限关闭
                CloseLogFile();
            }
            else
            {
                struct stat statbuf; //  存储获取到的文件的信息
                //  成功拿到文件的状态信息
                if (lstat(m_strLogPath.c_str(), &statbuf) == 0)
                {
                    //  判断上次写入的时间是否和获取到的最新的时间相等，如果跟上次检测的时间一致，那么就是上一次没写进去
                    if (write_checktimes == statbuf.st_mtime)
                    {
                        CloseLogFile();
                    }
                    else
                    {
                        write_checktimes = statbuf.st_mtime;
                    }
                }
            }
            m_preCheckTime = pnow.tv_sec;
        }
    }
    if (NULL == m_pFile)
    {
        //  如果获取目录的权限失败，则创建新的文件夹
        if (access(m_strDirPath.c_str(), 0) < 0)
        {
            CreateLogFolder(m_strDirPath, 0);
        }
        m_szLastDate = pNowTime;
        m_pFile = fopen(m_strLogPath.c_str(), "ab");
        if (NULL == m_pFile)
        {
            //  失败再创建一次
            CreateLogFolder(m_strDirPath, 0);
            m_pFile = fopen(m_strLogPath.c_str(), "ab");
        }
        if (NULL != m_pFile)
        {
            //  每次写入日志的时候，都将指针定位到末尾
            fseek(m_pFile, 0, SEEK_END);
            //  获取到当前文件大小
            m_pLogSize = ftell(m_pFile);
        }
    }
    //  返回文件是否创建成功或已经存在
    return (m_pFile != NULL);
}
//  关闭当前的日志文件
void Vnsp_WriteLog::CloseLogFile()
{
    if (m_pFile != NULL)
    {
        fclose(m_pFile);
        m_pFile = NULL;
        write_checktimes = 0;
    }
}
//  设置当前服务器的基本信息
void Vnsp_WriteLog::SetServiceBaseInfo(string service_name, string hostIp)
{
    m_ServiceName = service_name;
    m_ServiceIP = hostIp;
}
//  设置后台清理日志的线程
void *Vnsp_WriteLog::OnCleanLogThread(void *pParam)
{
    Vnsp_WriteLog *pThis = (Vnsp_WriteLog *)pParam;
    prctl(PR_SET_NAME, "LogFileClean");
    if (pThis != NULL)
    {
        pThis->ProcessCleanLog();
    }
    return (void *)0;
}
//  后台清理日志的处理函数
void Vnsp_WriteLog::ProcessCleanLog()
{
    while (m_bProcessRun)
    {
        // 检测日志对应目录权限并删除过期日志目录和文件
        if (access(m_strDirPath.c_str(), 0) == 0)
        {
            char tmp[LOG_CHAR_MAX_SIZE];
            DIR *dirptr = NULL;
            struct dirent *entry = NULL;
            struct stat statbuf;
            time_t cur_time_s = time(NULL);
            if ((dirptr = opendir(m_strDirPath.c_str())) != NULL)
            {
                //  逐个读取目录下的文件，进行日志属性判断
                while ((entry = readdir(dirptr)) != NULL)
                {
                    //  跳过特殊目录
                    if (is_special_dir(entry->d_name))
                    {
                        continue;
                    }
                    int tmplen = strlen(entry->d_name);
                    if (tmplen > 7 && strncmp((entry->d_name + (tmplen - 7)), ".tar.gz", 7) == 0)
                    {
                        continue;
                    }
                    snprintf(tmp, LOG_CHAR_MAX_SIZE, "%s/%s", m_strDirPath.c_str(), entry->d_name);
                    if (strcmp(tmp, m_strLogPath.c_str()) == 0)
                    {
                        continue;
                    }
                    if (lstat(tmp, &statbuf) == 0)
                    {
                        if ((cur_time_s - statbuf.st_mtime) / (24 * 60 * 60) >= m_pLogDay)
                        {
                            tar_dirs(tmp);
                        }
                    }
                }
                closedir(dirptr);
            }
        }

        // 超过清除大小,删除旧日志,先删除压缩包的，删完了的话再删log文件
        uint64_t log_day_total_size = get_dirs_size(m_strDirPath.c_str());
        if (log_day_total_size > m_pMaxLogDirSize)
        {
            DIR *dirptr = NULL;
            struct dirent *entry = NULL;
            struct stat statbuf;
            time_t oldest_time_s = 0;

            if ((dirptr = opendir(m_strDirPath.c_str())) != NULL)
            {
                char tmp[LOG_CHAR_MAX_SIZE];
                char dealFile[LOG_CHAR_MAX_SIZE];
                while ((entry = readdir(dirptr)) != NULL)
                {
                    if (is_special_dir(entry->d_name))
                    {
                        continue;
                    }
                    int tmplen = strlen(entry->d_name);
                    if (tmplen > 4 && strncmp((entry->d_name + (tmplen - 4)), ".log", 4) == 0)
                    {
                        continue;
                    }
                    snprintf(tmp, LOG_CHAR_MAX_SIZE, "%s/%s", m_strDirPath.c_str(), entry->d_name);
                    if (strcmp(tmp, m_strLogPath.c_str()) == 0)
                    {
                        continue;
                    }
                    if (lstat(tmp, &statbuf) == 0 && S_ISREG(statbuf.st_mode))
                    {
                        if (0 == oldest_time_s || oldest_time_s > statbuf.st_mtime)
                        {
                            oldest_time_s = statbuf.st_mtime;
                            snprintf(dealFile, LOG_CHAR_MAX_SIZE, "%s", tmp);
                        }
                    }
                }
                if (oldest_time_s > 0)
                {
                    rm_dirs(dealFile);
                }
                closedir(dirptr);
            }
            if (0 == oldest_time_s && (dirptr = opendir(m_strDirPath.c_str())) != NULL)
            {
                char tmp[LOG_CHAR_MAX_SIZE];
                char dealFile[LOG_CHAR_MAX_SIZE];
                while ((entry = readdir(dirptr)) != NULL)
                {
                    if (is_special_dir(entry->d_name))
                    {
                        continue;
                    }
                    snprintf(tmp, LOG_CHAR_MAX_SIZE, "%s/%s", m_strDirPath.c_str(), entry->d_name);
                    if (strcmp(tmp, m_strLogPath.c_str()) == 0)
                    {
                        continue;
                    }
                    if (lstat(tmp, &statbuf) == 0 && S_ISREG(statbuf.st_mode))
                    {
                        if (0 == oldest_time_s || oldest_time_s > statbuf.st_mtime)
                        {
                            oldest_time_s = statbuf.st_mtime;
                            snprintf(dealFile, LOG_CHAR_MAX_SIZE, "%s", tmp);
                        }
                    }
                }
                if (oldest_time_s > 0)
                {
                    rm_dirs(dealFile);
                }
                closedir(dirptr);
            }
        }
        //  检查日志配置
        CheckLogConf();
        threadDelay(10, 0);
    }
}
//  处理日志配置文件
void Vnsp_WriteLog::CheckLogConf()
{
    CMarkup xml;
    LogLevel tmpLogLevel = m_nLogLevel;
    uint64_t tmpLogSize = m_pMaxLogSize;
    int64_t tmpLogDay = m_pLogDay;
    uint64_t tmpMaxLogDirSize = m_pMaxLogDirSize;
    if (xml.Load("./logConfig.xml"))
    {
        if (xml.FindChildElem("LogLevel"))
        {
            string tmpLevel = xml.GetChildData();
            const char *level = tmpLevel.c_str();
            if (level != NULL)
            {
                for (int cur_level = LOG_DEBUG; cur_level <= LOG_FATAL; cur_level++)
                {
                    if (strlen(level) >= strlen(LOG_levels[cur_level]) && strncasecmp(level, LOG_levels[cur_level], strlen(LOG_levels[cur_level])) == 0)
                    {
                        tmpLogLevel = (LogLevel)cur_level;
                        break;
                    }
                }
            }
        }
        if (xml.FindChildElem("LogFileMaxSize"))
        {
            tmpLogSize = atoll(xml.GetChildData().c_str());
            if (tmpLogSize < 1)
            {
                tmpLogSize = 1;
            }
            tmpLogSize *= 1024 * 1024;
        }
        if (xml.FindChildElem("LogFileValidDays"))
        {
            tmpLogDay = atoll(xml.GetChildData().c_str());
            if (tmpLogDay < 0)
            {
                tmpLogDay = 0;
            }
        }
        if (xml.FindChildElem("LogClearSize"))
        {
            tmpMaxLogDirSize = atoll(xml.GetChildData().c_str());
            if (tmpMaxLogDirSize < 1)
            {
                tmpMaxLogDirSize = 1;
            }
            tmpMaxLogDirSize *= 1024 * 1024 * 1024;
        }
    }
    if (tmpLogLevel != m_nLogLevel)
    {
        m_nLogLevel = tmpLogLevel;
        VNSP_LOG(LOG_INFO, "", "config log reload LogLevel: %s", LOG_levels[m_nLogLevel]);
    }
    if (tmpLogSize != m_pMaxLogSize)
    {
        m_pMaxLogSize = tmpLogSize;
        VNSP_LOG(LOG_INFO, "", "config log reload LogFileMaxSize:%llu", m_pMaxLogSize);
    }
    if (tmpLogDay != m_pLogDay)
    {
        m_pLogDay = tmpLogDay;
        VNSP_LOG( LOG_INFO, "", "config log reload LogDay:%lld", m_pLogDay);
    }
    if (tmpMaxLogDirSize != m_pMaxLogDirSize)
    {
        tmpMaxLogDirSize = m_pMaxLogDirSize;
        VNSP_LOG(LOG_INFO,  "", "config log reload MaxDirSize:%lld", m_pMaxLogDirSize);
    }
}
//  获取日志所在目录路径
string Vnsp_WriteLog::GetStrLogPath()
{
    return m_strDirPath;
}