#ifndef MODELS_H
#define MODELS_H

#include <string>
#include <vector>
#include <algorithm>
#include <ctime>
#include <mutex>
#include <functional>
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
// 与上面同义，但**调用方必须已持 g_state_mutex**。供需要在一次持锁内完成
// 「改内存 + 重建索引」的路径使用（若改用公开版本会重复加锁 → 自死锁）。
void rebuild_user_indexes_locked();

// ====== 批次 1 / 1-2（B3+B5）：共享状态的唯一一把应用级锁 ======
//
// **强制规则（R1~R8，详见 models.cpp 中 g_state_mutex 的说明）**：
//   * 任何对 users / points_records / roles / permissions / role_permissions 以及五个
//     索引容器的访问，都必须经本文件提供的 with_* / *_copy / for_each_* / *_count 入口，
//     或在 models.cpp 内部显式持锁；
//   * routes_*.cpp 与 auth.h **不得**自行加锁，也不得持锁调用本锁（会自死锁）；
//   * 需要「读-判断-改-写」的路径必须用 with_user_record 在**一次**持锁内完成，
//     否则并发扣分会各自基于陈旧 points 计算并互相覆盖。
extern std::recursive_mutex g_state_mutex;

// 只读查找（返回拷贝；查不到时返回默认构造的 User，其 id 为空）。
// 旧的 `User* find_user_by_id` / `find_user_by_username` 已**删除**：
// 返回裸指针会在并发 push_back/erase 后悬垂，调用点继续解引用即写入已释放内存（B5）。
User find_user_by_id_copy(const string& user_id);
bool find_user_by_id_copy(const string& user_id, User& out);      // 返回是否找到
User find_user_by_username_copy(const string& username);
bool find_user_by_username_copy(const string& username, User& out);

// 读-判断-改-写：在**一次**持锁内把 users 中的可变引用交给回调。
// 回调返回 false 表示业务上拒绝（此时不做任何写入）。找不到用户时直接返回 false。
// not_found（可选出参）：区分「用户不存在」与「回调拒绝」，便于调用方给出 404 vs 400。
bool with_user_record(const string& user_id, const std::function<bool(User&)>& fn,
                      bool* not_found = nullptr);

// 受锁遍历（回调内**不得**再调用任何会加锁的入口）
void for_each_user(const std::function<void(const User&)>& fn);
void for_each_points_record(const std::function<void(const PointsRecord&)>& fn);
size_t users_count();

// 原子建号：在同一临界区内完成「用户名唯一性校验 → 填充 → 插入 → 重建索引」。
// fill 回调负责给 User 赋全部字段（含 id，可用 generate_user_id 生成 —— 它同样持锁且可重入）。
// required_username 非空时先查重，已存在则返回 false（不插入）；generated_id 回传实际 id。
bool add_user_record(const std::function<void(User&)>& fill, std::string& generated_id,
                     const std::string& required_username = "");

// 原子建号**并落库**（推荐用于所有对外建号路径）：一次持锁内完成
// 唯一性校验 → 生成 id → 先写 DB → 成功后写内存 → 重建索引。
// DB 冲突即返回 false 且内存不变（修复「报成功却覆盖/丢数据」的 N2 缺陷）。
bool add_user_record_persisted(const std::function<void(User&)>& fill, std::string& generated_id,
                               const std::string& required_username = "");

// 更新既有用户的整行（仅用于导入覆盖等明确要覆盖的场景；INSERT 在行已存在时会失败）
bool update_user_in_db(const User& user);

// 受锁存在性查询：返回匹配用户的 id，不存在则返回空串。
// 索引容器（user_id_map / user_username_map）**不对外暴露**，否则锁协议会被撕开。
std::string find_user_id_by_username(const string& username);
std::string find_user_id_by_id_key(const string& user_id);

// 受锁权限判定（user.role_id → role_permissions → permission.code）。
// 批次 1：Auth::check_permission 已委托到本函数，因此它不再是死代码，
// 「活调用点 = 0」的旧结论作废。
// 注意：原实现只定义在 models.cpp、未在头文件声明，故此前**不可能**被其他 TU 调用。
bool check_permission_optimized(const string& user_id, const string& permission_code);

// ====== 批次 1 / 1-5（B9）：教师-学生归属判定原语 ======
// 判据统一为 teacher_classes JOIN classes JOIN users（决策④）：
// 教职工号 → users.className → classes.name → teacher_classes.class_id → 该教师是否任课。
// **禁止**再用 className 字符串比较做归属判定（users.className 无外键约束）。
bool teacher_owns_student(const string& teacher_id, const string& student_id);
// 与上面同义，直接以 className 判断（调用方已解析出班级时用，避免重复查库）
bool teacher_owns_class_name(const string& teacher_id, const string& class_name);

#endif
