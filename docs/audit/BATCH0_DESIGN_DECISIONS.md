# 批次 0 设计决策裁决（BATCH0_DESIGN_DECISIONS）

- 任务：team `audit-batch0-remediation` 任务 **T1**（requirements-architect）
- 产物性质：**决策文档**。只新增本文件，不修改任何源码、配置或构建产物。
- 权威依据：`docs/audit/VERIFICATION.md` > `CODE_QUALITY_REVIEW.md`（§6 路线图、§4.2、§7.1）> `CODE_QUALITY_SUMMARY.md`。
- 行号口径：全部 `文件:行号` 由我在当前工作区用 read/grep 核对（核对清单见 `docs/audit/BATCH0_CONTRACT.md` §6）。
- **本文件是「实现前必须定论」的 4 个决策的唯一结论**。下游任务（T2–T12）**不得**再做架构推断；若认为某决策不可行，必须先回报 T1，不得自行改判。

---

## 决策总览

| # | 决策 | 唯一结论 | 被否方案 | 主要受影响任务 |
|---|---|---|---|---|
| ① | B1 存量 `pbkdf2$` 数据处置 | **一律视为失效并强制重置；同时移除「登录成功即自动升级」链路**。因工作区无任何数据库文件，本机不执行数据迁移，只交付运维强制重置流程与本机可验证的失效判据 | (a) 保留升级链 + 运行时标记；(b) 自动批量重写为重算哈希；(c) 本机编写并运行一次性迁移脚本 | T4、T7、T10、T12 |
| ② | B2 落库与接口形态 | **`users` 表加单列 `must_change_password INTEGER NOT NULL DEFAULT 0`**；**双中间件统一门禁**；**专用改密接口 `POST /api/auth/change-password`**；旧库 `ALTER TABLE` 兼容迁移；随机口令只在控制台一次性输出 | (a) 独立 `password_change_requests` 表；(b) 独立 `sessions.must_change_password` 列；(c) 复用 `/api/admin/users/reset-password` 做自助改密；(d) 仅在登录响应里加标志、由前端阻断 | T7、T8、T10、T11 |
| ③ | F13 默认口令在产物中的处置 | **登录页 4 个 `fillTestAccount` 按钮收进 mock/演示门控**（保留演示能力）；**`release.yml` Release Notes 的 3 行明文口令改写为「首次启动按控制台提示」**；**后端源码与兜底表内的明文口令种子彻底移除** | (a) 直接删除 4 个按钮；(b) 保留按钮但改为提示文字；(c) 只改 Release Notes 不改前端；(d) 删除 Pages 演示站 | T7、T8、T10 |
| ④ | B6 修复取向 | **本批取「erase 后 clear + 重建用户索引」**（新增 `rebuild_user_indexes()`）；彻底版（索引改存稳定键 / `deque` / `vector<unique_ptr>`）**留批次 1-3** | (a) 现在就改稳定键；(b) 只修 `remove_user_index` 让它做「后续下标 -1」；(c) 删除路径改调 `init_indexes()`；(d) `erase` 改为「按 id 交换到末尾再 pop_back」 | T9、T10、T12 |

---

## 决策① — B1 存量 `pbkdf2$` 数据的处置

### 必须与决策同读的事实

| 事实 | 证据（我实测） |
|---|---|
| 工作区**不存在任何数据库文件** | `Get-ChildItem -Recurse -Force -File -Include *.db,*.db-wal,*.db-shm,*.sqlite,*.sqlite3` → 空；`config.json:8` 的 `database.path = "campus_system.db"` 指向的文件不存在 |
| 因此**本机没有可迁移的存量数据** | 同上（这是可直接验证的否定事实，非 UNVERIFIABLE） |
| 后门当前**尚未武装**，但一次合法登录即永久打开 | 种子账号是裸 SHA256（`main.cpp:250-254` + `models.cpp:17-22`）→ 走 `sha256.h:187` 的正确分支；`routes_public.cpp:45-51` 在「匹配成功且哈希非 `pbkdf2$` 前缀」时**就地**改写为坏哈希并落库 |
| 主键是 TEXT，删除/重建 DB 会导致**用户与积分历史丢失** | `main.cpp:80` `id TEXT PRIMARY KEY`；`main.cpp:63-72` 已有「检测到旧 schema 就 `std::remove()` 删库重建」的**危险先例**（`server.log:22809` 证明该路径真实执行过） |
| `verify_password` 的 `pbkdf2$` 分支依赖 `salt`/`iters` 字段 | `sha256.h:169-188` |

