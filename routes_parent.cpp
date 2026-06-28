#include "routes.h"
#include "auth.h"
#include "models.h"
#include "logger.h"
#include <unordered_map>

// 辅助函数：安全获取字符串字段（处理 NULL 情况）
static std::string get_string_field(const json& row, const std::string& key) {
    if (row.contains(key) && !row[key].is_null()) {
        return row[key].get<std::string>();
    }
    return "";
}

// 辅助函数：安全获取整数字段（处理 NULL 情况）
static int get_int_field(const json& row, const std::string& key, int default_val = 0) {
    if (row.contains(key) && !row[key].is_null()) {
        try { return row[key].get<int>(); } catch (...) { return default_val; }
    }
    return default_val;
}

// 辅助函数：检查 target_student_id 是否与 parent_id 在 parent_students 表中关联
// 用于家长端越权校验，确保家长只能访问自己子女的数据
static bool check_same_parent(const std::string& parent_id, const std::string& target_student_id) {
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT 1 FROM parent_students WHERE parent_id = '%s' AND student_id = '%s' LIMIT 1",
        db.escapeString(parent_id).c_str(),
        db.escapeString(target_student_id).c_str());
    auto result = db.query(sql);
    return !result.empty();
}

// 评价维度名称映射
static std::string get_dimension_name(int dimension_id) {
    static const std::unordered_map<int, std::string> dimension_map = {
        {1, "德育"}, {2, "智育"}, {3, "体育"}, {4, "美育"}, {5, "劳育"}
    };
    auto it = dimension_map.find(dimension_id);
    if (it != dimension_map.end()) {
        return it->second;
    }
    return "未知";
}

