#include "routes.h"
#include "auth.h"
#include "models.h"
#include "sha256.h"
#include "logger.h"
#include <unordered_map>
#include <regex>

// 辅助函数：安全获取字符串字段（处理 NULL 情况）
static std::string get_string_field(const json& row, const std::string& key) {
    if (row.contains(key) && !row[key].is_null()) {
        return row[key].get<std::string>();
    }
    return "";
}

// 班级编号自动匹配工具函数
// 年级名→两位编码映射：高一→01, 高二→02, 高三→03, 初一→07, 初二→08, 初三→09
static std::string grade_to_code(const std::string& grade) {
    if (grade == "高一") return "01";
    if (grade == "高二") return "02";
    if (grade == "高三") return "03";
    if (grade == "初一") return "07";
    if (grade == "初二") return "08";
    if (grade == "初三") return "09";
    return "";
}

// 班号数字→两位编码：1→01, 2→02
static std::string classno_to_code(int class_no) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", class_no);
    return std::string(buf);
}

// 从班级名称中提取班号并生成两位编码，如 "高二(1)班" → "01"
static std::string extract_class_code_from_name(const std::string& name) {
    std::regex re("\\((\\d+)\\)");
    std::smatch m;
    if (std::regex_search(name, m, re) && m.size() > 1) {
        try {
            return classno_to_code(std::stoi(m[1].str()));
        } catch (...) {
            return "";
        }
    }
    return "";
}

