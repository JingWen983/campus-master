// logger.cpp - Logger 静态成员定义
#include "logger.h"

std::string Logger::log_file_;
size_t Logger::log_max_size_ = 10 * 1024 * 1024;
int Logger::log_max_files_ = 5;
std::mutex Logger::mutex_;
