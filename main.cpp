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
        Logger::warning("SQLite 数据库连接失败，使用内存存储");
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

            INSERT OR IGNORE INTO users (id, username, password_hash, role_id, name, className, points) VALUES
                ('admin-01', 'admin', '240be518fabd2724ddb6f04eeb1da5967448d7e831c08c8fa822809f74c720a9', 1, '管理员', '系统管理', 0),
                ('teacher-001', 'teacher', 'cde383eee8ee7a4400adf7a15f716f179a2eb97646b37e089eb8d6d04e663416', 2, '王老师', '高二(1)班', 0),
                ('student-02-01-01', 'student', '703b0a3d6ad75b649a28adde7d83c6251da457549263bc7ff45ec709b0a8448b', 3, '张同学', '高二(1)班', 150),
                ('parent-001', 'parent', '82e3edf5f5f3a46b5f94579b61817fd9a1f356adcef5ee22da3b96ef775c4860', 4, '张同学家长', '', 0);

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
            if (load_users_from_db()) {
                Logger::info("从数据库加载用户数据成功");
            }
        } else {
            Logger::error("数据库表结构初始化失败");
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