### 关键分析：为什么「运行时判别存量坏哈希」是不可行方案

理论上有一种「精巧」的处置：区分「好 pbkdf2」与「坏 pbkdf2」，只作废后者。**必须否掉它**，理由是**信息论上不可判别**：

- 坏实现签名为 `F(salt, iters)`，好实现签名为 `F(password, salt, iters)`；
- 存储串只有 `pbkdf2$<iters>$<salt_hex>$<dk_hex>`（`sha256.h:162-166`），**没有版本标记**；
- 好/坏实现写入的 `iters` 都是 `100000`、`salt` 都是 16 字节随机 hex → **两者格式完全同构**；
- 故对一条既存记录，**无法在不掌握原口令的前提下判断它是好哈希还是坏哈希**。任何「自动重写」都只能对「已知原口令」的记录生效，而这恰好不成立（否则就不需要重置了）。

**推论**：处置只能是「**一律失效 + 由人重置**」，或者「什么都不做（保留后门）」。后者不是选项（B1 是 blocker）。

### 唯一结论

**①-A 数据处置策略：所有既存 `pbkdf2$` 记录一律视为失效。**

- **本机不执行任何数据迁移**（无数据库文件，无可迁移对象）。实现者**不得**为此编写「一次性迁移脚本并运行」——那会制造「迁移已验证」的假象。
- **交付物是运维流程而非代码**：在 `BATCH0_CLOSURE.md`（T12）中写明操作规程：
  1. 先修 B1（T4）→ 再改默认口令（T7）；顺序见 `BATCH0_CONTRACT.md` §1。
  2. 存量库升级步骤（运维侧，不属本批代码）：启动新版 `server.exe` → 用管理员会话逐个或批量调用 `POST /api/admin/users/reset-password`（`routes_admin.cpp:838` 起，支持 `auto_generate`）重置全部受影响账号 → 被重置账号**强制首登改密**（决策② 的机制）。
  3. 若运维需要识别「哪些账号曾走过升级链」：**无法从库内判别**（见上）；因此**默认全部重置**，不做选择性处置。
- **代码侧的判据**：`verify_password` 对 `pbkdf2$` 分支必须 fail-closed（畸形输入返回 `false`），且**迭代次数非法（`<= 0` / 非数字 / 超上限）一律返回 `false`**（`BATCH0_CONTRACT.md` V1-5）。注意：**不得**用「迭代次数小于阈值即判坏哈希」来区分好坏——好/坏记录的 `iters` 相同（`100000`），该判据**无效**，会误伤全部合法记录。

**①-B 移除「登录成功即自动升级」链路。**

- 删除 `routes_public.cpp:47-51` 的 `if (password_match && hash 非 pbkdf2$ 前缀) { hash_password(password) … save_user_to_db }` 整段（保留 `:45` 的 `verify_password` 调用与 `:52-57` 的失败分支）。
- 理由：这段代码的功能是「把裸 SHA256 就地升级为 pbkdf2」。在 B1 修复前，它是**提权链本身**（把合法账号永久变为任意口令可登录）；在 B1 修复后，它虽然不再产生坏哈希，但**本批同时要移除裸 SHA256 种子**（决策③/契约 3.7），于是**不存在**需要升级的裸哈希记录 → 该段成为死代码，且是「静默改写口令哈希」的高风险路径。
- 兼容性代价（必须如实记录）：保留 `verify_password` 的旧 SHA256 校验分支（`sha256.h:186-187`），使**历史上已存在的裸 SHA256 库**仍能登录；但**不再自动改写**。这类记录由运维按 ①-A 的流程重置。**这是有意接受的兼容性取舍**，`BATCH0_CLOSURE.md` 需列为残余风险。
- **不得**顺手把 `sha256.h:186-187` 的 `std::string::operator==` 改成常量时间比较——那属 B20（low，未独立复核），超出批次 0 的 `inScope`。

### 被否方案与否决理由

| 方案 | 否决理由 |
|---|---|
| (a) 保留升级链 + 运行时打标记 | 升级链是提权链；且没有可靠的标记载体（见决策②的载体对比）。保留 = 让 B1 的放大器继续存在 |
| (b) 自动批量重写为重算哈希 | 见上文「不可判别」论证：无法对未知原口令的记录重写；能做的是「用随机口令重置」，那本质就是 ①-A + 运维流程，不应伪装成「迁移脚本」 |
| (c) 本机编写并运行一次性迁移脚本 | 工作区无数据库文件 → 脚本无从验证，只会产出「迁移成功」的假证据（违反「严禁把静默跳过当通过」的纪律） |
| (d) 直接删除数据库文件重建 | `main.cpp:63-72` 已有此先例，属 B27（low，但**已在生产中真实执行过**）。本批不新增此类自动破坏行为；重置必须由运维显式发起 |

