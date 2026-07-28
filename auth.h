#ifndef AUTH_H
#define AUTH_H

#include <string>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <random>
#include <sstream>
#include <iomanip>
#include "json.hpp"
#include "httplib.h"
#include "logger.h"
#include "models.h"
#include "config.h"
#include "sha256.h"

using json = nlohmann::json;
using namespace std;

// 安全修复 V9：CORS 头设置 —— 仅在请求 Origin 命中白名单时回显
inline void set_cors_headers(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-CSRF-Token");
    if (g_config.cors_allowed_origins.empty()) {
        // 未配置白名单：不回显 Origin，禁止跨域凭据
        return;
    }
    // 没有 Origin 头视为同源，不回显
    auto it = res.headers.find("Origin");
    // httplib Response 通常没有 Origin；尝试从 Request 读取由路由层注入
    // 此处保留默认不回显；具体 Origin 回显在 set_cors_headers(req, res) 重载中处理
}

// 安全修复 V9：CORS —— 基于请求 Origin 反射白名单
inline void set_cors_headers(const httplib::Request& req, httplib::Response& res) {
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-CSRF-Token");
    if (g_config.cors_allowed_origins.empty()) return;
    auto it = req.headers.find("Origin");
    if (it == req.headers.end()) return;
    const std::string& origin = it->second;
    for (const auto& allowed : g_config.cors_allowed_origins) {
        if (origin == allowed) {
            res.set_header("Access-Control-Allow-Origin", origin);
            res.set_header("Vary", "Origin");
            res.set_header("Access-Control-Allow-Credentials", "true");
            return;
        }
    }
}

// 请求日志
inline void log_request(const httplib::Request& req) {
    std::string log_msg = req.method + " " + req.path;
    Logger::info(log_msg);
}

inline void log_response(const std::string& path, int status) {
    std::string log_msg = path + " responded with status " + std::to_string(status);
    Logger::info(log_msg);
}

// ====== Cookie 辅助函数 ======

// 从 Cookie 头中提取指定 cookie 的值
inline std::string get_cookie_value(const httplib::Request& req, const std::string& name) {
    auto it = req.headers.find("Cookie");
    if (it == req.headers.end()) return "";

    std::string cookie_header = it->second;
    std::string search = name + "=";
    size_t pos = cookie_header.find(search);
    if (pos == std::string::npos) return "";

    pos += search.size();
    size_t end = cookie_header.find(';', pos);
    if (end == std::string::npos) {
        return cookie_header.substr(pos);
    }
    return cookie_header.substr(pos, end - pos);
}

// 安全修复 V11：Cookie 加 Secure 属性（仅 HTTPS 下开启）
inline void set_session_cookie(httplib::Response& res, const std::string& session_id, int max_age) {
    std::string cookie = "sid=" + session_id + "; HttpOnly; Path=/; Max-Age=" +
                         std::to_string(max_age) + "; SameSite=Lax";
    if (g_config.cookie_secure) cookie += "; Secure";
    res.set_header("Set-Cookie", cookie);
}

// 清除认证 cookie
inline void clear_session_cookie(httplib::Response& res) {
    std::string cookie = "sid=; HttpOnly; Path=/; Max-Age=0; SameSite=Lax";
    if (g_config.cookie_secure) cookie += "; Secure";
    res.set_header("Set-Cookie", cookie);
}

// ====== Session 管理函数 ======

// 安全修复 V4：生成 64 字符随机十六进制会话 ID，使用 CSPRNG
inline std::string generate_session_id() {
    return generate_random_hex(32); // 32 字节 = 64 hex 字符
}

// ====== 安全修复 V8：登录失败锁定（内存计数，按用户名 + 客户端 IP 维度）======
struct LoginAttempt {
    int fails = 0;
    time_t locked_until = 0;
};
inline std::map<std::string, LoginAttempt>& login_attempts() {
    static std::map<std::string, LoginAttempt> m;
    return m;
}

// 返回 true 表示当前允许尝试登录（未被锁定）
inline bool login_can_try(const std::string& key) {
    auto& m = login_attempts();
    auto it = m.find(key);
    if (it == m.end()) return true;
    if (it->second.locked_until > time(nullptr)) return false;
    return true;
}

// 登录失败累计 +1，超过阈值则锁定
inline void login_record_fail(const std::string& key) {
    auto& m = login_attempts();
    auto& a = m[key];
    a.fails++;
    if (a.fails >= g_config.max_login_attempts) {
        a.locked_until = time(nullptr) + g_config.lockout_minutes * 60;
        a.fails = 0; // 锁定后清零，解锁后重新计数
    }
}

