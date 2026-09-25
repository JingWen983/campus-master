#include "routes.h"
#include "auth.h"
#include "models.h"
#include "sha256.h"
#include "logger.h"
#include <sstream>
#include <unordered_map>

// ===========================================================================
// 批次 1 / 1-5（B9）：对象级归属校验（教师只能操作自己任课班级的学生）
// ---------------------------------------------------------------------------
// 为什么必须有：`check_permission_middleware(..., "student:manage")` 只回答
// 「这个角色有没有该权限」——教师与管理员都有。**它不回答「这个对象是不是你的」**。
// 修复前实测：任意教师可以改/删/加任意班级的学生、给任意学生加减积分、
// 甚至回复别人班家长留言，全部 HTTP 200。
//
// 判定统一走 models 层的 teacher_owns_student / teacher_owns_class_name
// （teacher_classes JOIN classes JOIN users），失败即拒绝（fail-closed）。
// 返回 403 的响应体与既有权限门禁同族，便于前端统一判定。
static bool require_teacher_owns_class(const httplib::Request& req, httplib::Response& res,
                                       const std::string& teacher_id, const std::string& class_name) {
    if (class_name.empty() || !teacher_owns_class_name(teacher_id, class_name)) {
        res.status = 403;
        res.set_content(json{{"code", 403}, {"msg", "无权操作该班级的学生"}}.dump(), "application/json");
        return false;
    }
    return true;
}

static bool require_teacher_owns_student(const httplib::Request& req, httplib::Response& res,
                                         const std::string& teacher_id, const std::string& student_id) {
    if (student_id.empty() || !teacher_owns_student(teacher_id, student_id)) {
        res.status = 403;
        res.set_content(json{{"code", 403}, {"msg", "无权操作该学生（不在您任课的班级）"}}.dump(), "application/json");
        return false;
    }
    return true;
}

// 取当前会话的教师 id（空表示无有效会话）。写端点在归属校验前必须先拿到它。
static std::string current_teacher_id(const httplib::Request& req) {
    return verify_session(get_cookie_value(req, "sid"));
}

