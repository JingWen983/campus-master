#include "routes.h"
#include "auth.h"
#include "models.h"
#include "logger.h"
#include <unordered_map>

void register_student_routes(httplib::Server& svr) {
    // ================= 学生端 API =================

    // 17. 获取学生个人信息 API（需要学生权限）
    svr.Get("/api/student/info", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "mall:manage")) {
            return;
        }

        std::string session_id = get_cookie_value(req, "sid");
        string user_id = verify_session(session_id);
        if (user_id.empty()) {
            json response = {{"code", 401}, {"msg", "会话无效或已过期"}};
            set_cors_headers(res);
            res.set_content(response.dump(), "application/json");
            return;
        }
        User* user = find_user_by_id(user_id);

        if (!user) {
            json response = {{"code", 404}, {"msg", "用户不存在"}};
            res.set_content(response.dump(), "application/json");
            return;
        }

        // 计算积分排名（按积分降序，同分同名次）
        int rank = 1;
        for (const auto& u : users) {
            if (u.role_id == 3 && u.points > user->points) {
                rank++;
            }
        }

        json student_info;
        student_info["id"] = user->id;
        student_info["name"] = user->name;
        student_info["username"] = user->username;
        student_info["className"] = user->className;
        student_info["points"] = user->points;
        student_info["rank"] = rank;

        json response;
        response["code"] = 200;
        response["data"] = student_info;
        res.set_content(response.dump(), "application/json");
    });

    // 18. 获取学生积分记录 API（需要学生权限）
    svr.Get("/api/student/points/records", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 学生查看自己的积分记录，仅需登录验证，无需特殊权限
        std::string session_id = get_cookie_value(req, "sid");
        string user_id = verify_session(session_id);
        if (user_id.empty()) {
            json response = {{"code", 401}, {"msg", "会话无效或已过期"}};
            set_cors_headers(res);
            res.set_content(response.dump(), "application/json");
            return;
        }

        json records = json::array();

        json result = db.query_bind(
            "SELECT id, points, reason, created_at FROM points_records WHERE student_id = ? ORDER BY created_at DESC",
            {SqliteDb::Bind(user_id)});
        for (const auto& row : result) {
            json record;
            record["id"] = row.value("id", 0);
            record["points"] = row.value("points", 0);
            record["type"] = row.value("points", 0) >= 0 ? "奖励" : "扣除";
            record["reason"] = row.value("reason", "");
            record["time"] = row.value("created_at", "");
            records.push_back(record);
        }

        json response;
        response["code"] = 200;
        response["data"] = records;
        res.set_content(response.dump(), "application/json");
    });

    // 19. 获取学生评价信息 API（需要学生权限）
    svr.Get("/api/student/evaluation", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "mall:manage")) {
            return;
        }

        std::string session_id = get_cookie_value(req, "sid");
        string user_id = verify_session(session_id);
        if (user_id.empty()) {
            json response = {{"code", 401}, {"msg", "会话无效或已过期"}};
            set_cors_headers(res);
            res.set_content(response.dump(), "application/json");
            return;
        }

        json evaluations_json = json::array();

        // 从数据库获取评价数据
        json result = db.query_bind(
            "SELECT e.id, e.student_id, e.dimension_id, e.score, e.comment, e.evaluator_id, e.created_at, "
            "u.name as evaluator_name "
            "FROM evaluations e "
            "LEFT JOIN users u ON e.evaluator_id = u.id "
            "WHERE e.student_id = ? "
            "ORDER BY e.created_at DESC",
            {SqliteDb::Bind(user_id)});

        // 评价维度名称映射
        unordered_map<int, string> dimension_names = {
            {1, "德育"},
            {2, "智育"},
            {3, "体育"},
            {4, "美育"},
            {5, "劳育"}
        };

        for (const auto& row : result) {
            json eval;
            eval["id"] = row.value("id", 0);
            eval["student_id"] = row.value("student_id", "");
            eval["dimension_id"] = row.value("dimension_id", 0);
            eval["dimension_name"] = dimension_names[row.value("dimension_id", 0)];
            eval["score"] = row.value("score", 0);
            eval["comment"] = row.value("comment", "");
            eval["evaluator_name"] = row.value("evaluator_name", "");
            eval["time"] = row.value("created_at", "");
            evaluations_json.push_back(eval);
        }

        json response;
        response["code"] = 200;
        response["data"] = evaluations_json;
        res.set_content(response.dump(), "application/json");
    });

    // 20. 获取商城商品 API（需要学生权限）
    svr.Get("/api/student/mall", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "mall:manage")) {
            return;
        }

        json items_json = json::array();

        try {
            Logger::info("学生端开始查询商城商品列表");
            json result = db.query("SELECT id, name, description, cost, stock, image_url, status, created_at FROM mall_items WHERE status = 1 ORDER BY id");
            Logger::info("学生端查询完成，结果数量: " + std::to_string(result.size()));
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
                item["price"] = row.value("cost", 0);  // 学生端使用 price 字段名
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
                items_json.push_back(item);
            }
            json response = {{"code", 200}, {"data", items_json}};
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            Logger::error("学生端商城 API 错误: " + std::string(e.what()));
            json response = {{"code", 500}, {"msg", "服务器内部错误"}};
            res.status = 500;
            res.set_content(response.dump(), "application/json");
        }
    });

    // 21. 学生查看兑换记录 API（需要学生权限）
    svr.Get("/api/student/redemptions", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "mall:manage")) {
            return;
        }

        std::string session_id = get_cookie_value(req, "sid");
        string user_id = verify_session(session_id);
        if (user_id.empty()) {
            json response = {{"code", 401}, {"msg", "会话无效或已过期"}};
            set_cors_headers(res);
            res.set_content(response.dump(), "application/json");
            return;
        }

        json records_json = json::array();

        json result = db.query_bind(
            "SELECT r.id, r.student_id, r.item_id, r.cost, r.created_at, "
            "m.name as item_name, m.description as item_description "
            "FROM redemption_records r "
            "LEFT JOIN mall_items m ON r.item_id = m.id "
            "WHERE r.student_id = ? "
            "ORDER BY r.created_at DESC",
            {SqliteDb::Bind(user_id)});
        for (const auto& row : result) {
            json record;
            record["id"] = row.value("id", 0);
            record["item_id"] = row.value("item_id", 0);
            record["item_name"] = row.value("item_name", "");
            record["item_description"] = row.value("item_description", "");
            record["cost"] = row.value("cost", 0);
            record["time"] = row.value("created_at", "");
            records_json.push_back(record);
        }

        json response = {{"code", 200}, {"data", records_json}};
        res.set_content(response.dump(), "application/json");
    });
}
