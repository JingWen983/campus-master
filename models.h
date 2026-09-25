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
    // 安全修复 V18（B2）：首登强制改密标记。
    // 注意：本字段必须保持为**最后一个成员** —— 仓库存在位置聚合初始化
    // （models.cpp 的默认表、routes_teacher.cpp 的建号路径），把新字段插到中间
    // 会造成初始值整体错位（见 BATCH0_DESIGN_DECISIONS.md 决策②）。
    //
    // **fail-safe 默认值 = true**（队长补充要求：既有六条口令写入路径必须一致）。
    // 理由：仓库中所有默认构造 User 的位置都是「由他人代设口令」的路径
    // （routes_admin 建号/导入、routes_teacher 建号/导入）—— 代设方知情、用户本人不知情，
    // 必须首登改密。只有「用户自己选定的口令」才允许免改，故全仓库仅两处显式置 false：
    //   1) routes_public.cpp 自助注册（用户自己设的口令）；
    //   2) routes_public.cpp 修改口令接口成功后（改密结果本身）。
    // 反例（务必保持）：改回 false 会让管理员/教师代设的口令不再强制改密，
    // 从而绕过本批次「去除可预测默认凭据」的目标。
    bool must_change_password = true;
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

// 安全修复 V18（B2）：查询某用户是否处于「必须修改口令」状态。
// 供服务端门禁（auth.h 的两个中间件）与改密流程使用；用户不存在时返回 false
// （不存在 ≠ 需要改密：鉴权失败应由调用方按 401 处理）。
bool user_must_change_password(const string& user_id);

// 生成用户 ID
// - role_id=1 (admin): admin-01, admin-02, ... (2位序号)
// - role_id=2 (teacher): teacher-001, ... (3位序号)
// - role_id=4 (parent): parent-001, ... (3位序号)
// - role_id=3 (student): student-<grade_code>-<class_code>-<seq>，序号2位
string generate_user_id(int role_id, const string& grade_code = "", const string& class_code = "");

// ====== 索引优化 ======
// 安全修复 T21：init_indexes() 现已幂等 —— 五个索引容器（user_id_map / user_username_map /
// role_id_map / permission_id_map / role_permission_map）在重建前全部 clear，再依 users /
// roles / permissions / role_permissions 四个源容器重建；可反复调用，且会一并清掉已删除
// 用户的陈旧下标。详见 models.cpp 中该函数的注释。
void init_indexes();
void update_user_index(const User& user);
void remove_user_index(const string& user_id, const string& username);
// 安全修复 B6：users.erase() 之后清空并重建用户索引，修复下标错位导致的「取到错误用户」。
// 与 init_indexes() 共用同一个内部实现，只重建用户相关索引（不重建权限映射）。
void rebuild_user_indexes();
User* find_user_by_id(const string& user_id);
User* find_user_by_username(const string& username);
bool check_permission_optimized(const string& user_id, const string& permission_code);

#endif
