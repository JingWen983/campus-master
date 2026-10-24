// routes_cards.cpp - 积分充值卡（卡密）路由
// ============================================================================
// 功能概述：
//   * 管理员/教师批量生成带面额的积分卡（16 位卡密 XXXX-XXXX-XXXX-XXXX）；
//   * 定向卡（bound_student_id 非空）：仅绑定学生本人可兑换，兑换即时到账；
//   * 通用卡（bound_student_id 为空）：任何学生提交后进入 pending，由
//     「签发教师 / 该学生任课教师 / 管理员」确认到账（教师可驳回）；
//   * 配套：作废未使用卡、已使用卡重新激活（可选解除绑定，实物卡回收复用）。
//
// 权限模型（不新增权限点）：
//   * 管理端/教师端接口统一走 points:manage（管理员、教师均有此权限）；
//   * 教师的可见/可操作范围在 handler 内按会话角色二次收敛：
//       - 生成定向卡：仅可绑定自己任课班级的学生（teacher_owns_student）；
//       - 卡列表/作废/激活：仅本人签发的卡；
//       - 待确认/确认/驳回：本人签发的通用卡（且待确认学生须在其任课班级），
//         外加「管理员签发、且待确认学生在该教师任课班级」的卡；
//       - 管理员不受以上限制。
//   * 学生端兑换/记录走 mall:manage（学生角色权限），与现有学生接口族一致。
//
// 并发与一致性（沿用批次 1 的既定协议）：
//   * 所有状态变更用「条件 UPDATE + affected rows 校验」，并发下只有一方成功；
//   * 加分走 with_user_record（DB 先写、成功后才改内存）；
//   * 卡状态与积分/积分记录在同一个 RAII 事务内提交，失败整体回滚。
// ============================================================================
#include "routes.h"
#include "auth.h"
#include "models.h"
#include "sha256.h"
#include "logger.h"
#include <sstream>

// 判断会话用户是否管理员（role_id=1）。查不到按非管理员处理（fail-closed）。
static bool is_admin_session(const std::string& user_id) {
    User u;
    if (!find_user_by_id_copy(user_id, u)) return false;
    return u.role_id == 1;
}

// 卡状态 → 中文
static const char* card_status_text(const std::string& status) {
    if (status == "unused") return "未使用";
    if (status == "pending") return "待确认";
    if (status == "used") return "已使用";
    if (status == "void") return "已作废";
    return "未知";
}

// 安全取字符串列：NULL → 空串。
// nlohmann 的 value("col", "") 在列值为 NULL 时抛 type_error
// （与 routes_public.cpp image_url 同坑），本表多个列可空，统一走这里。
static std::string str_or_empty(const json& row, const char* col) {
    if (!row.contains(col) || row[col].is_null()) return "";
    try {
        return row[col].get<std::string>();
    } catch (...) {
        return "";
    }
}

// 组装一张卡的 JSON（供列表/生成结果共用；with_names=true 时附带姓名）
static json card_to_json(const json& row, bool with_names, const std::string& student_name = "",
                         const std::string& pending_name = "", const std::string& issuer_name = "") {
    json item;
    item["id"] = row.value("id", 0LL);
    item["code"] = str_or_empty(row, "code");
    item["face_value"] = row.value("face_value", 0);
    item["status"] = str_or_empty(row, "status");
    item["status_text"] = card_status_text(str_or_empty(row, "status"));
    item["batch_no"] = str_or_empty(row, "batch_no");
    item["remark"] = str_or_empty(row, "remark");
    item["issuer_id"] = str_or_empty(row, "issuer_id");
    item["bound_student_id"] = str_or_empty(row, "bound_student_id");
    item["pending_student_id"] = str_or_empty(row, "pending_student_id");
    item["used_by"] = str_or_empty(row, "used_by");
    item["used_at"] = str_or_empty(row, "used_at");
    item["confirmed_by"] = str_or_empty(row, "confirmed_by");
    item["created_at"] = str_or_empty(row, "created_at");
    if (with_names) {
        item["bound_student_name"] = student_name;
        item["pending_student_name"] = pending_name;
        item["issuer_name"] = issuer_name;
    }
    return item;
}

// 卡列表/批次查询共用的行→JSON（LEFT JOIN users 取姓名）
static json card_row_to_json(const json& row) {
    json bound_name = row.contains("bound_name") && !row["bound_name"].is_null()
                      ? row["bound_name"] : json("");
    json pending_name = row.contains("pending_name") && !row["pending_name"].is_null()
                        ? row["pending_name"] : json("");
    json issuer_name = row.contains("issuer_name") && !row["issuer_name"].is_null()
                       ? row["issuer_name"] : json("");
    return card_to_json(row, true,
                        bound_name.is_string() ? bound_name.get<std::string>() : "",
                        pending_name.is_string() ? pending_name.get<std::string>() : "",
                        issuer_name.is_string() ? issuer_name.get<std::string>() : "");
}

