// main.cpp - 服务器入口文件
// 重构后的精简版本，所有模块拆分到独立头文件和源文件中
#include "config.h"
#include "logger.h"
#include "auth.h"
#include "models.h"
#include "routes.h"
#include "httplib.h"

#include <cstdio>
#include <set>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

// 全局配置
ServerConfig g_config;

// 全局数据库实例
SqliteDb db;

// 检查 users 表是否为旧 schema（id 为 INTEGER AUTOINCREMENT）
// 如果是旧 schema，返回 true；否则返回 false
static bool users_table_is_old_schema() {
    if (!db.isOpen()) return false;
    json result = db.query("PRAGMA table_info(users)");
    if (result.empty()) return false; // 表不存在，不算旧 schema
    for (const auto& col : result) {
        std::string name = col.value("name", "");
        if (name == "id") {
            std::string type = col.value("type", "");
            // 旧 schema 的 id 列类型为 INTEGER；新 schema 为 TEXT
            if (type == "INTEGER") {
                return true;
            }
            return false;
        }
    }
    return false;
}

// ===========================================================================
// 安全修复 V18（B2）：首启随机口令 + 强制首登改密
// ===========================================================================

// users 表是否已有指定列（PRAGMA table_info 探测）
static bool users_has_column(const std::string& column) {
    if (!db.isOpen()) return false;
    json result = db.query("PRAGMA table_info(users)");
    for (const auto& col : result) {
        if (col.value("name", "") == column) return true;
    }
    return false;
}

// 旧库兼容迁移：为 users 增加 must_change_password 列。
// 幂等（列已存在则不动），**不删库、不丢数据** —— 与 users_table_is_old_schema() 那条
// 只针对史前 id 类型变更的破坏性重建路径不同。存量用户默认 0（不强制改密），
// 只有首启新建的种子账号才置 1。
static void migrate_users_must_change_password() {
    if (!db.isOpen()) return;

    json has_users_table = db.query("SELECT name FROM sqlite_master WHERE type='table' AND name='users'");
    if (has_users_table.empty()) return;
    if (users_has_column("must_change_password")) return;

    Logger::warning("检测到旧库缺少 users.must_change_password 列，执行 ALTER TABLE 兼容迁移（不删库）");
    if (db.execute("ALTER TABLE users ADD COLUMN must_change_password INTEGER NOT NULL DEFAULT 0")) {
        Logger::info("迁移完成：users.must_change_password 已添加，存量用户默认 0");
    } else {
        Logger::error("迁移失败：ALTER TABLE users ADD COLUMN must_change_password");
    }
}

// 首启（users 表为空）创建 4 个种子账号：
//   * 口令由 sha256.h 的 CSPRNG（generate_random_password）生成，两两不同；
//   * 用 hash_password() 哈希后**参数化**写入（SQL 内不再出现任何口令哈希）；
//   * 全部标记 must_change_password = 1（首登必须改密）；
//   * 明文口令**只输出到标准输出一次**，不写入任何文件（Logger 会落盘 server.log，
//     故刻意不走 Logger），也不会重复输出（仅在 users 表为空时触发）。
static void seed_default_users_if_empty() {
    if (!db.isOpen()) return;

    json cnt = db.query("SELECT COUNT(*) AS c FROM users");
    int existing = cnt.empty() ? 0 : cnt[0].value("c", 0);
    if (existing > 0) return;

    struct SeedSpec {
        const char* id;
        const char* username;
        int role_id;
        const char* name;
        const char* className;
        int points;
    };
    const SeedSpec specs[4] = {
        {"admin-01",         "admin",   1, "管理员",     "系统管理",   0},
        {"teacher-001",      "teacher", 2, "王老师",     "高二(1)班",  0},
        {"student-02-01-01", "student", 3, "张同学",     "高二(1)班",  150},
        {"parent-001",       "parent",  4, "张同学家长", "",           0},
    };

    // 两两不同的随机强口令（碰撞则重试；实际碰撞概率可忽略，但仍显式保证）
    std::string passwords[4];
    for (int i = 0; i < 4; i++) {
        std::string pwd = generate_random_password();
        int guard = 0;
        bool dup = true;
        while (dup && guard++ < 100) {
            dup = false;
            for (int j = 0; j < i; j++) {
                if (passwords[j] == pwd) { dup = true; break; }
            }
            if (dup) pwd = generate_random_password();
        }
        passwords[i] = pwd;
    }

    bool all_ok = true;
    for (int i = 0; i < 4; i++) {
        bool ok = db.execute_bind(
            "INSERT INTO users (id, username, password_hash, role_id, name, className, points, must_change_password) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
            {SqliteDb::Bind(specs[i].id),
             SqliteDb::Bind(specs[i].username),
             SqliteDb::Bind(hash_password(passwords[i])),
             SqliteDb::Bind((long long)specs[i].role_id),
             SqliteDb::Bind(specs[i].name),
             SqliteDb::Bind(specs[i].className),
             SqliteDb::Bind((long long)specs[i].points),
             SqliteDb::Bind((long long)1)});
        if (!ok) {
            all_ok = false;
            Logger::error(std::string("种子账号写入失败: ") + specs[i].username);
        }
    }
    if (!all_ok) return;

    Logger::info("首启检测：users 表为空，已创建 4 个种子账号（随机强口令 + 强制首登改密）");

    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  首次启动：已为下列账号生成随机初始口令（仅本次显示，请立即保存）\n";
    std::cout << "  这些账号首次登录后必须先修改口令，否则无法调用其它接口。\n";
    std::cout << "----------------------------------------------------------------\n";
    for (int i = 0; i < 4; i++) {
        std::cout << "  " << specs[i].username << "    " << passwords[i] << "\n";
    }
    std::cout << "================================================================\n" << std::endl;
}

