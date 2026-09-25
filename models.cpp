// models.cpp - 全局数据定义和辅助函数实现
// 注意包含顺序：logger.h（引入 windows.h）必须在 models.h 之前。
// models.h 中的 using namespace std 会把 std::byte 注入全局作用域，
// 若 windows.h 在其之后解析，rpcndr.h 的 ::byte 将产生二义性编译错误（C++17）
#include "logger.h"
#include "models.h"
#include <unordered_map>

using namespace std;

// ====== 全局数据定义 ======

// 积分记录数据
vector<PointsRecord> points_records;

// 安全修复 V18（B2）：**移除**内存兜底账号表。
// 旧实现在此硬编码 4 个账号的无盐 SHA256 口令哈希（admin/teacher/student/parent + "xxx123"），
// 一旦磁盘库打开失败，服务会静默降级到内存模式并继续用这份可预测凭据对外提供服务，
// 且此后的口令修改/新增用户全部无法持久化。现在：
//   * main.cpp 在磁盘库打开失败时直接 exit(1)，不再存在内存兜底模式；
//   * 本向量只作为「从数据库加载」的容器，初始为空，由 load_users_from_db() 填充。
// 因此这份可预测凭据已从仓库中彻底删除（见 BATCH0_DESIGN_DECISIONS.md 决策①/②）。
vector<User> users;

// 角色数据
vector<Role> roles = {
    {1, "管理员", "系统管理员，拥有所有权限"},
    {2, "教师", "教师角色，管理学生和积分"},
    {3, "学生", "学生角色，查看个人信息和兑换"},
    {4, "家长", "家长角色，查看孩子学习成绩和积分情况"}
};

// 权限数据
vector<Permission> permissions = {
    {1, "系统管理", "system:manage", "系统配置管理"},
    {2, "用户管理", "user:manage", "用户和角色管理"},
    {3, "学生管理", "student:manage", "学生信息管理"},
    {4, "积分管理", "points:manage", "积分操作管理"},
    {5, "评价管理", "evaluation:manage", "学生评价管理"},
    {6, "商城管理", "mall:manage", "兑换商城管理"},
    {7, "数据统计", "statistics:view", "数据统计查看"},
    {8, "家长管理", "parent:manage", "家长账号与绑定管理"},
    {9, "留言管理", "message:manage", "家校留言管理"},
    {10, "班级管理", "class:manage", "班级信息管理"},
    {11, "兑换管理", "redemption:manage", "兑换记录管理"},
    {12, "数据导出", "data:export", "数据导出与备份"}
};

// 角色权限关联数据
vector<RolePermission> role_permissions = {
    {1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 5}, {1, 6}, {1, 7}, {1, 8}, {1, 9}, {1, 10}, {1, 11}, {1, 12}, // 管理员拥有所有权限
    {2, 3}, {2, 4}, {2, 5}, {2, 7}, {2, 9}, {2, 10}, {2, 11}, // 教师权限
    {3, 6}, // 学生权限
    {4, 6}, {4, 9}, {4, 11} // 家长权限
};

// ====== 数据库辅助函数 ======

bool load_users_from_db() {
    json result = db.query("SELECT id, username, password_hash, role_id, name, className, points, must_change_password FROM users");
    users.clear();
    for (const auto& row : result) {
        User u;
        u.id = row.value("id", "");
        u.username = row.value("username", "");
        u.password_hash = row.value("password_hash", "");
        u.role_id = row.value("role_id", 0);
        u.name = row.value("name", "");
        u.className = row.value("className", "");
        u.points = row.value("points", 0);
        u.student_id = u.username; // 学生用户名即为学号
        // 安全修复 V18（B2）：列可能为 NULL（极端情况下），按「不需要改密」处理
        u.must_change_password = !row.is_null() && row.contains("must_change_password")
                                 && !row["must_change_password"].is_null()
                                 && row.value("must_change_password", 0) != 0;
        users.push_back(u);
    }
    return !users.empty();
}

// 安全修复 V18（B2）：查询用户是否需要强制改密。
// 用户不存在返回 false（鉴权失败由调用方按 401 处理，不应被误判为「需改密」）。
bool user_must_change_password(const string& user_id) {
    User* u = find_user_by_id(user_id);
    return u != nullptr && u->must_change_password;
}

