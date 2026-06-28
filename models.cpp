// models.cpp - 全局数据定义和辅助函数实现
#include "models.h"
#include "logger.h"
#include <unordered_map>

using namespace std;

// ====== 全局数据定义 ======

// 积分记录数据
vector<PointsRecord> points_records;

// 默认用户数据（内存模式时使用，密码均为 SHA256 哈希）
vector<User> users = {
    {"admin-01", "admin", "240be518fabd2724ddb6f04eeb1da5967448d7e831c08c8fa822809f74c720a9", 1, "管理员", "系统管理", 0, ""},
    {"teacher-001", "teacher", "cde383eee8ee7a4400adf7a15f716f179a2eb97646b37e089eb8d6d04e663416", 2, "王老师", "高二(1)班", 0, ""},
    {"student-02-01-01", "student", "703b0a3d6ad75b649a28adde7d83c6251da457549263bc7ff45ec709b0a8448b", 3, "张同学", "高二(1)班", 150, "student"},
    {"parent-001", "parent", "82e3edf5f5f3a46b5f94579b61817fd9a1f356adcef5ee22da3b96ef775c4860", 4, "张同学家长", "", 0, ""}
};

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
    json result = db.query("SELECT id, username, password_hash, role_id, name, className, points FROM users");
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
        users.push_back(u);
    }
    return !users.empty();
}

bool save_user_to_db(const User& user) {
    char sql[2048];
    snprintf(sql, sizeof(sql),
        "INSERT OR REPLACE INTO users (id, username, password_hash, role_id, name, className, points) VALUES ('%s', '%s', '%s', %d, '%s', '%s', %d)",
        db.escapeString(user.id).c_str(), db.escapeString(user.username).c_str(),
        db.escapeString(user.password_hash).c_str(), user.role_id,
        db.escapeString(user.name).c_str(), db.escapeString(user.className).c_str(), user.points);
    return db.execute(sql);
}

bool delete_user_from_db(const string& user_id) {
    char sql[512];
    snprintf(sql, sizeof(sql), "DELETE FROM users WHERE id = '%s'", db.escapeString(user_id).c_str());
    return db.execute(sql);
}

bool update_user_points_in_db(const string& user_id, int points) {
    char sql[512];
    snprintf(sql, sizeof(sql), "UPDATE users SET points = %d WHERE id = '%s'", points, db.escapeString(user_id).c_str());
    return db.execute(sql);
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

// 初始化索引
void init_indexes() {
    for (size_t i = 0; i < users.size(); i++) {
        user_id_map[users[i].id] = i;
        user_username_map[users[i].username] = i;
    }
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
