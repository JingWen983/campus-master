#include "routes.h"
#include "auth.h"
#include "models.h"
#include "sha256.h"
#include "logger.h"
#include "config.h"
#include <unordered_map>
#include <algorithm>
#include <vector>

void register_public_routes(httplib::Server& svr) {
    // 1. 登录 API
    svr.Post("/api/auth/login", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        json response;

        try {
            auto req_json = json::parse(req.body);
            string username = req_json.value("username", "");
            string password = req_json.value("password", "");

            if (username.empty() || password.empty()) {
                response = {{"code", 400}, {"msg", "用户名和密码不能为空"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            User* user = find_user_by_username(username);
            if (!user) {
                response = {{"code", 401}, {"msg", "用户名或密码错误"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            bool password_match = verify_password(password, user->password_hash);
            if (!password_match) {
                response = {{"code", 401}, {"msg", "用户名或密码错误"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 生成会话并存入数据库（家长角色需标记 is_parent=true 以通过家长端中间件校验）
            cleanup_expired_sessions();
            std::string session_id = generate_session_id();
            bool is_parent = (user->role_id == 4);
            create_session(session_id, user->id, user->role_id, g_config.session_expiry_hours, is_parent, is_parent ? user->id : "");

            // 设置 HttpOnly cookie
            int max_age = g_config.session_expiry_hours * 3600;
            set_session_cookie(res, session_id, max_age);

            response = {
                {"code", 200},
                {"msg", "登录成功"},
                {"data", {
                    {"user", {
                        {"id", user->id},
                        {"username", user->username},
                        {"name", user->name},
                        {"role_id", user->role_id},
                        {"className", user->className}
                    }}
                }}
            };

        } catch (json::parse_error& e) {
            response = {{"code", 400}, {"msg", "请求数据格式错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 退出登录 API
    svr.Post("/api/auth/logout", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        std::string session_id = get_cookie_value(req, "sid");
        if (!session_id.empty()) {
            delete_session(session_id);
        }
        clear_session_cookie(res);
        json response = {{"code", 200}, {"msg", "退出成功"}};
        res.set_content(response.dump(), "application/json");
    });

    // 2. 注册 API
    svr.Post("/api/auth/register", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        json response;

        try {
            auto req_json = json::parse(req.body);
            string username = req_json.value("username", "");
            string password = req_json.value("password", "");
            string name = req_json.value("name", "");
            string className = req_json.value("className", "");
            int role_id = req_json.value("role_id", 3);

            if (username.empty() || password.empty() || name.empty()) {
                response = {{"code", 400}, {"msg", "用户名、密码和姓名不能为空"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            if (find_user_by_username(username) != nullptr) {
                response = {{"code", 400}, {"msg", "用户名已存在"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 使用 generate_user_id 生成符合规范的字符串 ID
            string grade_code = req_json.value("grade_code", "");
            string class_code = req_json.value("class_code", "");
            string new_id = generate_user_id(role_id, grade_code, class_code);

            User new_user = {
                new_id,
                username,
                hash_password(password),
                role_id,
                name,
                className,
                0,
                (role_id == 3) ? username : ""
            };

            users.push_back(new_user);
            update_user_index(new_user);

            save_user_to_db(new_user);

            response = {{"code", 200}, {"msg", "注册成功"}, {"data", {"user_id", new_id}}};

        } catch (json::parse_error& e) {
            response = {{"code", 400}, {"msg", "请求数据格式错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 3. 获取当前用户信息 API
    svr.Get("/api/auth/me", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        json response;

        std::string session_id = get_cookie_value(req, "sid");
        std::string user_id = verify_session(session_id);
        if (user_id.empty()) {
            response = {{"code", 401}, {"msg", "会话无效或已过期"}};
            res.set_content(response.dump(), "application/json");
            return;
        }
        User* user = find_user_by_id(user_id);

        if (!user) {
            response = {{"code", 404}, {"msg", "用户不存在"}};
            res.set_content(response.dump(), "application/json");
            return;
        }

        // 检查是否为家长会话
        int sess_role_id;
        std::string sess_student_id;
        bool sess_is_parent;
        bool sess_ok = get_session_info(session_id, user_id, sess_role_id, sess_is_parent, sess_student_id);
        int effective_role_id = (sess_ok && sess_is_parent) ? 4 : user->role_id;
        std::string effective_name = (sess_ok && sess_is_parent) ? (user->name + "家长") : user->name;

        response = {
            {"code", 200},
            {"msg", "success"},
            {"data", {
                {"id", user->id},
                {"username", user->username},
                {"name", effective_name},
                {"role_id", effective_role_id},
                {"className", user->className},
                {"points", user->points}
            }}
        };

        res.set_content(response.dump(), "application/json");
    });

    // 4. 获取个人信息 API
    svr.Get("/api/user/info", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        json response;

        std::string session_id = get_cookie_value(req, "sid");
        std::string user_id = verify_session(session_id);
        if (user_id.empty()) {
            response = {{"code", 401}, {"msg", "会话无效或已过期"}};
            res.set_content(response.dump(), "application/json");
            return;
        }
        User* user = find_user_by_id(user_id);

        if (!user) {
            response = {{"code", 404}, {"msg", "用户不存在"}};
            res.set_content(response.dump(), "application/json");
            return;
        }

        response = {
            {"code", 200},
            {"msg", "success"},
            {"data", {
                {"name", user->name},
                {"className", user->className},
                {"points", user->points}
            }}
        };
        res.set_content(response.dump(), "application/json");
    });

    // 5. 获取行为记录 API
    svr.Get("/api/behavior/history", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        json response;

        std::string session_id = get_cookie_value(req, "sid");
        std::string user_id = verify_session(session_id);
        if (user_id.empty()) {
            response = {{"code", 401}, {"msg", "会话无效或已过期"}};
            res.set_content(response.dump(), "application/json");
            return;
        }

        json records = json::array();

        char sql[512];
        snprintf(sql, sizeof(sql),
            "SELECT id, points, reason, created_at FROM points_records WHERE student_id = '%s' ORDER BY created_at DESC",
            db.escapeString(user_id).c_str());
        json result = db.query(sql);
        for (const auto& row : result) {
            json record;
            record["id"] = row.value("id", 0);
            record["score"] = row.value("points", 0);
            record["reason"] = row.value("reason", "");
            record["time"] = row.value("created_at", "");
            records.push_back(record);
        }

        response = {{"code", 200}, {"data", records}};
        res.set_content(response.dump(), "application/json");
    });

    // 6. 获取兑换商城列表 API
    svr.Get("/api/mall/items", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        json items = json::array();

        json result = db.query("SELECT id, name, description, cost, stock, image_url, status FROM mall_items WHERE status = 1 ORDER BY id");
        for (const auto& row : result) {
            json item;
            item["id"] = row.value("id", 0);
            item["name"] = row.value("name", "");
            item["description"] = row.value("description", "");
            item["cost"] = row.value("cost", 0);
            item["stock"] = row.value("stock", 0);
            item["image_url"] = row.value("image_url", "");
            items.push_back(item);
        }

        json response = {{"code", 200}, {"data", items}};
        res.set_content(response.dump(), "application/json");
    });

    // 7. 兑换商品 API
    svr.Post("/api/mall/redeem", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        json response;

        std::string session_id = get_cookie_value(req, "sid");
        std::string user_id = verify_session(session_id);
        if (user_id.empty()) {
            response = {{"code", 401}, {"msg", "会话无效或已过期"}};
            res.set_content(response.dump(), "application/json");
            return;
        }
        User* user = find_user_by_id(user_id);
        if (!user) {
            response = {{"code", 404}, {"msg", "用户不存在"}};
            res.set_content(response.dump(), "application/json");
            return;
        }

        try {
            auto req_json = json::parse(req.body);
            int item_id = req_json.value("item_id", 0);

            if (item_id <= 0) {
                response = {{"code", 400}, {"msg", "商品参数无效"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 后端验证：从数据库读取商品真实价格和库存，忽略客户端传入的 cost
            json item_result = db.query("SELECT cost, stock FROM mall_items WHERE id = " + to_string(item_id) + " AND status = 1");
            if (item_result.empty()) {
                response = {{"code", 404}, {"msg", "商品不存在或已下架"}};
                res.set_content(response.dump(), "application/json");
                return;
            }
            int cost = item_result[0].value("cost", 0);
            int stock = item_result[0].value("stock", -1);

            if (cost <= 0) {
                response = {{"code", 400}, {"msg", "商品价格异常"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            if (user->points < cost) {
                response = {{"code", 400}, {"msg", "积分不足！"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            bool stock_ok = true;
            if (stock == 0) {
                stock_ok = false;
            } else if (stock > 0) {
                char stock_sql[256];
                snprintf(stock_sql, sizeof(stock_sql), "UPDATE mall_items SET stock = stock - 1 WHERE id = %d AND stock > 0", item_id);
                db.execute(stock_sql);
            }

            if (!stock_ok) {
                response = {{"code", 400}, {"msg", "商品库存不足"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            user->points -= cost;
            update_user_points_in_db(user->id, user->points);
            char redeem_sql[512];
            snprintf(redeem_sql, sizeof(redeem_sql),
                "INSERT INTO redemption_records (student_id, item_id, cost, created_at) VALUES ('%s', %d, %d, '%s')",
                db.escapeString(user_id).c_str(), item_id, cost, get_current_time().c_str());
            db.execute(redeem_sql);

            response = {
                {"code", 200},
                {"msg", "兑换成功！"},
                {"data", {{"remain_points", user->points}}}
            };
        } catch (json::parse_error& e) {
            response = {{"code", 400}, {"msg", "请求数据格式错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 8. 获取风采榜 API
    svr.Get("/api/rank/class", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        json ranks = json::array();

        json result = db.query(
            "SELECT className, SUM(points) as totalPoints FROM users "
            "WHERE role_id = 3 AND className IS NOT NULL AND className != '' "
            "GROUP BY className ORDER BY totalPoints DESC");
        int rank = 1;
        for (const auto& row : result) {
            json item;
            item["id"] = rank++;
            item["name"] = row.value("className", "");
            item["totalPoints"] = row.value("totalPoints", 0);
            ranks.push_back(item);
        }

        json response = {{"code", 200}, {"data", ranks}};
        res.set_content(response.dump(), "application/json");
    });
}