// 安全修复 V2：用户相关 SQL 全部改用参数化绑定
bool save_user_to_db(const User& user) {
    return db.execute_bind(
        "INSERT OR REPLACE INTO users (id, username, password_hash, role_id, name, className, points, must_change_password) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        {SqliteDb::Bind(user.id), SqliteDb::Bind(user.username),
         SqliteDb::Bind(user.password_hash), SqliteDb::Bind((long long)user.role_id),
         SqliteDb::Bind(user.name), SqliteDb::Bind(user.className),
         SqliteDb::Bind((long long)user.points),
         SqliteDb::Bind((long long)(user.must_change_password ? 1 : 0))});
}

bool delete_user_from_db(const string& user_id) {
    return db.execute_bind(
        "DELETE FROM users WHERE id = ?",
        {SqliteDb::Bind(user_id)});
}

bool update_user_points_in_db(const string& user_id, int points) {
    return db.execute_bind(
        "UPDATE users SET points = ? WHERE id = ?",
        {SqliteDb::Bind((long long)points), SqliteDb::Bind(user_id)});
}

// 从 u.id 中提取最后一段数字（最后一个 '-' 之后的部分）
// 例如 "teacher-001" -> 1, "student-02-01-05" -> 5, "admin-02" -> 2
static int parse_id_sequence(const string& id) {
    size_t pos = id.find_last_of('-');
    if (pos == string::npos) return 0;
    string seq_str = id.substr(pos + 1);
    try {
        return std::stoi(seq_str);
    } catch (...) {
        return 0;
    }
}

string generate_user_id(int role_id, const string& grade_code, const string& class_code) {
    char buf[64];
    int max_seq = 0;

    if (role_id == 1) { // admin
        for (const auto& u : users) {
            if (u.role_id == 1) {
                int seq = parse_id_sequence(u.id);
                if (seq > max_seq) max_seq = seq;
            }
        }
        snprintf(buf, sizeof(buf), "admin-%02d", max_seq + 1);
    } else if (role_id == 2) { // teacher
        for (const auto& u : users) {
            if (u.role_id == 2) {
                int seq = parse_id_sequence(u.id);
                if (seq > max_seq) max_seq = seq;
            }
        }
        snprintf(buf, sizeof(buf), "teacher-%03d", max_seq + 1);
    } else if (role_id == 4) { // parent
        for (const auto& u : users) {
            if (u.role_id == 4) {
                int seq = parse_id_sequence(u.id);
                if (seq > max_seq) max_seq = seq;
            }
        }
        snprintf(buf, sizeof(buf), "parent-%03d", max_seq + 1);
    } else if (role_id == 3) { // student
        // 学生 ID 格式：student-<grade_code>-<class_code>-<seq>
        // 仅在同一班级内累计序号
        for (const auto& u : users) {
            if (u.role_id == 3) {
                // 通过 className 匹配班级（传入的 grade_code/class_code 应已对应一个班级）
                // 简化做法：仅按 id 前缀 student-<grade_code>-<class_code>- 匹配
                string prefix = "student-" + grade_code + "-" + class_code + "-";
                if (u.id.compare(0, prefix.size(), prefix) == 0) {
                    int seq = parse_id_sequence(u.id);
                    if (seq > max_seq) max_seq = seq;
                }
            }
        }
        snprintf(buf, sizeof(buf), "student-%s-%s-%02d", grade_code.c_str(), class_code.c_str(), max_seq + 1);
    } else {
        // 未知角色，使用通用序号
        snprintf(buf, sizeof(buf), "user-%03d", (int)users.size() + 1);
    }
    return string(buf);
}

// ====== 索引优化结构 ======

// 安全修复 V12：索引改为存储 users 向量的下标，避免副本与原数据不一致
unordered_map<string, size_t> user_id_map;           // 用户ID到 users 下标的映射
unordered_map<string, size_t> user_username_map;     // 用户名到 users 下标的映射
unordered_map<int, Role> role_id_map;           // 角色ID到角色的映射
unordered_map<int, Permission> permission_id_map; // 权限ID到权限的映射
unordered_map<int, vector<int>> role_permission_map; // 角色ID到权限ID列表的映射

// 重建用户索引的内部实现（init_indexes() 与 rebuild_user_indexes() 共用的唯一实现）。
// 先清空再按 users 当前下标赋值：清空这一步同时消除「已删除用户」留在映射里的陈旧下标，
// 使两个调用方都得到「与 users 向量逐项一致」的索引，不存在第二份可能漂移的逻辑。
static void clear_and_rebuild_user_indexes() {
    user_id_map.clear();
    user_username_map.clear();
    for (size_t i = 0; i < users.size(); i++) {
        user_id_map[users[i].id] = i;
        user_username_map[users[i].username] = i;
    }
}