// 登录成功清除计数
inline void login_record_success(const std::string& key) {
    login_attempts().erase(key);
}

inline std::string login_client_key(const httplib::Request& req, const std::string& username) {
    std::string ip;
    auto it = req.headers.find("X-Forwarded-For");
    if (it != req.headers.end()) {
        ip = it->second;
        auto comma = ip.find(',');
        if (comma != std::string::npos) ip = ip.substr(0, comma);
    } else {
        auto rit = req.headers.find("REMOTE_ADDR");
        if (rit != req.headers.end()) ip = rit->second;
    }
    if (ip.empty()) ip = "unknown";
    return ip + "|" + username;
}

// ====== 安全修复 V10：CSRF Token（双重提交 Cookie）======
// 校验：请求头 X-CSRF-Token 与 Cookie 中 csrf_token 相等且非空
inline bool csrf_check(const httplib::Request& req) {
    if (!g_config.csrf_enabled) return true;
    std::string cookie_token = get_cookie_value(req, "csrf_token");
    if (cookie_token.empty()) return false;
    auto it = req.headers.find("X-CSRF-Token");
    if (it == req.headers.end()) return false;
    if (it->second.size() != cookie_token.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < cookie_token.size(); i++) diff |= (unsigned char)(cookie_token[i] ^ it->second[i]);
    return diff == 0;
}

// 生成新 CSRF Token 并写入 Cookie
inline std::string issue_csrf_token(httplib::Response& res) {
    std::string token = generate_random_hex(16);
    std::string cookie = "csrf_token=" + token + "; HttpOnly; Path=/; SameSite=Lax";
    if (g_config.cookie_secure) cookie += "; Secure";
    res.set_header("Set-Cookie", cookie);
    return token;
}

// 便捷中间件：对状态变更类请求（POST/PUT/DELETE）做 CSRF 校验
inline bool require_csrf(const httplib::Request& req, httplib::Response& res) {
    if (req.method == "GET" || req.method == "HEAD" || req.method == "OPTIONS") return true;
    if (csrf_check(req)) return true;
    set_cors_headers(req, res);
    res.status = 403;
    res.set_content(R"({"code":403,"msg":"CSRF token 校验失败"})", "application/json");
    return false;
}

// 创建会话并存入 SQLite
inline bool create_session(const std::string& session_id, const std::string& user_id, int role_id,
                           int expiry_hours, bool is_parent = false, const std::string& student_id = "") {
    time_t now = time(nullptr);
    time_t expires = now + expiry_hours * 3600;

    // 学生 ID 字段的 SQL 文本表示（NULL 或 'escaped_id'）
    std::string student_id_sql = student_id.empty() ? std::string("NULL")
                                                     : (std::string("'") + db.escapeString(student_id) + "'");

    char sql[1024];
    snprintf(sql, sizeof(sql),
        "INSERT OR REPLACE INTO sessions (session_id, user_id, role_id, created_at, expires_at, is_parent, student_id) "
        "VALUES ('%s', '%s', %d, %ld, %ld, %d, %s)",
        db.escapeString(session_id).c_str(),
        db.escapeString(user_id).c_str(),
        role_id,
        static_cast<long>(now),
        static_cast<long>(expires),
        is_parent ? 1 : 0,
        student_id_sql.c_str()
    );
    return db.execute(sql);
}

// 验证会话有效性，返回 user_id（成功）或空串（失败）
inline std::string verify_session(const std::string& session_id) {
    if (session_id.empty()) return "";

    char sql[1024];
    snprintf(sql, sizeof(sql),
        "SELECT user_id, expires_at FROM sessions WHERE session_id = '%s'",
        db.escapeString(session_id).c_str());
    auto result = db.query(sql);
    if (result.empty()) return "";

    long expires_at = result[0].value("expires_at", 0L);
    time_t now = time(nullptr);
    if (now > expires_at) {
        // 会话已过期，删除
        char del_sql[256];
        snprintf(del_sql, sizeof(del_sql), "DELETE FROM sessions WHERE session_id = '%s'",
                 db.escapeString(session_id).c_str());
        db.execute(del_sql);
        return "";
    }

    return result[0].value("user_id", "");
}

// 获取会话信息
inline bool get_session_info(const std::string& session_id, std::string& user_id, int& role_id,
                             bool& is_parent, std::string& student_id) {
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "SELECT user_id, role_id, is_parent, student_id FROM sessions WHERE session_id = '%s' AND expires_at > %ld",
        db.escapeString(session_id).c_str(), static_cast<long>(time(nullptr)));
    auto result = db.query(sql);
    if (result.empty()) return false;

    user_id = result[0].value("user_id", "");
    role_id = result[0].value("role_id", 0);
    is_parent = result[0].value("is_parent", 0) != 0;
    if (result[0].contains("student_id") && !result[0]["student_id"].is_null()) {
        student_id = result[0].value("student_id", "");
    } else {
        student_id = "";
    }
    return true;
}