void register_teacher_routes(httplib::Server& svr) {
    // 教师获取自己绑定的班级列表 API（需要教师权限）
    svr.Get("/api/teacher/my-classes", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "student:manage")) {
            return;
        }

        std::string session_id = get_cookie_value(req, "sid");
        string teacher_id = verify_session(session_id);
        if (teacher_id.empty()) {
            json response = {{"code", 401}, {"msg", "未登录"}};
            res.status = 401;
            res.set_content(response.dump(), "application/json");
            return;
        }

        json classes_arr = json::array();
        json result = db.query_bind(
            "SELECT c.id, c.name, c.grade, c.grade_code, c.class_code, c.head_teacher, c.description "
            "FROM classes c "
            "JOIN teacher_classes tc ON c.id = tc.class_id "
            "WHERE tc.teacher_id = ? "
            "ORDER BY c.id",
            {SqliteDb::Bind(teacher_id)});
        for (const auto& row : result) {
            json item;
            item["id"] = row.value("id", 0);
            item["name"] = row.contains("name") && !row["name"].is_null() ? row["name"].get<std::string>() : "";
            item["grade"] = row.contains("grade") && !row["grade"].is_null() ? row["grade"].get<std::string>() : "";
            item["grade_code"] = row.contains("grade_code") && !row["grade_code"].is_null() ? row["grade_code"].get<std::string>() : "";
            item["class_code"] = row.contains("class_code") && !row["class_code"].is_null() ? row["class_code"].get<std::string>() : "";
            item["head_teacher"] = row.contains("head_teacher") && !row["head_teacher"].is_null() ? row["head_teacher"].get<std::string>() : "";
            item["description"] = row.contains("description") && !row["description"].is_null() ? row["description"].get<std::string>() : "";
            classes_arr.push_back(item);
        }

        json response = {{"code", 200}, {"data", classes_arr}};
        res.set_content(response.dump(), "application/json");
    });

    // 8. 学生管理 API（需要教师权限）
    svr.Get("/api/teacher/students", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "student:manage")) {
            return;
        }

        std::string session_id = get_cookie_value(req, "sid");
        string teacher_id = verify_session(session_id);

        // 获取教师绑定的班级名称集合
        std::vector<std::string> bound_class_names;
        if (!teacher_id.empty()) {
            json tc_result = db.query_bind(
                "SELECT c.name FROM teacher_classes tc JOIN classes c ON tc.class_id = c.id WHERE tc.teacher_id = ?",
                {SqliteDb::Bind(teacher_id)});
            for (const auto& tc_row : tc_result) {
                if (tc_row.contains("name") && !tc_row["name"].is_null()) {
                    bound_class_names.push_back(tc_row["name"].get<std::string>());
                }
            }
        }

        // 学生列表：仅返回 className 在绑定班级集合中的学生
        json students = json::array();
        for (const auto& user : users) {
            if (user.role_id == 3) { // 学生角色
                // 若已获取绑定班级，则过滤；否则保留旧行为
                if (!bound_class_names.empty()) {
                    bool in_bound = false;
                    for (const auto& cn : bound_class_names) {
                        if (cn == user.className) { in_bound = true; break; }
                    }
                    if (!in_bound) continue;
                }
                students.push_back({
                    {"id", user.id},
                    {"studentId", user.student_id.empty() ? user.username : user.student_id},
                    {"name", user.name},
                    {"className", user.className},
                    {"points", user.points}
                });
            }
        }

        json response = {{"code", 200}, {"data", students}};
        res.set_content(response.dump(), "application/json");
    });

    // 9. 添加学生 API（需要教师权限）
    svr.Post("/api/teacher/students", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "student:manage")) {
            return;
        }
        // 安全修复 V10：CSRF 校验
        if (!require_csrf(req, res)) return;

        json response;
        try {
            auto req_json = json::parse(req.body);
            string studentId = req_json.value("studentId", "");
            string name = req_json.value("name", "");
            string className = req_json.value("className", "");
            int points = req_json.value("points", 0);
            string grade_code = req_json.value("grade_code", "");
            string class_code = req_json.value("class_code", "");

            if (studentId.empty() || name.empty() || className.empty()) {
                response = {{"code", 400}, {"msg", "请填写完整信息"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 批次 1 / 1-5（B9）：只能把学生建到**自己任课**的班级
            const std::string teacher_id = current_teacher_id(req);
            if (!require_teacher_owns_class(req, res, teacher_id, className)) return;

            // 创建新学生（批次 1 / N2：原子建号并落库，避免并发下同名/同 id 互相覆盖）
            // 安全修复 V13：默认密码由服务端生成随机强密码并返回，不再硬编码 123456
            std::string default_password = generate_random_password();
            string new_id;
            bool added = add_user_record_persisted([&](User& nu) {
                nu.id = generate_user_id(3, grade_code, class_code);
                nu.username = studentId;
                nu.password_hash = hash_password(default_password);
                nu.role_id = 3;
                nu.name = name;
                nu.className = className;
                nu.points = points;
                nu.student_id = studentId;
            }, new_id, studentId);
            if (!added) {
                response = {{"code", 400}, {"msg", "学号已存在或添加冲突"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            response = {
                {"code", 200},
                {"msg", "学生添加成功"},
                {"data", {
                    {"id", new_id},
                    {"studentId", studentId},
                    {"name", name},
                    {"className", className},
                    {"points", points},
                    {"initial_password", default_password}
                }}
            };

        } catch (json::parse_error& e) {
            response = {{"code", 400}, {"msg", "请求数据格式错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 10. 编辑学生 API（需要教师权限）
    svr.Put(R"(/api/teacher/students/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "student:manage")) {
            return;
        }
        // 安全修复 V10：CSRF 校验
        if (!require_csrf(req, res)) return;

        json response;
        try {
            string student_id = req.matches[1];
            auto req_json = json::parse(req.body);

            string name = req_json.value("name", "");
            string className = req_json.value("className", "");
            int points = req_json.value("points", -999999);

            if (name.empty() || className.empty()) {
                response = {{"code", 400}, {"msg", "姓名和班级不能为空"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 批次 1 / 1-5（B9）：只能改**自己任课班级**的学生，
            // 且不允许把学生改到别人任课的班级（否则等于把人「送」进别人的班）。
            const std::string teacher_id = current_teacher_id(req);
            if (!require_teacher_owns_student(req, res, teacher_id, student_id)) return;
            if (!require_teacher_owns_class(req, res, teacher_id, className)) return;

            // 批次 1 / B5：一次持锁内完成「存在性校验 → 改内存 → 写 DB → 重建索引」，
            // 不再在锁外通过裸指针改内存（原实现是 find_if 拿 User* 后直接赋值）。
            bool found = false;
            bool db_ok = false;
            std::string out_name, out_class;
            int out_points = 0;
            with_user_record(student_id, [&](User& u) -> bool {
                if (u.role_id != 3) return false;
                found = true;
                u.name = name;
                u.className = className;
                if (points != -999999) u.points = points;
                db_ok = db.execute_bind(
                    "UPDATE users SET name = ?, className = ?, points = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
                    {SqliteDb::Bind(name), SqliteDb::Bind(className),
                     SqliteDb::Bind((long long)u.points), SqliteDb::Bind(student_id)});
                rebuild_user_indexes_locked();
                out_name = u.name;
                out_class = u.className;
                out_points = u.points;
                return db_ok;
            });

            if (!found) {
                response = {{"code", 404}, {"msg", "学生不存在"}};
                res.set_content(response.dump(), "application/json");
                return;
            }
            if (!db_ok) {
                response = {{"code", 500}, {"msg", "学生信息更新失败"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            response = {
                {"code", 200},
                {"msg", "学生信息更新成功"},
                {"data", {
                    {"id", student_id},
                    {"name", out_name},
                    {"className", out_class},
                    {"points", out_points}
                }}
            };

        } catch (const exception& e) {
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 11. 删除学生 API（需要教师权限）
    svr.Delete("/api/teacher/students", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "student:manage")) {
            return;
        }
        // 安全修复 V10：CSRF 校验
        if (!require_csrf(req, res)) return;

        json response;
        try {
            auto req_json = json::parse(req.body);
            string student_id = req_json.value("id", "");

            // 批次 1 / 1-5（B9）：只能删**自己任课班级**的学生
            if (!require_teacher_owns_student(req, res, current_teacher_id(req), student_id)) return;

            // 批次 1 / B6：一次持锁内完成「存在性校验 → 删除关联 → 删库 → erase → 重建索引」。
            // erase 会让后续元素整体前移，必须在**同一临界区**内重建索引，
            // 否则并发的用户查找会取到错误学生。
            bool found = false;
            bool db_ok = false;
            with_user_record(student_id, [&](User& u) -> bool {
                if (u.role_id != 3) return false;
                found = true;
                // DB 先删；成功后再同步内存 + 重建索引（R5：DB 先、内存后）
                if (!delete_user_from_db(student_id)) return false;
                for (auto it2 = users.begin(); it2 != users.end(); ++it2) {
                    if (it2->id == student_id) { users.erase(it2); break; }
                }
                rebuild_user_indexes_locked();
                db_ok = true;
                return true;
            });

            if (!found) {
                response = {{"code", 404}, {"msg", "学生不存在"}};
            } else {
                response = {{"code", 200}, {"msg", "学生删除成功"}};
            }

        } catch (const exception& e) {
            response = {{"code", 400}, {"msg", "无效的学生 ID"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 11. 积分操作 API（需要教师权限）
    svr.Post("/api/teacher/points", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "points:manage")) {
            return;
        }
        // 安全修复 V10：CSRF 校验
        if (!require_csrf(req, res)) return;

        json response;
        try {
            auto req_json = json::parse(req.body);
            string student_id = req_json.value("studentId", "");
            string type = req_json.value("type", "");
            int points = req_json.value("points", 0);
            string reason = req_json.value("reason", "");

            if (student_id.empty() || type.empty() || points <= 0 || reason.empty()) {
                response = {{"code", 400}, {"msg", "请填写完整信息"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 批次 1 / 1-5（B9）：只能给**自己任课班级**的学生加减积分
            // （修复前任意教师可给任意学生加/扣分，是权限提升面）
            const std::string teacher_id = current_teacher_id(req);
            if (!require_teacher_owns_student(req, res, teacher_id, student_id)) return;

            // 批次 1 / B3+B5：把「查找 → 校验积分 → 改分 → 落库」放进**一次**持锁的
            // with_user_record 回调。修复前这三步分散在三处（find_if 拿裸指针、锁外判断、
            // 锁外改值），并发扣分会各自基于陈旧 points 计算并互相覆盖（丢失扣减/超发）。
            if (type != "add" && type != "deduct") {
                response = {{"code", 400}, {"msg", "无效的操作类型"}};
                res.set_content(response.dump(), "application/json");
                return;
            }
            bool not_found = false;
            bool not_enough = false;
            bool db_ok = false;
            int points_after = 0;
            std::string student_name, student_class;
            with_user_record(student_id, [&](User& u) -> bool {
                if (u.role_id != 3) return false;          // 非学生
                student_name = u.name;
                student_class = u.className;
                if (type == "deduct" && u.points < points) { not_enough = true; return false; }
                int next = (type == "add") ? (u.points + points) : (u.points - points);
                if (!update_user_points_in_db(u.id, next)) return false;   // DB 失败：不改内存
                u.points = next;
                points_after = next;
                db_ok = true;
                return true;
            }, &not_found);

            if (not_found) {
                response = {{"code", 404}, {"msg", "学生不存在"}};
                res.set_content(response.dump(), "application/json");
                return;
            }
            if (not_enough) {
                response = {{"code", 400}, {"msg", "积分不足"}};
                res.set_content(response.dump(), "application/json");
                return;
            }
            if (!db_ok) {
                response = {{"code", 500}, {"msg", "积分更新失败"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 创建积分记录
            std::string session_id = get_cookie_value(req, "sid");
            string current_user_id = verify_session(session_id);
            int new_record_id = 0;
            PointsRecord new_record = {
                0,
                student_id,
                type == "add" ? points : -points,
                reason,
                current_user_id,
                get_current_time()
            };
            // 批次 1 / 伴-2'：记录 id 改由 DB 分配（last_insert_rowid），
            // 不再用 points_records.size()+1 —— 后者重启即重号、且与库内真实 id 脱钩。
            {
                int affected = 0;
                int rc = db.execute_bind_affected(
                    "INSERT INTO points_records (student_id, points, reason, operator_id, created_at) VALUES (?, ?, ?, ?, ?)",
                    {SqliteDb::Bind(student_id),
                     SqliteDb::Bind((long long)(type == "add" ? points : -points)),
                     SqliteDb::Bind(reason), SqliteDb::Bind(current_user_id),
                     SqliteDb::Bind(get_current_time())}, affected);
                if (rc == 0 && affected == 1) {
                    new_record_id = db.last_insert_rowid();
                    new_record.id = new_record_id;
                }
            }
            if (new_record_id > 0) {
                // 内存记录追加（供统计/导出读取）——与库内使用同一个 id
                std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
                points_records.push_back(new_record);
            }

            response = {
                {"code", 200},
                {"msg", "积分操作成功"},
                {"data", {
                    {"points", points_after},
                    {"record", {
                        {"id", new_record_id},
                        {"studentId", student_id},
                        {"studentName", student_name},
                        {"className", student_class},
                        {"points", type == "add" ? points : -points},
                        {"reason", reason},
                        {"time", get_current_time()},
                        {"operatorName", "王老师"}
                    }}
                }}
            };

        } catch (json::parse_error& e) {
            response = {{"code", 400}, {"msg", "请求数据格式错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 12. 获取积分记录 API（需要教师权限）
    svr.Get("/api/teacher/points/records", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "points:manage")) {
            return;
        }

        std::string session_id = get_cookie_value(req, "sid");
        string teacher_id = verify_session(session_id);

        json records = json::array();

        json result = db.query_bind(
            "SELECT pr.id, pr.student_id, pr.points, pr.reason, pr.operator_id, pr.created_at, "
            "s.name as student_name, s.className, "
            "o.name as operator_name "
            "FROM points_records pr "
            "LEFT JOIN users s ON pr.student_id = s.id "
            "LEFT JOIN users o ON pr.operator_id = o.id "
            "WHERE s.className IN (SELECT c.name FROM teacher_classes tc JOIN classes c ON tc.class_id = c.id WHERE tc.teacher_id = ?) "
            "ORDER BY pr.created_at DESC",
            {SqliteDb::Bind(teacher_id)});
        for (const auto& row : result) {
            json record;
            record["id"] = row.value("id", 0);
            record["studentId"] = row.value("student_id", "");
            record["studentName"] = row.value("student_name", "");
            record["className"] = row.value("className", "");
            record["points"] = row.value("points", 0);
            record["reason"] = row.value("reason", "");
            record["time"] = row.value("created_at", "");
            record["operatorName"] = row.value("operator_name", "");
            records.push_back(record);
        }

        json response = {{"code", 200}, {"data", records}};
        res.set_content(response.dump(), "application/json");
    });

    // 13. 获取评价维度 API（需要教师权限）
    svr.Get("/api/teacher/evaluation/dimensions", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "evaluation:manage")) {
            return;
        }

        // 评价维度数据
        json dimensions = json::array({
            {{
                "id", 1},
                {"name", "德育"},
                {"description", "思想品德和道德素养"},
                {"scoreMax", 100}
            },
            {{
                "id", 2},
                {"name", "智育"},
                {"description", "学习成绩和学习能力"},
                {"scoreMax", 100}
            },
            {{
                "id", 3},
                {"name", "体育"},
                {"description", "体育锻炼和健康状况"},
                {"scoreMax", 100}
            },
            {{
                "id", 4},
                {"name", "美育"},
                {"description", "艺术素养和审美能力"},
                {"scoreMax", 100}
            },
            {{
                "id", 5},
                {"name", "劳育"},
                {"description", "劳动技能和实践能力"},
                {"scoreMax", 100}
            }
        });

        json response = {{"code", 200}, {"data", dimensions}};
        res.set_content(response.dump(), "application/json");
    });

    // 14. 提交评价 API（需要教师权限）
    svr.Post("/api/teacher/evaluation", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "evaluation:manage")) {
            return;
        }
        // 安全修复 V10：CSRF 校验
        if (!require_csrf(req, res)) return;

        json response;
        try {
            auto req_json = json::parse(req.body);
            string student_id = req_json.value("studentId", "");
            json scores = req_json.value("scores", json::object());
            string comment = req_json.value("comment", "");

            if (student_id.empty() || scores.empty()) {
                response = {{"code", 400}, {"msg", "请填写完整信息"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 批次 1 / 1-5（B9）：只能评价**自己任课班级**的学生
            // （修复前任意教师可对任意学生写评价：评测数据被污染且属越权写）
            if (!require_teacher_owns_student(req, res, current_teacher_id(req), student_id)) return;

            // 查找学生（批次 1 / B5：索引受锁查询 + 身份校验，不再遍历全局向量）
            if (find_user_id_by_id_key(student_id).empty()) {
                response = {{"code", 404}, {"msg", "学生不存在"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 保存评价到数据库
            std::string session_id = get_cookie_value(req, "sid");
            string current_user_id = verify_session(session_id);
            json evaluations = json::array();
            for (const auto& dim : {1, 2, 3, 4, 5}) {
                int score = scores.value(std::to_string(dim), 0);

                db.execute_bind(
                    "INSERT INTO evaluations (student_id, dimension_id, score, comment, evaluator_id, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?)",
                    {SqliteDb::Bind(student_id), SqliteDb::Bind((long long)dim),
                     SqliteDb::Bind((long long)score), SqliteDb::Bind(comment),
                     SqliteDb::Bind(current_user_id), SqliteDb::Bind(get_current_time()),
                     SqliteDb::Bind(get_current_time())});

                json eval;
                eval["id"] = dim;
                eval["studentId"] = student_id;
                eval["dimensionId"] = dim;
                eval["score"] = score;
                eval["comment"] = comment;
                eval["time"] = get_current_time();
                evaluations.push_back(eval);
            }

            response = {
                {"code", 200},
                {"msg", "评价提交成功"},
                {"data", {
                    {"evaluations", evaluations}
                }}
            };

        } catch (json::parse_error& e) {
            response = {{"code", 400}, {"msg", "请求数据格式错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 15. 获取工作台统计数据 API（需要教师权限）
    svr.Get("/api/teacher/dashboard", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "student:manage")) {
            return;
        }

        std::string session_id = get_cookie_value(req, "sid");
        string teacher_id = verify_session(session_id);

        // 统计数据
        int student_count = 0;
        int today_points = 0;
        int pending_evaluations = 0;

        // 获取今天的日期字符串
        time_t now = time(nullptr);
        tm* today_tm = localtime(&now);
        char today_str[16];
        strftime(today_str, sizeof(today_str), "%Y-%m-%d", today_tm);

        // 仅统计教师绑定班级的学生
        for (const auto& user : users) {
            if (user.role_id == 3) {
                json tc_result = db.query_bind(
                    "SELECT 1 FROM teacher_classes tc JOIN classes c ON tc.class_id = c.id "
                    "WHERE tc.teacher_id = ? AND c.name = ? LIMIT 1",
                    {SqliteDb::Bind(teacher_id), SqliteDb::Bind(user.className)});
                if (!tc_result.empty()) {
                    student_count++;
                }
            }
        }

        // 查询今日积分变动总数（限定绑定班级学生）
        json today_result = db.query_bind(
            "SELECT COALESCE(SUM(pr.points), 0) as total FROM points_records pr "
            "LEFT JOIN users s ON pr.student_id = s.id "
            "WHERE pr.created_at >= ? "
            "AND s.className IN (SELECT c.name FROM teacher_classes tc JOIN classes c ON tc.class_id = c.id WHERE tc.teacher_id = ?)",
            {SqliteDb::Bind(std::string(today_str) + " 00:00:00"), SqliteDb::Bind(teacher_id)});
        if (!today_result.empty()) {
            today_points = today_result[0].value("total", 0);
        }

        // 查询待评价学生数（限定绑定班级学生，没有评价记录的学生）
        json pending_result = db.query_bind(
            "SELECT COUNT(*) as cnt FROM users WHERE role_id=3 "
            "AND className IN (SELECT c.name FROM teacher_classes tc JOIN classes c ON tc.class_id = c.id WHERE tc.teacher_id = ?) "
            "AND id NOT IN (SELECT DISTINCT student_id FROM evaluations)",
            {SqliteDb::Bind(teacher_id)});
        if (!pending_result.empty()) {
            pending_evaluations = pending_result[0].value("cnt", 0);
        }

        json stats = {
            {"studentCount", student_count},
            {"todayPoints", today_points},
            {"pendingEvaluations", pending_evaluations}
        };

        json response = {{"code", 200}, {"data", stats}};
        res.set_content(response.dump(), "application/json");
    });

    // 16. 获取数据统计 API（需要教师权限）
    svr.Get("/api/teacher/statistics", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "statistics:view")) {
            return;
        }

        std::string session_id = get_cookie_value(req, "sid");
        string teacher_id = verify_session(session_id);

        json statistics;

        // 1. 班级积分统计：按绑定班级分组，计算每个班级所有学生的积分总和
        json classPoints = json::array();
        json classResult = db.query_bind(
            "SELECT className, SUM(points) as totalPoints "
            "FROM users "
            "WHERE role_id = 3 AND className IS NOT NULL AND className != '' "
            "AND className IN (SELECT c.name FROM teacher_classes tc JOIN classes c ON tc.class_id = c.id WHERE tc.teacher_id = ?) "
            "GROUP BY className "
            "ORDER BY totalPoints DESC",
            {SqliteDb::Bind(teacher_id)});
        for (const auto& row : classResult) {
            json item;
            item["className"] = row.value("className", "");
            item["points"] = row.value("totalPoints", 0);
            classPoints.push_back(item);
        }

        // 如果没有数据，提供默认值
        if (classPoints.empty()) {
            classPoints = json::array({
                {{"className", "暂无班级数据"}, {"points", 0}}
            });
        }
        statistics["classPoints"] = classPoints;

        // 2. 积分趋势：最近6个月的积分变动统计
        json pointsTrend = json::array();

        // 获取当前时间
        time_t now = time(nullptr);
        tm* current_tm = localtime(&now);
        int current_year = current_tm->tm_year + 1900;
        int current_month = current_tm->tm_mon + 1;

        // 生成最近6个月的月份列表
        vector<pair<int, int>> months;
        for (int i = 5; i >= 0; i--) {
            int m = current_month - i;
            int y = current_year;
            while (m <= 0) {
                m += 12;
                y--;
            }
            months.push_back({y, m});
        }

        // 查询每个月的积分变动
        for (size_t i = 0; i < months.size(); i++) {
            int year = months[i].first;
            int month = months[i].second;
            char monthStart[20], monthEnd[20];
            snprintf(monthStart, sizeof(monthStart), "%04d-%02d-01 00:00:00", year, month);

            // 计算下个月第一天
            int nextMonth = month + 1;
            int nextYear = year;
            if (nextMonth > 12) {
                nextMonth = 1;
                nextYear++;
            }
            snprintf(monthEnd, sizeof(monthEnd), "%04d-%02d-01 00:00:00", nextYear, nextMonth);

            json result = db.query_bind(
                "SELECT COALESCE(SUM(points), 0) as monthPoints FROM points_records "
                "WHERE created_at >= ? AND created_at < ?",
                {SqliteDb::Bind(monthStart), SqliteDb::Bind(monthEnd)});
            int monthPoints = 0;
            if (!result.empty()) {
                monthPoints = result[0].value("monthPoints", 0);
            }

            json trendItem;
            char monthName[10];
            snprintf(monthName, sizeof(monthName), "%d月", month);
            trendItem["month"] = monthName;
            trendItem["points"] = monthPoints;
            pointsTrend.push_back(trendItem);
        }
        statistics["pointsTrend"] = pointsTrend;

        // 3. 评价分布统计
        json evaluationDistribution = json::array();

        // 优秀: score >= 90
        json excellentResult = db.query("SELECT COUNT(*) as count FROM evaluations WHERE score >= 90");
        int excellentCount = excellentResult.empty() ? 0 : excellentResult[0].value("count", 0);

        // 良好: 80 <= score < 90
        json goodResult = db.query("SELECT COUNT(*) as count FROM evaluations WHERE score >= 80 AND score < 90");
        int goodCount = goodResult.empty() ? 0 : goodResult[0].value("count", 0);

        // 中等: 70 <= score < 80
        json mediumResult = db.query("SELECT COUNT(*) as count FROM evaluations WHERE score >= 70 AND score < 80");
        int mediumCount = mediumResult.empty() ? 0 : mediumResult[0].value("count", 0);

        // 待改进: score < 70
        json improveResult = db.query("SELECT COUNT(*) as count FROM evaluations WHERE score < 70");
        int improveCount = improveResult.empty() ? 0 : improveResult[0].value("count", 0);

        evaluationDistribution.push_back({{"level", "优秀"}, {"count", excellentCount}});
        evaluationDistribution.push_back({{"level", "良好"}, {"count", goodCount}});
        evaluationDistribution.push_back({{"level", "中等"}, {"count", mediumCount}});
        evaluationDistribution.push_back({{"level", "待改进"}, {"count", improveCount}});

        statistics["evaluationDistribution"] = evaluationDistribution;

        json response = {{"code", 200}, {"data", statistics}};
        res.set_content(response.dump(), "application/json");
    });

    // 20. 教师获取所有评价 API（需要教师权限）
    svr.Get("/api/teacher/evaluations", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "evaluation:manage")) {
            return;
        }

        std::string session_id = get_cookie_value(req, "sid");
        string teacher_id = verify_session(session_id);

        json evaluations_json = json::array();

        json result = db.query_bind(
            "SELECT e.id, e.student_id, e.dimension_id, e.score, e.comment, e.evaluator_id, e.created_at, "
            "s.name as student_name, s.className, u.name as evaluator_name "
            "FROM evaluations e "
            "LEFT JOIN users s ON e.student_id = s.id "
            "LEFT JOIN users u ON e.evaluator_id = u.id "
            "WHERE s.className IN (SELECT c.name FROM teacher_classes tc JOIN classes c ON tc.class_id = c.id WHERE tc.teacher_id = ?) "
            "ORDER BY e.created_at DESC",
            {SqliteDb::Bind(teacher_id)});

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
            eval["student_name"] = row.value("student_name", "");
            eval["className"] = row.value("className", "");
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

    // 21. 修改评价 API（需要教师权限）
    svr.Put(R"(/api/teacher/evaluation/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "evaluation:manage")) {
            return;
        }
        // 安全修复 V10：CSRF 校验
        if (!require_csrf(req, res)) return;

        json response;
        try {
            int eval_id = stoi(req.matches[1]);
            auto req_json = json::parse(req.body);
            int score = req_json.value("score", 0);
            string comment = req_json.value("comment", "");

            if (db.execute_bind(
                "UPDATE evaluations SET score = ?, comment = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
                {SqliteDb::Bind((long long)score), SqliteDb::Bind(comment), SqliteDb::Bind((long long)eval_id)})) {
                response = {{"code", 200}, {"msg", "评价修改成功"}};
            } else {
                response = {{"code", 404}, {"msg", "评价不存在"}};
            }

        } catch (const exception& e) {
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 22. 删除评价 API（需要教师权限）
    svr.Delete(R"(/api/teacher/evaluation/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "evaluation:manage")) {
            return;
        }
        // 安全修复 V10：CSRF 校验
        if (!require_csrf(req, res)) return;

        json response;
        try {
            int eval_id = stoi(req.matches[1]);

            if (db.execute_bind("DELETE FROM evaluations WHERE id = ?",
                {SqliteDb::Bind((long long)eval_id)})) {
                response = {{"code", 200}, {"msg", "评价删除成功"}};
            } else {
                response = {{"code", 404}, {"msg", "评价不存在"}};
            }

        } catch (const exception& e) {
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // ====== CSV 学生批量导入 API ======
    svr.Post("/api/teacher/students/import", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "student:manage")) {
            return;
        }
        // 安全修复 V10：CSRF 校验
        if (!require_csrf(req, res)) return;

        json response;
        int success_count = 0;
        int fail_count = 0;
        json errors = json::array();

        try {
            // 从 multipart 表单或 JSON body 获取 CSV 内容
            string csv_content;
            if (req.has_file("file")) {
                const auto& file = req.get_file_value("file");
                csv_content = file.content;
            } else {
                // 尝试从 JSON body 获取
                auto req_json = json::parse(req.body);
                csv_content = req_json.value("csv_data", "");
            }

            if (csv_content.empty()) {
                response = {{"code", 400}, {"msg", "未收到CSV数据"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 解析 CSV（格式：学号,姓名,班级,初始密码）
            vector<vector<string>> rows;
            stringstream ss(csv_content);
            string line;
            bool first_line = true;

            while (getline(ss, line)) {
                if (line.empty()) continue;
                // 跳过 BOM
                if (first_line) {
                    if (line.size() >= 3 && line[0] == '\xEF' && line[1] == '\xBB' && line[2] == '\xBF') {
                        line = line.substr(3);
                    }
                    first_line = false;
                }
                // 移除 \r
                if (!line.empty() && line.back() == '\r') line.pop_back();

                vector<string> fields;
                stringstream field_ss(line);
                string field;
                while (getline(field_ss, field, ',')) {
                    // 去除首尾空格和引号
                    size_t start = field.find_first_not_of(" \t\"");
                    size_t end = field.find_last_not_of(" \t\"");
                    if (start == string::npos) {
                        fields.push_back("");
                    } else {
                        fields.push_back(field.substr(start, end - start + 1));
                    }
                }

                // 跳过表头行
                if (!rows.empty() || (fields.size() >= 4 && fields[0] != "学号" && fields[0] != "studentId")) {
                    rows.push_back(fields);
                } else if (fields.size() >= 4 && fields[0] == "学号") {
                    // 这是表头，跳过
                } else {
                    rows.push_back(fields);
                }
            }

            for (size_t i = 0; i < rows.size(); i++) {
                auto& row = rows[i];
                if (row.size() < 3) {
                    fail_count++;
                    errors.push_back({{"row", i + 2}, {"msg", "字段不足，至少需要学号、姓名、班级"}});
                    continue;
                }

                string student_id = row[0];
                string name = row[1];
                string className = row[2];
                // 安全修复 V13：批量导入未提供密码时随机生成，不再硬编码 123456
                string password = row.size() >= 4 ? row[3] : generate_random_password();

                if (student_id.empty() || name.empty()) {
                    fail_count++;
                    errors.push_back({{"row", i + 2}, {"msg", "学号和姓名不能为空"}});
                    continue;
                }

                // 检查用户名是否已存在（批次 1 / B5：索引受锁查询，不再返回裸指针）
                if (!find_user_id_by_username(student_id).empty()) {
                    fail_count++;
                    errors.push_back({{"row", i + 2}, {"msg", "学号 " + student_id + " 已存在"}});
                    continue;
                }

                // 批次 1 / N2：原子建号并落库（唯一性校验 + 生成 id + 写 DB + 写内存一次持锁）
                string new_id;
                bool added = add_user_record_persisted([&](User& nu) {
                    nu.id = generate_user_id(3, "", "");
                    nu.username = student_id;
                    nu.password_hash = hash_password(password);
                    nu.role_id = 3;
                    nu.name = name;
                    nu.className = className;
                    nu.points = 0;
                    nu.student_id = student_id;
                }, new_id, student_id);
                if (!added) {
                    fail_count++;
                    errors.push_back({{"row", i + 2}, {"msg", "学号 " + student_id + " 已存在"}});
                    continue;
                }
                success_count++;
            }

            response = {
                {"code", 200},
                {"msg", "导入完成"},
                {"data", {
                    {"success", success_count},
                    {"failed", fail_count},
                    {"errors", errors}
                }}
            };
        } catch (json::parse_error& e) {
            response = {{"code", 400}, {"msg", "数据格式错误"}};
        } catch (exception& e) {
            // 安全修复 V15：不向客户端回显内部异常细节
            Logger::error(string("学生批量导入失败: ") + e.what());
            response = {{"code", 500}, {"msg", "导入失败"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // ====== 家长留言管理 API ======

    // 23. 获取家长留言列表 API（需要教师权限）
    svr.Get("/api/teacher/parent-messages", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "student:manage")) {
            return;
        }

        std::string session_id = get_cookie_value(req, "sid");
        string teacher_id = verify_session(session_id);
        Logger::info("教师 " + teacher_id + " 获取家长留言列表");

        json messages = json::array();

        try {
        json result = db.query_bind(
            "SELECT pm.id, pm.student_id, pm.sender_type, pm.sender_id, pm.content, pm.reply_to, pm.created_at, pm.read_status, "
            "u.name as student_name, u.className as class_name "
            "FROM parent_messages pm "
            "LEFT JOIN users u ON pm.student_id = u.id "
            "WHERE u.className IN (SELECT c.name FROM teacher_classes tc JOIN classes c ON tc.class_id = c.id WHERE tc.teacher_id = ?) "
            "ORDER BY pm.read_status ASC, pm.created_at DESC",
            {SqliteDb::Bind(teacher_id)});
        for (const auto& row : result) {
            json msg;
            msg["id"] = row.value("id", 0);
            msg["student_id"] = row.value("student_id", "");
            msg["student_name"] = row.contains("student_name") && !row["student_name"].is_null() ? row["student_name"].get<std::string>() : "";
            msg["class_name"] = row.contains("class_name") && !row["class_name"].is_null() ? row["class_name"].get<std::string>() : "";
            msg["sender_type"] = row.contains("sender_type") && !row["sender_type"].is_null() ? row["sender_type"].get<std::string>() : "";
            msg["sender_id"] = row.contains("sender_id") && !row["sender_id"].is_null() ? row["sender_id"].get<std::string>() : "";
            msg["content"] = row.contains("content") && !row["content"].is_null() ? row["content"].get<std::string>() : "";
            msg["reply_to"] = row.contains("reply_to") && !row["reply_to"].is_null() ? row["reply_to"].get<int>() : 0;
            msg["created_at"] = row.contains("created_at") && !row["created_at"].is_null() ? row["created_at"].get<std::string>() : "";
            msg["read_status"] = row.value("read_status", 0);
            messages.push_back(msg);
        }

        json response = {{"code", 200}, {"data", messages}};
        res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            Logger::error("教师获取家长留言列表异常: " + std::string(e.what()));
            json response = {{"code", 500}, {"msg", "服务器内部错误"}};
            res.status = 500;
            res.set_content(response.dump(), "application/json");
        }
    });

    // 24. 教师回复家长留言 API（需要教师权限）
    svr.Post(R"(/api/teacher/parent-messages/(\d+)/reply)", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "student:manage")) {
            return;
        }
        // 安全修复 V10：CSRF 校验
        if (!require_csrf(req, res)) return;

        json response;
        try {
            int message_id = stoi(req.matches[1]);
            auto req_json = json::parse(req.body);
            string content = req_json.value("content", "");
            std::string session_id = get_cookie_value(req, "sid");
            string teacher_id = verify_session(session_id);

            if (content.empty()) {
                response = {{"code", 400}, {"msg", "回复内容不能为空"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 1. 从原留言获取 student_id
            json query_result = db.query_bind(
                "SELECT student_id FROM parent_messages WHERE id = ?",
                {SqliteDb::Bind((long long)message_id)});
            if (query_result.empty()) {
                response = {{"code", 404}, {"msg", "原留言不存在"}};
                res.set_content(response.dump(), "application/json");
                return;
            }
            string student_id = query_result[0].value("student_id", "");

            // 批次 1 / 1-5（B9）：必须校验**该留言所属学生**在自己任课的班级。
            // 修复前此处直接用原留言的 student_id 插入回复，完全没有归属校验
            // → 任意教师可回复别人班家长的留言（实测 HTTP 200）。
            if (!require_teacher_owns_student(req, res, teacher_id, student_id)) return;

            // 2. 插入教师回复
            if (!db.execute_bind(
                "INSERT INTO parent_messages (student_id, sender_type, sender_id, content, reply_to, created_at, read_status) "
                "VALUES (?, 'teacher', ?, ?, ?, ?, 1)",
                {SqliteDb::Bind(student_id), SqliteDb::Bind(teacher_id), SqliteDb::Bind(content),
                 SqliteDb::Bind((long long)message_id), SqliteDb::Bind(get_current_time())})) {
                Logger::error("教师 " + teacher_id + " 回复留言 " + to_string(message_id) + " 失败");
                response = {{"code", 500}, {"msg", "回复失败"}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // 3. 将原留言标记为已读
            db.execute_bind("UPDATE parent_messages SET read_status = 1 WHERE id = ?",
                {SqliteDb::Bind((long long)message_id)});

            Logger::info("教师 " + teacher_id + " 回复了留言 " + to_string(message_id));

            response = {{"code", 200}, {"msg", "回复成功"}};

        } catch (json::parse_error& e) {
            response = {{"code", 400}, {"msg", "请求数据格式错误"}};
        } catch (const exception& e) {
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 25. 标记家长留言为已读 API（需要教师权限）
    svr.Put(R"(/api/teacher/parent-messages/(\d+)/read)", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        // 检查权限
        if (!check_permission_middleware(req, res, "student:manage")) {
            return;
        }
        // 安全修复 V10：CSRF 校验
        if (!require_csrf(req, res)) return;

        json response;
        try {
            int message_id = stoi(req.matches[1]);

            if (db.execute_bind("UPDATE parent_messages SET read_status = 1 WHERE id = ?",
                {SqliteDb::Bind((long long)message_id)})) {
                response = {{"code", 200}, {"msg", "已标记为已读"}};
            } else {
                response = {{"code", 404}, {"msg", "留言不存在"}};
            }

        } catch (const exception& e) {
            response = {{"code", 400}, {"msg", "请求参数错误"}};
        }

        res.set_content(response.dump(), "application/json");
    });

    // 教师查看绑定班级学生的兑换记录 API
    svr.Get("/api/teacher/redemptions", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);

        if (!check_permission_middleware(req, res, "student:manage")) {
            return;
        }

        std::string session_id = get_cookie_value(req, "sid");
        string teacher_id = verify_session(session_id);
        if (teacher_id.empty()) {
            json response = {{"code", 401}, {"msg", "未登录"}};
            res.status = 401;
            res.set_content(response.dump(), "application/json");
            return;
        }

        json records = json::array();
        // 仅查询教师绑定班级下的学生兑换记录
        json result = db.query_bind(
            "SELECT r.id, r.student_id, r.item_id, r.cost, r.created_at, "
            "s.name as student_name, s.className, "
            "m.name as item_name "
            "FROM redemption_records r "
            "LEFT JOIN users s ON r.student_id = s.id "
            "LEFT JOIN mall_items m ON r.item_id = m.id "
            "WHERE s.className IN ("
            "  SELECT c.name FROM teacher_classes tc JOIN classes c ON tc.class_id = c.id WHERE tc.teacher_id = ?"
            ") "
            "ORDER BY r.created_at DESC",
            {SqliteDb::Bind(teacher_id)});
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
            records.push_back(record);
        }

        json response = {{"code", 200}, {"data", records}};
        res.set_content(response.dump(), "application/json");
    });
}