---

## 决策② — B2「首启随机口令 + 强制首登改密」的落库与接口形态

### 载体对比（为何选 `users` 单列）

| 载体 | 覆盖「登录后、未改密前」的每个业务请求？ | 旧库迁移成本 | 与登出/会话重建的关系 | 判定 |
|---|---|---|---|---|
| **`users.must_change_password INTEGER NOT NULL DEFAULT 0`** | ✅ 中间件本来就要查会话与用户（`auth.h:323-364`、`:369-402` 两条路径都会拿到 `user_id`），一次 `find_user_by_id` 即可判定 | ✅ 一条 `ALTER TABLE users ADD COLUMN must_change_password INTEGER NOT NULL DEFAULT 0;`（SQLite 对带 `DEFAULT` 的加列是就地元数据变更，不重写全表） | 与会话无关：改密成功后置 0，**所有会话立即放行** | **采用** |
| 独立 `password_change_requests` 表 | ✅ 需要额外查表（多一次往返） | ❌ 建表 + 状态机 + 过期清理 | 需与 users 保持一致（双写风险） | 否决：为一个布尔状态引入状态机 |
| `sessions.must_change_password` | ❌ **漏判**：用户可登出再登录，新会话若忘了带该列就被放行；且多次登录会派生多个会话副本 | 需改 `create_session`（`auth.h:190-217`）签名与表结构 | 与「改密后清标记」冲突（要清全部会话） | 否决：状态本就属于用户而非会话 |
| 仅登录响应带标志 + 前端阻断 | ❌ 纯前端遮挡，直接调接口即绕过（`curl` 一行） | 低 | — | 否决：不满足「不可绕过」的判据 |

### 唯一结论（落库形态）

**②-1 表结构**

```sql
-- 新库：直接进 main.cpp:78-273 的 init_sql 的 users 定义（main.cpp:79-89）
CREATE TABLE IF NOT EXISTS users (
    id TEXT PRIMARY KEY,
    username TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    role_id INTEGER NOT NULL,
    name TEXT NOT NULL,
    className TEXT,
    points INTEGER DEFAULT 0,
    must_change_password INTEGER NOT NULL DEFAULT 0,   -- 新增
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

**②-2 旧库兼容迁移（不得删库）**

- 在 `main.cpp` 的 `db.isOpen()` 块内、执行 `init_sql` **之前**，用 `PRAGMA table_info(users)` 检查 `must_change_password` 是否存在（**可复用 `main.cpp:26-42` `users_table_is_old_schema()` 的同款写法**）；不存在则执行：
  `ALTER TABLE users ADD COLUMN must_change_password INTEGER NOT NULL DEFAULT 0;`
- 迁移是**幂等**的：第二次启动 `PRAGMA table_info` 已含该列 → 不再 ALTER。
- `users` 表不存在时（全新库）：`PRAGMA table_info` 返回空 → **跳过 ALTER**，让 `init_sql` 建出含新列的完整表。**不得**把「表不存在」当成错误退出。
- **禁止**任何 `DROP TABLE` / `std::remove(db_path)` 形式的迁移（`main.cpp:63-72` 的旧路径保持原样不动，属 B27，不属本批）。
- 迁移后**默认值语义**：`DEFAULT 0` 意味着「旧库里所有既有账号默认不强制改密」。这是**有意选择**：既有账号的哈希状态未知（决策① 已论证不可判别），批量强制改密会锁死生产；运维按 ①-A 的流程显式重置时才置 1。**本批不对既有库做批量置 1。**（诚实记录：这是一处被接受的残余风险）
- 种子账号在**全新库首启**时以 `must_change_password = 1` 插入（见 ②-3）。

**②-3 首启随机强口令写入形态**

- 判定「首次创建」：在种子插入**之后**、或在插入前先查 `SELECT COUNT(*) FROM users` → 0。
  **契约选择**：在 `init_sql` 的种子段以参数化 `INSERT OR IGNORE` 写入，**随机口令在 C++ 侧生成**（`init_sql` 是静态 SQL 串，无法携带运行时口令）→ 因此**必须把 `main.cpp:250-254` 的种子段从 `init_sql` 中移出**，改为 `db.execute(init_sql)` 成功后单独执行：
  1. `SELECT COUNT(*) AS n FROM users`（`db.query`）；
  2. 若 `n == 0` → 生成 4 个互不相同的随机强口令（判据：长度 **≥ 12**、至少含大写/小写/数字各一；现有 `generate_random_password()`（`sha256.h:200-227`）长度仅 8~12，**必须提高下限或改用等强度生成器**）；
  3. 对每个账号执行参数化 `INSERT OR IGNORE INTO users (id, username, password_hash, role_id, name, className, points, must_change_password) VALUES (?,?,?,?,?,?,?,1)`，`password_hash = hash_password(明文口令)`；
  4. **只在标准输出以一次性明文块输出 4 组 `用户名 / 口令`**（`std::cout`，UTF-8 控制台已在 `main.cpp:45-47` 设置），**日志（`Logger::*`）不得出现明文口令**；不得写入任何文件。
- 若 `n > 0`（非首次启动）→ **不生成、不输出、不改写任何口令哈希**。

**②-4 强制首登改密的门禁（服务端、不可绕过）**

新增一个共享判定（放在 `auth.h`，与两条中间件同文件）：

```cpp
// 返回 true 表示「已放行」；返回 false 表示「已写响应，调用方立即 return」
inline bool enforce_password_change(const httplib::Request& req, httplib::Response& res,
                                    const std::string& user_id);