void register_card_routes(httplib::Server& svr) {

    // =========================================================================
    // 管理端/教师端（points:manage）
    // =========================================================================

    // 1. 批量生成积分卡
    //    body: {count, face_value, remark?, bound_student_id?}
    //    教师签发定向卡时 bound_student_id 必须是其任课班级学生。
    svr.Post("/api/point-cards/generate", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(req, res);
        if (!check_permission_middleware(req, res, "points:manage")) return;
        if (!require_csrf(req, res)) return;

        std::string session_id = get_cookie_value(req, "sid");
        std::string user_id = verify_session(session_id);
        if (user_id.empty()) {
            res.status = 401;
            res.set_content(json{{"code", 401}, {"msg", "会话无效或已过期"}}.dump(), "application/json");
            return;
        }
        bool is_admin = is_admin_session(user_id);

        try {
            auto body = json::parse(req.body);
            int count = body.value("count", 0);
            int face_value = body.value("face_value", 0);
            std::string remark = body.value("remark", "");
            std::string bound_student_id = body.value("bound_student_id", "");

            if (count < 1 || count > 200) {
                res.status = 400;
                res.set_content(json{{"code", 400}, {"msg", "生成数量须在 1-200 之间"}}.dump(), "application/json");
                return;
            }
            if (face_value < 1 || face_value > 100000) {
                res.status = 400;
                res.set_content(json{{"code", 400}, {"msg", "面额须在 1-100000 之间"}}.dump(), "application/json");
                return;
            }

            // 定向卡：校验绑定学生存在且为学生角色；教师还须拥有该学生（任课班级）
            if (!bound_student_id.empty()) {
                User stu;
                if (!find_user_by_id_copy(bound_student_id, stu) || stu.role_id != 3) {
                    res.status = 400;
                    res.set_content(json{{"code", 400}, {"msg", "绑定的学生不存在"}}.dump(), "application/json");
                    return;
                }
                if (!is_admin && !teacher_owns_student(user_id, bound_student_id)) {
                    res.status = 403;
                    res.set_content(json{{"code", 403}, {"msg", "无权为非任课班级的学生签发定向卡"}}.dump(), "application/json");
                    return;
                }
            }

            // 批次号：PC-yyyyMMdd-HHMMSS
            time_t now = time(nullptr);
            struct tm* tmv = localtime(&now);
            char batch_buf[32];
            strftime(batch_buf, sizeof(batch_buf), "PC-%Y%m%d-%H%M%S", tmv);
            std::string batch_no(batch_buf);

            // 逐张生成（库内查重 + 唯一索引兜底；冲突重试）
            json cards = json::array();
            int created = 0;
            for (int i = 0; i < count; i++) {
                std::string code;
                for (int attempt = 0; attempt < 10; attempt++) {
                    std::string candidate = generate_card_code();
                    json dup = db.query_bind("SELECT 1 FROM point_cards WHERE code = ?",
                                             {SqliteDb::Bind(candidate)});
                    if (dup.empty()) { code = candidate; break; }
                }
                if (code.empty()) {
                    Logger::error("积分卡生成失败：卡密碰撞重试耗尽（已生成 " + std::to_string(created) + " 张）");
                    break;
                }
                bool ok = db.execute_bind(
                    "INSERT INTO point_cards (code, face_value, status, batch_no, remark, issuer_id, bound_student_id, created_at) "
                    "VALUES (?, ?, 'unused', ?, ?, ?, ?, ?)",
                    {SqliteDb::Bind(code), SqliteDb::Bind((long long)face_value),
                     SqliteDb::Bind(batch_no), SqliteDb::Bind(remark),
                     SqliteDb::Bind(user_id),
                     bound_student_id.empty() ? SqliteDb::Bind::null() : SqliteDb::Bind(bound_student_id),
                     SqliteDb::Bind(get_current_time())});
                if (!ok) {
                    Logger::error("积分卡写入失败：code=" + code);
                    continue;
                }
                json row = {{"id", db.last_insert_rowid()},
                            {"code", code},
                            {"face_value", face_value},
                            {"status", "unused"},
                            {"batch_no", batch_no},
                            {"remark", remark},
                            {"issuer_id", user_id},
                            {"bound_student_id", bound_student_id},
                            {"pending_student_id", ""},
                            {"used_by", ""}, {"used_at", ""}, {"confirmed_by", ""},
                            {"created_at", get_current_time()}};
                cards.push_back(card_to_json(row, false));
                created++;
            }

            if (created == 0) {
                res.status = 500;
                res.set_content(json{{"code", 500}, {"msg", "积分卡生成失败，请重试"}}.dump(), "application/json");
                return;
            }
            Logger::info("积分卡批次 " + batch_no + " 生成 " + std::to_string(created) + " 张，面额 " +
                         std::to_string(face_value) + "，签发人 " + user_id);
            json response = {{"code", 200}, {"msg", "成功生成 " + std::to_string(created) + " 张积分卡"},
                             {"data", {{"batch_no", batch_no}, {"count", created}, {"cards", cards}}}};
            res.set_content(response.dump(), "application/json");
        } catch (json::parse_error& e) {
            res.status = 400;
            res.set_content(json{{"code", 400}, {"msg", "请求数据格式错误"}}.dump(), "application/json");
        }
    });

    // 2. 卡列表（分页 + 筛选）。管理员看全部；教师看本人签发。
    svr.Get("/api/point-cards", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(req, res);
        if (!check_permission_middleware(req, res, "points:manage")) return;

        std::string session_id = get_cookie_value(req, "sid");
        std::string user_id = verify_session(session_id);
        bool is_admin = is_admin_session(user_id);

        std::string status = req.get_param_value("status");
        std::string batch_no = req.get_param_value("batch_no");
        int page = std::max(1, atoi(req.get_param_value("page").c_str()));
        int page_size = std::min(200, std::max(1, atoi(req.get_param_value("page_size").c_str())));

        std::string where = " WHERE 1=1";
        std::vector<SqliteDb::Bind> params;
        if (!is_admin) {
            where += " AND c.issuer_id = ?";
            params.push_back(SqliteDb::Bind(user_id));
        }
        if (!status.empty()) {
            where += " AND c.status = ?";
            params.push_back(SqliteDb::Bind(status));
        }
        if (!batch_no.empty()) {
            where += " AND c.batch_no = ?";
            params.push_back(SqliteDb::Bind(batch_no));
        }

        // 总数
        json cnt = db.query_bind("SELECT COUNT(*) AS n FROM point_cards c" + where, params);
        long long total = cnt.empty() ? 0 : cnt[0].value("n", 0LL);

        json rows = db.query_bind(
            "SELECT c.id, c.code, c.face_value, c.status, c.batch_no, c.remark, c.issuer_id, "
            "c.bound_student_id, c.pending_student_id, c.used_by, c.used_at, c.confirmed_by, c.created_at, "
            "bu.name AS bound_name, pu.name AS pending_name, iu.name AS issuer_name "
            "FROM point_cards c "
            "LEFT JOIN users bu ON c.bound_student_id = bu.id "
            "LEFT JOIN users pu ON c.pending_student_id = pu.id "
            "LEFT JOIN users iu ON c.issuer_id = iu.id " +
                where + " ORDER BY c.id DESC LIMIT ? OFFSET ?",
            [&params, page, page_size]() {
                std::vector<SqliteDb::Bind> p = params;
                p.push_back(SqliteDb::Bind((long long)page_size));
                p.push_back(SqliteDb::Bind((long long)((page - 1) * page_size)));
                return p;
            }());

        json items = json::array();
        for (const auto& row : rows) items.push_back(card_row_to_json(row));
        json response = {{"code", 200},
                         {"data", {{"items", items}, {"total", total}, {"page", page}, {"page_size", page_size}}}};
        res.set_content(response.dump(), "application/json");
    });

    // 3. 批次汇总。管理员看全部批次；教师看本人签发批次。
    svr.Get("/api/point-cards/batches", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(req, res);
        if (!check_permission_middleware(req, res, "points:manage")) return;

        std::string session_id = get_cookie_value(req, "sid");
        std::string user_id = verify_session(session_id);
        bool is_admin = is_admin_session(user_id);

        std::string sql =
            "SELECT c.batch_no, c.face_value, c.remark, c.issuer_id, MIN(c.created_at) AS created_at, "
            "COUNT(*) AS total, "
            "SUM(CASE WHEN c.status='unused' THEN 1 ELSE 0 END) AS unused_cnt, "
            "SUM(CASE WHEN c.status='pending' THEN 1 ELSE 0 END) AS pending_cnt, "
            "SUM(CASE WHEN c.status='used' THEN 1 ELSE 0 END) AS used_cnt, "
            "SUM(CASE WHEN c.status='void' THEN 1 ELSE 0 END) AS void_cnt, "
            "iu.name AS issuer_name "
            "FROM point_cards c LEFT JOIN users iu ON c.issuer_id = iu.id ";
        if (!is_admin) {
            sql += "WHERE c.issuer_id = ? ";
        }
        sql += "GROUP BY c.batch_no, c.face_value, c.remark, c.issuer_id, iu.name ORDER BY c.batch_no DESC LIMIT 100";

        json rows = is_admin ? db.query(sql)
                             : db.query_bind(sql, {SqliteDb::Bind(user_id)});
        json items = json::array();
        for (const auto& row : rows) {
            json item;
            item["batch_no"] = row.value("batch_no", "");
            item["face_value"] = row.value("face_value", 0);
            item["remark"] = str_or_empty(row, "remark");
            item["issuer_id"] = str_or_empty(row, "issuer_id");
            item["issuer_name"] = row.contains("issuer_name") && !row["issuer_name"].is_null()
                                  ? row["issuer_name"].get<std::string>() : "";
            item["created_at"] = row.value("created_at", "");
            item["total"] = row.value("total", 0);
            item["unused_cnt"] = row.value("unused_cnt", 0);
            item["pending_cnt"] = row.value("pending_cnt", 0);
            item["used_cnt"] = row.value("used_cnt", 0);
            item["void_cnt"] = row.value("void_cnt", 0);
            items.push_back(item);
        }
        json response = {{"code", 200}, {"data", items}};
        res.set_content(response.dump(), "application/json");
    });

    // 4. 待确认列表（通用卡 pending）。
    //    教师可见：① 本人签发；② 管理员签发、且待确认学生在该教师任课班级。
    //    管理员可见：全部。
    svr.Get("/api/point-cards/pending", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(req, res);
        if (!check_permission_middleware(req, res, "points:manage")) return;

        std::string session_id = get_cookie_value(req, "sid");
        std::string user_id = verify_session(session_id);
        bool is_admin = is_admin_session(user_id);

        // 教师专属子查询：该教师任课班级的学生 id 集合
        // （teacher_classes JOIN classes JOIN users，与 models.cpp 归属判定同链路）
        json rows;
        if (is_admin) {
            rows = db.query(
                "SELECT c.id, c.code, c.face_value, c.status, c.batch_no, c.remark, c.issuer_id, "
                "c.bound_student_id, c.pending_student_id, c.used_by, c.used_at, c.confirmed_by, c.created_at, "
                "bu.name AS bound_name, pu.name AS pending_name, iu.name AS issuer_name "
                "FROM point_cards c "
                "LEFT JOIN users bu ON c.bound_student_id = bu.id "
                "LEFT JOIN users pu ON c.pending_student_id = pu.id "
                "LEFT JOIN users iu ON c.issuer_id = iu.id "
                "WHERE c.status = 'pending' ORDER BY c.id DESC LIMIT 200");
        } else {
            rows = db.query_bind(
                "SELECT c.id, c.code, c.face_value, c.status, c.batch_no, c.remark, c.issuer_id, "
                "c.bound_student_id, c.pending_student_id, c.used_by, c.used_at, c.confirmed_by, c.created_at, "
                "bu.name AS bound_name, pu.name AS pending_name, iu.name AS issuer_name "
                "FROM point_cards c "
                "LEFT JOIN users bu ON c.bound_student_id = bu.id "
                "LEFT JOIN users pu ON c.pending_student_id = pu.id "
                "LEFT JOIN users iu ON c.issuer_id = iu.id "
                "WHERE c.status = 'pending' AND ("
                "  c.issuer_id = ? OR ("
                "    EXISTS (SELECT 1 FROM roles r WHERE r.id = 1 AND c.issuer_id IN (SELECT id FROM users WHERE role_id = 1))"
                "    AND EXISTS (SELECT 1 FROM teacher_classes tc JOIN classes cl ON tc.class_id = cl.id "
                "                WHERE tc.teacher_id = ? AND cl.name = "
                "                  (SELECT className FROM users WHERE id = c.pending_student_id))"
                "  )"
                ") ORDER BY c.id DESC LIMIT 200",
                {SqliteDb::Bind(user_id), SqliteDb::Bind(user_id)});
        }

        json items = json::array();
        for (const auto& row : rows) items.push_back(card_row_to_json(row));
        json response = {{"code", 200}, {"data", items}};
        res.set_content(response.dump(), "application/json");
    });

    // 5. 确认待确认卡到账（pending → used，加分 + 积分记录，同一事务）
    svr.Post("/api/point-cards/confirm", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(req, res);
        if (!check_permission_middleware(req, res, "points:manage")) return;
        if (!require_csrf(req, res)) return;

        std::string session_id = get_cookie_value(req, "sid");
        std::string user_id = verify_session(session_id);
        bool is_admin = is_admin_session(user_id);

        try {
            auto body = json::parse(req.body);
            long long card_id = body.value("id", 0LL);
            if (card_id <= 0) {
                res.status = 400;
                res.set_content(json{{"code", 400}, {"msg", "参数错误"}}.dump(), "application/json");
                return;
            }

            // 取卡（锁前读；状态流转靠下方条件 UPDATE 保证原子）
            json cards = db.query_bind(
                "SELECT id, code, face_value, status, issuer_id, pending_student_id FROM point_cards WHERE id = ?",
                {SqliteDb::Bind(card_id)});
            if (cards.empty()) {
                res.status = 404;
                res.set_content(json{{"code", 404}, {"msg", "积分卡不存在"}}.dump(), "application/json");
                return;
            }
            const auto& card = cards[0];
            std::string status = card.value("status", "");
            std::string issuer = str_or_empty(card, "issuer_id");
            std::string student_id = str_or_empty(card, "pending_student_id");
            int face_value = card.value("face_value", 0);

            if (status != "pending") {
                res.status = 400;
                res.set_content(json{{"code", 400},
                    {"msg", std::string("该卡当前状态为「") + card_status_text(status) + "」，无法确认"}}.dump(),
                    "application/json");
                return;
            }
            if (student_id.empty()) {
                res.status = 400;
                res.set_content(json{{"code", 400}, {"msg", "该卡没有待确认的兑换请求"}}.dump(), "application/json");
                return;
            }

            // 授权：管理员任意；教师须「本人签发且学生任课」或「管理员签发且学生任课」
            if (!is_admin) {
                bool owns_student = teacher_owns_student(user_id, student_id);
                bool mine = (issuer == user_id);
                bool admin_issued = false;
                json ir = db.query_bind("SELECT role_id FROM users WHERE id = ?",
                                        {SqliteDb::Bind(issuer)});
                if (!ir.empty()) {
                    admin_issued = ir[0].value("role_id", 0) == 1;
                }
                if (owns_student && !(mine || admin_issued)) {
                    res.status = 403;
                    res.set_content(json{{"code", 403}, {"msg", "该卡由其他教师签发，且学生不在可代确认范围"}}.dump(), "application/json");
                    return;
                }
                if (!mine && !(admin_issued && owns_student)) {
                    res.status = 403;
                    res.set_content(json{{"code", 403}, {"msg", "无权确认该卡（仅签发教师或学生任课教师可确认管理员签发的卡）"}}.dump(), "application/json");
                    return;
                }
                // 本人签发但学生已不在任课班级：仍允许（签发人对自己发的卡负责）
            }

            // 事务：条件置 used + 加分 + 积分记录
            SqliteDb::Transaction txn(db);
            if (!txn.active()) {
                res.status = 500;
                res.set_content(json{{"code", 500}, {"msg", "服务暂时不可用，请稍后重试"}}.dump(), "application/json");
                return;
            }

            int affected = 0;
            int rc = db.execute_bind_affected(
                "UPDATE point_cards SET status = 'used', used_by = ?, used_at = ?, confirmed_by = ? "
                "WHERE id = ? AND status = 'pending'",
                {SqliteDb::Bind(student_id), SqliteDb::Bind(get_current_time()),
                 SqliteDb::Bind(user_id), SqliteDb::Bind(card_id)}, affected);
            if (rc != 0 || affected != 1) {
                res.status = 400;
                res.set_content(json{{"code", 400}, {"msg", "确认失败：该卡状态已变化，请刷新"}}.dump(), "application/json");
                return;   // RAII 回滚
            }

            // 加分（DB 先写，成功后改内存）
            std::string student_name;
            int points_after = 0;
            bool add_ok = false;
            bool stu_not_found = false;
            with_user_record(student_id, [&](User& u) -> bool {
                if (u.role_id != 3) return false;
                student_name = u.name;
                int next = u.points + face_value;
                if (!update_user_points_in_db(u.id, next)) return false;
                u.points = next;
                points_after = next;
                add_ok = true;
                return true;
            }, &stu_not_found);
            if (stu_not_found) {
                res.status = 404;
                res.set_content(json{{"code", 404}, {"msg", "待确认学生已不存在"}}.dump(), "application/json");
                return;   // 回滚
            }
            if (!add_ok) {
                res.status = 500;
                res.set_content(json{{"code", 500}, {"msg", "积分入账失败，请重试"}}.dump(), "application/json");
                return;   // 回滚
            }

            // 积分记录（reason 带卡号后 4 段便于对账）
            std::string code = card.value("code", "");
            std::string reason = "积分卡充值 " + code;
            int rec_affected = 0;
            db.execute_bind_affected(
                "INSERT INTO points_records (student_id, points, reason, operator_id, created_at) "
                "VALUES (?, ?, ?, ?, ?)",
                {SqliteDb::Bind(student_id), SqliteDb::Bind((long long)face_value),
                 SqliteDb::Bind(reason), SqliteDb::Bind(user_id), SqliteDb::Bind(get_current_time())},
                rec_affected);
            if (rec_affected == 1) {
                // 内存积分记录同步（供统计/导出读取）
                PointsRecord pr{0, student_id, face_value, reason, user_id, get_current_time()};
                pr.id = db.last_insert_rowid();
                std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
                points_records.push_back(pr);
            }

            if (!txn.commit()) {
                res.status = 500;
                res.set_content(json{{"code", 500}, {"msg", "确认提交失败，请重试"}}.dump(), "application/json");
                return;
            }
            Logger::info("积分卡确认到账：card=" + code + " student=" + student_id +
                         " +" + std::to_string(face_value) + " 确认人=" + user_id);
            json response = {{"code", 200}, {"msg", "已确认到账 " + std::to_string(face_value) + " 积分"},
                             {"data", {{"student_name", student_name}, {"points", points_after}}}};
            res.set_content(response.dump(), "application/json");
        } catch (json::parse_error& e) {
            res.status = 400;
            res.set_content(json{{"code", 400}, {"msg", "请求数据格式错误"}}.dump(), "application/json");
        }
    });

    // 6. 驳回待确认卡（pending → unused，学生可再次提交或改由他人兑换）
    svr.Post("/api/point-cards/reject", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(req, res);
        if (!check_permission_middleware(req, res, "points:manage")) return;
        if (!require_csrf(req, res)) return;

        std::string session_id = get_cookie_value(req, "sid");
        std::string user_id = verify_session(session_id);
        bool is_admin = is_admin_session(user_id);

        try {
            auto body = json::parse(req.body);
            long long card_id = body.value("id", 0LL);
            json cards = db.query_bind(
                "SELECT issuer_id, status, pending_student_id FROM point_cards WHERE id = ?",
                {SqliteDb::Bind(card_id)});
            if (cards.empty()) {
                res.status = 404;
                res.set_content(json{{"code", 404}, {"msg", "积分卡不存在"}}.dump(), "application/json");
                return;
            }
            std::string issuer = str_or_empty(cards[0], "issuer_id");
            std::string status = cards[0].value("status", "");
            std::string pending_student = str_or_empty(cards[0], "pending_student_id");

            if (status != "pending") {
                res.status = 400;
                res.set_content(json{{"code", 400},
                    {"msg", std::string("该卡当前状态为「") + card_status_text(status) + "」，无法驳回"}}.dump(),
                    "application/json");
                return;
            }
            // 授权与确认（/confirm）保持同一规则：
            //   管理员任意；教师须「本人签发」或「管理员签发且待确认学生在其任课班级」。
            // （与待确认列表的可见性一致：能看到的卡就能驳回，避免「能确认不能驳回」的规则裂缝。）
            if (!is_admin && issuer != user_id) {
                bool admin_issued = false;
                json ir = db.query_bind("SELECT role_id FROM users WHERE id = ?",
                                        {SqliteDb::Bind(issuer)});
                if (!ir.empty()) admin_issued = ir[0].value("role_id", 0) == 1;
                if (!(admin_issued && !pending_student.empty()
                      && teacher_owns_student(user_id, pending_student))) {
                    res.status = 403;
                    res.set_content(json{{"code", 403},
                        {"msg", "仅签发教师、学生任课教师或管理员可驳回"}}.dump(), "application/json");
                    return;
                }
            }

            int affected = 0;
            int rc = db.execute_bind_affected(
                "UPDATE point_cards SET status = 'unused', pending_student_id = NULL "
                "WHERE id = ? AND status = 'pending'",
                {SqliteDb::Bind(card_id)}, affected);
            if (rc != 0 || affected != 1) {
                res.status = 400;
                res.set_content(json{{"code", 400}, {"msg", "驳回失败：该卡状态已变化"}}.dump(), "application/json");
                return;
            }
            Logger::info("积分卡驳回：card_id=" + std::to_string(card_id) + " 原提交学生=" + pending_student);
            json response = {{"code", 200}, {"msg", "已驳回，卡恢复为未使用"}};
            res.set_content(response.dump(), "application/json");
        } catch (json::parse_error& e) {
            res.status = 400;
            res.set_content(json{{"code", 400}, {"msg", "请求数据格式错误"}}.dump(), "application/json");
        }
    });

    // 7. 作废未使用卡（unused → void）
    svr.Post("/api/point-cards/void", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(req, res);
        if (!check_permission_middleware(req, res, "points:manage")) return;
        if (!require_csrf(req, res)) return;

        std::string session_id = get_cookie_value(req, "sid");
        std::string user_id = verify_session(session_id);
        bool is_admin = is_admin_session(user_id);

        try {
            auto body = json::parse(req.body);
            long long card_id = body.value("id", 0LL);
            json cards = db.query_bind(
                "SELECT issuer_id, status FROM point_cards WHERE id = ?",
                {SqliteDb::Bind(card_id)});
            if (cards.empty()) {
                res.status = 404;
                res.set_content(json{{"code", 404}, {"msg", "积分卡不存在"}}.dump(), "application/json");
                return;
            }
            std::string issuer = str_or_empty(cards[0], "issuer_id");
            std::string status = cards[0].value("status", "");
            if (status == "pending") {
                res.status = 400;
                res.set_content(json{{"code", 400}, {"msg", "该卡有待确认的兑换请求，请先驳回再作废"}}.dump(), "application/json");
                return;
            }
            if (status != "unused") {
                res.status = 400;
                res.set_content(json{{"code", 400},
                    {"msg", std::string("该卡当前状态为「") + card_status_text(status) + "」，仅未使用的卡可作废"}}.dump(),
                    "application/json");
                return;
            }
            if (!is_admin && issuer != user_id) {
                res.status = 403;
                res.set_content(json{{"code", 403}, {"msg", "仅签发教师或管理员可作废"}}.dump(), "application/json");
                return;
            }

            int affected = 0;
            int rc = db.execute_bind_affected(
                "UPDATE point_cards SET status = 'void' WHERE id = ? AND status = 'unused'",
                {SqliteDb::Bind(card_id)}, affected);
            if (rc != 0 || affected != 1) {
                res.status = 400;
                res.set_content(json{{"code", 400}, {"msg", "作废失败：该卡状态已变化"}}.dump(), "application/json");
                return;
            }
            Logger::info("积分卡作废：card_id=" + std::to_string(card_id) + " 操作人=" + user_id);
            json response = {{"code", 200}, {"msg", "已作废"}};
            res.set_content(response.dump(), "application/json");
        } catch (json::parse_error& e) {
            res.status = 400;
            res.set_content(json{{"code", 400}, {"msg", "请求数据格式错误"}}.dump(), "application/json");
        }
    });

    // 8. 重新激活已使用卡（used → unused；可选解除绑定 → 变为通用卡）
    //    用途：实物卡回收后二次发放。
    svr.Post("/api/point-cards/reactivate", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(req, res);
        if (!check_permission_middleware(req, res, "points:manage")) return;
        if (!require_csrf(req, res)) return;

        std::string session_id = get_cookie_value(req, "sid");
        std::string user_id = verify_session(session_id);
        bool is_admin = is_admin_session(user_id);

        try {
            auto body = json::parse(req.body);
            long long card_id = body.value("id", 0LL);
            bool unbind = body.value("unbind", false);
            json cards = db.query_bind(
                "SELECT issuer_id, status FROM point_cards WHERE id = ?",
                {SqliteDb::Bind(card_id)});
            if (cards.empty()) {
                res.status = 404;
                res.set_content(json{{"code", 404}, {"msg", "积分卡不存在"}}.dump(), "application/json");
                return;
            }
            std::string issuer = str_or_empty(cards[0], "issuer_id");
            std::string status = cards[0].value("status", "");
            if (status != "used") {
                res.status = 400;
                res.set_content(json{{"code", 400},
                    {"msg", std::string("该卡当前状态为「") + card_status_text(status) + "」，仅已使用的卡可重新激活"}}.dump(),
                    "application/json");
                return;
            }
            if (!is_admin && issuer != user_id) {
                res.status = 403;
                res.set_content(json{{"code", 403}, {"msg", "仅签发教师或管理员可重新激活"}}.dump(), "application/json");
                return;
            }

            int affected = 0;
            int rc = db.execute_bind_affected(
                unbind
                    ? "UPDATE point_cards SET status = 'unused', bound_student_id = NULL, pending_student_id = NULL, "
                      "used_by = NULL, used_at = NULL, confirmed_by = NULL, reactivated_by = ?, reactivated_at = ? "
                      "WHERE id = ? AND status = 'used'"
                    : "UPDATE point_cards SET status = 'unused', pending_student_id = NULL, "
                      "used_by = NULL, used_at = NULL, confirmed_by = NULL, reactivated_by = ?, reactivated_at = ? "
                      "WHERE id = ? AND status = 'used'",
                {SqliteDb::Bind(user_id), SqliteDb::Bind(get_current_time()), SqliteDb::Bind(card_id)}, affected);
            if (rc != 0 || affected != 1) {
                res.status = 400;
                res.set_content(json{{"code", 400}, {"msg", "激活失败：该卡状态已变化"}}.dump(), "application/json");
                return;
            }
            Logger::info("积分卡重新激活：card_id=" + std::to_string(card_id) +
                         (unbind ? "（已解除绑定）" : "（保留绑定）") + " 操作人=" + user_id);
            json response = {{"code", 200},
                             {"msg", unbind ? "已重新激活并解除绑定（转为通用卡）" : "已重新激活（绑定关系保留）"}};
            res.set_content(response.dump(), "application/json");
        } catch (json::parse_error& e) {
            res.status = 400;
            res.set_content(json{{"code", 400}, {"msg", "请求数据格式错误"}}.dump(), "application/json");
        }
    });

    // =========================================================================
    // 学生端（mall:manage）
    // =========================================================================

    // 9. 学生兑换积分卡
    //    定向卡：校验本人 → 事务内即时到账（unused → used + 加分 + 积分记录）；
    //    通用卡：unused → pending（等待教师/管理员确认）。
    svr.Post("/api/student/point-cards/redeem", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(req, res);
        if (!check_permission_middleware(req, res, "mall:manage")) return;
        if (!require_csrf(req, res)) return;

        std::string session_id = get_cookie_value(req, "sid");
        std::string user_id = verify_session(session_id);

        try {
            auto body = json::parse(req.body);
            std::string code = normalize_card_code(body.value("code", ""));
            if (code.empty()) {
                res.status = 400;
                res.set_content(json{{"code", 400}, {"msg", "卡密格式错误（应为 XXXX-XXXX-XXXX-XXXX）"}}.dump(), "application/json");
                return;
            }

            json cards = db.query_bind(
                "SELECT id, code, face_value, status, bound_student_id FROM point_cards WHERE code = ?",
                {SqliteDb::Bind(code)});
            if (cards.empty()) {
                res.status = 404;
                res.set_content(json{{"code", 404}, {"msg", "卡密不存在，请核对后重试"}}.dump(), "application/json");
                return;
            }
            const auto& card = cards[0];
            std::string status = card.value("status", "");
            // 注意：bound_student_id 可为 NULL；value() 遇 null 与 string 默认值类型不符会抛
            // type_error（与 routes_public.cpp:328 image_url 同坑），必须先判 is_null。
            std::string bound = (card.contains("bound_student_id") && !card["bound_student_id"].is_null())
                                ? card["bound_student_id"].get<std::string>() : "";
            long long card_id = card.value("id", 0LL);
            int face_value = card.value("face_value", 0);

            if (status == "used") {
                res.status = 400;
                res.set_content(json{{"code", 400}, {"msg", "该卡已被使用"}}.dump(), "application/json");
                return;
            }
            if (status == "void") {
                res.status = 400;
                res.set_content(json{{"code", 400}, {"msg", "该卡已作废"}}.dump(), "application/json");
                return;
            }
            if (status == "pending") {
                res.status = 400;
                res.set_content(json{{"code", 400}, {"msg", "该卡已提交，等待老师确认"}}.dump(), "application/json");
                return;
            }
            if (status != "unused") {
                res.status = 400;
                res.set_content(json{{"code", 400}, {"msg", "该卡当前无法兑换"}}.dump(), "application/json");
                return;
            }

            // ---- 定向卡：仅绑定学生本人，即时到账 ----
            if (!bound.empty()) {
                if (bound != user_id) {
                    res.status = 403;
                    res.set_content(json{{"code", 403}, {"msg", "该卡已绑定其他同学，无法兑换"}}.dump(), "application/json");
                    return;
                }

                SqliteDb::Transaction txn(db);
                if (!txn.active()) {
                    res.status = 500;
                    res.set_content(json{{"code", 500}, {"msg", "服务暂时不可用，请稍后重试"}}.dump(), "application/json");
                    return;
                }
                int affected = 0;
                int rc = db.execute_bind_affected(
                    "UPDATE point_cards SET status = 'used', used_by = ?, used_at = ? "
                    "WHERE id = ? AND status = 'unused'",
                    {SqliteDb::Bind(user_id), SqliteDb::Bind(get_current_time()), SqliteDb::Bind(card_id)}, affected);
                if (rc != 0 || affected != 1) {
                    res.status = 400;
                    res.set_content(json{{"code", 400}, {"msg", "兑换失败：该卡状态已变化，请重试"}}.dump(), "application/json");
                    return;
                }

                int points_after = 0;
                bool add_ok = false;
                with_user_record(user_id, [&](User& u) -> bool {
                    if (u.role_id != 3) return false;
                    int next = u.points + face_value;
                    if (!update_user_points_in_db(u.id, next)) return false;
                    u.points = next;
                    points_after = next;
                    add_ok = true;
                    return true;
                });
                if (!add_ok) {
                    res.status = 500;
                    res.set_content(json{{"code", 500}, {"msg", "积分入账失败，请重试"}}.dump(), "application/json");
                    return;   // 回滚
                }

                std::string reason = "积分卡充值 " + code;
                int rec_affected = 0;
                db.execute_bind_affected(
                    "INSERT INTO points_records (student_id, points, reason, operator_id, created_at) "
                    "VALUES (?, ?, ?, ?, ?)",
                    {SqliteDb::Bind(user_id), SqliteDb::Bind((long long)face_value),
                     SqliteDb::Bind(reason), SqliteDb::Bind(user_id), SqliteDb::Bind(get_current_time())},
                    rec_affected);
                if (rec_affected == 1) {
                    PointsRecord pr{0, user_id, face_value, reason, user_id, get_current_time()};
                    pr.id = db.last_insert_rowid();
                    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
                    points_records.push_back(pr);
                }

                if (!txn.commit()) {
                    res.status = 500;
                    res.set_content(json{{"code", 500}, {"msg", "兑换提交失败，请重试"}}.dump(), "application/json");
                    return;
                }
                Logger::info("积分卡定向兑换到账：card=" + code + " student=" + user_id +
                             " +" + std::to_string(face_value));
                json response = {{"code", 200}, {"msg", "充值成功！"},
                                 {"data", {{"result", "credited"},
                                           {"face_value", face_value},
                                           {"points", points_after}}}};
                res.set_content(response.dump(), "application/json");
                return;
            }

            // ---- 通用卡：置 pending，等待确认 ----
            int affected = 0;
            int rc = db.execute_bind_affected(
                "UPDATE point_cards SET status = 'pending', pending_student_id = ? "
                "WHERE id = ? AND status = 'unused'",
                {SqliteDb::Bind(user_id), SqliteDb::Bind(card_id)}, affected);
            if (rc != 0 || affected != 1) {
                res.status = 400;
                res.set_content(json{{"code", 400}, {"msg", "提交失败：该卡状态已变化，请重试"}}.dump(), "application/json");
                return;
            }
            Logger::info("积分卡通用卡提交待确认：card=" + code + " student=" + user_id);
            json response = {{"code", 200}, {"msg", "已提交，等待老师确认后到账"},
                             {"data", {{"result", "pending"}, {"face_value", face_value}}}};
            res.set_content(response.dump(), "application/json");
        } catch (json::parse_error& e) {
            res.status = 400;
            res.set_content(json{{"code", 400}, {"msg", "请求数据格式错误"}}.dump(), "application/json");
        }
    });

    // 10. 学生本人充值记录（提交过的卡：pending / used；卡号中间掩码）
    svr.Get("/api/student/point-cards", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(req, res);
        if (!check_permission_middleware(req, res, "mall:manage")) return;

        std::string session_id = get_cookie_value(req, "sid");
        std::string user_id = verify_session(session_id);

        json rows = db.query_bind(
            "SELECT id, code, face_value, status, used_at, created_at FROM point_cards "
            "WHERE pending_student_id = ? OR used_by = ? OR bound_student_id = ? "
            "ORDER BY id DESC LIMIT 100",
            {SqliteDb::Bind(user_id), SqliteDb::Bind(user_id), SqliteDb::Bind(user_id)});

        json items = json::array();
        for (const auto& row : rows) {
            std::string code = str_or_empty(row, "code");
            // 掩码：XXXX-****-****-XXXX
            if (code.size() == 19) code = code.substr(0, 9) + "****-****" + code.substr(17, 2);
            std::string status = str_or_empty(row, "status");
            // 绑定但未使用的卡不算充值记录（还没发生兑换动作）
            if (status == "unused") {
                bool bound_to_me = false;
                json chk = db.query_bind("SELECT bound_student_id FROM point_cards WHERE id = ?",
                                         {SqliteDb::Bind(row.value("id", 0LL))});
                if (!chk.empty()) bound_to_me = (str_or_empty(chk[0], "bound_student_id") == user_id);
                if (bound_to_me) {
                    items.push_back({{"id", row.value("id", 0LL)},
                                     {"code", code},
                                     {"face_value", row.value("face_value", 0)},
                                     {"status", "ready"},
                                     {"status_text", "待兑换（已绑定你）"},
                                     {"time", str_or_empty(row, "created_at")}});
                }
                continue;
            }
            // used_at 可为 NULL（pending 状态），取不到时回落到 created_at
            std::string used_at = str_or_empty(row, "used_at");
            if (used_at.empty()) used_at = str_or_empty(row, "created_at");
            items.push_back({{"id", row.value("id", 0LL)},
                             {"code", code},
                             {"face_value", row.value("face_value", 0)},
                             {"status", status},
                             {"status_text", card_status_text(status)},
                             {"time", used_at}});
        }
        json response = {{"code", 200}, {"data", items}};
        res.set_content(response.dump(), "application/json");
    });
}
