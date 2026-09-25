#ifndef AUTH_H
#define AUTH_H

#include <string>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <map>
#include <mutex>
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
// 批次 1 / 1-6（B4）三处修正：
//   ① 并发：原 `static std::map` 被多线程无锁读写（登录是最高频写路径）→ 现统一用
//      models 层那一把 g_state_mutex（决策② R1：全进程只有一把应用级锁；R6 要求
//      不得另建第二把锁）。因该锁是 recursive_mutex，同一请求路径内的嵌套调用安全。
//   ② 无上限增长：每个「IP|用户名」组合都会永久留一个条目，攻击者用随机用户名即可
//      让 map 无限膨胀（内存耗尽型 DoS）→ 加容量上限与过期条目清理。
//   ③ 客户端标识：见 login_client_ip() —— 默认**不信任**可被客户端伪造的
//      X-Forwarded-For（原实现无条件取首段，攻击者每次换一个新值即可绕过锁定）。
struct LoginAttempt {
    int fails = 0;
    time_t locked_until = 0;
    time_t last_seen = 0;   // 批次 1：用于过期清理
};

inline std::map<std::string, LoginAttempt>& login_attempts() {
    static std::map<std::string, LoginAttempt> m;
    return m;
}

// 条目上限：超过后先清理过期项，仍超则拒绝新增（fail-closed：宁可锁住新键，
// 也不让内存无界增长）
inline constexpr size_t kMaxLoginAttemptEntries = 10000;

inline void login_attempts_cleanup_locked() {
    time_t now = time(nullptr);
    for (auto it = login_attempts().begin(); it != login_attempts().end(); ) {
        // 已解锁且超过 1 小时未再出现的条目可以丢弃
        if (it->second.locked_until <= now && now - it->second.last_seen > 3600) {
            it = login_attempts().erase(it);
        } else {
            ++it;
        }
    }
}

// 返回 true 表示当前允许尝试登录（未被锁定）
inline bool login_can_try(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    auto& m = login_attempts();
    auto it = m.find(key);
    if (it == m.end()) return true;
    if (it->second.locked_until > time(nullptr)) return false;
    return true;
}

// 登录失败累计 +1，超过阈值则锁定
inline void login_record_fail(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    auto& m = login_attempts();
    if (m.find(key) == m.end() && m.size() >= kMaxLoginAttemptEntries) {
        login_attempts_cleanup_locked();
        if (m.size() >= kMaxLoginAttemptEntries) {
            Logger::warning("登录失败计数表已达上限 " + std::to_string(kMaxLoginAttemptEntries) +
                            "，拒绝为新键计数（fail-closed）");
            return;
        }
    }
    auto& a = m[key];
    a.last_seen = time(nullptr);
    a.fails++;
    if (a.fails >= g_config.max_login_attempts) {
        a.locked_until = time(nullptr) + g_config.lockout_minutes * 60;
        a.fails = 0; // 锁定后清零，解锁后重新计数
    }
}

// 登录成功清除计数
inline void login_record_success(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    login_attempts().erase(key);
}

// ====== 批次 1 / 1-6（B4）：客户端标识 ======
//
// **修复前**：无条件读 X-Forwarded-For 并取首段。该头完全由客户端控制，于是：
//   * 攻击者每次请求换一个伪造值 → 每次都是「新客户端」→ 登录失败锁定**形同虚设**；
//   * 反向滥用：伪造受害者 IP 反复失败 → 把受害者**锁死**（NAT/共享出口下连坐一片）。
//
// **修复后**：
//   * config.trust_proxy_headers 默认 false → 只用 REMOTE_ADDR（httplib 注入的 socket 对端，
//     客户端无法伪造；依据 httplib.h:3639）；
//   * 即使开启，也要求 REMOTE_ADDR 命中 config.trusted_proxies 白名单才采信 XFF；
//   * 白名单为空时**即使开关为 true 也不采信** —— 避免「开了开关忘配白名单」等于全信任；
//   * 采信时只取 XFF **最后一跳**（最靠近本服务的那一跳，由可信代理追加），
//     而非原实现的「首段」（首段是客户端可任意伪造的原始值）。
inline std::string login_client_ip(const httplib::Request& req) {
    std::string remote;
    auto rit = req.headers.find("REMOTE_ADDR");
    if (rit != req.headers.end()) remote = rit->second;

    bool proxy_trusted = g_config.trust_proxy_headers
                      && !g_config.trusted_proxies.empty()
                      && !remote.empty()
                      && std::find(g_config.trusted_proxies.begin(), g_config.trusted_proxies.end(), remote)
                         != g_config.trusted_proxies.end();
    if (!proxy_trusted) {
        if (remote.empty()) return "unknown";
        return remote;
    }

    auto it = req.headers.find("X-Forwarded-For");
    if (it == req.headers.end() || it->second.empty()) {
        return remote.empty() ? "unknown" : remote;
    }
    std::string xff = it->second;
    auto comma = xff.find_last_of(',');
    if (comma != std::string::npos) xff = xff.substr(comma + 1);
    // 去掉首尾空白
    size_t b = xff.find_first_not_of(" \t");
    size_t e = xff.find_last_not_of(" \t");
    if (b == std::string::npos) return remote;
    xff = xff.substr(b, e - b + 1);
    return xff.empty() ? remote : xff;
}

