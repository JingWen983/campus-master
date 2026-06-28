#ifndef MODELS_H
#define MODELS_H

#include <string>
#include <vector>
#include <algorithm>
#include <ctime>
#include "json.hpp"
#include "sha256.h"

#include "sqlite_wrapper.h"
extern SqliteDb db;

using json = nlohmann::json;
using namespace std;

// 模拟数据库中的用户信息
struct User {
    string id;
    string username;
    string password_hash;
    int role_id;
    string name;
    string className;
    int points;
    string student_id; // 学号（学生角色专用）
};

// 角色定义
struct Role {
    int id;
    string name;
    string description;
};

// 权限定义
struct Permission {
    int id;
    string name;
    string code;
    string description;
};

// 角色权限关联
struct RolePermission {
    int role_id;
    int permission_id;
};

// 积分记录结构体
struct PointsRecord {
    int id;
    string student_id;
    int points;
    string reason;
    string operator_id;
    string created_at;
};

// ====== 全局数据 ======

// 积分记录数据
extern vector<PointsRecord> points_records;

// 默认用户数据（内存模式时使用，密码均为 SHA256 哈希）
extern vector<User> users;
extern vector<Role> roles;
extern vector<Permission> permissions;
extern vector<RolePermission> role_permissions;

// ====== 数据库辅助函数 ======

bool load_users_from_db();
bool save_user_to_db(const User& user);
bool delete_user_from_db(const string& user_id);
bool update_user_points_in_db(const string& user_id, int points);

// 生成用户 ID
// - role_id=1 (admin): admin-01, admin-02, ... (2位序号)
// - role_id=2 (teacher): teacher-001, ... (3位序号)
// - role_id=4 (parent): parent-001, ... (3位序号)
// - role_id=3 (student): student-<grade_code>-<class_code>-<seq>，序号2位
string generate_user_id(int role_id, const string& grade_code = "", const string& class_code = "");

// ====== 索引优化 ======
void init_indexes();
void update_user_index(const User& user);
void remove_user_index(const string& user_id, const string& username);
User* find_user_by_id(const string& user_id);
User* find_user_by_username(const string& username);
bool check_permission_optimized(const string& user_id, const string& permission_code);

#endif
