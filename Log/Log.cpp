#include "Log.h"

Log::Log()
{
    m_count = 0;
    m_is_async = false;
}

Log::~Log()
{

    if(writelog.is_open())
    {
        writelog.close();
    }
}

Log *Log::get_instance()
{
    static Log log;
    return &log;
}

bool Log::init(std::string path, int uselog, int log_buf_size, int split_line, int max_queue_size)
{
    m_split_lines = split_line;
    m_close_log = uselog;
    if(max_queue_size > 0)
    {
        m_is_async = true;
        pthread_t tid;
        pthread_create(&tid, NULL, flush_log_threaad, NULL);
    }

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    std::string logname;
    char logfile[256];
    m_today = my_tm.tm_mday;

    m_log_buf_size = log_buf_size;
    m_buf = new char[log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);

    snprintf(logfile, 255, "%s%d_%02d_%02d_%s", path.c_str(), my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, "Server.log");

    writelog.open(logfile, std::ios::app);
    if(!writelog.is_open())
    {
        return false;
    }

    return true;
}

void *Log::flush_log_threaad(void *args)
{
    Log::get_instance()->async_write_log();
}

void *Log::async_write_log()
{
    std::string single_log;
    while (m_log_que->pop(single_log))
    {
        m_mutex.lock();
        writelog << single_log << "\n";
        m_mutex.unlock();
    }
}

int file_num = 0;
char *setlogname()
{
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char re_time[64] = {0};
    std::strftime(re_time, sizeof(re_time), "%Y_%m_%d", std::localtime(&now));
    char logfile[256];
    if(file_num == 0)
    {
        snprintf(logfile, 255, "%s_%s", re_time, "Server.log");
    } else{
        snprintf(logfile, 255, "%s_%s%d%s", re_time, "Server", file_num, ".log");
    }
    return logfile;
}

void Log::write_log(int level, const char *format, ...)
{
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char re_time[64] = {0};
    std::strftime(re_time, sizeof(re_time), "%Y-%m-%d_%X  ", std::localtime(&now));
    int n = snprintf(m_buf, 64,"%s", re_time);

    struct timeval tnow = {0, 0};
    gettimeofday(&tnow, NULL);
    time_t t = tnow.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    char s[16] = {0};

    switch(level)
    {
        case 0:
            strcpy(s, "[DEBUG]: ");
            break;
        case 1:
            strcpy(s, "[INFO]: ");
            break;
        case 2:
            strcpy(s, "[WARNING]: ");
            break;
        case 3:
            strcpy(s, "[ERROR]: ");
            break;
        default:
            strcpy(s, "[INFO]: ");
            break;
    }

    m_mutex.lock();
    m_count++;
    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) //everyday log
    {
        writelog.close();
        if(m_today != my_tm.tm_mday)
        {
            char *new_log;
            new_log = setlogname();
            writelog.open(new_log, std::ios::app);
            if(!writelog.is_open())
            {
                LOG_ERROR("Create new logfile error");
                writelog.open(new_log, std::ios::app);
            }
        }
        else
        {
            char *new_log;
            file_num++;
            new_log = setlogname();
            writelog.open(new_log, std::ios::app);
            if(!writelog.is_open())
            {
                LOG_ERROR("Create new logfile error");
                writelog.open(new_log, std::ios::app);
            }
        }

    }
    m_mutex.unlock();

    va_list valst;
    va_start(valst, format);
    std::string log_str;

    m_mutex.lock();

    int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valst);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;

    m_mutex.unlock();

    if(m_is_async && !m_log_que->full())
    {
        m_log_que->push(log_str);
    }
    else
    {
        m_mutex.lock();
        writelog << log_str << "\n";
    }
}

void Log::flush()
{
    m_mutex.lock();
    writelog.flush();
    m_mutex.unlock();
}