// 删除会话（退出登录）
inline bool delete_session(const std::string& session_id) {
    if (session_id.empty()) return false;
    char sql[256];
    snprintf(sql, sizeof(sql), "DELETE FROM sessions WHERE session_id = '%s'",
             db.escapeString(session_id).c_str());
    return db.execute(sql);
}

// 清理过期会话
inline void cleanup_expired_sessions() {
    char sql[128];
    snprintf(sql, sizeof(sql), "DELETE FROM sessions WHERE expires_at < %ld",
             static_cast<long>(time(nullptr)));
    db.execute(sql);
}

// ====== 认证命名空间 ======
namespace Auth {
    // 检查用户是否有权限
    inline bool check_permission(const string& user_id, const string& permission_code) {
        auto user_it = find_if(users.begin(), users.end(), [&](const User& u) {
            return u.id == user_id;
        });

        if (user_it == users.end()) {
            return false;
        }

        int role_id = user_it->role_id;

        for (const auto& rp : role_permissions) {
            if (rp.role_id == role_id) {
                auto perm_it = find_if(permissions.begin(), permissions.end(), [&](const Permission& p) {
                    return p.id == rp.permission_id && p.code == permission_code;
                });
                if (perm_it != permissions.end()) {
                    return true;
                }
            }
        }

        return false;
    }

    // 检查用户是否有指定角色
    inline bool check_role(const string& user_id, int role_id) {
        auto user_it = find_if(users.begin(), users.end(), [&](const User& u) {
            return u.id == user_id;
        });

        if (user_it == users.end()) {
            return false;
        }

        return user_it->role_id == role_id;
    }
}

// ====== 认证中间件 ======

// 权限检查中间件（普通用户）
// 从 Cookie 中提取 session_id，验证会话，检查权限
inline bool check_permission_middleware(const httplib::Request& req, httplib::Response& res, const string& permission_code) {
    std::string session_id = get_cookie_value(req, "sid");

    if (session_id.empty()) {
        res.status = 401;
        json response = {{"code", 401}, {"msg", "未登录"}};
        set_cors_headers(res);
        res.set_content(response.dump(), "application/json");
        return false;
    }

    std::string user_id = verify_session(session_id);
    if (user_id.empty()) {
        res.status = 401;
        json response = {{"code", 401}, {"msg", "会话无效或已过期"}};
        set_cors_headers(res);
        res.set_content(response.dump(), "application/json");
        return false;
    }

    // 检查是否为家长会话（家长不应通过普通权限检查）
    int role_id;
    bool is_parent;
    std::string student_id;
    if (!get_session_info(session_id, user_id, role_id, is_parent, student_id) || is_parent) {
        res.status = 403;
        json response = {{"code", 403}, {"msg", "权限不足"}};
        set_cors_headers(res);
        res.set_content(response.dump(), "application/json");
        return false;
    }

    if (!Auth::check_permission(user_id, permission_code)) {
        res.status = 403;
        json response = {{"code", 403}, {"msg", "权限不足"}};
        set_cors_headers(res);
        res.set_content(response.dump(), "application/json");
        return false;
    }

    return true;
}

// 家长端权限中间件
// 从 Cookie 中提取 session_id，验证家长会话，返回家长 user_id（字符串）
// 验证失败时返回空串并设置响应
inline std::string check_parent_auth_middleware(const httplib::Request& req, httplib::Response& res) {
    std::string session_id = get_cookie_value(req, "sid");

    if (session_id.empty()) {
        res.status = 401;
        json response = {{"code", 401}, {"msg", "未登录"}};
        set_cors_headers(res);
        res.set_content(response.dump(), "application/json");
        return "";
    }

    std::string user_id = verify_session(session_id);
    if (user_id.empty()) {
        res.status = 401;
        json response = {{"code", 401}, {"msg", "会话无效或已过期"}};
        set_cors_headers(res);
        res.set_content(response.dump(), "application/json");
        return "";
    }

    int role_id;
    bool is_parent;
    std::string student_id;
    if (!get_session_info(session_id, user_id, role_id, is_parent, student_id) || !is_parent) {
        res.status = 403;
        json response = {{"code", 403}, {"msg", "非家长会话"}};
        set_cors_headers(res);
        res.set_content(response.dump(), "application/json");
        return "";
    }

    // 注意：本函数返回家长 user_id（字符串）；具体子女列表通过 parent_students 表 JOIN 获取
    return user_id;
}

#endif