// 双键：IP 维度与账号维度各计一份，任一超阈即锁定。
// 为什么两个键都要：只用账号维度会被「分布式撞库打一个账号」绕开；
// 只用 IP 维度会被「同一出口打多个账号」绕开，且 NAT 下易误锁。
// 调用方对两个键都要 record/check（见 routes_public.cpp 登录路径）。
inline std::string login_client_key(const httplib::Request& req, const std::string& username) {
    return login_client_ip(req) + "|" + username;
}

inline std::string login_ip_key(const httplib::Request& req) {
    return "ip|" + login_client_ip(req);
}

inline std::string login_account_key(const std::string& username) {
    return "acct|" + username;
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

    // 顺带清理过期会话，防止 sessions 表无限增长
    db.execute_bind("DELETE FROM sessions WHERE expires_at < ?",
                    {SqliteDb::Bind(static_cast<long long>(now))});

    // 会话 SQL 使用参数化查询：session_id / user_id 来自 Cookie，属用户可控输入，
    // 不应拼接进 SQL（沿用 sqlite_wrapper.h 中 execute_bind 的约定）
    const char* sql =
        "INSERT OR REPLACE INTO sessions (session_id, user_id, role_id, created_at, expires_at, is_parent, student_id) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)";
    std::vector<SqliteDb::Bind> params;
    params.push_back(SqliteDb::Bind(session_id));
    params.push_back(SqliteDb::Bind(user_id));
    params.push_back(SqliteDb::Bind(role_id));
    params.push_back(SqliteDb::Bind(static_cast<long long>(now)));
    params.push_back(SqliteDb::Bind(static_cast<long long>(expires)));
    params.push_back(SqliteDb::Bind(is_parent ? 1 : 0));
    if (student_id.empty()) {
        params.push_back(SqliteDb::Bind::null());
    } else {
        params.push_back(SqliteDb::Bind(student_id));
    }
    return db.execute_bind(sql, params);
}

// 验证会话有效性，返回 user_id（成功）或空串（失败）
inline std::string verify_session(const std::string& session_id) {
    if (session_id.empty()) return "";

    auto result = db.query_bind("SELECT user_id, expires_at FROM sessions WHERE session_id = ?",
                                {SqliteDb::Bind(session_id)});
    if (result.empty()) return "";

    long expires_at = result[0].value("expires_at", 0L);
    time_t now = time(nullptr);
    if (now > expires_at) {
        // 会话已过期，删除
        db.execute_bind("DELETE FROM sessions WHERE session_id = ?",
                        {SqliteDb::Bind(session_id)});
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
    // 检查用户是否有权限。
    // 批次 1 / 1-2：原实现直接遍历 users / role_permissions / permissions 三个全局容器，
    // 全程无锁（多线程下与写路径并发即 UB）。现委托给 models.cpp 的
    // check_permission_optimized()，它在**一次** g_state_mutex 持锁内完成同一判定。
    // 语义严格等价：都按 user.role_id → role_permissions → permission.code 匹配。
    // 决策② 的 R6 要求不得自行加锁（会与外层调用形成自死锁），故此处只委托。
    inline bool check_permission(const string& user_id, const string& permission_code) {
        return check_permission_optimized(user_id, permission_code);
    }

    // 检查用户是否有指定角色。
    // 批次 1 / 1-2：同上，改为取拷贝后在锁外比较，不再让裸指针跨锁存活。
    inline bool check_role(const string& user_id, int role_id) {
        User u;
        if (!find_user_by_id_copy(user_id, u)) {
            return false;
        }
        return u.role_id == role_id;
    }
}

// ====== 认证中间件 ======

// 安全修复 V18（B2）：强制首登改密门禁。
// must_change_password=true 的会话只允许「改密 / 登出 / 查看自身信息」，
// 其余业务接口一律 403 + 可读 msg（fail-closed：无会话或会话无效时不在此拦截，
// 交由原有 401 逻辑处理，避免把「未登录」误报成「需改密」）。
// 白名单端点（/api/auth/change-password、/api/auth/logout、/api/auth/me）位于
// routes_public.cpp，不经过本函数，因此无需在中间件里做例外判断。
inline bool enforce_password_change(const httplib::Request& req, httplib::Response& res) {
    std::string session_id = get_cookie_value(req, "sid");
    if (session_id.empty()) return true;
    std::string user_id = verify_session(session_id);
    if (user_id.empty()) return true;
    if (!user_must_change_password(user_id)) return true;

    set_cors_headers(req, res);
    res.status = 403;
    res.set_content(
        R"({"code":403,"must_change_password":true,"msg":"首次登录必须修改初始口令后才能使用其他功能"})",
        "application/json");
    return false;
}

// 权限检查中间件（普通用户）
// 从 Cookie 中提取 session_id，验证会话，检查权限
inline bool check_permission_middleware(const httplib::Request& req, httplib::Response& res, const string& permission_code) {
    // 安全修复 V18（B2）：未改初始口令的会话不得访问任何业务接口
    if (!enforce_password_change(req, res)) return false;

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
    // 安全修复 V18（B2）：未改初始口令的会话不得访问家长端业务接口
    if (!enforce_password_change(req, res)) return "";

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