// 初始化索引
// 安全修复 T21：本函数原先**非幂等**，有两个独立后果：
//   ① 权限映射重复累积：role_permission_map 用 push_back 写入且从不 clear，
//      每调用一次就重复追加一轮（role_permissions 源向量 23 条 → 23/46/69…）；
//   ② 已删除用户的陈旧下标永久残留：用户映射只做赋值、不 erase，
//      删除用户之后再调用本函数，会把「已不存在的用户 → 旧下标」重新固化下来，
//      于是 find_user_by_id() 会取到 users 里的**另一个用户**或返回 nullptr
//      （与 B6 同类的错位，触发路径不同：B6 是 erase 后不重建，这里是重建得不彻底）。
// 本函数唯一的调用方 main.cpp 只调一次所以历史影响有限；但 routes_admin.cpp 的
// 数据导入端点也会调用它，而该端点**可被反复调用**，故缺陷真实可达。
// 现在改为「五个索引容器全部 clear 后从源头重建」：
//   * users / roles / permissions / role_permissions 均为源数据（role_permissions 的
//     运行时写入点只有 routes_admin.cpp 的导入分支），clear 后立即由源头重建，数据不丢；
//   * 反复调用结果稳定，删除用户后再调用也不再留下陈旧键。
void init_indexes() {
    clear_and_rebuild_user_indexes();
    role_id_map.clear();
    permission_id_map.clear();
    role_permission_map.clear();
    for (const auto& role : roles) {
        role_id_map[role.id] = role;
    }
    for (const auto& perm : permissions) {
        permission_id_map[perm.id] = perm;
    }
    for (const auto& rp : role_permissions) {
        role_permission_map[rp.role_id].push_back(rp.permission_id);
    }
}

// 安全修复 B6：erase 之后重建用户索引。
// 背景：user_id_map / user_username_map 存的是 **users 向量的下标**，而 users.erase()
// 会让被删元素之后的全部元素整体前移一位。此前删除路径只调用 remove_user_index()
// 摘掉被删用户的键，其余用户的映射仍指向旧下标 —— 于是 find_user_by_id("B") 会返回
// 原 C 的对象（错位取到错误用户），越界时还会退化成 nullptr。
//
// 本函数按 T1 决策④实现「erase 后 clear + 重建」：只重建用户索引
// （user_id_map / user_username_map），与 init_indexes() 共用
// clear_and_rebuild_user_indexes()，两条路径不会各自漂移。
// 注意（T21 后语义更新）：当初「不调用 init_indexes()」的理由是它非幂等、会污染
// role_permission_map；该缺陷已在 T21 修掉，init_indexes() 现在同样幂等。
// 这里仍只重建用户索引，是因为删除路径只有用户部分发生了变化，重建权限映射纯属多余
// —— 属于实现选择，而不再是正确性要求。
// 稳定键（id → 稳定句柄）的彻底版按 T1 决策留待批次 1-3。
void rebuild_user_indexes() {
    clear_and_rebuild_user_indexes();
}

// 更新用户索引
void update_user_index(const User& user) {
    for (size_t i = 0; i < users.size(); i++) {
        if (users[i].id == user.id) {
            user_id_map[user.id] = i;
            user_username_map[user.username] = i;
            return;
        }
    }
}

// 删除用户索引
void remove_user_index(const string& user_id, const string& username) {
    user_id_map.erase(user_id);
    user_username_map.erase(username);
}

// 使用索引查找用户（返回 users 向量真实引用）
User* find_user_by_id(const string& user_id) {
    auto it = user_id_map.find(user_id);
    if (it != user_id_map.end() && it->second < users.size()) {
        return &users[it->second];
    }
    return nullptr;
}

// 使用索引查找用户
User* find_user_by_username(const string& username) {
    auto it = user_username_map.find(username);
    if (it != user_username_map.end() && it->second < users.size()) {
        return &users[it->second];
    }
    return nullptr;
}

// 使用索引检查权限
bool check_permission_optimized(const string& user_id, const string& permission_code) {
    User* user = find_user_by_id(user_id);
    if (!user) {
        return false;
    }

    int role_id = user->role_id;
    auto it = role_permission_map.find(role_id);
    if (it == role_permission_map.end()) {
        return false;
    }

    for (int perm_id : it->second) {
        auto perm_it = permission_id_map.find(perm_id);
        if (perm_it != permission_id_map.end() && perm_it->second.code == permission_code) {
            return true;
        }
    }

    return false;
}