// 信息性告警：统计仍在用旧版无盐 SHA256 哈希的存量用户。
// 按 T1 决策①，存量记录不在此处做破坏性改写，而是交付运维强制重置流程。
static void warn_legacy_weak_hashes() {
    int legacy = 0;
    for (const auto& u : users) {
        if (u.password_hash.compare(0, 7, "pbkdf2$") != 0) legacy++;
    }
    if (legacy > 0) {
        Logger::warning("检测到 " + std::to_string(legacy) +
                        " 个用户仍使用旧版无盐 SHA256 哈希（legacy 分支仅按 sha256(password)==hash 判定，"
                        "且自 V18 起不再被登录流程自动改写 —— 自动升级链已按决策①删除）");
        // 队长补充要求②：重置清单必须**同时覆盖**裸 SHA256 行与 pbkdf2$ 行。
        // 背景：reviewer 建议批次 1 直接删掉 legacy 分支；一旦删掉，只重置 pbkdf2$ 记录是不够的 ——
        // 裸 SHA256 历史行会变成「既不能登录、也没被重置」的死账号。
        Logger::warning("运维提醒：强制改密/存量重置清单必须同时覆盖**裸 SHA256 行**与 **pbkdf2$ 行**，"
                        "不能只点名 pbkdf2$；否则裸 SHA256 行将成为既不能登录也未被重置的死账号。"
                        "可用：UPDATE users SET must_change_password=1 WHERE password_hash NOT LIKE 'pbkdf2%';");
    }
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // 1. 加载配置文件
    g_config = load_config("config.json");

    // 2. 初始化日志系统
    Logger::init(g_config.log_file, g_config.log_max_size, g_config.log_max_files);
    Logger::info("日志系统初始化完成");
    Logger::info("配置加载成功: 端口=" + std::to_string(g_config.port) + ", 数据库=" + g_config.db_path);

    // 3. 初始化数据库
    // 先打开一次数据库，检查是否为旧 schema（id 为 INTEGER AUTOINCREMENT）
    // 如果是，关闭数据库、删除文件、重新打开，使新 schema 生效
    if (db.open(g_config.db_path)) {
        Logger::info("SQLite 数据库连接成功");

        if (users_table_is_old_schema()) {
            Logger::warning("检测到旧 schema（users.id 为 INTEGER），删除数据库文件并重建为新 schema (TEXT 主键)");
            db.close();
            if (std::remove(g_config.db_path.c_str()) != 0) {
                Logger::error("删除旧数据库文件失败: " + g_config.db_path);
            }
            if (!db.open(g_config.db_path)) {
                Logger::error("重新打开数据库失败");
            }
        }
    } else {
        // 安全修复 V18（B2）：磁盘库打开失败**不再静默降级为内存库**。
        // 旧行为（Logger::warning 后继续用内存存储）会带来三重风险：
        //   1) 服务在「所有口令修改/用户增删都不会落盘」的状态下继续对外服务；
        //   2) models.cpp 的内存表内曾带着 4 个可预测的默认账号（本次一并移除）；
        //   3) 故障完全静默，运维与攻击者都难以察觉。
        // 现改为 fail-fast：由运维修复 db_path 权限/磁盘问题后重启。
        Logger::error("SQLite 数据库连接失败，拒绝以内存模式启动（安全修复 V18）：" + g_config.db_path);
        std::cerr << "[FATAL] 无法打开数据库: " << g_config.db_path
                  << "\n        已按安全修复 V18 拒绝内存兜底启动（退出码 1），请检查路径/权限/磁盘后重启。"
                  << std::endl;
        return 1;
    }

    if (db.isOpen()) {
        string init_sql = R"(
            CREATE TABLE IF NOT EXISTS users (
                id TEXT PRIMARY KEY,
                username TEXT UNIQUE NOT NULL,
                password_hash TEXT NOT NULL,
                role_id INTEGER NOT NULL,
                name TEXT NOT NULL,
                className TEXT,
                points INTEGER DEFAULT 0,
                must_change_password INTEGER NOT NULL DEFAULT 0,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
            );

            CREATE TABLE IF NOT EXISTS roles (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT UNIQUE NOT NULL,
                description TEXT
            );

            CREATE TABLE IF NOT EXISTS permissions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                code TEXT UNIQUE NOT NULL,
                description TEXT
            );

            CREATE TABLE IF NOT EXISTS role_permissions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                role_id INTEGER NOT NULL,
                permission_id INTEGER NOT NULL,
                FOREIGN KEY (role_id) REFERENCES roles(id),
                FOREIGN KEY (permission_id) REFERENCES permissions(id),
                UNIQUE(role_id, permission_id)
            );

            CREATE TABLE IF NOT EXISTS points_records (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                student_id TEXT NOT NULL,
                points INTEGER NOT NULL,
                reason TEXT,
                operator_id TEXT NOT NULL,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (student_id) REFERENCES users(id),
                FOREIGN KEY (operator_id) REFERENCES users(id)
            );

            CREATE TABLE IF NOT EXISTS evaluations (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                student_id TEXT NOT NULL,
                dimension_id INTEGER NOT NULL,
                score INTEGER NOT NULL,
                comment TEXT,
                evaluator_id TEXT NOT NULL,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (student_id) REFERENCES users(id),
                FOREIGN KEY (evaluator_id) REFERENCES users(id)
            );

            CREATE TABLE IF NOT EXISTS mall_items (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                description TEXT,
                cost INTEGER NOT NULL,
                stock INTEGER DEFAULT -1,
                image_url TEXT,
                status INTEGER DEFAULT 1,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            );

            CREATE TABLE IF NOT EXISTS redemption_records (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                student_id TEXT NOT NULL,
                item_id INTEGER NOT NULL,
                cost INTEGER NOT NULL,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (student_id) REFERENCES users(id),
                FOREIGN KEY (item_id) REFERENCES mall_items(id)
            );

            CREATE TABLE IF NOT EXISTS classes (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT UNIQUE NOT NULL,
                grade TEXT,
                grade_code TEXT,
                class_code TEXT,
                head_teacher TEXT,
                description TEXT,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            );

            -- 教师-班级关联表
            CREATE TABLE IF NOT EXISTS teacher_classes (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                teacher_id TEXT NOT NULL,
                class_id INTEGER NOT NULL,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (class_id) REFERENCES classes(id),
                UNIQUE(teacher_id, class_id)
            );

            -- 家长-学生关联表
            CREATE TABLE IF NOT EXISTS parent_students (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                parent_id TEXT NOT NULL,
                student_id TEXT NOT NULL,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (parent_id) REFERENCES users(id),
                FOREIGN KEY (student_id) REFERENCES users(id),
                UNIQUE(parent_id, student_id)
            );

            -- 家长端消息表
            CREATE TABLE IF NOT EXISTS parent_messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                student_id TEXT NOT NULL,
                sender_type TEXT NOT NULL,
                sender_id TEXT,
                content TEXT NOT NULL,
                reply_to INTEGER,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                read_status INTEGER DEFAULT 0,
                FOREIGN KEY (student_id) REFERENCES users(id)
            );

            CREATE INDEX IF NOT EXISTS idx_parent_messages_student_id ON parent_messages(student_id);

            -- 会话表（Cookie 认证）
            CREATE TABLE IF NOT EXISTS sessions (
                session_id TEXT PRIMARY KEY,
                user_id TEXT NOT NULL,
                role_id INTEGER NOT NULL,
                created_at INTEGER NOT NULL,
                expires_at INTEGER NOT NULL,
                is_parent INTEGER DEFAULT 0,
                student_id TEXT DEFAULT NULL
            );

            CREATE INDEX IF NOT EXISTS idx_sessions_user_id ON sessions(user_id);
            CREATE INDEX IF NOT EXISTS idx_sessions_expires_at ON sessions(expires_at);

            CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
            CREATE INDEX IF NOT EXISTS idx_users_role_id ON users(role_id);
            CREATE INDEX IF NOT EXISTS idx_points_records_student_id ON points_records(student_id);
            CREATE INDEX IF NOT EXISTS idx_evaluations_student_id ON evaluations(student_id);

            INSERT OR IGNORE INTO roles (id, name, description) VALUES
                (1, '管理员', '系统管理员，拥有所有权限'),
                (2, '教师', '教师角色，管理学生和积分'),
                (3, '学生', '学生角色，查看个人信息和兑换'),
                (4, '家长', '家长角色，查看孩子学习成绩和积分情况');

            INSERT OR IGNORE INTO permissions (id, name, code, description) VALUES
                (1, '系统管理', 'system:manage', '系统配置管理'),
                (2, '用户管理', 'user:manage', '用户和角色管理'),
                (3, '学生管理', 'student:manage', '学生信息管理'),
                (4, '积分管理', 'points:manage', '积分操作管理'),
                (5, '评价管理', 'evaluation:manage', '学生评价管理'),
                (6, '商城管理', 'mall:manage', '兑换商城管理'),
                (7, '数据统计', 'statistics:view', '数据统计查看'),
                (8, '家长管理', 'parent:manage', '家长账号与绑定管理'),
                (9, '留言管理', 'message:manage', '家校留言管理'),
                (10, '班级管理', 'class:manage', '班级信息管理'),
                (11, '兑换管理', 'redemption:manage', '兑换记录管理'),
                (12, '数据导出', 'data:export', '数据导出与备份');

            INSERT OR IGNORE INTO role_permissions (role_id, permission_id) VALUES
                (1, 1), (1, 2), (1, 3), (1, 4), (1, 5), (1, 6), (1, 7), (1, 8), (1, 9), (1, 10), (1, 11), (1, 12),
                (2, 3), (2, 4), (2, 5), (2, 7), (2, 9), (2, 10), (2, 11),
                (3, 6),
                (4, 6), (4, 9), (4, 11);

            -- 安全修复 V18（B2）：种子用户**不再**内联在 SQL 里。
            -- 旧实现把 4 个账号（admin/teacher/student/parent）的无盐 SHA256 口令哈希
            -- （明文为 xxx123 这类可猜值）直接写死在此处，随所有构建产物分发。
            -- 现改为：首启时由 seed_default_users_if_empty() 用 CSPRNG 生成互不相同的
            -- 随机强口令，hash_password() 哈希后**参数化**写入，并标记 must_change_password=1。
            -- 明文只在标准输出一次性提示，不写入任何文件（见下方函数实现）。

            INSERT OR IGNORE INTO mall_items (name, description, cost, stock) VALUES
                ('文具套装', '精美文具套装一份', 50, 100),
                ('图书卡', '50元图书购物卡', 100, 50),
                ('电影票', '电影院观影券一张', 80, 30),
                ('体育用品', '篮球或足球一个', 150, 20),
                ('学习用品', '笔记本和笔套装', 30, 200);

            INSERT OR IGNORE INTO classes (id, name, grade, grade_code, class_code, head_teacher, description) VALUES
                (1, '高二(1)班', '高二', '02', '01', '王老师', '理科实验班'),
                (2, '高二(2)班', '高二', '02', '02', '李老师', '文科实验班'),
                (3, '高二(3)班', '高二', '02', '03', '赵老师', '普通班');

            INSERT OR IGNORE INTO teacher_classes (teacher_id, class_id) VALUES
                ('teacher-001', 1);

            INSERT OR IGNORE INTO parent_students (parent_id, student_id) VALUES
                ('parent-001', 'student-02-01-01');
        )";

        if (db.execute(init_sql)) {
            Logger::info("数据库表结构初始化成功");

            // 安全修复 V18（B2）：旧库缺列时先做 ALTER TABLE 兼容迁移（不删库），
            // 必须在 load_users_from_db() 之前完成，否则 SELECT 新列会失败。
            migrate_users_must_change_password();

            // 安全修复 V18（B2）：首启（users 表为空）生成随机强口令种子账号
            seed_default_users_if_empty();

            if (load_users_from_db()) {
                Logger::info("从数据库加载用户数据成功");
            } else {
                Logger::error("从数据库加载用户数据失败（users 表为空）");
            }
            warn_legacy_weak_hashes();
        } else {
            // 表结构都没建起来时继续监听会让所有接口都失败且难以察觉，
            // 与「不静默降级」同一原则，故同样 fail-fast。
            Logger::error("数据库表结构初始化失败，拒绝启动（安全修复 V18）");
            std::cerr << "[FATAL] 数据库表结构初始化失败，已退出。" << std::endl;
            return 1;
        }
    }

    // 4. 创建 HTTP 服务器
    httplib::Server svr;

    // 5. 设置请求日志
    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        log_request(req);
        log_response(req.path, res.status);
    });

    // 6. 注册所有路由模块
    register_static_routes(svr);
    register_public_routes(svr);
    register_admin_routes(svr);
    register_teacher_routes(svr);
    register_student_routes(svr);
    register_parent_routes(svr);

    // 7. 初始化索引以提高查询效率
    init_indexes();

    // 8. 设置线程池
    svr.new_task_queue = [&]() { return new httplib::ThreadPool(g_config.thread_count); };

    // 安全修复 V10：CSRF 校验在具体状态变更类路由内部通过 require_csrf() 完成
    // （当前 httplib 版本不支持 pre_routing_handler 全局中间件）

    // 9. HTTPS 支持提示
    if (g_config.https_enabled) {
        Logger::warning("HTTPS 已在配置中启用，但当前编译版本不支持 SSLServer。请使用 OpenSSL 版本编译。回退到 HTTP 模式。");
        g_config.https_enabled = false;
        // 安全修复 N1：回退到明文 HTTP 后必须同步关闭 Secure Cookie 开关。
        // config.h:84 在读到 https.enabled=true 时会把 cookie_secure 置 true，
        // 而本分支把 https_enabled 置回 false 后并未复位该开关 —— 结果是明文 HTTP 下
        // 仍下发带 Secure 的 Cookie，浏览器仅在 HTTPS 上下文回传，登录会话直接失效。
        // 此处为唯一开关（auth.h:88 / :95 / :174 三处 Set-Cookie 均只读 g_config.cookie_secure）。
        g_config.cookie_secure = false;
        Logger::warning("已同步复位 cookie_secure=false，明文 HTTP 下不再下发带 Secure 的 Cookie（安全修复 N1）");
    }

    // 10. 启动服务器
    std::string protocol = "http";
    std::string listen_url = protocol + "://" + g_config.host + ":" + std::to_string(g_config.port);
    Logger::info("C++ 后端服务器已启动！监听端口：" + listen_url);
    Logger::info("按 Ctrl+C 停止服务器...");

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::cout << "服务器已启动，监听端口：" << listen_url << std::endl;
    std::cout << "按 Ctrl+C 停止服务器..." << std::endl;
#ifdef _WIN32
    SetConsoleOutputCP(GetACP());
#endif

    Logger::info("服务器开始监听 " + g_config.host + ":" + std::to_string(g_config.port));
    svr.listen(g_config.host.c_str(), g_config.port);

    return 0;
}