void register_admin_routes(httplib::Server& svr) {
    // 7. 系统管理 API（需要管理员权限）
    svr.Get("/api/admin/system", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "system:manage")) {
            return;
        }

        json response = {
            {"code", 200},
            {"msg", "success"},
            {"data", {
                {"system_name", "校园文明能量站"},
                {"version", "1.0.0"},
                {"admin_count", 1},
                {"teacher_count", 1},
                {"student_count", 2}
            }}
        };
        res.set_content(response.dump(), "application/json");
    });

    // ================= 管理员端 API =================

    // 17. 商城管理 - 获取商品列表 API（需要管理员权限）
    svr.Get("/api/admin/mall", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "system:manage")) {
            return;
        }

        json items_json = json::array();

        try {
            Logger::info("开始查询商城商品列表");
            json result = db.query("SELECT id, name, description, cost, stock, image_url, status, created_at FROM mall_items ORDER BY id");
            Logger::info("查询完成，结果数量: " + std::to_string(result.size()));
            for (const auto& row : result) {
                json item;
                item["id"] = row.value("id", 0);
                // 处理可能为 NULL 的字符串字段
                if (row.contains("name")) {
                    if (!row["name"].is_null()) {
                        item["name"] = row["name"].get<std::string>();
                    } else {
                        item["name"] = "";
                    }
                } else {
                    item["name"] = "";
                }
                if (row.contains("description")) {
                    if (!row["description"].is_null()) {
                        item["description"] = row["description"].get<std::string>();
                    } else {
                        item["description"] = "";
                    }
                } else {
                    item["description"] = "";
                }
                item["cost"] = row.value("cost", 0);
                item["stock"] = row.value("stock", 0);
                if (row.contains("image_url")) {
                    if (!row["image_url"].is_null()) {
                        item["image_url"] = row["image_url"].get<std::string>();
                    } else {
                        item["image_url"] = "";
                    }
                } else {
                    item["image_url"] = "";
                }
                item["status"] = row.value("status", 1);
                if (row.contains("created_at")) {
                    if (!row["created_at"].is_null()) {
                        item["created_at"] = row["created_at"].get<std::string>();
                    } else {
                        item["created_at"] = "";
                    }
                } else {
                    item["created_at"] = "";
                }
                items_json.push_back(item);
            }
            json response = {{"code", 200}, {"data", items_json}};
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            Logger::error("商城 API 错误: " + std::string(e.what()));
            json response = {{"code", 500}, {"msg", "服务器内部错误"}};
            res.status = 500;
            res.set_content(response.dump(), "application/json");
        }
    });

    // 18. 商城管理 - 添加商品 API（需要管理员权限）
    svr.Post("/api/admin/mall", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "system:manage")) {
            return;
        }

        json response;
        try {
            auto req_json = json::parse(req.body);
            string name = req_json.value("name", "");
            string description = req_json.value("description", "");
            int cost = req_json.value("cost", 0);
            int stock = req_json.value("stock", 0);
            string image_url = req_json.value("image_url", "");
            int status = req_json.value("status", 1);

            if (name.empty() || cost <= 0) {
                response = {{"code", 400}, {"msg", "商品名称和价格不能为空"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            char sql[1024];
            snprintf(sql, sizeof(sql),
                "INSERT INTO mall_items (name, description, cost, stock, image_url, status) VALUES ('%s', '%s', %d, %d, '%s', %d)",
                db.escapeString(name).c_str(),
                db.escapeString(description).c_str(),
                cost, stock,
                db.escapeString(image_url).c_str(),
                status);

            if (db.execute(sql)) {
                response = {{"code", 200}, {"msg", "商品添加成功"}};
            } else {
                response = {{"code", 500}, {"msg", "商品添加失败"}};
            }

        } catch (json::parse_error& e) {
            response = {{"code", 400}, {"msg", "请求数据格式错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 19. 商城管理 - 更新商品 API（需要管理员权限）
    svr.Put(R"(/api/admin/mall/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "system:manage")) {
            return;
        }

        json response;
        try {
            int item_id = stoi(req.matches[1]);
            auto req_json = json::parse(req.body);
            string name = req_json.value("name", "");
            string description = req_json.value("description", "");
            int cost = req_json.value("cost", 0);
            int stock = req_json.value("stock", 0);
            string image_url = req_json.value("image_url", "");
            int status = req_json.value("status", 1);

            char sql[1024];
            snprintf(sql, sizeof(sql),
                "UPDATE mall_items SET name = '%s', description = '%s', cost = %d, stock = %d, image_url = '%s', status = %d WHERE id = %d",
                db.escapeString(name).c_str(),
                db.escapeString(description).c_str(),
                cost, stock,
                db.escapeString(image_url).c_str(),
                status, item_id);

            if (db.execute(sql)) {
                response = {{"code", 200}, {"msg", "商品更新成功"}};
            } else {
                response = {{"code", 404}, {"msg", "商品不存在"}};
            }

        } catch (const exception& e) {
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 20. 商城管理 - 删除商品 API（需要管理员权限）
    svr.Delete(R"(/api/admin/mall/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "system:manage")) {
            return;
        }

        json response;
        try {
            int item_id = stoi(req.matches[1]);

            char sql[256];
            snprintf(sql, sizeof(sql), "DELETE FROM mall_items WHERE id = %d", item_id);

            if (db.execute(sql)) {
                response = {{"code", 200}, {"msg", "商品删除成功"}};
            } else {
                response = {{"code", 404}, {"msg", "商品不存在"}};
            }

        } catch (const exception& e) {
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 22. 管理员查看所有兑换记录 API（需要管理员权限）
    svr.Get("/api/admin/redemptions", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "system:manage")) {
            return;
        }

        json records_json = json::array();

        char sql[1024];
        snprintf(sql, sizeof(sql),
            "SELECT r.id, r.student_id, r.item_id, r.cost, r.created_at, "
            "s.name as student_name, s.className, "
            "m.name as item_name "
            "FROM redemption_records r "
            "LEFT JOIN users s ON r.student_id = s.id "
            "LEFT JOIN mall_items m ON r.item_id = m.id "
            "ORDER BY r.created_at DESC");

        json result = db.query(sql);
        for (const auto& row : result) {
            json record;
            record["id"] = row.value("id", 0);
            record["student_id"] = row.value("student_id", "");
            record["student_name"] = row.value("student_name", "");
            record["className"] = row.value("className", "");
            record["item_id"] = row.value("item_id", 0);
            record["item_name"] = row.value("item_name", "");
            record["cost"] = row.value("cost", 0);
            record["time"] = row.value("created_at", "");
            records_json.push_back(record);
        }

        json response = {{"code", 200}, {"data", records_json}};
        res.set_content(response.dump(), "application/json");
    });

    // 23. 系统配置 - 保存 API（需要管理员权限）
    svr.Put("/api/admin/system/config", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "system:manage")) {
            return;
        }

        json response;
        try {
            auto req_json = json::parse(req.body);
            // 这里可以保存配置到数据库或文件

            response = {{"code", 200}, {"msg", "配置保存成功"}};
        } catch (json::parse_error& e) {
            response = {{"code", 400}, {"msg", "请求数据格式错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 24. 系统备份 API（需要管理员权限）
    svr.Post("/api/admin/system/backup", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "system:manage")) {
            return;
        }

        json response;

        string backup_path = "campus_system_backup_" + get_current_time();
        for (auto& c : backup_path) {
            if (c == ' ' || c == ':') c = '_';
        }
        backup_path += ".db";

        json backup_result = db.query("VACUUM INTO '" + backup_path + "'");
        if (backup_result.is_null()) {
            response = {{"code", 200}, {"msg", "备份成功"}, {"data", {{"path", backup_path}}}};
            Logger::info("数据库备份成功: " + backup_path);
        } else {
            response = {{"code", 500}, {"msg", "备份失败"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 25. 系统概览 API（需要管理员权限）
    svr.Get("/api/admin/dashboard", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "system:manage")) {
            return;
        }

        // 系统概览数据
        int admin_count = 0;
        int teacher_count = 0;
        int student_count = 0;
        int total_points = 0;

        for (const auto& user : users) {
            switch (user.role_id) {
                case 1: admin_count++; break;
                case 2: teacher_count++; break;
                case 3: student_count++; total_points += user.points; break;
            }
        }

        json today_result = db.query("SELECT COUNT(*) as cnt FROM points_records WHERE date(created_at) = date('now', 'localtime')");
        int today_activities = today_result.empty() ? 0 : today_result[0].value("cnt", 0);

        // 查询最近 10 条活动（积分记录 + 兑换记录合并），用于仪表盘"最近活动"展示
        json recent_result = db.query(
            "SELECT id, points, reason, created_at, student_name FROM ("
            "  SELECT pr.id, pr.points, pr.reason, pr.created_at, u.name as student_name "
            "  FROM points_records pr LEFT JOIN users u ON pr.student_id = u.id "
            "  UNION ALL "
            "  SELECT rr.id, -rr.cost as points, m.name as reason, rr.created_at, u.name as student_name "
            "  FROM redemption_records rr LEFT JOIN users u ON rr.student_id = u.id LEFT JOIN mall_items m ON rr.item_id = m.id "
            ") ORDER BY created_at DESC LIMIT 10"
        );
        json recent_activities = json::array();
        for (const auto& row : recent_result) {
            json activity;
            activity["id"] = row.value("id", 0);
            int points = row.value("points", 0);
            std::string student_name = get_string_field(row, "student_name");
            std::string reason = get_string_field(row, "reason");
            std::string time = get_string_field(row, "created_at");
            activity["description"] = student_name + " " + reason + " (" + (points > 0 ? "+" : "") + std::to_string(points) + ")";
            activity["time"] = time;
            recent_activities.push_back(activity);
        }

        json dashboard = {
            {"totalUsers", users.size()},
            {"studentCount", student_count},
            {"teacherCount", teacher_count},
            {"adminCount", admin_count},
            {"totalPoints", total_points},
            {"todayActivities", today_activities},
            {"recentActivities", recent_activities}
        };

        json response = {{"code", 200}, {"data", dashboard}};
        res.set_content(response.dump(), "application/json");
    });

    // 18. 获取用户列表 API（需要管理员权限）
    svr.Get("/api/admin/users", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "user:manage")) {
            return;
        }

        // 家长现在是真实用户（role_id=4），会被下面的循环直接包含
        json users_json = json::array();
        for (const auto& user : users) {
            json user_json = {
                {"id", user.id},
                {"username", user.username},
                {"name", user.name},
                {"role_id", user.role_id},
                {"status", "active"}
            };

            // 教师角色：附带 bound_class_ids
            if (user.role_id == 2) {
                json bound_class_ids = json::array();
                char tc_sql[256];
                snprintf(tc_sql, sizeof(tc_sql),
                    "SELECT class_id FROM teacher_classes WHERE teacher_id = '%s'",
                    db.escapeString(user.id).c_str());
                json tc_result = db.query(tc_sql);
                for (const auto& tc_row : tc_result) {
                    bound_class_ids.push_back(tc_row.value("class_id", 0));
                }
                user_json["bound_class_ids"] = bound_class_ids;
            }

            // 家长角色：附带 bound_student_ids
            if (user.role_id == 4) {
                json bound_student_ids = json::array();
                char ps_sql[256];
                snprintf(ps_sql, sizeof(ps_sql),
                    "SELECT student_id FROM parent_students WHERE parent_id = '%s'",
                    db.escapeString(user.id).c_str());
                json ps_result = db.query(ps_sql);
                for (const auto& ps_row : ps_result) {
                    std::string sid = ps_row.contains("student_id") && !ps_row["student_id"].is_null()
                        ? ps_row["student_id"].get<std::string>() : "";
                    bound_student_ids.push_back(sid);
                }
                user_json["bound_student_ids"] = bound_student_ids;
            }

            users_json.push_back(user_json);
        }

        json response = {{"code", 200}, {"data", users_json}};
        res.set_content(response.dump(), "application/json");
    });

    // 19. 添加用户 API（需要管理员权限）
    svr.Post("/api/admin/users", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "user:manage")) {
            return;
        }

        json response;
        try {
            auto req_json = json::parse(req.body);
            string username = req_json.value("username", "");
            string name = req_json.value("name", "");
            int role_id = req_json.value("role_id", 3);
            string password = req_json.value("password", "");
            string className = req_json.value("className", "");
            // 学生角色新增用户时可选传入年级码/班级码用于生成 ID
            string grade_code = req_json.value("grade_code", "");
            string class_code = req_json.value("class_code", "");

            if (username.empty() || name.empty() || password.empty()) {
                response = {{"code", 400}, {"msg", "用户名、姓名和密码不能为空"}};
            } else {
                // 如果是学生角色，班级不能为空
                if (role_id == 3 && className.empty()) {
                    response = {{"code", 400}, {"msg", "学生必须设置班级"}};
                    res.set_content(response.dump(), "application/json");
                    return;
                }

                // 检查用户名是否已存在
                auto it = find_if(users.begin(), users.end(), [&](const User& u) {
                    return u.username == username;
                });

                if (it != users.end()) {
                    response = {{"code", 400}, {"msg", "用户名已存在"}};
                } else {
                    // 生成新的字符串 ID
                    string new_id = generate_user_id(role_id, grade_code, class_code);
                    User new_user;
                    new_user.id = new_id;
                    new_user.username = username;
                    new_user.password_hash = hash_password(password);
                    new_user.role_id = role_id;
                    new_user.name = name;
                    new_user.className = className;
                    new_user.points = 0;
                    new_user.student_id = (role_id == 3) ? username : "";
                    users.push_back(new_user);
                    update_user_index(new_user);
                    save_user_to_db(new_user);

                    // 家长角色：插入 parent_students 绑定
                    if (role_id == 4) {
                        json bound_student_ids = req_json.value("bound_student_ids", json::array());
                        for (const auto& sid_val : bound_student_ids) {
                            std::string sid;
                            try { sid = sid_val.get<std::string>(); } catch (...) { continue; }
                            if (sid.empty()) continue;
                            char ins_sql[512];
                            snprintf(ins_sql, sizeof(ins_sql),
                                "INSERT OR IGNORE INTO parent_students (parent_id, student_id) VALUES ('%s', '%s')",
                                db.escapeString(new_id).c_str(), db.escapeString(sid).c_str());
                            db.execute(ins_sql);
                        }
                    }
                    // 教师角色：插入 teacher_classes 绑定
                    if (role_id == 2) {
                        json bound_class_ids = req_json.value("bound_class_ids", json::array());
                        for (const auto& cls_id : bound_class_ids) {
                            int cid = 0;
                            try { cid = cls_id.get<int>(); } catch (...) { continue; }
                            char ins_sql[256];
                            snprintf(ins_sql, sizeof(ins_sql),
                                "INSERT OR IGNORE INTO teacher_classes (teacher_id, class_id) VALUES ('%s', %d)",
                                db.escapeString(new_id).c_str(), cid);
                            db.execute(ins_sql);
                        }
                    }

                    response = {{"code", 200}, {"msg", "用户添加成功"}, {"data", {{"id", new_id}}}};
                }
            }
        } catch (const exception& e) {
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 19.5 批量导入学生 API（需要管理员权限）
    svr.Post("/api/admin/students/batch-import", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "user:manage")) {
            return;
        }

        json response;
        try {
            auto req_json = json::parse(req.body);
            json students = req_json.value("students", json::array());

            json success_records = json::array();
            json failed_records = json::array();
            int success_count = 0;
            int failed_count = 0;

            for (size_t i = 0; i < students.size(); i++) {
                const auto& stu = students[i];
                string name = stu.value("name", "");
                string className = stu.value("className", "");
                string username = stu.value("username", "");
                string password = stu.value("password", "");

                // 1. 校验非空
                if (name.empty() || className.empty() || username.empty() || password.empty()) {
                    failed_records.push_back({{"row", i + 1}, {"name", name}, {"reason", "姓名/班级/用户名/密码不能为空"}});
                    failed_count++;
                    continue;
                }

                // 2. 按 className 查 classes 表获取 grade_code、class_code
                char cls_sql[512];
                snprintf(cls_sql, sizeof(cls_sql),
                    "SELECT grade_code, class_code FROM classes WHERE name = '%s'",
                    db.escapeString(className).c_str());
                json cls_result = db.query(cls_sql);
                if (cls_result.empty()) {
                    string reason = "班级不存在：" + className;
                    failed_records.push_back({{"row", i + 1}, {"name", name}, {"reason", reason}});
                    failed_count++;
                    continue;
                }
                string grade_code = cls_result[0].value("grade_code", "");
                string class_code = cls_result[0].value("class_code", "");
                if (grade_code.empty() || class_code.empty()) {
                    failed_records.push_back({{"row", i + 1}, {"name", name}, {"reason", "班级编码缺失，请先在班级管理补全"}});
                    failed_count++;
                    continue;
                }

                // 3. username 兜底去重
                if (find_user_by_username(username) != nullptr) {
                    string reason = "用户名已存在：" + username;
                    failed_records.push_back({{"row", i + 1}, {"name", name}, {"reason", reason}});
                    failed_count++;
                    continue;
                }

                // 4. 生成学生 ID
                string new_id = generate_user_id(3, grade_code, class_code);

                // 5. 构造 User 并保存
                User new_user;
                new_user.id = new_id;
                new_user.username = username;
                new_user.password_hash = hash_password(password);
                new_user.role_id = 3;
                new_user.name = name;
                new_user.className = className;
                new_user.points = 0;
                new_user.student_id = username;
                users.push_back(new_user);
                update_user_index(new_user);
                save_user_to_db(new_user);

                // 6. 成功记录
                success_records.push_back({
                    {"row", i + 1},
                    {"name", name},
                    {"className", className},
                    {"username", username},
                    {"password", password},
                    {"student_id", new_id}
                });
                success_count++;
            }

            response = {
                {"code", 200},
                {"msg", "导入完成"},
                {"data", {
                    {"success", success_count},
                    {"failed", failed_count},
                    {"records", {
                        {"success_list", success_records},
                        {"failed_list", failed_records}
                    }}
                }}
            };
        } catch (const json::parse_error& e) {
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        } catch (const exception& e) {
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 20. 编辑用户 API（需要管理员权限）
    svr.Put(R"(/api/admin/users/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "user:manage")) {
            return;
        }

        json response;
        try {
            string user_id = req.matches[1];
            auto req_json = json::parse(req.body);

            string username = req_json.value("username", "");
            string name = req_json.value("name", "");
            int role_id = req_json.value("role_id", 0);
            string className = req_json.value("className", "");

            if (username.empty() || name.empty() || role_id == 0) {
                response = {{"code", 400}, {"msg", "用户名、姓名和角色不能为空"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            if (role_id == 3 && className.empty()) {
                response = {{"code", 400}, {"msg", "学生必须设置班级"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            auto user_it = find_if(users.begin(), users.end(), [&](const User& u) {
                return u.id == user_id;
            });

            if (user_it == users.end()) {
                response = {{"code", 404}, {"msg", "用户不存在"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            auto username_check = find_if(users.begin(), users.end(), [&](const User& u) {
                return u.username == username && u.id != user_id;
            });

            if (username_check != users.end()) {
                response = {{"code", 400}, {"msg", "用户名已存在"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            string old_username = user_it->username;
            user_it->username = username;
            user_it->name = name;
            user_it->role_id = role_id;
            user_it->className = className;

            char sql[2048];
            snprintf(sql, sizeof(sql),
                "UPDATE users SET username = '%s', name = '%s', role_id = %d, className = '%s', updated_at = CURRENT_TIMESTAMP WHERE id = '%s'",
                db.escapeString(username).c_str(), db.escapeString(name).c_str(), role_id,
                db.escapeString(className).c_str(), db.escapeString(user_id).c_str());
            db.execute(sql);

            update_user_index(*user_it);
            if (old_username != username) {
                remove_user_index(user_id, old_username);
            }

            // 教师角色：同步 teacher_classes 绑定
            if (role_id == 2) {
                json bound_class_ids = req_json.value("bound_class_ids", json::array());
                char del_sql[256];
                snprintf(del_sql, sizeof(del_sql),
                    "DELETE FROM teacher_classes WHERE teacher_id = '%s'",
                    db.escapeString(user_id).c_str());
                db.execute(del_sql);
                for (const auto& cls_id : bound_class_ids) {
                    int cid = 0;
                    try { cid = cls_id.get<int>(); } catch (...) { continue; }
                    char ins_sql[256];
                    snprintf(ins_sql, sizeof(ins_sql),
                        "INSERT OR IGNORE INTO teacher_classes (teacher_id, class_id) VALUES ('%s', %d)",
                        db.escapeString(user_id).c_str(), cid);
                    db.execute(ins_sql);
                }
            }

            // 家长角色：同步 parent_students 绑定
            if (role_id == 4) {
                json bound_student_ids = req_json.value("bound_student_ids", json::array());
                char del_sql[256];
                snprintf(del_sql, sizeof(del_sql),
                    "DELETE FROM parent_students WHERE parent_id = '%s'",
                    db.escapeString(user_id).c_str());
                db.execute(del_sql);
                for (const auto& sid_val : bound_student_ids) {
                    std::string sid;
                    try { sid = sid_val.get<std::string>(); } catch (...) { continue; }
                    if (sid.empty()) continue;
                    char ins_sql[512];
                    snprintf(ins_sql, sizeof(ins_sql),
                        "INSERT OR IGNORE INTO parent_students (parent_id, student_id) VALUES ('%s', '%s')",
                        db.escapeString(user_id).c_str(), db.escapeString(sid).c_str());
                    db.execute(ins_sql);
                }
            }

            response = {
                {"code", 200},
                {"msg", "用户编辑成功"},
                {"data", {
                    {"id", user_it->id},
                    {"username", user_it->username},
                    {"name", user_it->name},
                    {"role_id", user_it->role_id},
                    {"className", user_it->className}
                }}
            };

        } catch (const exception& e) {
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 21. 删除用户 API（需要管理员权限）
    svr.Delete("/api/admin/users", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "user:manage")) {
            return;
        }

        json response;
        try {
            auto req_json = json::parse(req.body);
            string user_id = req_json.value("id", "");

            // 不能删除管理员用户
            auto user_it = find_if(users.begin(), users.end(), [&](const User& u) {
                return u.id == user_id;
            });

            if (user_it == users.end()) {
                response = {{"code", 404}, {"msg", "用户不存在"}};
            } else if (user_it->role_id == 1) {
                response = {{"code", 400}, {"msg", "不能删除管理员用户"}};
            } else {
                string deleted_id = user_it->id;
                string deleted_username = user_it->username;

                // 删除关联的 teacher_classes 和 parent_students 记录
                char del_tc_sql[256];
                snprintf(del_tc_sql, sizeof(del_tc_sql),
                    "DELETE FROM teacher_classes WHERE teacher_id = '%s'",
                    db.escapeString(deleted_id).c_str());
                db.execute(del_tc_sql);

                char del_ps_sql[256];
                snprintf(del_ps_sql, sizeof(del_ps_sql),
                    "DELETE FROM parent_students WHERE parent_id = '%s'",
                    db.escapeString(deleted_id).c_str());
                db.execute(del_ps_sql);

                delete_user_from_db(deleted_id);
                users.erase(user_it);
                remove_user_index(deleted_id, deleted_username);
                response = {{"code", 200}, {"msg", "用户删除成功"}};
            }
        } catch (const exception& e) {
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 20.5 重置用户密码 API（需要管理员权限）
    svr.Post("/api/admin/users/reset-password", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "user:manage")) {
            return;
        }

        json response;
        try {
            auto req_json = json::parse(req.body);
            string user_id = req_json.value("user_id", "");
            string new_password;

            bool auto_generate = req_json.value("auto_generate", true);

            if (user_id.empty()) {
                response = {{"code", 400}, {"msg", "用户ID不能为空"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            if (auto_generate) {
                new_password = generate_random_password();
            } else {
                new_password = req_json.value("new_password", "");
                if (new_password.empty() || new_password.length() < 6) {
                    response = {{"code", 400}, {"msg", "密码长度不能少于6位"}};
                    res.set_content(response.dump(), "application/json");
                    return;
                }
            }

            auto user_it = find_if(users.begin(), users.end(), [&](const User& u) {
                return u.id == user_id;
            });

            if (user_it == users.end()) {
                response = {{"code", 404}, {"msg", "用户不存在"}};
            } else {
                user_it->password_hash = hash_password(new_password);

                char sql[1024];
                snprintf(sql, sizeof(sql),
                    "UPDATE users SET password_hash = '%s', updated_at = CURRENT_TIMESTAMP WHERE id = '%s'",
                    hash_password(new_password).c_str(), db.escapeString(user_id).c_str());
                db.execute(sql);

                std::string session_id = get_cookie_value(req, "sid");
                string operator_id = verify_session(session_id);
                string operator_name = "管理员";
                auto operator_it = find_if(users.begin(), users.end(), [&](const User& u) {
                    return u.id == operator_id;
                });
                if (operator_it != users.end()) {
                    operator_name = operator_it->name;
                }

                Logger::info("管理员 " + operator_name + " 重置了用户 " + user_it->name + " 的密码");

                response = {
                    {"code", 200},
                    {"msg", "密码重置成功"},
                    {"data", {
                        {"user_id", user_id},
                        {"username", user_it->username},
                        {"name", user_it->name},
                        {"new_password", new_password}
                    }}
                };
            }
        } catch (const exception& e) {
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 21. 获取角色列表 API（需要管理员权限）
    svr.Get("/api/admin/roles", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "user:manage")) {
            return;
        }

        json roles_json = json::array();
        for (const auto& role : roles) {
            json role_json = {
                {"id", role.id},
                {"name", role.name},
                {"description", role.description}
            };

            // 对每个角色查询关联权限
            char perm_sql[256];
            snprintf(perm_sql, sizeof(perm_sql),
                "SELECT p.id, p.name, p.code FROM permissions p "
                "INNER JOIN role_permissions rp ON p.id = rp.permission_id "
                "WHERE rp.role_id = %d", role.id);
            json perms_result = db.query(perm_sql);
            json perms_array = json::array();
            for (const auto& p : perms_result) {
                perms_array.push_back({
                    {"id", p.value("id", 0)},
                    {"name", p.value("name", "")},
                    {"code", p.value("code", "")}
                });
            }
            role_json["permissions"] = perms_array;

            roles_json.push_back(role_json);
        }

        json response = {{"code", 200}, {"data", roles_json}};
        res.set_content(response.dump(), "application/json");
    });

    // 22. 添加角色 API（需要管理员权限）
    svr.Post("/api/admin/roles", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "user:manage")) {
            return;
        }

        json response;
        try {
            auto req_json = json::parse(req.body);
            string name = req_json.value("name", "");
            string description = req_json.value("description", "");

            if (name.empty() || description.empty()) {
                response = {{"code", 400}, {"msg", "角色名称和描述不能为空"}};
            } else {
                // 添加新角色
                int new_id = roles.empty() ? 1 : roles.back().id + 1;
                roles.push_back(Role{new_id, name, description});
                response = {{"code", 200}, {"msg", "角色添加成功"}};
            }
        } catch (const exception& e) {
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 23. 获取权限列表 API（需要管理员权限）
    svr.Get("/api/admin/permissions", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "user:manage")) {
            return;
        }

        json permissions_json = json::array();
        for (const auto& perm : permissions) {
            json perm_json = {
                {"id", perm.id},
                {"name", perm.name},
                {"code", perm.code},
                {"description", perm.description}
            };
            permissions_json.push_back(perm_json);
        }

        json response = {{"code", 200}, {"data", permissions_json}};
        res.set_content(response.dump(), "application/json");
    });

    // 23.5 添加权限 API（需要管理员权限）
    svr.Post("/api/admin/permissions", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "user:manage")) {
            return;
        }

        json response;
        try {
            auto req_json = json::parse(req.body);
            string name = req_json.value("name", "");
            string code = req_json.value("code", "");
            string description = req_json.value("description", "");

            if (name.empty() || code.empty() || description.empty()) {
                response = {{"code", 400}, {"msg", "权限名称、代码和描述不能为空"}};
            } else {
                // 检查权限代码是否已存在
                auto it = find_if(permissions.begin(), permissions.end(), [&](const Permission& p) {
                    return p.code == code;
                });

                if (it != permissions.end()) {
                    response = {{"code", 400}, {"msg", "权限代码已存在"}};
                } else {
                    // 添加新权限
                    int new_id = permissions.empty() ? 1 : permissions.back().id + 1;
                    permissions.push_back(Permission{new_id, name, code, description});
                    response = {{"code", 200}, {"msg", "权限添加成功"}};
                }
            }
        } catch (const exception& e) {
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 23.6 删除权限 API（需要管理员权限）
    svr.Delete("/api/admin/permissions", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "user:manage")) {
            return;
        }

        json response;
        try {
            auto req_json = json::parse(req.body);
            int permission_id = req_json.value("id", 0);

            if (permission_id <= 12) {
                response = {{"code", 400}, {"msg", "不能删除系统内置权限"}};
            } else {
                auto it = find_if(permissions.begin(), permissions.end(), [&](const Permission& p) {
                    return p.id == permission_id;
                });

                if (it == permissions.end()) {
                    response = {{"code", 404}, {"msg", "权限不存在"}};
                } else {
                    permissions.erase(it);
                    response = {{"code", 200}, {"msg", "权限删除成功"}};
                }
            }
        } catch (const exception& e) {
            response = {{"code", 400}, {"msg", "无效的权限 ID"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 23.7 删除角色 API（需要管理员权限）
    svr.Delete("/api/admin/roles", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "user:manage")) {
            return;
        }

        json response;
        try {
            auto req_json = json::parse(req.body);
            int role_id = req_json.value("id", 0);

            if (role_id <= 4) {
                response = {{"code", 400}, {"msg", "不能删除系统内置角色"}};
            } else {
                auto it = find_if(roles.begin(), roles.end(), [&](const Role& r) {
                    return r.id == role_id;
                });

                if (it == roles.end()) {
                    response = {{"code", 404}, {"msg", "角色不存在"}};
                } else {
                    roles.erase(it);
                    response = {{"code", 200}, {"msg", "角色删除成功"}};
                }
            }
        } catch (const exception& e) {
            response = {{"code", 400}, {"msg", "无效的角色 ID"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 23.8 编辑角色 API（需要管理员权限）
    svr.Put(R"(/api/admin/roles/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "user:manage")) {
            return;
        }

        json response;
        try {
            int role_id = stoi(req.matches[1]);
            auto req_json = json::parse(req.body);
            string name = req_json.value("name", "");
            string description = req_json.value("description", "");

            if (name.empty() || description.empty()) {
                response = {{"code", 400}, {"msg", "角色名称和描述不能为空"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            auto it = find_if(roles.begin(), roles.end(), [&](const Role& r) {
                return r.id == role_id;
            });

            if (it == roles.end()) {
                response = {{"code", 404}, {"msg", "角色不存在"}};
            } else {
                it->name = name;
                it->description = description;
                response = {{"code", 200}, {"msg", "角色编辑成功"}};
            }
        } catch (const exception& e) {
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 23.9 编辑权限 API（需要管理员权限）
    svr.Put(R"(/api/admin/permissions/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "user:manage")) {
            return;
        }

        json response;
        try {
            int permission_id = stoi(req.matches[1]);
            auto req_json = json::parse(req.body);
            string name = req_json.value("name", "");
            string code = req_json.value("code", "");
            string description = req_json.value("description", "");

            if (name.empty() || code.empty() || description.empty()) {
                response = {{"code", 400}, {"msg", "权限名称、代码和描述不能为空"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            auto it = find_if(permissions.begin(), permissions.end(), [&](const Permission& p) {
                return p.id == permission_id;
            });

            if (it == permissions.end()) {
                response = {{"code", 404}, {"msg", "权限不存在"}};
            } else {
                // 检查代码是否与其他权限冲突
                auto code_it = find_if(permissions.begin(), permissions.end(), [&](const Permission& p) {
                    return p.code == code && p.id != permission_id;
                });
                if (code_it != permissions.end()) {
                    response = {{"code", 400}, {"msg", "权限代码已存在"}};
                } else {
                    it->name = name;
                    it->code = code;
                    it->description = description;
                    response = {{"code", 200}, {"msg", "权限编辑成功"}};
                }
            }
        } catch (const exception& e) {
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 24. 系统配置 API（需要管理员权限）
    svr.Get("/api/admin/system/config", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "system:manage")) {
            return;
        }

        json config = {
            {"systemConfig", {
                {"systemName", "校园文明能量站"},
                {"version", "1.0.0"},
                {"description", "校园文明能量站管理系统"}
            }},
            {"securityConfig", {
                {"loginTimeout", 30},
                {"minPasswordLength", 6},
                {"enableCaptcha", false}
            }},
            {"backupConfig", {
                {"frequency", "daily"},
                {"retentionDays", 7}
            }}
        };

        json response = {{"code", 200}, {"data", config}};
        res.set_content(response.dump(), "application/json");
    });

    // 25. 数据统计 API（需要管理员权限）
    svr.Get("/api/admin/statistics", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "statistics:view")) {
            return;
        }

        int admin_count = 0;
        int teacher_count = 0;
        int student_count = 0;

        for (const auto& user : users) {
            switch (user.role_id) {
                case 1: admin_count++; break;
                case 2: teacher_count++; break;
                case 3: student_count++; break;
            }
        }

        json user_distribution = json::array();
        user_distribution.push_back({{"name", "管理员"}, {"value", admin_count}});
        user_distribution.push_back({{"name", "教师"}, {"value", teacher_count}});
        user_distribution.push_back({{"name", "学生"}, {"value", student_count}});

        json login_trend = json::array();
        time_t now = time(nullptr);
        tm* tm_info = localtime(&now);

        const char* weekdays[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};

        for (int i = 6; i >= 0; i--) {
            time_t target_time = now - i * 86400;
            tm* target_tm = localtime(&target_time);
            int weekday = target_tm->tm_wday;

            login_trend.push_back({
                {"day", weekdays[weekday]},
                {"count", 0}
            });
        }

        int range_0_50 = 0;
        int range_51_100 = 0;
        int range_101_150 = 0;
        int range_151_200 = 0;
        int range_200_plus = 0;

        for (const auto& user : users) {
            if (user.role_id == 3) {
                if (user.points <= 50) {
                    range_0_50++;
                } else if (user.points <= 100) {
                    range_51_100++;
                } else if (user.points <= 150) {
                    range_101_150++;
                } else if (user.points <= 200) {
                    range_151_200++;
                } else {
                    range_200_plus++;
                }
            }
        }

        json points_distribution = json::array();
        points_distribution.push_back({{"range", "0-50"}, {"count", range_0_50}});
        points_distribution.push_back({{"range", "51-100"}, {"count", range_51_100}});
        points_distribution.push_back({{"range", "101-150"}, {"count", range_101_150}});
        points_distribution.push_back({{"range", "151-200"}, {"count", range_151_200}});
        points_distribution.push_back({{"range", "200+"}, {"count", range_200_plus}});

        json statistics = {
            {"userDistribution", user_distribution},
            {"loginTrend", login_trend},
            {"pointsDistribution", points_distribution}
        };

        json response = {{"code", 200}, {"data", statistics}};
        res.set_content(response.dump(), "application/json");
    });

    // 26. 数据导出 API（需要管理员权限）
    svr.Get("/api/admin/export", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "system:manage")) {
            return;
        }

        json export_data;
        export_data["version"] = "1.0.0";
        export_data["export_time"] = get_current_time();

        json users_json = json::array();
        for (const auto& user : users) {
            users_json.push_back({
                {"id", user.id},
                {"username", user.username},
                {"password_hash", user.password_hash},
                {"role_id", user.role_id},
                {"name", user.name},
                {"className", user.className},
                {"points", user.points}
            });
        }
        export_data["users"] = users_json;

        json roles_json = json::array();
        for (const auto& role : roles) {
            roles_json.push_back({
                {"id", role.id},
                {"name", role.name},
                {"description", role.description}
            });
        }
        export_data["roles"] = roles_json;

        json permissions_json = json::array();
        for (const auto& perm : permissions) {
            permissions_json.push_back({
                {"id", perm.id},
                {"name", perm.name},
                {"code", perm.code},
                {"description", perm.description}
            });
        }
        export_data["permissions"] = permissions_json;

        json role_permissions_json = json::array();
        for (const auto& rp : role_permissions) {
            role_permissions_json.push_back({
                {"role_id", rp.role_id},
                {"permission_id", rp.permission_id}
            });
        }
        export_data["role_permissions"] = role_permissions_json;

        json points_records_json = json::array();
        for (const auto& record : points_records) {
            points_records_json.push_back({
                {"id", record.id},
                {"student_id", record.student_id},
                {"points", record.points},
                {"reason", record.reason},
                {"operator_id", record.operator_id},
                {"created_at", record.created_at}
            });
        }
        export_data["points_records"] = points_records_json;

        json evaluations_json = db.query("SELECT id, student_id, dimension_id, score, comment, evaluator_id, created_at, updated_at FROM evaluations");
        export_data["evaluations"] = evaluations_json;

        json mall_items_json = db.query("SELECT id, name, description, cost, stock, image_url, status, created_at FROM mall_items");
        export_data["mall_items"] = mall_items_json;

        json redemption_records_json = db.query("SELECT id, student_id, item_id, cost, created_at FROM redemption_records");
        export_data["redemption_records"] = redemption_records_json;

        json response = {
            {"code", 200},
            {"msg", "导出成功"},
            {"data", export_data}
        };

        res.set_content(response.dump(), "application/json");
    });

    // 27. 数据导入 API（需要管理员权限）
    svr.Post("/api/admin/import", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "system:manage")) {
            return;
        }

        json response;

        try {
            auto req_json = json::parse(req.body);
            json import_data = req_json.value("data", json::object());
            string mode = req_json.value("mode", "skip"); // "overwrite" 或 "skip"

            if (import_data.empty()) {
                response = {{"code", 400}, {"msg", "导入数据为空"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            string version = import_data.value("version", "");
            if (version.empty()) {
                response = {{"code", 400}, {"msg", "无效的数据格式：缺少版本号"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            int imported_count = 0;
            int skipped_count = 0;
            int error_count = 0;

            db.execute("BEGIN TRANSACTION");

            if (import_data.contains("users") && import_data["users"].is_array()) {
                for (const auto& user_json : import_data["users"]) {
                    string id = user_json.value("id", "");
                    string username = user_json.value("username", "");

                    auto existing = find_if(users.begin(), users.end(), [&](const User& u) {
                        return u.id == id || u.username == username;
                    });

                    if (existing != users.end()) {
                        if (mode == "overwrite") {
                            existing->username = username;
                            existing->password_hash = user_json.value("password_hash", "");
                            existing->role_id = user_json.value("role_id", 3);
                            existing->name = user_json.value("name", "");
                            existing->className = user_json.value("className", "");
                            existing->points = user_json.value("points", 0);
                            save_user_to_db(*existing);
                            imported_count++;
                        } else {
                            skipped_count++;
                        }
                    } else {
                        User new_user;
                        new_user.id = id;
                        new_user.username = username;
                        new_user.password_hash = user_json.value("password_hash", "");
                        new_user.role_id = user_json.value("role_id", 3);
                        new_user.name = user_json.value("name", "");
                        new_user.className = user_json.value("className", "");
                        new_user.points = user_json.value("points", 0);
                        new_user.student_id = username;
                        users.push_back(new_user);
                        save_user_to_db(new_user);
                        imported_count++;
                    }
                }
            }

            if (import_data.contains("roles") && import_data["roles"].is_array()) {
                for (const auto& role_json : import_data["roles"]) {
                    int id = role_json.value("id", 0);
                    string name = role_json.value("name", "");

                    auto existing = find_if(roles.begin(), roles.end(), [&](const Role& r) {
                        return r.id == id || r.name == name;
                    });

                    if (existing != roles.end()) {
                        if (mode == "overwrite") {
                            existing->name = name;
                            existing->description = role_json.value("description", "");
                            imported_count++;
                        } else {
                            skipped_count++;
                        }
                    } else {
                        Role new_role;
                        new_role.id = id;
                        new_role.name = name;
                        new_role.description = role_json.value("description", "");
                        roles.push_back(new_role);
                        imported_count++;
                    }
                }
            }

            if (import_data.contains("permissions") && import_data["permissions"].is_array()) {
                for (const auto& perm_json : import_data["permissions"]) {
                    int id = perm_json.value("id", 0);
                    string code = perm_json.value("code", "");

                    auto existing = find_if(permissions.begin(), permissions.end(), [&](const Permission& p) {
                        return p.id == id || p.code == code;
                    });

                    if (existing != permissions.end()) {
                        if (mode == "overwrite") {
                            existing->name = perm_json.value("name", "");
                            existing->code = code;
                            existing->description = perm_json.value("description", "");
                            imported_count++;
                        } else {
                            skipped_count++;
                        }
                    } else {
                        Permission new_perm;
                        new_perm.id = id;
                        new_perm.name = perm_json.value("name", "");
                        new_perm.code = code;
                        new_perm.description = perm_json.value("description", "");
                        permissions.push_back(new_perm);
                        imported_count++;
                    }
                }
            }

            if (import_data.contains("role_permissions") && import_data["role_permissions"].is_array()) {
                for (const auto& rp_json : import_data["role_permissions"]) {
                    int role_id = rp_json.value("role_id", 0);
                    int permission_id = rp_json.value("permission_id", 0);

                    auto existing = find_if(role_permissions.begin(), role_permissions.end(), [&](const RolePermission& rp) {
                        return rp.role_id == role_id && rp.permission_id == permission_id;
                    });

                    if (existing == role_permissions.end()) {
                        role_permissions.push_back({role_id, permission_id});
                        imported_count++;
                    } else {
                        skipped_count++;
                    }
                }
            }

            if (import_data.contains("points_records") && import_data["points_records"].is_array()) {
                for (const auto& record_json : import_data["points_records"]) {
                    int id = record_json.value("id", 0);

                    auto existing = find_if(points_records.begin(), points_records.end(), [&](const PointsRecord& r) {
                        return r.id == id;
                    });

                    if (existing != points_records.end()) {
                        skipped_count++;
                    } else {
                        PointsRecord new_record;
                        new_record.id = id;
                        new_record.student_id = record_json.value("student_id", "");
                        new_record.points = record_json.value("points", 0);
                        new_record.reason = record_json.value("reason", "");
                        new_record.operator_id = record_json.value("operator_id", "");
                        new_record.created_at = record_json.value("created_at", "");
                        points_records.push_back(new_record);
                        imported_count++;
                    }
                }
            }

            if (import_data.contains("evaluations") && import_data["evaluations"].is_array()) {
                for (const auto& eval_json : import_data["evaluations"]) {
                    char sql[2048];
                    snprintf(sql, sizeof(sql),
                        "INSERT OR IGNORE INTO evaluations (id, student_id, dimension_id, score, comment, evaluator_id, created_at, updated_at) VALUES (%d, '%s', %d, %d, '%s', '%s', '%s', '%s')",
                        eval_json.value("id", 0),
                        db.escapeString(eval_json.value("student_id", "")).c_str(),
                        eval_json.value("dimension_id", 0),
                        eval_json.value("score", 0),
                        db.escapeString(eval_json.value("comment", "")).c_str(),
                        db.escapeString(eval_json.value("evaluator_id", "")).c_str(),
                        eval_json.value("created_at", "").c_str(),
                        eval_json.value("updated_at", "").c_str()
                    );
                    if (db.execute(sql)) {
                        imported_count++;
                    } else {
                        skipped_count++;
                    }
                }
            }

            if (import_data.contains("mall_items") && import_data["mall_items"].is_array()) {
                for (const auto& item_json : import_data["mall_items"]) {
                    char sql[1024];
                    snprintf(sql, sizeof(sql),
                        "INSERT OR IGNORE INTO mall_items (id, name, description, cost, stock, image_url, status, created_at) VALUES (%d, '%s', '%s', %d, %d, '%s', %d, '%s')",
                        item_json.value("id", 0),
                        db.escapeString(item_json.value("name", "")).c_str(),
                        db.escapeString(item_json.value("description", "")).c_str(),
                        item_json.value("cost", 0),
                        item_json.value("stock", -1),
                        item_json.value("image_url", "").c_str(),
                        item_json.value("status", 1),
                        item_json.value("created_at", "").c_str()
                    );
                    if (db.execute(sql)) {
                        imported_count++;
                    } else {
                        skipped_count++;
                    }
                }
            }

            if (import_data.contains("redemption_records") && import_data["redemption_records"].is_array()) {
                for (const auto& red_json : import_data["redemption_records"]) {
                    char sql[1024];
                    snprintf(sql, sizeof(sql),
                        "INSERT OR IGNORE INTO redemption_records (id, student_id, item_id, cost, created_at) VALUES (%d, '%s', %d, %d, '%s')",
                        red_json.value("id", 0),
                        db.escapeString(red_json.value("student_id", "")).c_str(),
                        red_json.value("item_id", 0),
                        red_json.value("cost", 0),
                        red_json.value("created_at", "").c_str()
                    );
                    if (db.execute(sql)) {
                        imported_count++;
                    } else {
                        skipped_count++;
                    }
                }
            }

            db.execute("COMMIT");

            init_indexes();

            response = {
                {"code", 200},
                {"msg", "导入完成"},
                {"data", {
                    {"imported", imported_count},
                    {"skipped", skipped_count},
                    {"errors", error_count}
                }}
            };

        } catch (json::parse_error& e) {
            response = {{"code", 400}, {"msg", "数据格式错误: " + string(e.what())}};
        } catch (const exception& e) {
            response = {{"code", 500}, {"msg", "导入失败: " + string(e.what())}};
            db.execute("ROLLBACK");
        }

        res.set_content(response.dump(), "application/json");
    });

    // ====== 班级管理 API ======

    // 获取班级列表
    svr.Get("/api/admin/classes", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "user:manage")) {
            return;
        }

        json classes_arr = json::array();

        json result = db.query("SELECT id, name, grade, grade_code, class_code, head_teacher, description FROM classes ORDER BY id");
        for (const auto& row : result) {
            // 统计每班学生数
            string class_name = row.value("name", "");
            char count_sql[256];
            snprintf(count_sql, sizeof(count_sql), "SELECT COUNT(*) as cnt FROM users WHERE className='%s' AND role_id=3", class_name.c_str());
            json count_result = db.query(count_sql);
            int student_count = count_result.empty() ? 0 : count_result[0].value("cnt", 0);

            string grade = row.value("grade", "");
            string grade_code = row.value("grade_code", "");
            string class_code = row.value("class_code", "");
            // 兜底：旧数据可能缺编码，按规则补全
            if (grade_code.empty() && !grade.empty()) {
                grade_code = grade_to_code(grade);
            }
            if (class_code.empty()) {
                class_code = extract_class_code_from_name(class_name);
            }

            json item;
            item["id"] = row.value("id", 0);
            item["name"] = class_name;
            item["grade"] = grade;
            item["grade_code"] = grade_code;
            item["class_code"] = class_code;
            item["head_teacher"] = row.value("head_teacher", "");
            item["description"] = row.value("description", "");
            item["student_count"] = student_count;
            classes_arr.push_back(item);
        }

        json response = {{"code", 200}, {"data", classes_arr}};
        res.set_content(response.dump(), "application/json");
    });

    // 新增班级
    svr.Post("/api/admin/classes", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "user:manage")) {
            return;
        }

        json response;
        try {
            auto req_json = json::parse(req.body);
            string name = req_json.value("name", "");
            string grade = req_json.value("grade", "");
            string head_teacher = req_json.value("head_teacher", "");
            string description = req_json.value("description", "");
            string grade_code = req_json.value("grade_code", "");
            string class_code = req_json.value("class_code", "");

            if (name.empty()) {
                response = {{"code", 400}, {"msg", "班级名称不能为空"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 兜底：前端漏传时按规则自动计算编码
            if (grade_code.empty() && !grade.empty()) {
                grade_code = grade_to_code(grade);
            }
            if (class_code.empty()) {
                class_code = extract_class_code_from_name(name);
            }

            char sql[512];
            snprintf(sql, sizeof(sql), "INSERT INTO classes (name, grade, grade_code, class_code, head_teacher, description) VALUES ('%s', '%s', '%s', '%s', '%s', '%s')",
                     name.c_str(), grade.c_str(), grade_code.c_str(), class_code.c_str(),
                     head_teacher.c_str(), description.c_str());
            if (db.execute(sql)) {
                response = {{"code", 200}, {"msg", "班级添加成功"}};
            } else {
                response = {{"code", 400}, {"msg", "班级名称已存在或添加失败"}};
            }
        } catch (json::parse_error& e) {
            response = {{"code", 400}, {"msg", "请求数据格式错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 修改班级
    svr.Put(R"(/api/admin/classes/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "user:manage")) {
            return;
        }

        json response;
        try {
            int class_id = stoi(req.matches[1]);
            auto req_json = json::parse(req.body);
            string name = req_json.value("name", "");
            string grade = req_json.value("grade", "");
            string head_teacher = req_json.value("head_teacher", "");
            string description = req_json.value("description", "");
            string grade_code = req_json.value("grade_code", "");
            string class_code = req_json.value("class_code", "");

            // 兜底：前端漏传时按规则自动计算编码
            if (grade_code.empty() && !grade.empty()) {
                grade_code = grade_to_code(grade);
            }
            if (class_code.empty()) {
                class_code = extract_class_code_from_name(name);
            }

            char sql[512];
            snprintf(sql, sizeof(sql), "UPDATE classes SET name='%s', grade='%s', grade_code='%s', class_code='%s', head_teacher='%s', description='%s' WHERE id=%d",
                     name.c_str(), grade.c_str(), grade_code.c_str(), class_code.c_str(),
                     head_teacher.c_str(), description.c_str(), class_id);
            if (db.execute(sql)) {
                response = {{"code", 200}, {"msg", "班级修改成功"}};
            } else {
                response = {{"code", 400}, {"msg", "班级修改失败"}};
            }
        } catch (json::parse_error& e) {
            response = {{"code", 400}, {"msg", "请求数据格式错误"}};
        } catch (exception& e) {
            response = {{"code", 400}, {"msg", "参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 删除班级
    svr.Delete(R"(/api/admin/classes/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        json response;

        if (!check_permission_middleware(req, res, "user:manage")) {
            return;
        }

        try {
            if (req.matches.size() < 2) {
                response = {{"code", 400}, {"msg", "缺少班级ID"}};
                res.set_content(response.dump(), "application/json");
                return;
            }
            int class_id = stoi(req.matches[1]);

            char sql[512];
            string class_name;

            snprintf(sql, sizeof(sql), "SELECT name FROM classes WHERE id=%d", class_id);
            json class_result = db.query(sql);
            if (!class_result.empty()) {
                class_name = class_result[0].value("name", "");
            }

            snprintf(sql, sizeof(sql), "DELETE FROM classes WHERE id=%d", class_id);
            if (db.execute(sql)) {
                if (!class_name.empty()) {
                    snprintf(sql, sizeof(sql), "UPDATE users SET className='' WHERE className='%s'", class_name.c_str());
                    db.execute(sql);
                }
                response = {{"code", 200}, {"msg", "班级删除成功"}};
            } else {
                response = {{"code", 500}, {"msg", "数据库删除失败"}};
            }
        } catch (exception& e) {
            response = {{"code", 400}, {"msg", string("参数错误: ") + e.what()}};
        }

        res.set_content(response.dump(), "application/json");
    });
}