void register_parent_routes(httplib::Server& svr) {
    // ================= 家长端 API =================

    // 1. 家长登录 API
    // 家长使用 parent-xxx 用户名 + password_hash 登录（不再用学生用户名 + parent_password）
    svr.Post("/api/parent/login", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        json response;

        try {
            auto req_json = json::parse(req.body);
            std::string username = req_json.value("username", "");
            std::string password = req_json.value("password", "");

            if (username.empty() || password.empty()) {
                response = {{"code", 400}, {"msg", "用户名和密码不能为空"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 通过用户名查找家长用户（role_id=4）
            User* user = find_user_by_username(username);
            if (!user || user->role_id != 4) {
                response = {{"code", 401}, {"msg", "家长账号不存在"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 验证家长密码
            if (!verify_password(password, user->password_hash)) {
                response = {{"code", 401}, {"msg", "家长密码错误"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 生成家长会话并存入数据库（user_id 为家长字符串 id）
            cleanup_expired_sessions();
            std::string session_id = generate_session_id();
            create_session(session_id, user->id, 4, g_config.session_expiry_hours, true, user->id);

            // 设置 HttpOnly cookie
            int max_age = g_config.session_expiry_hours * 3600;
            set_session_cookie(res, session_id, max_age);

            // 通过 parent_students 表 JOIN 查询所有子女
            json children = json::array();
            char sql[512];
            snprintf(sql, sizeof(sql),
                "SELECT u.id, u.name, u.className, u.points FROM users u "
                "JOIN parent_students ps ON u.id = ps.student_id "
                "WHERE ps.parent_id = '%s'",
                db.escapeString(user->id).c_str());
            auto children_result = db.query(sql);
            for (const auto& row : children_result) {
                json child;
                child["id"] = get_string_field(row, "id");
                child["name"] = get_string_field(row, "name");
                child["className"] = get_string_field(row, "className");
                child["points"] = row.value("points", 0);
                children.push_back(child);
            }

            Logger::info("家长登录成功，家长用户名: " + username);
            response = {
                {"code", 200},
                {"msg", "登录成功"},
                {"data", {
                    {"name", user->name},
                    {"username", username},
                    {"children", children}
                }}
            };

        } catch (json::parse_error& e) {
            response = {{"code", 400}, {"msg", "请求数据格式错误"}};
        } catch (const std::exception& e) {
            Logger::error("家长登录异常: " + std::string(e.what()));
            response = {{"code", 500}, {"msg", "服务器内部错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 家长退出登录 API
    svr.Post("/api/parent/logout", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        std::string session_id = get_cookie_value(req, "sid");
        if (!session_id.empty()) {
            delete_session(session_id);
        }
        clear_session_cookie(res);
        json response = {{"code", 200}, {"msg", "退出成功"}};
        res.set_content(response.dump(), "application/json");
    });

    // 2. 获取子女列表 API
    svr.Get("/api/parent/children", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 验证家长 token，返回家长 user_id（字符串）
        std::string parent_id = check_parent_auth_middleware(req, res);
        if (parent_id.empty()) {
            return;
        }

        json response;
        json children = json::array();

        // 通过 parent_students 表 JOIN 查询所有子女
        char sql[512];
        snprintf(sql, sizeof(sql),
            "SELECT u.id, u.name, u.className, u.points FROM users u "
            "JOIN parent_students ps ON u.id = ps.student_id "
            "WHERE ps.parent_id = '%s'",
            db.escapeString(parent_id).c_str());
        auto children_result = db.query(sql);
        for (const auto& row : children_result) {
            json child;
            child["id"] = get_string_field(row, "id");
            child["name"] = get_string_field(row, "name");
            child["className"] = get_string_field(row, "className");
            child["points"] = row.value("points", 0);
            children.push_back(child);
        }

        response = {{"code", 200}, {"data", children}};
        res.set_content(response.dump(), "application/json");
    });

    // 3. 获取子女信息 API
    svr.Get(R"(/api/parent/student/([^/]+)/info)", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 验证家长 token
        std::string parent_id = check_parent_auth_middleware(req, res);
        if (parent_id.empty()) {
            return;
        }

        json response;
        try {
            std::string target_id = req.matches[1];

            // 越权校验：目标学生必须与该家长在 parent_students 表中关联
            if (!check_same_parent(parent_id, target_id)) {
                response = {{"code", 403}, {"msg", "无权访问该学生信息"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 查询子女信息及班级排名（按 points 降序计算）
            char sql[512];
            snprintf(sql, sizeof(sql),
                "SELECT u.id, u.name, u.username, u.className, u.points, "
                "(SELECT COUNT(*) + 1 FROM users u2 WHERE u2.className = u.className AND u2.role_id = 3 AND u2.points > u.points) as rank "
                "FROM users u WHERE u.id = '%s' AND u.role_id = 3",
                db.escapeString(target_id).c_str());
            auto result = db.query(sql);
            if (result.empty()) {
                response = {{"code", 404}, {"msg", "学生不存在"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            const auto& row = result[0];
            json student_info;
            student_info["id"] = get_string_field(row, "id");
            student_info["name"] = get_string_field(row, "name");
            student_info["username"] = get_string_field(row, "username");
            student_info["className"] = get_string_field(row, "className");
            student_info["points"] = row.value("points", 0);
            student_info["rank"] = row.value("rank", 0);

            response = {{"code", 200}, {"data", student_info}};

        } catch (const std::exception& e) {
            Logger::error("获取子女信息异常: " + std::string(e.what()));
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 4. 获取子女积分记录 API
    svr.Get(R"(/api/parent/student/([^/]+)/points)", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        std::string parent_id = check_parent_auth_middleware(req, res);
        if (parent_id.empty()) {
            return;
        }

        json response;
        try {
            std::string target_id = req.matches[1];

            if (!check_same_parent(parent_id, target_id)) {
                response = {{"code", 403}, {"msg", "无权访问该学生信息"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            json records = json::array();

            char sql[512];
            snprintf(sql, sizeof(sql),
                "SELECT id, points, reason, created_at FROM points_records "
                "WHERE student_id = '%s' ORDER BY created_at DESC",
                db.escapeString(target_id).c_str());
            auto result = db.query(sql);
            for (const auto& row : result) {
                json record;
                int points = row.value("points", 0);
                record["id"] = row.value("id", 0);
                record["points"] = points;
                record["type"] = points >= 0 ? "奖励" : "扣除";
                record["reason"] = get_string_field(row, "reason");
                record["time"] = get_string_field(row, "created_at");
                records.push_back(record);
            }

            response = {{"code", 200}, {"data", records}};
        } catch (const std::exception& e) {
            Logger::error("获取子女积分记录异常: " + std::string(e.what()));
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 5. 获取子女评价记录 API
    svr.Get(R"(/api/parent/student/([^/]+)/evaluation)", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        std::string parent_id = check_parent_auth_middleware(req, res);
        if (parent_id.empty()) {
            return;
        }

        json response;
        try {
            std::string target_id = req.matches[1];

            if (!check_same_parent(parent_id, target_id)) {
                response = {{"code", 403}, {"msg", "无权访问该学生信息"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            json evaluations = json::array();

            char sql[512];
            snprintf(sql, sizeof(sql),
                "SELECT e.id, e.dimension_id, e.score, e.comment, e.created_at, "
                "u.name as evaluator_name "
                "FROM evaluations e "
                "LEFT JOIN users u ON e.evaluator_id = u.id "
                "WHERE e.student_id = '%s' "
                "ORDER BY e.created_at DESC",
                db.escapeString(target_id).c_str());
            auto result = db.query(sql);
            for (const auto& row : result) {
                json eval;
                int dimension_id = row.value("dimension_id", 0);
                eval["id"] = row.value("id", 0);
                eval["dimension_id"] = dimension_id;
                eval["dimension_name"] = get_dimension_name(dimension_id);
                eval["score"] = row.value("score", 0);
                eval["comment"] = get_string_field(row, "comment");
                eval["evaluator_name"] = get_string_field(row, "evaluator_name");
                eval["time"] = get_string_field(row, "created_at");
                evaluations.push_back(eval);
            }

            response = {{"code", 200}, {"data", evaluations}};
        } catch (const std::exception& e) {
            Logger::error("获取子女评价记录异常: " + std::string(e.what()));
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 6. 获取子女兑换记录 API
    svr.Get(R"(/api/parent/student/([^/]+)/redemptions)", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        std::string parent_id = check_parent_auth_middleware(req, res);
        if (parent_id.empty()) {
            return;
        }

        json response;
        try {
            std::string target_id = req.matches[1];

            if (!check_same_parent(parent_id, target_id)) {
                response = {{"code", 403}, {"msg", "无权访问该学生信息"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            json redemptions = json::array();

            char sql[512];
            snprintf(sql, sizeof(sql),
                "SELECT r.id, r.item_id, r.cost, r.created_at, "
                "m.name as item_name "
                "FROM redemption_records r "
                "LEFT JOIN mall_items m ON r.item_id = m.id "
                "WHERE r.student_id = '%s' "
                "ORDER BY r.created_at DESC",
                db.escapeString(target_id).c_str());
            auto result = db.query(sql);
            for (const auto& row : result) {
                json redemption;
                redemption["id"] = row.value("id", 0);
                redemption["item_id"] = row.value("item_id", 0);
                redemption["item_name"] = get_string_field(row, "item_name");
                redemption["cost"] = row.value("cost", 0);
                redemption["time"] = get_string_field(row, "created_at");
                redemptions.push_back(redemption);
            }

            response = {{"code", 200}, {"data", redemptions}};
        } catch (const std::exception& e) {
            Logger::error("获取子女兑换记录异常: " + std::string(e.what()));
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 7. 获取子女留言列表 API
    svr.Get(R"(/api/parent/student/([^/]+)/messages)", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        std::string parent_id = check_parent_auth_middleware(req, res);
        if (parent_id.empty()) {
            return;
        }

        json response;
        try {
            std::string target_id = req.matches[1];

            if (!check_same_parent(parent_id, target_id)) {
                response = {{"code", 403}, {"msg", "无权访问该学生信息"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            json messages = json::array();

            char sql[512];
            snprintf(sql, sizeof(sql),
                "SELECT id, sender_type, sender_id, content, reply_to, created_at, read_status "
                "FROM parent_messages "
                "WHERE student_id = '%s' ORDER BY created_at ASC",
                db.escapeString(target_id).c_str());
            auto result = db.query(sql);
            for (const auto& row : result) {
                json msg;
                msg["id"] = get_int_field(row, "id");
                msg["sender_type"] = get_string_field(row, "sender_type");
                msg["sender_id"] = get_string_field(row, "sender_id");
                msg["content"] = get_string_field(row, "content");
                msg["reply_to"] = get_int_field(row, "reply_to");
                msg["created_at"] = get_string_field(row, "created_at");
                msg["read_status"] = get_int_field(row, "read_status");
                messages.push_back(msg);
            }

            response = {{"code", 200}, {"data", messages}};
        } catch (const std::exception& e) {
            Logger::error("获取子女留言列表异常: " + std::string(e.what()));
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 8. 家长发送留言 API
    svr.Post(R"(/api/parent/student/([^/]+)/messages)", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        std::string parent_id = check_parent_auth_middleware(req, res);
        if (parent_id.empty()) {
            return;
        }

        json response;
        try {
            std::string target_id = req.matches[1];

            if (!check_same_parent(parent_id, target_id)) {
                response = {{"code", 403}, {"msg", "无权访问该学生信息"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            auto req_json = json::parse(req.body);
            std::string content = req_json.value("content", "");

            if (content.empty()) {
                response = {{"code", 400}, {"msg", "留言内容不能为空"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            char sql[1024];
            snprintf(sql, sizeof(sql),
                "INSERT INTO parent_messages (student_id, sender_type, sender_id, content, reply_to, created_at, read_status) "
                "VALUES ('%s', 'parent', '%s', '%s', NULL, '%s', 0)",
                db.escapeString(target_id).c_str(),
                db.escapeString(parent_id).c_str(),
                db.escapeString(content).c_str(),
                get_current_time().c_str());

            if (db.execute(sql)) {
                Logger::info("家长(parent_id=" + parent_id +
                             ") 给学生 " + target_id + " 发送了留言");
                response = {{"code", 200}, {"msg", "留言发送成功"}};
            } else {
                response = {{"code", 500}, {"msg", "留言发送失败"}};
            }

        } catch (json::parse_error& e) {
            response = {{"code", 400}, {"msg", "请求数据格式错误"}};
        } catch (const std::exception& e) {
            Logger::error("家长发送留言异常: " + std::string(e.what()));
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });
}