```

- 判定：`find_user_by_id(user_id)` → `u->must_change_password != 0`。为 0 → 放行。
- 为 1 时，**白名单**（按 `req.method + req.path` 精确匹配）：
  - `POST /api/auth/change-password`
  - `POST /api/auth/logout`
  - `GET  /api/auth/me`
  其余一律拒绝：`res.status = 403` + `{"code":403,"msg":"首次登录必须修改密码后才能使用其他功能","must_change_password":true}` + `set_cors_headers`。
- **接入点（两处，缺一不可）**：
  1. `auth.h:355`（`check_permission_middleware` 中 `Auth::check_permission` 调用**之前**）；
  2. `auth.h:392` 之后（`check_parent_auth_middleware` 在确认是家长会话**之后**、`return user_id` **之前**）。
- **不得**只在一处接入：家长端（`/api/parent/*`）走的是第二条中间件（`auth.h:369-402`）。种子里的 `parent-001` 就是家长账号。
- 若某路由**不使用**这两条中间件（例如 `routes_public.cpp` 的公开接口、`routes_teacher.cpp`/`routes_admin.cpp` 中个别只 `require_csrf` 的路径），T7/T10 必须逐条枚举并确认它们不泄露数据。**已知需要点名核对的清单**：`routes_public.cpp` 中 `:11` 起的全部 handler（登录/注册等公开接口，可接受）、`routes_static.cpp:73` 的 `/`. `/assets/.*`（静态资源，可接受）。

**②-5 改密接口形态（唯一契约，T8 按此实现前端）**

- 路由：`POST /api/auth/change-password`（放在 `routes_public.cpp`，与 `/api/auth/logout` 同文件；属于 `routes_public` 的 `inScope`）
- 请求 body（JSON）：`{ "old_password": string, "new_password": string }`
- 必需校验（顺序固定，任一失败即非 200 且不落库）：
  1. 会话有效（`get_cookie_value(req,"sid")` + `verify_session`，`auth.h:220-237`）；失败 → `401` + `{"code":401,"msg":"未登录"}`；
  2. `require_csrf(req,res)`（写接口必须，`auth.h:180-187`）；
  3. 新口令长度 ≥ **8**（本契约取值，**不得低于 6**；与既有 `routes_admin.cpp:866` 的 6 位校验不冲突，因为那是**管理员重置**路径，见 ②-6）；
  4. 新口令 ≠ 原始口令（新旧不同）；
  5. `verify_password(old_password, user->password_hash)` 必须为 true；
  6. 新口令 ≠ `old_password` 的**明文比较**（第 4 条的实现方式；不得依赖哈希比较）。
- 成功动作（**必须是单条 SQL**，避免「哈希已改、标记未清」窗口）：

  ```cpp
  db.execute_bind("UPDATE users SET password_hash = ?, must_change_password = 0, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
                  { SqliteDb::Bind(new_hash), SqliteDb::Bind(user->id) });
  ```
  同时同步内存：`user->password_hash = new_hash; user->must_change_password = 0;`
- 响应：`{"code":200,"msg":"密码修改成功"}`
- 失败响应：`{"code":400,"msg":"<可读原因>"}`（原口令错误 / 新口令过短 / 新旧相同 三类原因必须可区分，便于前端提示与 T10 判定）。

**②-6 与既有密码路径的一致性（必须一并调整，否则机制可被绕过）**

| 既有路径 | 现状（实测） | 本批要求 |
|---|---|---|
| 管理员新增用户 `POST /api/admin/users` | `routes_admin.cpp:469` 起；`:514` 用管理员传入的明文口令 `hash_password(password)` | **不强制首登改密**（管理员当面交付口令，置 0）。`User` 结构体新增字段的**默认值必须是 0**（见 ②-7），此处无需改动 |
| 管理员重置口令 `POST /api/admin/users/reset-password` | `routes_admin.cpp:838-916`；`auto_generate` 时用 `generate_random_password()`（`:862-863`）；口令经**响应体**回给管理员（`:900-909`） | **置 `must_change_password = 1`**：该口令是管理员代设的、用户未必知道 → 用户用旧口令登录后必须改密。改法：`:884-886` 的 `UPDATE` 增加 `, must_change_password = 1`，同步 `user_it->must_change_password = 1` |
| 教师新增学生 `POST /api/teacher/students` | `routes_teacher.cpp:103` 起；`:132` `generate_random_password()`、`:136` `hash_password(default_password)`、`:133-142` 的 `User` 聚合初始化 | **置 `must_change_password = 1`**（口令由服务端生成后交给教师转达，用户未必知道）。注意：`:133-142` 是**位置初始化**，按 ②-7 的约束新字段在末位，此处需改为逐个字段赋值或补上第 9 个实参 `1` |
| 批量导入 `POST /api/admin/import` | `routes_admin.cpp:1501` 起、`:1470`、`:1491-1492` | 若导入的明文口令由**对方**设定则置 0；若由系统生成（`:1491` 分支 `generate_random_password()`）则置 **1**。实现者须逐分支判断并在报告中列出所选取舍 |
| 注册 `POST /api/auth/register` | `routes_public.cpp:109` 起、`:159` `hash_password(password)` | 用户自设口令 → 置 **0** |
| 家长端 | `routes_parent.cpp:73` 走 `verify_password` | 门禁由 `check_parent_auth_middleware` 覆盖；家长端**本批不提供**改密入口 → 实际上会 403 且无法自助改密。**这是必须记录的缺口**：T7 至少要让家长端返回可读提示（例如引导联系管理员），并在 `BATCH0_CLOSURE.md` 列为残余风险 |

> 注：`routes_admin.cpp` 属 T7 的 `inScope`；`routes_teacher.cpp` 的 `:136` 不在 T7 显式 `inScope` 列表内。T7 若需要该点，**必须**把它作为「为满足强制改密不可绕过性而必需的最小改动」写进报告（属 `inScope` 的必然延伸）；若 T7 判定不改，则 **T11 必须以「教师代设学生口令未置需改密」开 finding**。契约采前者（改）。

**②-7 内存结构同步（含一条会导致编译失败/静默错位的硬约束）**

- `models.h:18-27` 的 `struct User` 新增字段，**必须追加在结构体最末**（即 `string student_id;` 之后），并**必须**带默认初始化器：

  ```cpp
  struct User {
      string id;
      string username;
      string password_hash;
      int role_id;
      string name;
      string className;
      int points;
      string student_id;      // 学号（学生角色专用）
      int must_change_password = 0;   // 新增：必须置于末位
  };
  ```

  **为什么必须放末位**：仓库里存在**聚合位置初始化**，例如 `models.cpp:17-22` 的兜底表（8 个值逐位对应）与 `routes_teacher.cpp:133-142`：

  ```cpp
  User new_user = { new_id, studentId, hash_password(default_password), 3, name, className, points, studentId };
  ```

  这类初始化按**位置**绑定。若把新字段插在中间，所有位置初始化会**整体错位**（`student_id` 会被写成 `must_change_password`）——在 C++17 下 `string` 与 `int` 类型不兼容，多半编译报错；一旦恰有隐式转换（如 `int` ↔ `string` 的某组重载）则可能**静默错位**，属最难排查的一类缺陷。放末位则既有初始化全部保持原有绑定，新字段被值初始化为 0（即「默认不强制改密」），与 ②-6 的语义一致。
- `models.cpp:58-74` `load_users_from_db()` 的 `SELECT` 增加 `must_change_password` 列，并 `u.must_change_password = row.value("must_change_password", 0);`。
- `models.cpp:17-22` 的内存兜底表按决策③ 把 `password_hash` 改为空串；`must_change_password` 因末位默认值即为 0，无需显式书写（写了也无害）。

**②-8 随机口令「只输出一次」的精确定义**

- 以**进程生命周期**为单位：同一进程内，首次创建数据库时输出一次。
- 重启后：`users` 表非空 → **不输出、不轮换**。
- **禁止**每次启动重新生成并输出（那会让日志成为口令泄露面，且造成「口令持续轮换」）。
- 输出格式（供 T10 判定与运维使用）：

  ```
  ============ 首次初始化：以下初始口令仅显示一次，请立即记录并登录后修改 ============
  admin   / <16 位随机>
  teacher / <16 位随机>
  student / <16 位随机>
  parent  / <16 位随机>
  ================================================================================
  ```

  口令长度与字符类由生成器决定（≥12，三类齐备）；4 个值两两不同。

### 被否方案与否决理由

| 方案 | 否决理由 |
|---|---|
| (a) 独立 `password_change_requests` 表 | 为布尔状态引入状态机与过期清理；仍要在请求路径上多查一次表；双写不一致风险 |
| (b) `sessions.must_change_password` | 用户登出再登录即产生新会话，漏判是本方案的固有形态；改密后需清所有会话副本 |
| (c) 复用 `POST /api/admin/users/reset-password` 做自助改密 | 该接口要求 `user:manage`（`routes_admin.cpp:842`），**普通用户调用会被 403**；且它由管理员指定 `user_id`，语义是「代设」而非「自改」，无法校验原口令。用它做自助改密 = 让任何管理员（或越权的教师）改任意人口令，且用户永远无法自救 |
| (d) 仅登录响应加标志、前端阻断 | 一行 `curl` 即绕过；不满足「未改密前业务接口被拒绝」的判据 |
| (e) 新老用户一律置 1 | 会锁死全部既有生产账号（且其哈希状态不可判别，见决策①），运维成本与风险都不可接受 |

---

## 决策③ — F13 默认口令在产物中的处置

### 事实（实测）

| 位置 | 内容 |
|---|---|
| `frontend/src/pages/Login.vue:177-210` | 「演示账号 · 一键填充」区块 + 4 个按钮：`:181` `fillTestAccount('admin','admin123')`、`:188` `('teacher','teacher123')`、`:195` `('student','student123')`、`:202` `('parent','parent123')` |
| `.github/workflows/release.yml:164-167` | Release Notes 明文 3 行：管理员/教师/学生 = `admin123`/`teacher123`/`student123` |
| `main.cpp:250-254`、`models.cpp:17-22` | 4 组同名账号的裸 SHA256 种子哈希（与上表口令一致，审计用 .NET SHA256 独立复算四组全 MATCH） |
| `frontend/src/mock/index.ts:657-659` | `isMockEnabled()` → `import.meta.env.VITE_USE_MOCK === 'true'`（**已存在**的 mock 门控，可直接复用） |
| `frontend/vite.config.ts:12-15` | `VITE_USE_MOCK` 的 define；`pages.yml:41-47` 显式设 `'true'`（Pages 演示站），`release.yml:53-55` 不设（正式包） |
| `frontend/package.json:6-12` | 无 test、无 lint（与 F13 无关，仅记录） |

### 性质口径（必须写准，禁止夸大）

- 这 **不是**前端注入，**不是**鉴权绕过，**不是**「后端校验只在前端做」。
- 它是「**可预测默认凭据随所有产物分发 + 无强制首登轮换机制**」。危害完全由 **B2** 承载，**单修 F13 收益≈0**。
- 因此处置必须与 B2 在同一批次完成（顺序门禁见 `BATCH0_CONTRACT.md` §1），且**不得**只改文案而不改 B2。

### 唯一结论

**③-1 登录页 4 个按钮 → 收进 mock/演示门控（不删除演示能力）**

- 在 `Login.vue` 中以 `isMockEnabled()`（`mock/index.ts:657`）作为 `v-if` 条件包裹整个「演示账号 · 一键填充」区块（`Login.vue:177-210`），使：
  - `VITE_USE_MOCK === 'true'`（Pages 演示站）→ 区块照常显示，演示能力保留；
  - 正式发布包 / 开发包（未设 `VITE_USE_MOCK`）→ 区块**不渲染**，产物中不再暴露可预测默认凭据。
- 引入方式：`import { isMockEnabled } from '../../mock'`。**注意副作用评估**：`api.ts:15` 已经静态 `import { isMockEnabled, mockRequest } from '../mock'`，`Login.vue` 新增同源导入**不会**改变既有 tree-shaking 结论（mock 模块本就通过 `api.ts` 进入依赖图）；但 T8 **必须**用「非 mock 构建产物的 mock 特征串 miss」判据（`BATCH0_CONTRACT.md` V2-5）实证，**不得**仅凭推理断言。
- 若该 `import` 导致非 mock 产物体积显著变化或引入 mock 代码，T8 须改用等价方案（例如 `import.meta.env.VITE_USE_MOCK === 'true'` 直接在模板条件中判定），并在报告中说明依据。
- **禁止**：直接删除 4 个按钮（会使 Pages 演示站失去一键体验，且与 `pages.yml:73-77` 的演示说明不一致）；**禁止**保留按钮但把口令改成假的/提示文字（那是「假功能」，审计已就同类做法（F10）立过 finding）。

**③-2 `release.yml` Release Notes 改写**

- 把 `release.yml:164-167` 三行替为（唯一文本，T8 照抄）：

  ```
  ## 首次启动
  - 运行 `server.exe` 后，控制台会输出 4 个初始账号的**一次性随机口令**（仅显示一次，请立即记录）
  - 首次登录后必须修改密码才能使用其他功能
  - 请勿使用任何固定默认口令（本项目不再内置可预测默认口令）
  ```

- **不得**在 Release Notes 中保留任何明文口令，**不得**写入任何「固定默认口令」的暗示（包括 `xxx123` 形式的示例）。
- `release.yml` 的 `## 快速开始`（`:159-162`）与其余段落保持不变。

**③-3 后端内不再存在可预测默认凭据（与 T7 联动，重复强调）**

- `main.cpp:250-254` 的 4 组硬编码哈希由 T7 移除（改运行时随机口令 + 参数化写入）。
- `models.cpp:17-22` 的 4 组硬编码哈希由 T7 改为空串哈希（不可登录）。
- 验收判据（T10 独立执行）：`grep -n '240be518fabd2724\|cde383eee8ee7a44\|703b0a3d6ad75b64\|82e3edf5f5f3a46b' *.cpp *.h` → **零命中**。

**③-4 Pages 演示站不受本决策影响**

- `pages.yml:41-47` 继续以 `VITE_USE_MOCK: 'true'` 构建（有意设计）；`pages.yml:73-77` 的输出文案继续说明「Demo 账号（任意密码均可登录）」——**这是 mock 层的行为，与后端无关**，**不得**把它当成「固定默认口令分发」。T10/T11 不得据此开 finding。

### 被否方案与否决理由

| 方案 | 否决理由 |
|---|---|
| (a) 直接删除 4 个按钮 | 破坏 Pages 演示站的核心体验（`pages.yml` 的部署说明与 Demo 账号提示随之失真）；且「演示」是既有有意设计 |
| (b) 保留按钮、把口令改成「请查看控制台提示」 | 按钮的语义就是「一键填充」，改成提示文字后按钮无功能 → 触发审计已立过的「假按钮/假功能」模式（F10）；不如门控 |
| (c) 只改 Release Notes 不动前端 | 正式包前端仍暴露 4 个可预测口令 → 不满足「正式发布包不再暴露可预测默认凭据」 |
| (d) 删除 Pages 演示站 | 超出批次 0 范围；`pages.yml` 的 mock 属有意设计（`CODE_QUALITY_REVIEW.md:513`） |

---

## 决策④ — B6 修复取向：本批取「先 clear 再重建索引」

### 事实（实测）

- 索引存的是 `users` **向量下标**：`models.cpp:164-169`（`user_id_map`、`user_username_map`），`models.cpp:205-212` `find_user_by_id` 用 `&users[it->second]` 返回**裸指针**。
- 删除点：`routes_admin.cpp:826-827`、`routes_teacher.cpp:259-260`——两处都只 `users.erase(...)` + `remove_user_index(...)`，**不修正后续元素下标**。
- `init_indexes()`（`models.cpp:171-186`）**无 `clear()`**，且对 `role_permission_map` 用 `push_back`（`:183-185`）。
- 结果：删中间元素后，`user_id_map["u3"]` 仍指向旧下标 → `find_user_by_id("u3")` 返回**另一个用户**（越权/串号），且错位被固化。
- 批次 1 的路线图条目 **1-3「B6（彻底版）：索引改存稳定键（或 `deque`/`vector<unique_ptr>`），取代 0-9 的临时修复」** 且**依赖 1-2 的锁方案**（`CODE_QUALITY_REVIEW.md:565`）。

### 唯一结论

**本批实现「erase 后 clear + 重建用户索引」，彻底版留批次 1-3。**

**④-1 新增函数（唯一命名与语义）**

```cpp
// models.h（声明）
// 删除用户后调用：清空并重建用户索引，消除下标错位。
// 批次 0 临时修复；批次 1-3 将由「索引改存稳定键」彻底取代（见 CODE_QUALITY_REVIEW.md 条目 1-3）。
void rebuild_user_indexes();
```

```cpp
// models.cpp（实现）
void rebuild_user_indexes() {
    user_id_map.clear();
    user_username_map.clear();
    for (size_t i = 0; i < users.size(); i++) {
        user_id_map[users[i].id] = i;
        user_username_map[users[i].username] = i;
    }
}
```

**④-2 调用点（两处，必须都在 `users.erase(...)` 之后）**

- `routes_admin.cpp:826-827` → `users.erase(user_it);` 之后调用 `rebuild_user_indexes();`（原 `remove_user_index(deleted_id, deleted_username);` 可删或保留，保留亦无害，但须在报告中说明取哪一种）
- `routes_teacher.cpp:259-260` → `users.erase(it);` 之后调用 `rebuild_user_indexes();`

**④-3 明确不做的事（本批的 `outOfScope`，必须写进实现者报告）**

- 不改索引键类型（仍存下标）→ 同源的 **B5（悬垂指针）** 与 **N2（并发重复 ID）** 仍然存在；
- 不引入任何互斥设施（属批次 1-1/1-2）；
- 不改 `find_user_by_id` 的返回类型（`User*` → `std::optional<User>` 属批次 1-2 的接口语义变更）；
- 不改三个新增用户路径（`routes_admin.cpp:520-521`、`:632-633`、`routes_teacher.cpp:143-144`、`:952-953`）与批量导入（`routes_admin.cpp:1501`，靠 `:1667` 全量重建兜底）；
- 不改改名路径（`routes_admin.cpp:721-735`，其「先 `update_user_index` 再对旧名 `remove_user_index`」顺序**正确**，已核对）；
- **不 commit / 不 push**。

**④-4 必须在文档与注释中标注的因果链**

```
erase 使后续元素下标 -1  →  map 仍存旧下标  →  find_user_by_id 返回错误对象
                                                    ↓
                          越权/串号：对 A 的请求可能作用在 B 上
                                                    ↓
                 本批：erase 后 clear+重建 → 至少保证「不返回错误用户」
                 批次 1-3：索引改存稳定键 → 从类型上杜绝下标错位
```

### 被否方案与否决理由

| 方案 | 否决理由 |
|---|---|
| (a) 现在就改稳定键 / `deque` / `vector<unique_ptr>` | 属批次 1-3，且**依赖 1-2 的锁与连接策略决策**。提前做意味着在没有回归测试（0 测试文件）的情况下改动跨文件的容器语义，风险远超收益；路线图明确把它划为「取代 0-9 的临时修复」（`CODE_REVIEW.md:565`） |
| (b) 只修 `remove_user_index`，让它把「old index 之后的项全部 -1」 | 可行但更易错：需要精确知道被删下标、遍历两个 map、对每个 value 比较；且对「同名/重复 id」等异常状态无定义。相比之下 clear+重建是 O(n) 且**无状态假设**，正确性论证更短 |
| (c) 删除路径改调 `init_indexes()` | ❌ **会产生新缺陷**：`init_indexes()`（`models.cpp:177-185`）对 `role_permission_map` 用 `push_back`，被删用户之后**每次删除都会重复追加**角色权限项（`role_permission_map[2]` 7 → 14 → 21 …）→ 权限判定被污染。**这是最容易被踩的坑，T9/T10/T11 必须以此作为对抗性输入** |
| (d) `erase` 改为「与末尾交换再 `pop_back`」 | 会破坏 `users` 的稳定顺序（接口返回顺序、前端表格顺序随之变化），且仍需修正「被交换元素」的下标——比 clear+重建更复杂，收益只是 O(1) vs O(n)，而在本项目的规模下无意义 |

---

## 与契约的一致性自检

- 本文件 4 个决策已固化进 `docs/audit/BATCH0_CONTRACT.md` 的 §1（顺序门禁）、3.1（决策①）、3.7（决策②③）、3.9（决策④）与 §4 摘要表。
- 实现者若发现任一决策与源码现状冲突（例如 `models.h` 不在其 `inScope`），**必须先回报 T1**，不得自行改判；契约已就该类情形给出边界（见 `BATCH0_CONTRACT.md` §8 第 11、12 条）。
- 本文件与 `BATCH0_CONTRACT.md` 均**只新增文档**：未修改 `*.cpp` / `*.h` / `frontend/**` / `.github/workflows/**`；未执行 `git commit` / `git push` / `git rm`。
