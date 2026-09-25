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
        set_cors_headers(req, res);
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

            // 安全修复 V8 + 批次 1 / 1-6（B4）：登录失败锁定（IP 与账号**双键**）。
            // 双键的理由：只用账号维度会被「分布式撞库打同一个账号」绕开；
            // 只用 IP 维度会被「同一出口打多个账号」绕开，且 NAT 下容易误锁整片。
            // 客户端标识由 login_client_ip() 提供 —— 默认不信任可伪造的 X-Forwarded-For。
            const std::string ip_key = login_ip_key(req);
            const std::string acct_key = login_account_key(username);
            if (!login_can_try(ip_key) || !login_can_try(acct_key)) {
                response = {{"code", 429}, {"msg", "登录尝试过于频繁，请稍后再试"}};
                res.status = 429;
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 批次 1 / B5：改为取拷贝（find_user_by_* 不再返回可在并发下悬垂的裸指针）
            User user;
            if (!find_user_by_username_copy(username, user)) {
                login_record_fail(ip_key);
                login_record_fail(acct_key);
                response = {{"code", 401}, {"msg", "用户名或密码错误"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            bool password_match = verify_password(password, user.password_hash);
            // 安全修复 V18（B2）：删除旧版「登录成功即就地升级为 PBKDF2」的自动升级链。
            // 依据 BATCH0_DESIGN_DECISIONS.md 决策①：在 B1 存在时这条链会把任何一次合法登录
            // 变成对 admin 的永久接管放大器；且存量 pbkdf2$ 记录「好坏同构」，无法在信息论上区分。
            // 故统一改为「存量一律失效 + 运维强制重置」流程（启动时会统计并告警仍在使用
            // 旧版无盐 SHA256 的账号，见 main.cpp warn_legacy_weak_hashes()）。
            // 注意：verify_password 的旧版裸 SHA256 兼容分支仍然保留，旧账号依然能登录，
            // 只是不再被静默改写其存储串。
            if (!password_match) {
                login_record_fail(ip_key);
                login_record_fail(acct_key);
                response = {{"code", 401}, {"msg", "用户名或密码错误"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            login_record_success(ip_key);
            login_record_success(acct_key);

            // 生成会话并存入数据库（家长角色需标记 is_parent=true 以通过家长端中间件校验）
            cleanup_expired_sessions();
            std::string session_id = generate_session_id();
            bool is_parent = (user.role_id == 4);
            create_session(session_id, user.id, user.role_id, g_config.session_expiry_hours, is_parent, is_parent ? user.id : "");

            // 设置 HttpOnly cookie
            int max_age = g_config.session_expiry_hours * 3600;
            set_session_cookie(res, session_id, max_age);
            // 安全修复 V10：登录成功后下发 CSRF Token
            std::string csrf = issue_csrf_token(res);

            response = {
                {"code", 200},
                {"msg", "登录成功"},
                {"data", {
                    {"user", {
                        {"id", user.id},
                        {"username", user.username},
                        {"name", user.name},
                        {"role_id", user.role_id},
                        {"className", user.className},
                        // 安全修复 V18（B2）：告知客户端必须先改初始口令
                        {"must_change_password", user.must_change_password}
                    }},
                    {"csrf_token", csrf},
                    {"must_change_password", user.must_change_password}
                }}
            };

        } catch (json::parse_error& e) {
            response = {{"code", 400}, {"msg", "请求数据格式错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 退出登录 API
    svr.Post("/api/auth/logout", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(req, res);
        // 安全修复 V10：登出要求 CSRF Token，防止跨站强制登出
        if (!require_csrf(req, res)) return;
        std::string session_id = get_cookie_value(req, "sid");
        if (!session_id.empty()) {
            delete_session(session_id);
        }
        clear_session_cookie(res);
        json response = {{"code", 200}, {"msg", "退出成功"}};
        res.set_content(response.dump(), "application/json");
    });

    // 2. 注册 API（公开接口，不需要 CSRF）
    svr.Post("/api/auth/register", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(req, res);
        json response;

        try {
            auto req_json = json::parse(req.body);
            string username = req_json.value("username", "");
            string password = req_json.value("password", "");
            string name = req_json.value("name", "");
            string className = req_json.value("className", "");

            // 安全修复 V1：开放注册仅允许学生角色，杜绝 role_id 注入提权
            // 忽略客户端传入的 role_id，强制为 3（学生）
            const int role_id = 3;

            if (username.empty() || password.empty() || name.empty()) {
                response = {{"code", 400}, {"msg", "用户名、密码和姓名不能为空"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 安全修复 V8：密码最小长度校验
            if (password.length() < 6) {
                response = {{"code", 400}, {"msg", "密码长度不能少于6位"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 安全修复 V1：学生必须设置班级
            if (className.empty()) {
                response = {{"code", 400}, {"msg", "学生必须设置班级"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            User existing_user;
            if (find_user_by_username_copy(username, existing_user)) {
                response = {{"code", 400}, {"msg", "用户名已存在"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 使用 generate_user_id 生成符合规范的字符串 ID
            string grade_code = req_json.value("grade_code", "");
            string class_code = req_json.value("class_code", "");

            // 批次 1 / N2：改为「原子建号并落库」—— 唯一性校验、生成 id、写 DB、写内存、
            // 重建索引全在一次持锁内完成。修复前是 push_back + update_user_index 后再单独
            // save_user_to_db（且当时用 INSERT OR REPLACE），并发注册会互相覆盖：
            // 实测 8 并发注册返回 8 个 200 而库内只剩 1 个用户。
            std::string new_id;
            bool added = add_user_record_persisted([&](User& nu) {
                nu.id = generate_user_id(role_id, grade_code, class_code);
                nu.username = username;
                nu.password_hash = hash_password(password);
                nu.role_id = role_id;
                nu.name = name;
                nu.className = className;
                nu.points = 0;
                nu.student_id = username;
                // 安全修复 V18（B2）：自助注册由**用户自己**设定口令，不需要首登改密。
                // （struct User 的 must_change_password 默认值为 true，属 fail-safe 设计，故这里必须显式置 false）
                nu.must_change_password = false;
            }, new_id, username);

            if (!added) {
                response = {{"code", 400}, {"msg", "用户名已存在或注册冲突，请更换用户名"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 注意：这里必须是对象形式 {"user_id", new_id}（花括号成对），
            // 原写法 {"data", {"user_id", new_id}} 会把 data 序列化成数组 ["user_id", new_id]。
            response = {{"code", 200}, {"msg", "注册成功"}, {"data", {{"user_id", new_id}}}};

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
        User user;
        if (!find_user_by_id_copy(user_id, user)) {
            response = {{"code", 404}, {"msg", "用户不存在"}};
            res.set_content(response.dump(), "application/json");
            return;
        }

        // 检查是否为家长会话
        int sess_role_id;
        std::string sess_student_id;
        bool sess_is_parent;
        bool sess_ok = get_session_info(session_id, user_id, sess_role_id, sess_is_parent, sess_student_id);
        int effective_role_id = (sess_ok && sess_is_parent) ? 4 : user.role_id;
        std::string effective_name = (sess_ok && sess_is_parent) ? (user.name + "家长") : user.name;

        response = {
            {"code", 200},
            {"msg", "success"},
            {"data", {
                {"id", user.id},
                {"username", user.username},
                {"name", effective_name},
                {"role_id", effective_role_id},
                {"className", user.className},
                {"points", user.points},
                // 安全修复 V18（B2）：前端据此在刷新后仍能识别「必须先改密」状态
                {"must_change_password", user.must_change_password}
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
        // 安全修复 V18（B2）：未改初始口令 → 403（白名单：改密/登出/me）
        if (!enforce_password_change(req, res)) return;
        User user;
        if (!find_user_by_id_copy(user_id, user)) {
            response = {{"code", 404}, {"msg", "用户不存在"}};
            res.set_content(response.dump(), "application/json");
            return;
        }

        response = {
            {"code", 200},
            {"msg", "success"},
            {"data", {
                {"name", user.name},
                {"className", user.className},
                {"points", user.points}
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
        // 安全修复 V18（B2）：未改初始口令 → 403
        if (!enforce_password_change(req, res)) return;

        json records = json::array();

        json result = db.query_bind(
            "SELECT id, points, reason, created_at FROM points_records WHERE student_id = ? ORDER BY created_at DESC",
            {SqliteDb::Bind(user_id)});
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
            // image_url 在库中可为 NULL（种子商品即未填写）；
            // value() 遇到 null 与 string 默认值类型不符会抛 type_error 导致 500
            item["image_url"] = row.at("image_url").is_null() ? json("") : row.at("image_url");
            items.push_back(item);
        }

        json response = {{"code", 200}, {"data", items}};
        res.set_content(response.dump(), "application/json");
    });

    // 7. 兑换商品 API
    svr.Post("/api/mall/redeem", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        // 安全修复 V10：CSRF 校验
        if (!require_csrf(req, res)) return;
        json response;

        std::string session_id = get_cookie_value(req, "sid");
        std::string user_id = verify_session(session_id);
        if (user_id.empty()) {
            response = {{"code", 401}, {"msg", "会话无效或已过期"}};
            res.set_content(response.dump(), "application/json");
            return;
        }
        // 安全修复 V18（B2）：未改初始口令 → 403
        if (!enforce_password_change(req, res)) return;

        try {
            auto req_json = json::parse(req.body);
            int item_id = req_json.value("item_id", 0);

            if (item_id <= 0) {
                response = {{"code", 400}, {"msg", "商品参数无效"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 后端验证：从数据库读取商品真实价格和库存，忽略客户端传入的 cost
            json item_result = db.query_bind(
                "SELECT cost, stock FROM mall_items WHERE id = ? AND status = 1",
                {SqliteDb::Bind((long long)item_id)});
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

            // ====== 批次 1 / 1-4（B8）兑换原子性 ======
            // 修复前的三处缺陷（均由独立验证实测）：
            //   ① 检查与扣减非原子：`:375` 判 user->points < cost 与 `:396` user->points -= cost
            //      之间隔着库存 UPDATE，两个并发请求会各自基于陈旧 points 放行 → 双花/超卖；
            //   ② 扣库存的 UPDATE 结果被**丢弃**（`stock_ok` 只看 `:381-383` 的陈旧判断），
            //      并发下 stock 已为 0 时该语句影响 0 行，却仍被判为成功 → 可**无故扣库存**；
            //   ③ 扣库存与扣积分不在同一事务，中途失败会留下「扣了一样没扣另一样」。
            // 现在：整段包在 RAII 事务里（任何提前 return / 异常都会自动回滚），
            // 扣库存用 execute_bind_affected 校验**实际影响行数**，积分用 with_user_record
            // 在一次持锁内完成「读-判断-改」，且仅在 DB 写入成功后才改内存值。
            SqliteDb::Transaction txn(db);
            if (!txn.active()) {
                response = {{"code", 500}, {"msg", "服务暂时不可用，请稍后重试"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 先读当前积分（取拷贝，不跨锁持有裸指针）
            User current;
            if (!find_user_by_id_copy(user_id, current)) {
                response = {{"code", 404}, {"msg", "用户不存在"}};
                res.set_content(response.dump(), "application/json");
                return;
            }
            if (current.points < cost) {
                response = {{"code", 400}, {"msg", "积分不足！"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 扣库存：必须确认**真的**扣掉了 1 件（0 行受影响 = 库存已被并发抢空）
            if (stock >= 0) {
                int affected = 0;
                int rc = db.execute_bind_affected(
                    "UPDATE mall_items SET stock = stock - 1 WHERE id = ? AND stock > 0",
                    {SqliteDb::Bind((long long)item_id)}, affected);
                if (rc != 0 || affected != 1) {
                    response = {{"code", 400}, {"msg", "商品库存不足"}};
                    res.set_content(response.dump(), "application/json");
                    return;   // 事务由 RAII 自动回滚
                }
            }

            // 扣积分：DB 先写。必须在**一次**持锁内完成「再次判断 + 扣减 + 落库」，
            // 否则两个并发请求都可能在各自读到相同 points 后写成同一个值（丢失一次扣减）。
            int new_points = 0;
            bool db_ok = false;
            with_user_record(user_id, [&](User& u) -> bool {
                if (u.points < cost) return false;      // 锁内二次校验；失败则不修改
                int next = u.points - cost;
                if (!update_user_points_in_db(u.id, next)) return false;  // DB 失败：不改内存
                u.points = next;
                new_points = next;
                db_ok = true;
                return true;
            });
            if (!db_ok) {
                response = {{"code", 400}, {"msg", "积分不足或扣减失败！"}};
                res.set_content(response.dump(), "application/json");
                return;   // RAII 回滚库存
            }

            if (!db.execute_bind(
                    "INSERT INTO redemption_records (student_id, item_id, cost, created_at) VALUES (?, ?, ?, ?)",
                    {SqliteDb::Bind(user_id), SqliteDb::Bind((long long)item_id), SqliteDb::Bind((long long)cost), SqliteDb::Bind(get_current_time())})) {
                response = {{"code", 500}, {"msg", "兑换记录写入失败"}};
                res.set_content(response.dump(), "application/json");
                return;   // RAII 回滚库存与积分
            }

            if (!txn.commit()) {
                response = {{"code", 500}, {"msg", "兑换提交失败，已回滚"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            response = {
                {"code", 200},
                {"msg", "兑换成功！"},
                {"data", {{"remain_points", new_points}}}
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

    // 9. 修改口令 API（安全修复 V18 / B2）
    // 既是「首登强制改密」的唯一出口，也是常规改密入口 —— 因此它**不在**强制改密门禁的
    // 拦截范围内（白名单：改密 / 登出 / me）。
    // 校验：有效会话 + CSRF + 原口令正确 + 强度达标 + 新旧不同 + 非可预测默认口令；
    // 成功后写入新哈希、清除 must_change_password 并立即落库。
    svr.Post("/api/auth/change-password", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(req, res);
        if (!require_csrf(req, res)) return;
        json response;

        std::string session_id = get_cookie_value(req, "sid");
        std::string user_id = verify_session(session_id);
        if (user_id.empty()) {
            res.status = 401;
            response = {{"code", 401}, {"msg", "会话无效或已过期"}};
            res.set_content(response.dump(), "application/json");
            return;
        }
        User user;
        if (!find_user_by_id_copy(user_id, user)) {
            res.status = 404;
            response = {{"code", 404}, {"msg", "用户不存在"}};
            res.set_content(response.dump(), "application/json");
            return;
        }

        try {
            auto req_json = json::parse(req.body);
            std::string old_password = req_json.value("old_password", "");
            std::string new_password = req_json.value("new_password", "");

            if (old_password.empty() || new_password.empty()) {
                res.status = 400;
                response = {{"code", 400}, {"msg", "原密码与新密码不能为空"}};
                res.set_content(response.dump(), "application/json");
                return;
            }
            // ① 原口令校验（B1 修复后这一步才真正具备校验力）
            if (!verify_password(old_password, user.password_hash)) {
                res.status = 400;
                response = {{"code", 400}, {"msg", "原密码错误"}};
                res.set_content(response.dump(), "application/json");
                return;
            }
            // ② 新旧必须不同
            if (old_password == new_password) {
                res.status = 400;
                response = {{"code", 400}, {"msg", "新密码不能与原密码相同"}};
                res.set_content(response.dump(), "application/json");
                return;
            }
            // ③ 强度：至少 8 位，且同时包含字母与数字
            const std::string letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
            const std::string digits = "0123456789";
            if (new_password.size() < 8 ||
                new_password.find_first_of(letters) == std::string::npos ||
                new_password.find_first_of(digits) == std::string::npos) {
                res.status = 400;
                response = {{"code", 400}, {"msg", "新密码至少 8 位，且必须同时包含字母和数字"}};
                res.set_content(response.dump(), "application/json");
                return;
            }
            // ④ 拒绝可预测的默认口令（直接对应 B2「去除可预测默认凭据」的目标）
            static const char* weak_defaults[] = {
                "admin123", "teacher123", "student123", "parent123",
                "12345678", "123456789", "password", "password1", "admin@123"
            };
            for (const char* w : weak_defaults) {
                if (new_password == w) {
                    res.status = 400;
                    response = {{"code", 400}, {"msg", "新密码过于常见/可预测，请更换"}};
                    res.set_content(response.dump(), "application/json");
                    return;
                }
            }
            // ⑤ 不得与用户名相同
            if (new_password == user.username) {
                res.status = 400;
                response = {{"code", 400}, {"msg", "新密码不能与用户名相同"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // ⑥ 落库：**单条 UPDATE** 同时写入新哈希并清除强制改密标记。
            // 不用 save_user_to_db 的 INSERT OR REPLACE —— 那会重建整行、把 created_at 重置为当前时间。
            // 批次 1 / 交接缺陷②（H-②）：原实现只看 execute_bind 的布尔返回，无法区分
            // 「语句执行成功」与「影响 0 行」—— 实测「先由外部删除该行、再改密」会返回
            // HTTP 200「密码修改成功」而库内 0 行。现在用 execute_bind_affected 校验行数。
            // 批次 1 / B5：内存写入放进 with_user_record 的一次持锁内，且**先 DB 后内存**，
            // DB 失败时内存不动（原实现先改内存再靠手动回滚恢复）。
            std::string new_hash = hash_password(new_password);
            bool db_ok = false;
            int rows_changed = 0;
            bool memory_updated = with_user_record(user_id, [&](User& u) -> bool {
                int affected = 0;
                int rc = db.execute_bind_affected(
                    "UPDATE users SET password_hash = ?, must_change_password = 0, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
                    {SqliteDb::Bind(new_hash), SqliteDb::Bind(u.id)}, affected);
                rows_changed = affected;
                if (rc != 0 || affected != 1) {
                    return false;      // 0 行 = 记录已被删除/并发改动，必须如实失败
                }
                u.password_hash = new_hash;
                u.must_change_password = false;
                db_ok = true;
                return true;
            });
            if (!db_ok) {
                res.status = 500;
                response = {{"code", 500}, {"msg", "密码保存失败，请重试"}};
                res.set_content(response.dump(), "application/json");
                Logger::error("改密失败：用户 " + user_id + " 的 UPDATE 影响行数=" +
                              std::to_string(rows_changed) + "（预期 1）");
                return;
            }

            // 只记录用户名，绝不记录口令（沿用「日志不记敏感信息」的既有约定）
            Logger::info("用户 " + user.username + " 修改口令成功，已清除强制改密标记");
            response = {{"code", 200}, {"msg", "密码修改成功"},
                        {"data", {{"must_change_password", false}}}};
        } catch (json::parse_error& e) {
            res.status = 400;
            response = {{"code", 400}, {"msg", "请求数据格式错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });
}
