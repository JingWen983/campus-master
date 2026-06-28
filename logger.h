#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <iostream>
#include <ctime>
#include <cstring>
#include <vector>
#include <sstream>
#include <iomanip>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif

class Logger {
public:
    enum Level {
        LOG_INFO,
        LOG_WARNING,
        LOG_ERROR
    };

    static void init(const std::string& log_file = "server.log", size_t max_size = 10 * 1024 * 1024, int max_files = 5) {
        log_file_ = log_file;
        log_max_size_ = max_size;
        log_max_files_ = max_files;
        std::ofstream ofs(log_file_, std::ios::app);
        if (!ofs) {
            std::cerr << "无法打开日志文件" << std::endl;
        }
    }

    // 日志轮转：文件超过最大大小时，滚动重命名旧文件
    static void rotate_if_needed() {
        std::ifstream ifs(log_file_, std::ios::ate | std::ios::binary);
        if (!ifs) return;
        size_t size = ifs.tellg();
        ifs.close();

        if (size < log_max_size_) return;

        // 滚动重命名: server.log -> server.log.1, server.log.1 -> server.log.2, ...
        for (int i = log_max_files_ - 1; i >= 1; i--) {
            std::string old_name = log_file_ + "." + std::to_string(i);
            std::string new_name = log_file_ + "." + std::to_string(i + 1);
            std::rename(old_name.c_str(), new_name.c_str());
        }
        std::rename(log_file_.c_str(), (log_file_ + ".1").c_str());
    }

    static void log(Level level, const std::string& message) {
        rotate_if_needed();

        std::lock_guard<std::mutex> lock(mutex_);
        std::ofstream ofs(log_file_, std::ios::app);
        if (!ofs) return;

        std::string level_str;
        switch (level) {
            case LOG_INFO:    level_str = "INFO";    break;
            case LOG_WARNING: level_str = "WARNING"; break;
            case LOG_ERROR:   level_str = "ERROR";   break;
        }

        time_t now = time(nullptr);
        tm* tm_info = localtime(&now);
        char time_buf[64];
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

        ofs << "[" << time_buf << "] [" << level_str << "] " << message << std::endl;
        std::cout << "[" << level_str << "] " << message << std::endl;
    }

    static void info(const std::string& msg)    { log(LOG_INFO, msg); }
    static void warning(const std::string& msg) { log(LOG_WARNING, msg); }
    static void error(const std::string& msg)   { log(LOG_ERROR, msg); }

private:
    static std::string log_file_;
    static size_t log_max_size_;
    static int log_max_files_;
    static std::mutex mutex_;
};

#endif
