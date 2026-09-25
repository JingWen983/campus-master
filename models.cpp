// models.cpp - 全局数据定义和辅助函数实现
// 注意包含顺序：logger.h（引入 windows.h）必须在 models.h 之前。
// models.h 中的 using namespace std 会把 std::byte 注入全局作用域，
// 若 windows.h 在其之后解析，rpcndr.h 的 ::byte 将产生二义性编译错误（C++17）
#include "logger.h"
#include "models.h"
#include <unordered_map>
#include <mutex>

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
// 批次 1 / B5：改为取拷贝，不再返回可被并发失效的裸指针。
bool user_must_change_password(const string& user_id) {
    User u;
    if (!find_user_by_id_copy(user_id, u)) return false;
    return u.must_change_password;
}

// 安全修复 V2：用户相关 SQL 全部改用参数化绑定。
// 批次 1 / N2（+伴-4）：由 `INSERT OR REPLACE` 改为 `INSERT`。
// 为什么必须改：OR REPLACE 会在主键/唯一键冲突时**静默删除旧行并写入新行** ——
// 并发建号一旦生成同一个 id（或同名），后写入者会悄悄覆盖前者，调用方却拿到成功。
// 实测：8 并发注册返回 8 个 HTTP 200，而库内只剩 1 个用户。
// 改成普通 INSERT 后冲突会返回错误码，由调用方显式处理（失败即撤回内存并如实报错）。
bool save_user_to_db(const User& user) {
    return db.execute_bind(
        "INSERT INTO users (id, username, password_hash, role_id, name, className, points, must_change_password) "
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
    // 批次 1 / B3：本函数遍历全局 users 求最大序号。它既被锁外的建号路径调用，
    // 也被 with_user_record 临界区内的路径调用 —— 故必须持锁，且锁必须可重入。
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
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

// ====== 批次 1 / 1-2：进程内共享状态的唯一一把应用级锁 ======
//
// 为什么是**一把** std::mutex 而不是 shared_mutex（决策②，理由见
// docs/audit/BATCH1_DESIGN_DECISIONS.md）：
//   * 本项目所有临界区都极短（一次哈希查找 / 一次线性扫描 / 一次 map 更新），
//     共享读锁换来的吞吐在 8 线程 + DB 往返的现实下不可测量；
//   * shared_mutex 不可重入，而本项目读路径大量嵌套（如 check_permission_optimized
//     内部要查用户），一旦某读路径在持 shared_lock 时再进读路径即自死锁。
//
// 保护对象（R1 单锁协议）：users / points_records / roles / permissions /
// role_permissions，以及五个索引容器 user_id_map / user_username_map /
// role_id_map / permission_id_map / role_permission_map。
// 不保护：db 的**连接**（A1 已改为每线程一条，是线程局部资源，见 sqlite_wrapper.h）、
// Logger（自有锁，且按 R7 不得与本锁互套）。
//
// **强制规则（R1~R8）**：所有对上述容器的访问都必须经本文件的 with_* / *_locked 入口
// 或显式 lock_guard；routes_*.cpp 与 auth.h **不得**自行加锁，也不得持锁调用本锁
// （会自死锁）。需要跨多次访问保持一致性的写-改-写，必须用 with_user_record 在
// **一次**持锁内完成。
// **为什么用 recursive_mutex 而不是 std::mutex**：本批把大量写路径收进
// with_user_record 的「一次持锁」临界区，而这些路径内部还会调用同样加锁的公开入口
// （例如 generate_user_id() 要遍历 users、导入端点要在同一事务内多次重建索引）。
// 非可重入锁会让这些嵌套调用**自死锁**，而共享状态只有一份、临界区又极短，
// 递归锁在此不损失任何互斥保证（同一时刻仍只有一个线程进入），代价可忽略。
// 因此 R1「全进程只允许一把应用级锁」不变，只是该锁允许同线程重入。
std::recursive_mutex g_state_mutex;

// ====== 索引优化结构 ======

// 安全修复 V12 起索引存储「users 向量的下标」；批次 1-3 追加**身份校验**：
// 命中后必须验证 users[idx].id 等于所查的键，否则视为未找到并告警 ——
// 这样即便索引因任何路径未重建而残留陈旧下标，「取到另一个用户」（越权/串号）也
// 不可能再发生，最坏退化为「查不到」。
unordered_map<string, size_t> user_id_map;           // 用户ID到 users 下标的映射
unordered_map<string, size_t> user_username_map;     // 用户名到 users 下标的映射
unordered_map<int, Role> role_id_map;           // 角色ID到角色的映射
unordered_map<int, Permission> permission_id_map; // 权限ID到权限的映射
unordered_map<int, vector<int>> role_permission_map; // 角色ID到权限ID列表的映射

// 重建用户索引的内部实现（init_indexes() 与 rebuild_user_indexes() 共用的唯一实现）。
// 先清空再按 users 当前下标赋值：清空这一步同时消除「已删除用户」留在映射里的陈旧下标，
// 使两个调用方都得到「与 users 向量逐项一致」的索引，不存在第二份可能漂移的逻辑。
// 调用方必须已持 g_state_mutex（static，仅本文件可见）。
static void clear_and_rebuild_user_indexes() {
    user_id_map.clear();
    user_username_map.clear();
    for (size_t i = 0; i < users.size(); i++) {
        user_id_map[users[i].id] = i;
        user_username_map[users[i].username] = i;
    }
}

// 带**身份校验**的按 id 查找（未持锁的内部实现，调用方必须已持 g_state_mutex）。
// 返回 nullptr = 未找到或校验失败；两种情况调用方语义相同（视为不存在），
// 但校验失败会留下一次 Logger::warning 以便发现残留陈旧键。
static const User* find_user_record_locked(const string& user_id) {
    auto it = user_id_map.find(user_id);
    if (it == user_id_map.end()) return nullptr;
    if (it->second >= users.size()) {
        Logger::warning("用户索引残留越界下标： id=" + user_id +
                        " idx=" + std::to_string(it->second) +
                        " users.size=" + std::to_string(users.size()));
        return nullptr;
    }
    if (users[it->second].id != user_id) {
        Logger::warning("用户索引身份校验失败（陈旧下标）： 查找=" + user_id +
                        " 实际=" + users[it->second].id);
        return nullptr;
    }
    return &users[it->second];
}

// 带身份校验的按用户名查找（未持锁的内部实现；同上）
static const User* find_user_record_by_username_locked(const string& username) {
    auto it = user_username_map.find(username);
    if (it == user_username_map.end()) return nullptr;
    if (it->second >= users.size()) {
        Logger::warning("用户名索引残留越界下标： username=" + username);
        return nullptr;
    }
    if (users[it->second].username != username) {
        Logger::warning("用户名索引身份校验失败（陈旧下标）： 查找=" + username +
                        " 实际=" + users[it->second].username);
        return nullptr;
    }
    return &users[it->second];
}

// ====== 带锁的公开查找/遍历入口（批次 1 / B5）======
//
// 为什么不再返回 User*：`users` 是 vector，任何并发的 push_back/erase 都会让先前取到的
// `User*` 立即悬垂，而调用点会继续解引用写（写已释放内存）。改为「在持锁期间拷贝出来」
// 或「在持锁期间执行回调」，从接口上就不存在可跨线程存活的裸指针。

User find_user_by_id_copy(const string& user_id) {
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    const User* u = find_user_record_locked(user_id);
    return u ? *u : User();
}

bool find_user_by_id_copy(const string& user_id, User& out) {
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    const User* u = find_user_record_locked(user_id);
    if (!u) return false;
    out = *u;
    return true;
}

User find_user_by_username_copy(const string& username) {
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    const User* u = find_user_record_by_username_locked(username);
    return u ? *u : User();
}

bool find_user_by_username_copy(const string& username, User& out) {
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    const User* u = find_user_record_by_username_locked(username);
    if (!u) return false;
    out = *u;
    return true;
}

// 「读-判断-改-写」必须在**一次**持锁内完成：否则两个并发扣分会各自基于陈旧的
// points 计算并互相覆盖（本批 B3/B8 的同源表现）。回调允许返回 false 表示「业务上
// 拒绝」，此时不做任何写入。
// not_found（可选出参）：置 true 表示「该用户不存在」——用于把「查不到」与「回调拒绝」
// 区分开（两者都应拒绝请求，但 HTTP 语义不同：404 vs 400）。
bool with_user_record(const string& user_id, const std::function<bool(User&)>& fn, bool* not_found) {
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    auto it = user_id_map.find(user_id);
    if (it == user_id_map.end() || it->second >= users.size()) {
        if (not_found) *not_found = true;
        return false;
    }
    User& u = users[it->second];
    if (u.id != user_id) {
        Logger::warning("用户索引身份校验失败（with_user_record）： 查找=" + user_id +
                        " 实际=" + u.id);
        if (not_found) *not_found = true;
        return false;
    }
    return fn(u);
}

void for_each_user(const std::function<void(const User&)>& fn) {
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    for (const auto& u : users) fn(u);
}

void for_each_points_record(const std::function<void(const PointsRecord&)>& fn) {
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    for (const auto& r : points_records) fn(r);
}

size_t users_count() {
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    return users.size();
}

// 受锁存在性查询（批次 1：索引容器不对外暴露，只提供查询入口）。
// 返回对应的 user_id；不存在返回空串。
// 为什么不让调用方直接用 user_id_map / user_username_map：它们此前只在 models.cpp
// 定义、未在头文件声明（属实现细节）；直接暴露会把锁协议撕开一个口子。
string find_user_id_by_username(const string& username) {
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    const User* u = find_user_record_by_username_locked(username);
    return u ? u->id : string();
}

string find_user_id_by_id_key(const string& user_id) {
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    const User* u = find_user_record_locked(user_id);
    return u ? u->id : string();
}

// 原子建号（批次 1 / B3 + N2）：唯一性校验 → 生成 ID → 插入内存 → 重建索引，
// 全部在**一次**持锁内完成。修复前这些步骤散落在各端点里（先 find_if 查重、
// 再 generate_user_id、再 push_back、再 update_user_index），并发下两个请求可以：
//   * 都通过查重 → 同名双开；
//   * 生成同一个 ID → 后写入者静默覆盖前者（N2，配合 INSERT OR REPLACE 更隐蔽）。
// generated_id 回传实际分配的 ID，供调用方写库与回显。
bool add_user_record(const std::function<void(User&)>& fill, std::string& generated_id,
                     const std::string& required_username) {
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    if (!required_username.empty() && user_username_map.count(required_username)) {
        return false;   // 用户名已存在
    }
    User nu;
    fill(nu);           // 调用方填 id/username/hash/role/name/className/points 等
    generated_id = nu.id;
    users.push_back(nu);
    clear_and_rebuild_user_indexes();
    return true;
}

// 原子建号 **并落库**（批次 1 / N2 的关键修复）。
//
// 为什么不能「先 add_user_record 再单独 save_user_to_db」：那样内存插入与 DB 写入
// 落在两把（同一把锁的）两次临界区之间，存在窗口——两个并发请求可以都通过唯一性校验、
// 生成同一个 id，随后各自落库，其中一个把另一个覆盖掉。这正是实测
// 「8 并发注册返回 8 个 200 但库内只剩 1 个用户」的成因。
//
// 本函数在**一次**持锁内完成：唯一性校验 → 生成 id → **先写 DB** → 成功后写内存 →
// 重建索引。DB 冲突（主键/唯一键）即返回 false，内存保持未修改，
// 调用方据此如实返回失败（不再有「报成功却丢数据」）。
bool add_user_record_persisted(const std::function<void(User&)>& fill, std::string& generated_id,
                               const std::string& required_username) {
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    if (!required_username.empty() && user_username_map.count(required_username)) {
        return false;   // 用户名已存在
    }
    User nu;
    fill(nu);
    if (nu.id.empty()) return false;
    // 内存里也先看一眼 id 是否已被占用（索引带身份校验，不会误判）
    if (find_user_record_locked(nu.id) != nullptr) return false;
    // DB 先写：普通 INSERT（save_user_to_db 已去掉 OR REPLACE）→ 冲突会失败
    if (!save_user_to_db(nu)) {
        Logger::warning("建号落库失败（id 冲突或唯一键冲突）：id=" + nu.id + " username=" + nu.username);
        return false;
    }
    generated_id = nu.id;
    users.push_back(nu);
    clear_and_rebuild_user_indexes();
    return true;
}

// 更新既有用户的整行（仅用于「导入覆盖」这类明确要覆盖既有行的场景）。
// 注意：必须用 UPDATE，不能用 save_user_to_db —— 后者的 INSERT 在行已存在时会失败。
bool update_user_in_db(const User& user) {
    return db.execute_bind(
        "UPDATE users SET username = ?, password_hash = ?, role_id = ?, name = ?, className = ?, "
        "points = ?, must_change_password = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
        {SqliteDb::Bind(user.username), SqliteDb::Bind(user.password_hash),
         SqliteDb::Bind((long long)user.role_id), SqliteDb::Bind(user.name),
         SqliteDb::Bind(user.className), SqliteDb::Bind((long long)user.points),
         SqliteDb::Bind((long long)(user.must_change_password ? 1 : 0)),
         SqliteDb::Bind(user.id)});
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
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
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
void rebuild_user_indexes_locked() {
    clear_and_rebuild_user_indexes();
}

// 公开入口：自行加锁（供 routes_*.cpp 在不持锁时调用）
void rebuild_user_indexes() {
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    rebuild_user_indexes_locked();
}

// 更新用户索引（写路径，必须持锁）
void update_user_index(const User& user) {
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    for (size_t i = 0; i < users.size(); i++) {
        if (users[i].id == user.id) {
            user_id_map[user.id] = i;
            user_username_map[user.username] = i;
            return;
        }
    }
}

// 删除用户索引（写路径，必须持锁）
void remove_user_index(const string& user_id, const string& username) {
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    user_id_map.erase(user_id);
    user_username_map.erase(username);
}

// 使用索引查找用户（批次 1 / B5：旧的 `User* find_user_by_id` 已**删除**，
// 不保留任何同名占位实现 —— 占位会返回 nullptr，被调用方误读为「用户不存在」，
// 属于无声失败。调用方一律改用上方带锁入口：
//   * 只读需要一份数据 → find_user_by_id_copy / find_user_by_username_copy
//   * 需要读-判断-改-写 → with_user_record
//   * 需要遍历全体     → for_each_user

// 使用索引检查权限（读路径；持锁后全部在锁内完成，避免读-读嵌套自死锁）
bool check_permission_optimized(const string& user_id, const string& permission_code) {
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    const User* user = find_user_record_locked(user_id);
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

// ====== 批次 1 / 1-5（B9）：教师-学生归属判定 ======
//
// 现状（批次 1 开工前实测）：仓库中**不存在**任何统一的归属判定函数，归属是靠散落的
// inline SQL 子查询表达的，而且两种口径混用：
//   * routes_teacher.cpp:563-566 用 `tc.teacher_id = ? AND c.name = ?`（班级名字符串）
//   * routes_teacher.cpp:578 等用 `s.className IN (SELECT c.name FROM teacher_classes ...)`
//   * routes_teacher.cpp:1071 的「回复家长留言」**根本没有归属校验**（越权面）
//
// 统一口径（决策④）：teacher_classes JOIN classes JOIN users。
// 链路：student_id → users.className → classes.name → classes.id → teacher_classes.teacher_id。
// **禁止**再用 className 字符串比较做判定（users.className 无外键约束）。
//
// 失败即拒绝（fail-closed）：数据库异常、查不到学生、学生无班级，一律返回 false。
// 归属判定绝不能「查不到就放行」。
bool teacher_owns_class_name(const string& teacher_id, const string& class_name) {
    if (teacher_id.empty() || class_name.empty()) return false;
    try {
        json r = db.query_bind(
            "SELECT 1 FROM teacher_classes tc JOIN classes c ON tc.class_id = c.id "
            "WHERE tc.teacher_id = ? AND c.name = ? LIMIT 1",
            {SqliteDb::Bind(teacher_id), SqliteDb::Bind(class_name)});
        return !r.empty();
    } catch (const std::exception& e) {
        Logger::error(std::string("归属判定查询失败（按班级）：") + e.what());
        return false;
    } catch (...) {
        Logger::error("归属判定查询失败（按班级）：未知异常");
        return false;
    }
}

bool teacher_owns_student(const string& teacher_id, const string& student_id) {
    if (teacher_id.empty() || student_id.empty()) return false;
    try {
        // 一次查询完成「学生 → 班级 → 该教师是否任课」的整条判定
        json r = db.query_bind(
            "SELECT 1 FROM users s "
            "JOIN classes c ON c.name = s.className "
            "JOIN teacher_classes tc ON tc.class_id = c.id "
            "WHERE s.id = ? AND tc.teacher_id = ? LIMIT 1",
            {SqliteDb::Bind(student_id), SqliteDb::Bind(teacher_id)});
        return !r.empty();
    } catch (const std::exception& e) {
        Logger::error(std::string("归属判定查询失败（按学生）：") + e.what());
        return false;
    } catch (...) {
        Logger::error("归属判定查询失败（按学生）：未知异常");
        return false;
    }
}
