#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <fstream>
#include <iostream>
#include "json.hpp"

using json = nlohmann::json;

// ====== 配置管理 ======
struct ServerConfig {
    std::string host = "0.0.0.0";
    int port = 8080;
    int thread_count = 8;
    std::string db_path = "campus_system.db";
    std::string log_file = "server.log";
    size_t log_max_size = 10 * 1024 * 1024; // 10MB
    int log_max_files = 5;
    int token_expiry_hours = 24;
    int session_expiry_hours = 24;
    std::string cookie_name = "sid";
    bool https_enabled = false;
    std::string https_cert_path;
    std::string https_key_path;
};

inline ServerConfig load_config(const std::string& config_path = "config.json") {
    ServerConfig config;
    std::ifstream ifs(config_path);
    if (!ifs) {
        std::cerr << "配置文件 " << config_path << " 未找到，使用默认配置" << std::endl;
        return config;
    }
    try {
        json cfg = json::parse(ifs);
        if (cfg.contains("server")) {
            config.host = cfg["server"].value("host", config.host);
            config.port = cfg["server"].value("port", config.port);
            config.thread_count = cfg["server"].value("thread_count", config.thread_count);
        }
        if (cfg.contains("database")) {
            config.db_path = cfg["database"].value("path", config.db_path);
        }
        if (cfg.contains("log")) {
            config.log_file = cfg["log"].value("file", config.log_file);
            int max_mb = cfg["log"].value("max_size_mb", 10);
            config.log_max_size = static_cast<size_t>(max_mb) * 1024 * 1024;
            config.log_max_files = cfg["log"].value("max_files", config.log_max_files);
        }
        if (cfg.contains("security")) {
            config.token_expiry_hours = cfg["security"].value("token_expiry_hours", config.token_expiry_hours);
            config.session_expiry_hours = cfg["security"].value("session_expiry_hours", config.session_expiry_hours);
            config.cookie_name = cfg["security"].value("cookie_name", config.cookie_name);
        }
        if (cfg.contains("https")) {
            config.https_enabled = cfg["https"].value("enabled", false);
            config.https_cert_path = cfg["https"].value("cert_path", "");
            config.https_key_path = cfg["https"].value("key_path", "");
        }
    } catch (json::parse_error& e) {
        std::cerr << "配置文件解析失败: " << e.what() << "，使用默认配置" << std::endl;
    }
    return config;
}

extern ServerConfig g_config;

#endif
