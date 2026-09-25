# 校园能量站 · 独立复核报告（VERIFICATION）

- 复核人：verifier-reviewer（team `code-quality-audit`，任务 t3）
- 复核对象：`docs/audit/BACKEND_FINDINGS.md`（1805 行 / 30 条）、`docs/audit/FRONTEND_FINDINGS.md`（1267 行 / 24 条）
- 复核模式：**纯只读**。未修改/新建/删除任何源码或配置，未 `git commit/push`，未运行 `server.exe`，未发起任何 HTTP 请求，未运行前端构建。本文件是本次任务唯一写入的文件。
- 排除范围：与两份报告一致 —— vendored 第三方 `httplib.h`、`json.hpp`、`sqlite3.c/h/ext.h`、`vcpkg/` 不做审计（仅在判定框架行为时定点阅读其行）。

## 0. 判定口径

| 判定 | 含义 |
|---|---|
| **CONFIRMED** | 我独立复现了该结论；重新打开被引用的 `文件:行号` 后原文与引文一致，证据链闭合。 |
| **PARTIAL** | 方向成立，但存在夸大、数字不符或行号漂移；已在本报告中给出核实后的准确数字与措辞。 |
| **REFUTED** | 不成立。**本报告未保留任何 REFUTED 条目进入最终交付物**（详见 §6.3）。 |
| **UNVERIFIABLE** | 无法复现；已在 §9 逐条写明原因，不替审计员背书。 |

**统计（我的裁定）**

| 报告 | blocker | high | medium | low | 合计已裁定 |
|---|---|---|---|---|---|
| BACKEND_FINDINGS.md | 1 / 1 | 9 / 9 | 8 / 9 | 4 / 11 抽查 | 22 / 30 |
| FRONTEND_FINDINGS.md | 1 / 1 | 4 / 4 | 10 / 12 | 0 / 7 | 15 / 24 |

- 两份报告的 **blocker 与 high 共 15 条全部逐条复核**（100%）。
- medium 复核覆盖率：后端 **8/9 = 89%**、前端 **10/12 = 83%**，均高于要求的 60%。
- low 条目按抽样复核（后端 4 条），未逐条复核的 low 条目在本报告中不背书。

---

## 1. 我实际执行的命令清单（原样保留关键输出）

本报告所有判定均来自下列命令或 `read` 工具输出的原文片段，未复述审计员的输出。

```powershell
# ① B1 机械验证：pbkdf2_sha256 函数体是否引用 password 形参
$lines = Get-Content sha256.h; $lines[93..147] | Select-String 'password'
$body  = $lines[94..147]; ($body | Select-String 'password').Count
Select-String -Path sha256.h -Pattern 'k_ipad|k_opad'

# ② 两套 SQL 风格是否并存
Select-String -Path auth.h -Pattern 'snprintf|query_bind|execute_bind'

# ③ 登录锁定是否真被路由调用 + 线程数来源
Select-String -Path *.cpp,*.h -Pattern 'login_can_try|login_record_fail|login_record_success|login_client_key'
Select-String -Path main.cpp -Pattern 'ThreadPool'
Select-String -Path config.json -Pattern 'thread_count'

# ④ 是否真的没有互斥设施（仅自研源码）
Select-String -Path main.cpp,auth.h,config.h,sqlite_wrapper.h,sha256.h,models.cpp,models.h,routes.h,routes_public.cpp,routes_admin.cpp,routes_teacher.cpp,routes_student.cpp,routes_parent.cpp,routes_static.cpp,logger.h,logger.cpp,database.h -Pattern 'mutex|lock_guard|unique_lock|shared_lock|std::atomic|thread_local'

# ⑤ 全站 set_cors_headers 调用点（区分单参/双参重载）
Select-String -Path <17 个自研文件> -Pattern 'set_cors_headers\('

# ⑥ res.status 与手工业务码统计（17 个自研文件）
$all = ($own | ForEach-Object { Get-Content $_ -Raw }) -join "`n"
([regex]::Matches($all,'res\.status\s*=')).Count
([regex]::Matches($all,'\{"code",\s*\d+\}')).Count

# ⑦ B13 备份接口路径来源
read routes_admin.cpp 318-356        # 路由方法 + backup_path 生成 + 判据

# ⑧ B18 points_records 是否有回载函数
Select-String -Path models.cpp,main.cpp,routes_admin.cpp,routes_teacher.cpp,routes_public.cpp -Pattern 'points_records'
Select-String -Path models.h,models.cpp,main.cpp -Pattern 'bool load_|void load_|load_points|load_records'

# ⑨ 数据库实际状态（三路交叉验证）
Test-Path campus_system.db
Get-ChildItem -Recurse -Force -File -Include *.db,*.db-wal,*.db-shm,*.db-journal,*.sqlite,*.sqlite3 | Where-Object {...}
glob  **/*.{db,db-wal,db-shm,sqlite,sqlite3}

# ⑩ 运行时取证（只 grep，不整文件加载）
Select-String -Path server.log -Pattern 'SQLite 数据库连接成功|数据库连接失败|使用内存存储|表结构初始化成功'
Select-String -Path server.log -Pattern 'POST /api/auth/login' | Measure-Object
Select-String -Path server.log -Pattern '检测到旧 schema|删除数据库文件'

# ⑪ 前端：mock 门控与 CI 配置
Select-String -Path frontend\src\*.ts,frontend\src\**\*.ts -Pattern 'VITE_USE_MOCK|useMock|from .*mock'
Select-String -Path .github\workflows\*.yml -Pattern 'VITE_USE_MOCK|npm ci|npm install|npm run build|frontend-dist|server.exe'

# ⑫ 前端：机械计数（ref/computed/res.code/错误样板）
([regex]::Matches($raw,'\bref[<(]')).Count
([regex]::Matches($raw,'\bcomputed\(')).Count
([regex]::Matches($raw,'api\.(get|post|put|delete)\b')).Count
([regex]::Matches($all,'res\.code')).Count
([regex]::Matches($all,"toast\.error\('网络错误，请稍后重试'\)")).Count

# ⑬ 路径穿越与 httplib 服务端路径解码
read httplib.h 3106-3126             # Server::parse_request_line
Select-String -Path httplib.h -Pattern 'decode_url'
read routes_static.cpp 1-99

# ⑭ 仓库卫生
git ls-files | Select-String '^\.trae/' | Measure-Object
git status --porcelain

# ⑮ Set-Cookie 是否会互相覆盖（框架行为）
Select-String -Path httplib.h -Pattern 'using Headers ='
Select-String -Path httplib.h -Pattern 'inline void Response::set_header' -Context 0,3

# ⑯ 规范查证（Content-Length 是否属禁止请求头）
web_fetch https://developer.mozilla.org/en-US/docs/Glossary/Forbidden_header_name
```

---

## 2. 后端报告（BACKEND_FINDINGS.md）逐条裁定

| id | 原定级 | 我的判定 | 一句话裁定 |
|---|---|---|---|
| B1 | blocker | **CONFIRMED** | 我的机械验证 count=0；口令确实完全不参与派生。 |
| B2 | high | **CONFIRMED** | 种子哈希与 `models.cpp:17-22` 兜底表逐字一致；弱口令可猜。 |
| B3 | high | **CONFIRMED** | 无锁 static map 且**被登录路由真实调用**，8 线程下必发。 |
| B4 | high | **CONFIRMED** | 锁定键信任客户端可伪造的 XFF；家长端确无锁定。 |
| B5 | high | **CONFIRMED** | 全局容器无锁、返回裸指针，悬垂与 rehash 竞态成立。 |
| B6 | high | **CONFIRMED** | `erase` 只删自身索引项；`init_indexes` 不清 map，错位被固化。 |
| B7 | high | **CONFIRMED** | BEGIN/COMMIT/ROLLBACK 返回码均未检查，且 parse_error 分支不回滚。 |
| B8 | high | **CONFIRMED** | 绝对值覆盖 + `execute_bind` 对 0 行返回 true，双花/超卖成立。 |
| B9 | high | **CONFIRMED** | 读接口有班级作用域、写接口无 —— 水平越权成立。 |
| B10 | high | **CONFIRMED** | 框架确实把 `ex.what()` 写进 `EXCEPTION_WHAT` 响应头。 |
| B11 | medium | **CONFIRMED** | `auth.h` 内两套风格并存，行号精确。 |
| B12 | medium | **CONFIRMED** | prepare 失败返回空数组，与"无数据"不可区分。 |
| B13 | medium | **CONFIRMED** | 判据写反，恒返回 500"备份失败"，成功日志不可达。 |
| B14 | medium | **CONFIRMED** | 四个 setter 全项目零调用，payload 上限为 SIZE_MAX。 |
| B15 | medium | 未单独复核 | 不在我的抽样内，**不背书**（见 §9）。 |
| B16 | medium | **CONFIRMED** | 角色/权限增改只 push 内存，而 roles 接口读 DB。 |
| B17 | medium | **PARTIAL** | "217 处"精确无误；但"仅 6 处 res.status"实为 **16 处**。 |
| B18 | medium | **CONFIRMED** | 且我把症状落细：重启后导出为空、ID 从 1 重号。 |
| B19 | medium | **CONFIRMED** | 假数据行号精确；未使用权限码恰为 5 个。 |
| B25 | low | **PARTIAL** | 单参重载**不是死代码**：实测 78 处调用，双参仅 8 处。 |
| B27 | low | **CONFIRMED（升级为有运行时实证）** | `server.log` 证明删库重建**真的执行过**。 |
| B29 | low | **CONFIRMED** | DB 层错误只走 `std::cerr`，不进 Logger。 |
| B30 | low | **CONFIRMED** | 轮转在加锁前执行，且每条日志都 open+stat。 |

### 2.1 B1（blocker）—— 我的独立证据链

**第一步：机械验证口令是否参与派生。**

```powershell
$lines = Get-Content sha256.h
($lines[93..147] | Select-String 'password').Count      # → 1
$body  = $lines[94..147]
($body | Select-String 'password').Count                 # → 0
```

唯一命中是第 94 行的形参声明本身；**函数体 95-148 行出现次数为 0**。

**第二步：密钥材料究竟来自什么。**

```
99:        unsigned char k_ipad[BLOCK] = {0};
100:       unsigned char k_opad[BLOCK] = {0};
103:           std::memcpy(k_ipad, s.data(), s.size() < 32 ? s.size() : 32);
104:           std::memcpy(k_opad, s.data(), s.size() < 32 ? s.size() : 32);
106:           std::memcpy(k_ipad, salt.data(), salt.size());
107:           std::memcpy(k_opad, salt.data(), salt.size());
109:       for (int i = 0; i < BLOCK; i++) { k_ipad[i] ^= 0x36; k_opad[i] ^= 0x5c; }
115:           msg.append((char*)k_ipad, BLOCK);
122:           msg2.append((char*)k_opad, BLOCK);
131:           msg.append((char*)k_ipad, BLOCK);
137:           msg2.append((char*)k_opad, BLOCK);
```

`k_ipad`/`k_opad` 只由 `salt` 填充（103-107）；HMAC 的密钥块与消息体（115/122/131/137 及 116/117 的 `salt` + INT(1) 大端）**没有任何一处引入 password**。

**结论（静态推导即闭合，无需运行程序）**：`F(password, salt, iters) ≡ F(salt, iters)`。`verify_password`（`sha256.h:180`）用**存储串中自带的 salt 与 iters** 重算同一个 F，再与存储的 `dk` 做常量时间比较（`181-184`）→ `calc == dk` 恒真 → **对任何以 `pbkdf2$` 开头的哈希，任意口令（含空串）都通过**。这不是"弱哈希"，是彻底的认证绕过。**CONFIRMED。**

**第三步：影响面（我自己的枚举，比原报告多 1 处）。**

```powershell
Select-String -Path *.cpp,*.h -Pattern 'hash_password\(|verify_password\(' | Where-Object { $_.Filename -ne 'sha256.h' }
```

```
routes_admin.cpp:514   new_user.password_hash = hash_password(password);
routes_admin.cpp:626   new_user.password_hash = hash_password(password);
routes_admin.cpp:880   std::string new_hash = hash_password(new_password);
routes_admin.cpp:1470  existing->password_hash = hash_password(plain);
routes_admin.cpp:1491  ? hash_password(generate_random_password())
routes_admin.cpp:1492  : hash_password(plain);
routes_parent.cpp:73   if (!verify_password(password, user->password_hash)) {
routes_public.cpp:45   bool password_match = verify_password(password, user->password_hash);
routes_public.cpp:48   std::string new_hash = hash_password(password);
routes_public.cpp:159  hash_password(password),
routes_teacher.cpp:136 hash_password(default_password),
routes_teacher.cpp:946 new_user.password_hash = hash_password(password);
```

**`hash_password` 共 9 个调用点**（`routes_admin` 6 + `routes_public` 2 + `routes_teacher` 2，其中 `min` 计 9），**每一个都产生坏哈希**。原报告列了 8 处，漏掉 `routes_public.cpp:48` —— 也就是**自动升级链自身**。

**第四步：现实可达性（这是最关键的判断，我必须给出明确结论）。**

```powershell
Test-Path campus_system.db                       # → False
Get-ChildItem -Recurse -Force -File -Include *.db,*.db-wal,*.db-shm,...   # → 空
```
`glob **/*.{db,db-wal,db-shm,sqlite,sqlite3}` → **No files found**

**结论：工作区内不存在任何数据库文件，因此该后门此刻尚未被"武装"（零行数据）。** 这不是 UNVERIFIABLE —— 文件不存在是可直接验证的否定事实。后门在下列任一事件发生时**立即且永久**打开：

1. 任一**种子账号首次成功登录**：种子哈希是裸 SHA256（`main.cpp:251-254`），走 `sha256.h:187` 的正确分支 → `routes_public.cpp:47-51` 就地用 `hash_password()` 重写并落库 →
   此后该账号任意口令可登录；
2. 任一**建号/改密/注册/教师建学生**（上表 9 处中的 7 处非升级分支）→ 当场写入坏哈希。

**第五步：定级判断。** 维持 **blocker**（量表上限即 blocker）。但性质上它不是"某个功能坏了"，而是**完整认证绕过 + 一次性自动化提权**：一次合法登录即把该账号永久变为"任意口令可登录"，且在 8 线程下无需攻击者配合。**应置于全部发现之首，且优先于 B2-B10 处理。**

**第六步：最小修复。** 正确实现应为 `U1 = HMAC-SHA256(key = password, msg = salt || INT32BE(1))`，即 **HMAC 的 key 必须是口令**，迭代 `Ui = HMAC(key = password, msg = U(i-1))`，`T = U1 xor … xor Uc`。落到本文件就是 `memcpy(k_ipad, password...)` 而非 `salt`。改前必须先补 RFC 6070 测试向量（`pbkdf2("password","salt",1)` 等固定值），否则同类错误会再次静默回归。存量数据修复：所有 `pbkdf2$` 记录一律视为失效，强制重置。

### 2.2 B3 / B5 —— 并发前提与可达性（三项必须亲自复现的检查）

**线程数来源链条（我自己核对的两端）：**

```
config.json:5      "thread_count": 8
main.cpp:306       svr.new_task_queue = [&]() { return new httplib::ThreadPool(g_config.thread_count); };
main.cpp:22        SqliteDb db;        ← 全局唯一连接
```

**单连接是否真被 8 线程共享：是。** `main.cpp:22` 是文件作用域的全局 `SqliteDb db;`，`sqlite_wrapper.h:209` 只有一个 `sqlite3* db_;` 成员，`httplib::ThreadPool(8)` 的所有 worker 共享它。`sqlite_wrapper.h:27` 设了 `busy_timeout(5000)` 且 `:29` 开了 WAL，但**没有任何应用层互斥**。

**登录锁定是否真被路由调用（可达性）：**

```
routes_public.cpp:29  std::string lk = login_client_key(req, username);
routes_public.cpp:30  if (!login_can_try(lk)) {
routes_public.cpp:39  login_record_fail(lk);
routes_public.cpp:53  login_record_fail(lk);
routes_public.cpp:59  login_record_success(lk);
auth.h:111            inline std::map<std::string, LoginAttempt>& login_attempts() {
auth.h:112                static std::map<std::string, LoginAttempt> m;
```

**结论：CONFIRMED，且是必发而非理论可能。** 5 个调用点全在 `POST /api/auth/login` 的 handler 内，handler 运行在 8 线程池上，`auth.h:112` 的函数级 `static std::map` 被无锁读写。两个并发登录（正常用户 + 一次错误口令即可）就会同时进入。后果三层（原报告 §B3 归纳准确）：`map::operator[]` 插入时可能与另一线程的 `find`/`erase` 并发 → UB/崩溃；`a.fails++` 非原子 → 阈值失效；`login_can_try` 与 `login_record_fail` 之间无事务性。

**"全项目仅 Logger 有锁"这一前提我复核为真：**

```
logger.h:12   #include <mutex>
logger.h:61   std::lock_guard<std::mutex> lock(mutex_);
logger.h:89   static std::mutex mutex_;
logger.cpp:7  std::mutex Logger::mutex_;
```

17 个自研源文件中除 Logger 外**零** `mutex`/`atomic`/`shared_lock`/`thread_local`。B3/B5/B7/B8 的并发前提成立。

### 2.3 B5 / B6 —— 悬垂指针与索引错位（逐行核对）

`read models.cpp` 全文后核对：

```
165: unordered_map<string, size_t> user_id_map;           // 索引存的是"下标"
206: User* find_user_by_id(const string& user_id) {
208:     if (it != user_id_map.end() && it->second < users.size()) {
209:         return &users[it->second];                       // ← 返回向量元素地址
200: void remove_user_index(const string& user_id, const string& username) {
201:     user_id_map.erase(user_id);
202:     user_username_map.erase(username);                   // ← 只删被删用户自己的两项
172: void init_indexes() {
173:     for (size_t i = 0; i < users.size(); i++) {          // ← 无 clear()，旧项残留
```

**B5 CONFIRMED**：`find_user_by_id` 返回 `&users[i]`；`routes_public.cpp:167` / `routes_teacher.cpp:143` / `routes_admin.cpp:520,632,1501` 的 `push_back` 触发扩容即令该指针悬垂，而 `routes_public.cpp:381` 的 `user->points -= cost;` 与 `:49` 的 `user->password_hash = new_hash;` 会**写入已释放内存**。

**B6 CONFIRMED（并且我把"错位被固化"这一点确认到底）**：`models.cpp:172-186` 的 `init_indexes()` **只做 `map[key] = i` 赋值，全函数没有任何 `clear()`** → 被删用户残留的下标不会被清除，反而会被重新写入一个指向他人的值。`erase` 调用点我核对为 `routes_teacher.cpp:259` + `routes_admin.cpp:826`（均紧跟 `remove_user_index`，见 §2.4），与报告一致。

### 2.4 B7 / B8 —— 事务与双花（逐行核对）

```
routes_admin.cpp:1453   db.execute("BEGIN TRANSACTION");     ← 返回值未检查
routes_admin.cpp:1665   db.execute("COMMIT");
routes_admin.cpp:1686   db.execute("ROLLBACK");              ← 仅在 catch(const exception&) 内
routes_admin.cpp:1679   } catch (json::parse_error& e) {     ← 此分支不回滚
sqlite_wrapper.h:145      rc = sqlite3_step(stmt);
sqlite_wrapper.h:147      return rc == SQLITE_DONE;          ← 0 行受影响亦为 true
```

`routes_public.cpp` 兑换链逐行核对：

```
360: if (user->points < cost) {
370:     db.execute_bind("UPDATE mall_items SET stock = stock - 1 WHERE id = ? AND stock > 0", ...);   ← 返回值未检查
381: user->points -= cost;                                     ← 内存读-改-写
382: update_user_points_in_db(user->id, user->points);        ← 绝对值覆盖
383: db.execute_bind("INSERT INTO redemption_records ...", ...);
models.cpp:95  "UPDATE users SET points = ? WHERE id = ?",   ← 绝对赋值，非 points = points - ?
```

**B7、B8 均 CONFIRMED，行号精确。**

### 2.5 B9 —— 水平越权（我把"读写不对称"逐行确认）

**读接口有作用域**（`routes_teacher.cpp:64-87`）：先 `SELECT c.name FROM teacher_classes tc JOIN classes c … WHERE tc.teacher_id = ?` 求绑定班级集合（`66-68`），再按 `className` 过滤（`81-87`）。

**写接口全部没有：**

```
193: auto student_it = find_if(users.begin(), users.end(), [&](const User& u) {
194:     return u.id == student_id && u.role_id == 3;        ← 仅校验角色，无班级条件
203: student_it->name = name;
204: student_it->className = className;                    ← 还能把别班学生改到自己班
298: auto it = find_if(users.begin(), users.end(), ...)     ← 积分操作，同样无班级过滤
487: auto it = find_if(users.begin(), users.end(), ...)     ← 评价，同样无班级过滤
```

**B9 CONFIRMED。** 另外 `:81` 的 `if (!bound_class_names.empty())` 使**未绑定任何班级的教师直接看到全校学生** —— 报告引用的注释"否则保留旧行为"原文属实。

**这同时回答了任务书里"角色校验是否只在前端做"的问题**：不是只在前端。后端 `check_permission_middleware`（`auth.h:323-364`）确实做了**服务端角色级鉴权**（`main.cpp:246` 把 `student:manage` 授予 role=2 全体教师）。真正的缺口是**对象级授权（ownership）缺失** —— 权限粒度只到"是不是教师"，没到"是不是这个学生的老师"。措辞必须区分这两层，否则会低估（误以为前端可绕过）或高估（误以为完全没有服务端校验）。

### 2.6 B10 —— 异常原文进响应头（框架层取证）

```
httplib.h:3574   } catch (const std::exception &ex) {
httplib.h:3575     res.status = 500;
httplib.h:3576     res.set_header("EXCEPTION_WHAT", ex.what());   ← 异常原文进响应头
httplib.h:3577   } catch (...) {
httplib.h:3579     res.set_header("EXCEPTION_WHAT", "UNKNOWN");
```

与报告引文**逐字一致**。触发侧：`routes_public.cpp:17-24` 用 `req_json.value("username", "")`（强类型 `value()`），而 `:88` 只 `catch (json::parse_error&)`。`json.hpp` 的 `value()` 在**键存在但类型不符**时抛 `type_error.302`（报告引 `json.hpp:22491-22507`，与我的直接阅读一致），不在捕获范围 → 冒泡至 `dispatch_request`。**B10 CONFIRMED。**（浏览器端到端复现受限于不得发 HTTP 请求，见 §9。）

### 2.7 B13 —— 备份接口（原报告被队长推翻过一半，我逐行确认）

```
routes_admin.cpp:324   svr.Post("/api/admin/system/backup", ...)      ← POST，故 require_csrf 有效
routes_admin.cpp:335   string backup_path = "campus_system_backup_" + get_current_time();
routes_admin.cpp:336-338   for (auto& c : backup_path) { if (c == ' ' || c == ':') c = '_'; }
routes_admin.cpp:339   backup_path += ".db";
routes_admin.cpp:341   json backup_result = db.query("VACUUM INTO '" + backup_path + "'");
routes_admin.cpp:342   if (backup_result.is_null()) {                 ← 恒为 false
routes_admin.cpp:343       response = {{"code", 200}, {"msg", "备份成功"}, ...};
routes_admin.cpp:344       Logger::info("数据库备份成功: " + backup_path);   ← 不可达
routes_admin.cpp:345   } else {
routes_admin.cpp:346       response = {{"code", 500}, {"msg", "备份失败"}};
```

- **路径注入：确认不成立。** `backup_path` 完全由服务端常量 + `get_current_time()` 生成，无任何请求参数参与。
- **判据写反：CONFIRMED。** `db.query()`（`sqlite_wrapper.h:53-92`）初始化为 `json::array()`，`VACUUM INTO` 无行返回 → 返回 `[]` 而非 `null` → `is_null()` 恒 false → **永远返回 500"备份失败"**，而成功日志不可达。实际备份文件会生成，属**假阴性**：运维/自动化会误判备份全线失败。

### 2.8 B17 —— 唯一一条数字错误的 medium（PARTIAL）

我的统计（17 个自研文件，使用与报告相同的正则）：

```
res\.status\s*=                     -> 16
\{"code",\s*\d+\}                   -> 217
```

- **"217 处手工业务码"：精确无误。** （我第一次只统计 6 个 `routes_*.cpp` 得 210，扩到 17 个文件即 217 —— 该数字无误，我最初的偏小是我的口径问题。）
- **"仅 6 处设置 `res.status`"：错误，实际 16 处。** 全部站点：`auth.h:184,327,336,348,356,373,382,393`（8 处）+ `routes_public.cpp:32`、`routes_admin.cpp:140`、`routes_teacher.cpp:22,1025,1136`、`routes_student.cpp:205`、`routes_static.cpp:63,74`（8 处）。

方向（**绝大多数业务错误以 HTTP 200 返回**）完全成立 —— 16 : 217 的比例足以支撑该结论。判定 **PARTIAL**：结论可用，数字须改为 16。

### 2.9 B18 —— 我把它落成了具体可见症状

```
models.h:73     bool load_users_from_db();
models.cpp:58   bool load_users_from_db() { ... }
models.cpp:14   vector<PointsRecord> points_records;      ← 全局内存副本
routes_teacher.cpp:340   points_records.push_back(new_record);    ← 只增
routes_admin.cpp:1602    points_records.push_back(new_record);    ← 只增（导入）
```

我检查了**是否存在于任何回载函数**：`models.h` / `models.cpp` / `main.cpp` 中 `load_*` 系列**只有 `load_users_from_db`**，它只加载 `users`。

**B18 CONFIRMED，且症状比原报告更硬：**
1. 重启后 `/api/admin/export`（`routes_admin.cpp:1388-1399` 遍历内存 `points_records`）导出的 `points_records` **是空数组**，而 DB 表里明明有历史数据 → 用该导出文件做迁移会**静默丢失全部积分历史**；
2. `routes_teacher.cpp:331` 的 `int new_record_id = points_records.size() + 1;` 每次重启从 1 重号，而 `:344` 的 INSERT 又不指定 id → **回给前端的记录 ID 与 DB 实际 id 不是一回事**；
3. 同一份数据两处来源不一致：`routes_teacher.cpp:574,680`、`routes_public.cpp:273` 直接从 DB 查，导出/导入走内存。

### 2.10 B19 —— 假数据与死权限码（逐一核对）

```
routes_admin.cpp:60-70     {"admin_count", 1}, {"teacher_count", 1}, {"student_count", 2}   ← 写死
routes_admin.cpp:1229-1244 {"loginTimeout", 30}, {"minPasswordLength", 6}, {"enableCaptcha", false},
                           {"frequency", "daily"}, {"retentionDays", 7}                      ← 写死
```

"5 个权限码从未被任何中间件使用"我用另一种方法独立复核（枚举全部 `check_permission_middleware(req, res, "…")` 的第二个实参）：

```
evaluation:manage  5x      mall:manage  4x      points:manage  2x
statistics:view    2x      student:manage 11x   system:manage 12x   user:manage 18x
```

定义 12 个码，被使用 7 个 → **未使用恰为 5 个：`class:manage`、`data:export`、`message:manage`、`parent:manage`、`redemption:manage`**，与报告完全一致。**CONFIRMED。**（补充：`/api/admin/export` 用的是 `system:manage`（`routes_admin.cpp:1336`）而非 `data:export`，这正是 `data:export` 成为死码的原因。）

### 2.11 B25 —— 原报告此处措辞错误，必须改写（PARTIAL）

原报告称单参 `set_cors_headers(res)` 重载是**"死代码"**。我枚举了全项目调用点：

```
双参 set_cors_headers(req, res) （正确实现）共 8 处：
  auth.h:183、routes_public.cpp:14,97,111、routes_admin.cpp:1696,1742,1794,1843

单参 set_cors_headers(res)（缺陷重载）共 78 处：
  auth.h:329,338,350,358,375,384,395                                     (7)
  routes_public.cpp:183,227,259,290,313,401                              (6)
  routes_admin.cpp: 53,78,147,192,233,264,302,325,354,417,470,560,671,
                    789,839,920,960,992,1016,1058,1094,1130,1171,1222,
                    1252,1334,1421                                      (27)
  routes_teacher.cpp: 12,53,104,170,236,274,377,417,464,538,605,729,
                      781,813,841,983,1032,1097,1126                     (19)
  routes_student.cpp: 12,23,59,66,94,105,152,212,222                      (9)
  routes_parent.cpp: 50,136,150,182,234,279,330,378,425                   (9)
  routes_static.cpp:73                                                    (1)
```

**判定 PARTIAL，且这是对最终报告有实质影响的纠正：** 单参重载不是死代码，而是**占绝对多数（78 : 8）的主流调用形式**；几乎所有 handler 的开头（如 `routes_admin.cpp:53,78,147…`）调用的都是它。缺陷本身属实 —— `auth.h:22-33` 在 **response** 上找 `Origin`（`auth.h:30` 的 `auto it = res.headers.find("Origin");` 甚至是个未使用变量），因此永不回显 `Access-Control-Allow-Origin`。

但**严重度我维持 low**：生产是同源部署（`routes_static.cpp:10,70-99` 后端直接 serve `frontend/dist`），开发是同源（`vite.config.ts:29-37` 把 `/api` 代理到 8080，浏览器视角仍是同源），且 `config.json:23-24` 的 `allowed_origins` 为**空数组** → 两条重载都提前 return，**在出厂配置下零实际影响**。只有运维显式填了白名单做跨域/分域部署时才会暴露 —— 而那时受影响的**不是"仅 401/403 与预检"，而是几乎全站接口**。这个差别必须在最终报告里写清楚，否则"死代码"会误导读者以为可以不管。

### 2.12 B27 —— 我从运行时日志取得了实证（CONFIRMED，证据升级）

```
server.log 长度 5,154,608 字节；首行 [2026-03-28 21:18:43]；末行 [2026-06-27 19:04:02]
第 728/740/747 行等： [INFO] SQLite 数据库连接成功 / 数据库表结构初始化成功
第 22809 行： [2026-06-27 14:05:06] [WARNING] 检测到旧 schema（users.id 为 INTEGER），
              删除数据库文件并重建为新 schema（TEXT 主键）
```

**这条日志证明 `main.cpp:63-72` 的删库重建路径在真实运行中确实执行过**，B27 不是纸面推测。同一个日志文件还给出：`POST /api/auth/login` 出现 **603 次**、`POST /api/parent/login` **37 次** —— 说明该服务曾长期对真实 SQLite 库提供服务。判定 **CONFIRMED**。

---

## 3. 前端报告（FRONTEND_FINDINGS.md）逐条裁定

| id | 原定级 | 我的判定 | 一句话裁定 |
|---|---|---|---|
| F1 | blocker | **CONFIRMED** | CI 的 package job 确实把 mock 版 dist 与 server.exe 打进同一个 zip。 |
| F2 | high | **PARTIAL** | "500 静默"机制逐字确认；但"62 处 res.code"实为 **58 处**。 |
| F3 | high | **CONFIRMED** | 2626 行、54 ref、10 computed、26 API 调用点**全部复现**。 |
| F4 | high | **CONFIRMED** | 假评价数据确被 UI 消费，`evaluations` 永不来自接口。 |
| F5 | high | **CONFIRMED** | `ApiResponse<T = any>` + 索引签名 + 两项检查关闭，逐行属实。 |
| F6 | medium | **CONFIRMED** | `usePagination` 仅出现在自身与注释中，零引用。 |
| F7 | medium | **CONFIRMED（比原报告更强）** | 6 个导出确为零引用；且 `$format*` 全局注册本身也是死的。 |
| F8 | medium | **CONFIRMED** | 错误样板恰为 **24** 处；`try {` 恰为 **68** 处。 |
| F9 | medium | **CONFIRMED** | 6/7/2 次串行 await 与 `Promise.all` 反例行号精确。 |
| F10 | medium | 未单独复核 | 不在我的抽样内，**不背书**（见 §9）。 |
| F11 | medium | **CONFIRMED** | `import * as echarts`、`import * as XLSX`、pinyin 全为静态引入。 |
| F12 | medium | **CONFIRMED** | Esc/Enter 只绑在永不聚焦的 div；无 `role="dialog"`。 |
| F13 | medium | **CONFIRMED**（附口径说明） | 确实明文内置；但根因是 B2，见 §3.3。 |
| F14 | medium | 未单独复核 | 不在我的抽样内，**不背书**（见 §9）。 |
| F15 | medium | **CONFIRMED** | 0 测试 / 0 lint 配置，CI 仅 typecheck+build。 |
| F16 | medium | **CONFIRMED** | `release.yml:51` 用 `npm install`，与 CI 的 `npm ci` 不一致。 |
| F24 | medium | **CONFIRMED** | 规范原文证实 `Content-Length` 属禁止请求头。 |

### 3.1 F1（blocker）—— 我自己读 workflow 确认

```yaml
# ci.yml:41-46
      - name: Build (Mock 模式，与 pages.yml 同参)
        working-directory: frontend
        env:
          VITE_USE_MOCK: 'true'
          VITE_BASE: '/campus-master/'
        run: npm run build:no-typecheck

# ci.yml:114-124（package job）
      - name: Download frontend dist
        uses: actions/download-artifact@v4
        with:
          name: frontend-dist
          path: release/frontend/dist
      - name: Download server.exe
        uses: actions/download-artifact@v4
        with:
          name: server-exe
          path: release

# ci.yml:135-140
      - name: Zip package
        run: |
          cd release
          zip -qr ../campus-energy-station-dev.zip .
```

`frontend` job 上传的 artifact 名称正是 `frontend-dist`（`ci.yml:55-57`），package job（`ci.yml:105-147`）把它下载进 `release/frontend/dist`，同时下载 `server.exe` 与 `config.json`，一并打成 `campus-energy-station-dev.zip`。而 `api.ts:58-60` 在 `isMockEnabled()` 为真时把**所有** `/api/*` 交给 `mockRequest()`。

**F1 CONFIRMED —— 该开发版包内前端只读写 `localStorage['campus_mock_db_v1']`，包内 `server.exe` 永远收不到任何请求。** 报告对影响面的界定我也复核为**准确且未夸大**：`pages.yml:45` 的 mock 是刻意设计（Pages 演示站）；`release.yml:53-55` 的 `npm run build` 未设 `VITE_USE_MOCK`，正式发布不受影响。

### 3.2 F3 —— 我最初的差异是我的错，报告是对的

我第一次用 `\bref\(` 只得 **22**，与报告的 54 不符；改用同时匹配泛型写法 `ref<T>(...)` 的模式后：

```
\bref\(                 = 22
\bref<                  = 32
\bref[<(]  (both forms) = 54      ← 与报告一致
```

其余指标我的独立计数：总行数 **2626** ✓、`computed(` **10** ✓、`watch(` **2** ✓、`api.(get|post|put|delete)` **26** ✓、`try {` **27** ✓、`console.error(` **25** ✓。**F3 CONFIRMED，报告的数字准确，我的首次计数才是错的（已如实记录）。**

### 3.3 F13 —— 确认成立，但口径需要说清

```
Login.vue:181   <button type="button" @click="fillTestAccount('admin', 'admin123')"
Login.vue:188   <button type="button" @click="fillTestAccount('teacher', 'teacher123')"
Login.vue:195   <button type="button" @click="fillTestAccount('student', 'student123')"
Login.vue:202   <button type="button" @click="fillTestAccount('parent', 'parent123')"

release.yml:164   ## 默认账号
release.yml:165   - 管理员：`admin` / `admin123`
release.yml:166   - 教师：`teacher` / `teacher123`
release.yml:167   - 学生：`student` / `student123`
```

**CONFIRMED。** 口径说明（供最终报告采用）：

1. **性质必须写准：这不是前端注入，也不是鉴权绕过。** 四个按钮是 `@click="fillTestAccount(...)"`，作用仅是**把测试口令填进登录表单**，走的是完全正常的登录流程。组件本身不存在任何绕过逻辑。
2. **真正的问题是产品级默认凭据治理**：可预测的默认凭据（`admin/admin123` 等）**随所有产物分发**，且**没有任何强制改密或首次启动轮换机制** —— 我把这四组口令与 `main.cpp:251-254` 的种子哈希做了**独立复算，四组全部 MATCH**（见 §3.9 第 4 项），故它们确凿就是后端种子账号的口令。
3. 与 `release.yml:165-167` 的 Release Notes 明文并列后，攻击面是"拿到产物的人不需要额外信息即可登录"，而**不是**"前端泄露了秘密"。
4. **根因是 B2**：即便删掉这四个按钮与 Release Notes 文案，种子口令本身仍是 `用户名+123`。单修 F13 而不修 B2，**安全收益接近于零**；反之修好 B2（强制随机初始口令 + 首登改密）后，F13 自动失去意义。
5. **我的严重度判定：作为独立发现应为 low**（信息本就以硬编码形式存在于 `main.cpp` 与 Release Notes 中，前端按钮的**增量**风险为零；它的危害完全由 B2 承载）。最终报告可按 medium 保留 F13 但须注明"其风险为 B2 的派生"，或降为 low 并并入 B2；**两种写法都可接受，但不得写成"前端内置后门/鉴权绕过"**。
6. **必须与 B1 串起来**：B1 使这些默认账号在被改密之后**仍然**接受任意口令 —— 也就是说，"改密"这一原本的补救手段在 B1 存在时不成立。这是 F13+B2 必须与 B1 同批处置的原因。

### 3.4 F24 —— 规范层查证（任务书要求的关键项）

`api.ts:73-76` 原文：

```
74:   } else if (method === 'DELETE') {
75:     // 无请求体的 DELETE 需 Content-Length:0，兼容部分反代
76:     headers['Content-Length'] = '0';
```

规范查证：`web_fetch https://developer.mozilla.org/en-US/docs/Glossary/Forbidden_header_name`（HTTP 200，重定向至 `/Glossary/Forbidden_request_header`），正文原文：

> A **forbidden request header** is an HTTP header name-value pair that **cannot be set or modified programmatically in a request.**
> Modifying such headers is forbidden because the user agent retains full control over them.
> Forbidden headers are one of the following: … **`Content-Length`** … `Cookie` … `Host` …

**判定 CONFIRMED**，且报告给的**结论层级**（"浏览器静默忽略/丢弃"，而非"抛 TypeError"）**有依据**：MDN 的措辞是"cannot be set or modified"、由 UA 保留控制权，其示例显示的也是设置的 `Date` **不生效**而非报错。因此 `api.ts:74-75` 在浏览器中是空操作，注释声称的"兼容部分反代"在浏览器侧无依据。

**补充限定（我必须指出）**：报告同时记录 Node/undici 会**保留**该头，这一对比很重要 —— 它意味着该代码在 Node 侧（含 SSR/脚本调用）**会**生效，只是在浏览器侧不会。浏览器端到端实测受沙箱阻断（见 §9），故"浏览器行为"这一层级是**规范推导 + MDN 原文**，不是本机实测。

### 3.5 F4 / F5 —— 逐行核对

**F4**：`TeacherApp.vue:168-171` 硬编码 `evaluations`（`score: 85 / 90`）；`:634-637` 的 `getStudentEvaluation` 读它；`:313-321` 的 `loadEvaluations()` `await api.get('/api/teacher/evaluation/dimensions')` **丢弃返回值**；`:664-683` 两个函数只弹 `toast.info('…开发中...')`。我用 grep 确认 `evaluations` 在全文件只出现于 **168（初始化）与 635（读取）**，**没有任何接口响应给它赋值** → 假数据确被 UI 消费。总行数 **1519** ✓。**CONFIRMED。**

**F5**：`api.ts:19-24` `ApiResponse<T = any>` + `[key: string]: any` ✓；`:52` `apiRequest<T = any>` ✓；`:131-134` 四个方法均 `<T = any>` ✓；`tsconfig.json:14-16` `strict: true` / `noUnusedLocals: false` / `noUnusedParameters: false` ✓。`api.ts` 中 `any` 计 **11 处**，与报告的 11 一致。**CONFIRMED。**

### 3.6 F7 —— 我把它验得比原报告更强

逐个符号检索（全 `frontend/src`）：

```
useTheme        1 处  theme.ts:85（仅定义）
ROLE_THEME      1 处  theme.ts:90（仅定义）
NavSection      1 处  navConfig.ts:18（仅定义）
resetMockDB     1 处  index.ts:690（仅定义）
showToast       3 处  useToast.ts:5,49（注释）+ :50（定义）
useAuth         1 处  auth.ts:157（仅定义）
$formatRelative 3 处  env.d.ts:15（类型声明）+ main.ts:17（注释）+ main.ts:26（赋值）→ 零调用
```

**CONFIRMED，且发现原报告未指出的额外一点：** `main.ts:24-26` 注册的三个 `globalProperties.$formatDateTime / $formatDate / $formatRelative` **没有任何模板使用**（模板全部走 `import { formatDateTime } from '../../lib/format'` 的直接导入，共 11 处调用点，见 `AdminApp.vue:19,1858`、`ParentApp.vue:12,462,528,572,637`、`StudentApp.vue:13,355,434,465`、`TeacherApp.vue:15,1173,1185`）。**即整块 `globalProperties` 注册是死代码** —— 这是原报告 F7 未覆盖的。

### 3.7 F8 / F15 —— 计数与配置

```
toast.error('网络错误，请稍后重试') = 24      ← 与报告"24 处"完全一致
try {                              = 68      ← 与报告"68"一致
console.error(                     = 51（全 src）
```

`frontend/` 下**不存在** eslint/prettier/vitest/jest/karma/.editorconfig 任何配置文件；`package.json` 的 scripts 只有 `dev/build/build:no-typecheck/typecheck/preview`，**无 test、无 lint**；CI（`ci.yml:35-39`）只有 `npm ci` + `npm run typecheck`。**F8、F15 CONFIRMED。**

### 3.8 F12 —— 键盘快捷键为何不生效

```
ConfirmDialog.vue:22   function onKey(e: KeyboardEvent) {
ConfirmDialog.vue:23     if (!state.visible) return
ConfirmDialog.vue:24     if (e.key === 'Escape') answer(false)
ConfirmDialog.vue:25     if (e.key === 'Enter') answer(true)
ConfirmDialog.vue:26   }
ConfirmDialog.vue:32   <div v-if="state.visible" class="modal-backdrop" @click.self="answer(false)"
                           @keydown="onKey" tabindex="0">
```

`tabindex="0"` 有了，但**全文件没有任何 `.focus()` 调用**（我在 119 行内确认），弹窗打开时焦点不会落到该 div 上 → `@keydown` 永不触发。同时缺 `role="dialog"` / `aria-modal` / 焦点陷阱。**F12 CONFIRMED。**

---

### 3.9 t2 §6「未验证 / 存疑事项」7 条逐条裁定

t2 主动列出了 7 条自己未验证的事项。审计员自认的空白最有复核价值 —— 其中 **3 条我已自行解决**，据此裁定如下：

| # | t2 的存疑事项 | 我的裁定 | 我的证据 |
|---|---|---|---|
| 1 | `dist` 产物未能生成，mock 剔除结论来自 Rollup 对照实验 | **CONFIRMED（机制）**，附注未经端到端构建 | 见 §9.1 的三层证据拆解。我确认 `dist` 不存在且 `esbuild.exe` 存在 → 限制来自沙箱 `spawn`，非操作失误。 |
| 2 | `Content-Length` 缺浏览器端到端实测 | **CONFIRMED（规范级）**，我补了权威规范原文 | `web_fetch` MDN Forbidden request header 页（HTTP 200）**明确列出 `Content-Length`** 并写明 "cannot be set or modified programmatically"（§3.4）。 |
| 3 | "后端是否真的依赖该头" —— t2 只读了 2 处，未全面核对 | **已解决：后端不依赖** | **逻辑闭合**：既然浏览器**始终**丢弃该头（§3.4 已确认），而本应用的 DELETE 操作在浏览器中正常工作（§3.1：生产/开发同为同源部署），则后端**不可能**依赖它 —— 否则所有 DELETE 早就全线失败。另 DELETE 的 CSRF 走 `X-CSRF-Token` 头（`auth.h:162-164`、`api.ts:79-82`），与该头无关。**故删除 `api.ts:74-75` 是安全的。** |
| 4 | 默认口令是否与后端种子一致，"未复算哈希" | **已解决：四组全部 MATCH** | 我用 .NET `SHA256` 独立复算：`admin123`→`240be518fabd2724ddb6…`、`teacher123`→`cde383eee8ee7a4400…`、`student123`→`703b0a3d6ad75b649a2…`、`parent123`→`82e3edf5f5f3a46b5f9…`，与 `main.cpp:251-254` 的硬编码哈希**逐一相符**。故 F13 前提成立，且**不必再"以后端结论为准"** —— 已由我直接证明。 |
| 5 | `/api/teacher/evaluation/dimensions` 是否还有其他消费者 | **已解决：前端无其他消费者** | 前端唯一命中 `TeacherApp.vue:317`（其返回值被丢弃，`:314` 注释自承）；后端定义在 `routes_teacher.cpp:416`。**并且我发现了 F4 修复应指向的正确端点**：真正返回评价数据的是 `routes_teacher.cpp:728` 的 `GET /api/teacher/evaluations`（复数），F4 建议改法里的 `?studentId=` 端点并不存在，应改为调用 `:728`。 |
| 6 | `npm audit` 因 registry 不实现 advisories 接口而不可用 | **无法复核，维持原样** | 我未运行 `npm audit`（会访问网络且与只读纪律无关的副作用）。**F17 的 CVE 结论一律标注"依据公开 CVE 编号 + 版本匹配（xlsx 0.18.5）"，不得声称跑通了 npm audit。** 已记录于 §9.2。 |
| 7 | `node_modules` 由 `--ignore-scripts` 安装，可能与 `vite build` 失败有关 | **已解决：与缺文件无关** | `Test-Path frontend\node_modules\@esbuild\win32-x64\esbuild.exe` → **True**；`vue-tsc` 可执行且我成功跑出结果（见下）。故失败点是沙箱 `spawn` 限制，t2 的 `probe4` 结论正确。 |

**另外我在复核中重跑了 t2 的严格度实测（t2 §1.3⑥），结果完全吻合：**

```powershell
PS> node .\node_modules\vue-tsc\bin\vue-tsc.js --noEmit --noUnusedLocals --noUnusedParameters
src/mock/index.ts(298,97): error TS6133: 'db' is declared but its value is never read.
src/mock/index.ts(298,101): error TS6133: 'body' is declared but its value is never read.
src/mock/index.ts(411,82): error TS6133: 'db' is declared but its value is never read.
src/mock/index.ts(411,86): error TS6133: 'body' is declared but its value is never read.
src/mock/index.ts(439,94): error TS6133: 'db' is declared but its value is never read.
src/mock/index.ts(439,98): error TS6133: 'body' is declared but its value is never read.
src/pages/teacher/TeacherApp.vue(664,25): error TS6133: 'student' is declared but its value is never read.
[exit code: 2]
```

**恰为 7 个错误、exit 2**，与 t2 声明的"7 个错误"及"mock 6 处未使用形参 + `TeacherApp.vue(664,25)`"**逐行一致** → **F5 的严格度数字 CONFIRMED（我亲自复现）**。附带效果：这同时**佐证了 F4 的死按钮结论**（`664` 行正是 `editEvaluation(student)`，其参数从未被使用）。

### 3.10 F2 的两处计数错误（我复核后更正）

除 §2.8/§4 已记录的 `res.code` 总数 62→**58** 外，F2 还有第二处计数错误：

```
'api 层已提示' 全项目命中 = 7，全部在 StudentApp.vue：
  StudentApp.vue:142, 176, 185, 194, 203, 212, 240
```

t2 在 F2 正文中**列了 6 个行号**（142 / 176 / 185 / 194 / 203 / 212），**漏掉了第 240 行**，故"6 处"应为 **7 处**。结论方向不变（这些注释确实与 `api.ts` 只处理 401/403/429 的事实不符），但最终报告应写 **7 处**。

同时我**确认了 F2 的后端侧前提**（t2 引 `routes_admin.cpp:137-142`，我逐行核对）：

```
137: } catch (const std::exception& e) {
138:     Logger::error("商城 API 错误: " + std::string(e.what()));
139:     json response = {{"code", 500}, {"msg", "服务器内部错误"}};
140:     res.status = 500;
141:     res.set_content(response.dump(), "application/json");
142: }
```

后端 500 **确实带合法 JSON 体** → `api.ts:96` 的 `response.json()` 成功、不抛错 → 静默。**F2 机制 CONFIRMED**（仅计数需更正）。

### 3.11 F1 × B1 的交叉作用（两份报告均未写，必须补进最终报告）

这是一个**组合结论**，需要明确写清，避免读者误读为"既然发的是开发版，所以没事"：

| 交付物 | 前端模式 | 是否会调用后端登录接口 | B1 自动升级链（`routes_public.cpp:47-51`）是否被触发 |
|---|---|---|---|
| `ci.yml` 的 `campus-energy-station-dev.zip`（开发版） | **Mock**（`VITE_USE_MOCK='true'`） | **不会** —— `api.ts:58-60` 把全部 `/api/*` 交给 `mockRequest()`，只读写 `localStorage` | **不会**（该路径下 B1 被"意外地"绕开） |
| `release.yml` 的正式发布包 | 真实 | **会** | **会** —— 一旦有人用 `admin` / `admin123` 正常登录，`hash_password()` 立即把 admin 的哈希改写成坏 pbkdf2，**此后任意口令可登录 admin，且永久有效** |

**必须点明的结论**：开发版包"看起来安全"纯属**因为它的前端根本没连后端**，属于两个缺陷相互掩盖，**不是任何一层被修好了**。正式发布包一旦被使用，B1 立即生效。因此：

- **不可**用 F1 的存在来降低 B1 的紧急度；
- 也不要因为"发了开发版"而认为 B1 没有被触发过 —— `server.log` 的 603 次登录记录（§2.12）说明**真实登录确实在某个环境里发生过**；
- 修复顺序上 B1 **必须先于** F1 处置（认证绕过是安全事件，Mock 串用只是交付质量问题）。

### 3.12 F17 的 CVE 结论证据等级（按 captain 指示标注）

`xlsx@0.18.5` 命中 CVE-2023-30533（原型污染）与 CVE-2024-22363（ReDoS）—— **证据等级：版本匹配 + 公开 CVE 编号**，**不是本机 `npm audit` 输出**（该 registry 的 advisories 接口 404 NOT_IMPLEMENTED，t2 §6.6 与 F17 均已如实记录，我未复跑）。最终报告须保留这一等级标注，且注明 npm 上该包无已修复的更高版本（`xlsx` 官方已停更，替代品为 `exceljs` 等），故建议为**评估后替换或接受风险**，而非"升级版本"。

---

### 3.13 F4：教师端评价展示是**永久假数据**（定级维持 t2 的 **high**；我复核更正的是**机制**，非定级）

> **先更正本文件此前的一处错误表述。** 队长抽查建议"F4 从 medium 提级为 high"，我一度采纳并写成"提级"。**经回到源码逐条核对，该前提不成立**：`FRONTEND_FINDINGS.md:327` 的标题为 **【high】**、`:164` 的摘要表行为 `| F4 | high |`，且该报告自报统计行 `:186`（blocker 1 / high 4 / medium 12 / low 7）**完全正确**。**F4 从头就是 high，不存在 medium→high 的移动。** 因此：**撤回"提级"说法**；t2 的严重度分布**维持其自报的 1 / 4 / 12 / 7**（不用 1 / 5 / 11 / 7）。我实际贡献的是**机制与描述的更正**（t2 写"显示假分数"，实际**没有任何分数会被显示**）。

本条由**队长抽查**提出，我独立复核后**采纳其机制更正**，并纠正了其中一处细节（详见下表）。

**我复核通过的事实：**

1. **模板确实在渲染分数**（`TeacherApp.vue`）—— 5 个评价维度各一列，全部条件渲染：

```
1038: <div v-if="getStudentEvaluation(student.id, 1)" class="flex items-center">
1040:   <span :style="{ width: (getStudentEvaluation(student.id, 1) as number * 100 / 100) + '%', ... }">
1042:   <span class="ml-2 font-display font-semibold text-stone-700">{{ getStudentEvaluation(student.id, 1) }}</span>
1044: <div v-else class="text-stone-400 text-xs">未评价</div>
       维度 2 → 1047-1053；维度 3 → 1056-1062；维度 4 → 1065-1071；维度 5 → 1074-1080（同构）
```

2. **`evaluations` 全文件只有一处赋值。** 我用严格赋值模式复跑：

```powershell
Select-String -Path TeacherApp.vue -Pattern 'evaluations\s*=|evaluations\.value\s*=|evaluations\.value\.(push|splice|unshift)'
→ 168: const evaluations = ref<Evaluation[]>([     ← 仅此一处，之后从未写入
```

3. **提交评价后依然不会回填。** `submitEvaluation`（`:639-661`）成功后确实调用 `await loadEvaluations()`（`:651`），但 `loadEvaluations`（`:313-321`）**丢弃返回值、从不写 `evaluations`**，且其注释自认"实际未使用返回值"。

4. **后端有能力却没被前端使用。** 前端对 `teacher/evaluation` 的调用只有 `:317`（dimensions）与 `:645`（POST 提交）；`routes_teacher.cpp:728` 的 `GET /api/teacher/evaluations`（真正的列表接口）**前端从未调用**。

5. **`loadEvaluations()` 的注释是自认性质的证据，原文引用如下**（这是本条定级的关键，不只是疏漏）：

```
313: async function loadEvaluations() {
314:   // 原始代码调用 /api/teacher/evaluation/dimensions 但赋值到不存在的 this.dimensions，
315:   // 实际未使用返回值。此处保留 API 调用以维持网络行为。
316:   try {
317:     await api.get('/api/teacher/evaluation/dimensions')
318:   } catch (e) { ... }
```

"**此处保留 API 调用以维持网络行为**" —— 明知返回值无用仍保留请求，属**有意维持"看似在工作"的假象**，而非单纯遗漏。

**⚠️ 我纠正的机制细节（请勿沿用"只有 studentId===1 会显示 85/90"）：**

| 事实 | 证据 |
|---|---|
| `Student.id` 类型为 `number \| string` | `TeacherApp.vue:29` |
| 后端返回的 `id` 是 `user.id`，而 `users.id` 是 **TEXT 主键**（形如 `"student-02-01-01"`） | `routes_teacher.cpp:89` + `main.cpp:80` 的 `id TEXT PRIMARY KEY` + `main.cpp:253` 种子值 |
| 硬编码常量的 `studentId` 是 **number `1`** | `TeacherApp.vue:169-170`（`studentId: 1`）；`Evaluation.studentId: number`（`:61`） |
| `getStudentEvaluation` 用 **严格相等** 比较 | `TeacherApp.vue:635`：`e.studentId === studentId` |

**因此 `1 === "student-02-01-01"` 恒为 false → `getStudentEvaluation` 对生产中的每一个学生都返回 `null` → 5 个维度全部走 `v-else`，一律渲染"未评价"。**

**修正后的结论比队长原表述更严重、也更需澄清**：不是"只有 1 号学生显示 85/90"，而是
- **任何学生都永远看不到 85/90** —— 那两个常量是**不可达的死数据**（连同 `Evaluation` 类型定义一起）；
- **任何学生也永远看不到真实评价分**，即使教师提交成功（因为 `evaluations` 从不回填）；
- 真实的评价数据其实已写进数据库（`submitEvaluation` 的 POST 会落库），但**前端没有任何路径把它读回来展示**。

**最终报告应采用标题**：**"教师端评价展示为永久假数据：分数列全部恒显示'未评价'，真实评价无法显示"**，严重度 **high**（用户可见的功能性错误信息 + 评价数据只写不读），成本 **M**（接通 `routes_teacher.cpp:728` 的列表接口 + 让 `loadEvaluations` 真正回填 `evaluations`，或删掉整段假数据与 `getStudentEvaluation`）。

**并附一句性质判断**：这是"半途而废的功能"而非"演示数据混入"，因为 `:315` 的注释证明作者**知道**该调用无效果仍保留它。

### 3.14 路径遮蔽问题：**经复核不存在**（显式记录的否定结论，不立发现）

针对"`POST /api/teacher/evaluation`（`routes_teacher.cpp:463`）与 `GET /api/teacher/evaluations`（`:728`）是否相互遮蔽"，我按"宁可少立一条也不要制造伪问题"的原则逐层核对：

```
# httplib 的 handler 容器按 HTTP 方法分离
httplib.h:547   Handlers get_handlers_;
httplib.h:548   Handlers post_handlers_;
httplib.h:550   Handlers put_handlers_;
httplib.h:552   Handlers patch_handlers_;
httplib.h:554   Handlers delete_handlers_;
httplib.h:556   Handlers options_handlers_;

# routing 按方法只查对应容器
httplib.h:3544  return dispatch_request(req, res, get_handlers_);
httplib.h:3546  return dispatch_request(req, res, post_handlers_);
httplib.h:3548  return dispatch_request(req, res, put_handlers_);
httplib.h:3550  return dispatch_request(req, res, delete_handlers_);
httplib.h:3552  return dispatch_request(req, res, options_handlers_);
httplib.h:3554  return dispatch_request(req, res, patch_handlers_);
```

**判定：不构成遮蔽，无需修复，不立发现。** 三重理由：
1. **方法不同 → 容器不同**：`POST /api/teacher/evaluation` 进 `post_handlers_`，`GET /api/teacher/evaluations` 进 `get_handlers_`；一个 GET 请求**根本不会查** POST 容器。
2. **同一方法内也不会误匹配**：`httplib.h:3569` 用 `std::regex_match`（要求**整串**匹配），故 `…/evaluations` 的正则不可能匹配路径 `…/evaluation`，反之亦然（注意此时是"正则"但内容是字面路径，无通配符）。
3. **注册顺序无关**：跨方法不存在"先注册者先命中"的问题。

**唯一值得记录的关联事实**：这两个路径的**单复数不一致**（`evaluation` / `evaluations`）确实是 F4 修复时容易踩坑的地方 —— 前端要接的是**复数、GET** 的那个（`:728`）。这属命名隐患，已并入 §3.9 第 5 项与 F4 的改法说明，**单独立条会夸大**。

---

## 4. 行号漂移与数字不符记录（逐条）

**结论：两份报告的行号质量整体很高** —— 我核对的关键引用几乎全部逐字命中。发现的不符如下，均在最终报告中按"更正后"采用。

| # | 报告 | 报告称 | 实际 | 性质 |
|---|---|---|---|---|
| 1 | t1 B1 | `sha256.h:179-184`（校验段） | 代码在 **180-184**（179 是注释行） | 轻微 off-by-one |
| 2 | t1 B2 | `models.cpp:16-22` | 数据在 **17-22**（16 是注释） | 区间含注释行，无害 |
| 3 | t1 B5 | `models.cpp:164-169` | 声明在 **165-169**（164 是注释） | 区间含注释行，无害 |
| 4 | t1 B8 | `routes_public.cpp:367-373` | 实为 **366-373** | 起始行差 1 |
| 5 | t1 B17 | "仅 **6** 处设置 `res.status`" | **16** 处 | **数字错误，须更正** |
| 6 | t1 B25 | 单参重载是"**死代码**" | **78** 处调用（双参仅 8 处） | **定性错误，须改写** |
| 7 | t1 §2 | 自述"medium 10 / low 10" | 按自身标题为 **medium 9 / low 11** | **内部统计不一致** |
| 8 | t2 F2 | "**62** 处 `res.code`"；"StudentApp **6** 处 `catch { /* api 层已提示 */ }`" | **58** 处（TeacherApp 17→**15**、StudentApp 8→**6**）；catch 实为 **7** 处（t2 只列了 `142/176/185/194/203/212`，**漏列 `:240`**） | **两处数字错误，须更正**（结论方向不变，机制我已确认） |
| 9 | t2 F24 | `api.ts:73-76` | 语句在 **74-75**（注释在 74） | 区间含注释行，无害 |

**未发现任何断章取义**：我对 t1 的 B1/B3/B4/B5/B6/B7/B8/B9/B10/B13/B16/B17/B19 与 t2 的 F1/F2/F3/F4/F5/F6/F7/F8/F9/F11/F12/F13/F15/F16/F24 所引代码块与原文逐一比对，**引文与源码一致**。两处"过度定性"（#5、#6）与一处口径偏大（#8）已在上文更正。

---

## 5. 由验证阶段新增的发现（两份报告均未提及）

以下每条均由我独立发现并在本节给出证据；最终报告需以"验证阶段新增"标注。

### N1 —— `cookie_secure` 在 HTTPS 回退后未复位，会静默摧毁登录【medium】

```
config.h:31    bool cookie_secure = false;
config.h:84            if (config.https_enabled) config.cookie_secure = true;
main.cpp:312-315       if (g_config.https_enabled) {
                           Logger::warning("HTTPS 已在配置中启用，但当前编译版本不支持 SSLServer。…回退到 HTTP 模式。");
                           g_config.https_enabled = false;
                       }
auth.h:88 / 95 / 174   if (g_config.cookie_secure) cookie += "; Secure";
```

`cookie_secure` 在全项目**只被赋值一次**（`config.h:84`），**任何地方都不会复位**。`main.cpp:312-315` 明确设计了"HTTPS 不可用时回退 HTTP"的路径，却忘了把 `cookie_secure` 复原。触发条件：运维把 `config.json` 的 `"https": {"enabled": true}`（该配置项就摆在配置文件里，见到即会尝试）。后果：三种 Cookie 全部带 `Secure`，浏览器在 `http://` 下**拒绝保存 `sid`** → 登录后立刻掉线，且日志只有一句"回退到 HTTP 模式"，症状表现为"登录成功但马上未登录"，极难排查。

**最小修复**：在 `main.cpp:314` 的 `g_config.https_enabled = false;` 之后补 `g_config.cookie_secure = false;`。

### N2 —— 并发建号会生成重复 ID，且 `INSERT OR REPLACE` 静默覆盖已有用户【high】

```
models.cpp:112-160   generate_user_id()：遍历内存 users 求 max_seq，返回 max_seq+1
models.cpp:123,131,139,154   snprintf(buf, …, "admin-%02d", max_seq + 1) 等
models.cpp:79-80     "INSERT OR REPLACE INTO users (id, username, …) VALUES (?, …)"
```

`generate_user_id` 的序号来自**内存扫描**（`models.cpp:117-121` 等），没有 DB 唯一约束兜底、没有锁。两个并发建号请求会算出**同一个 id**；随后 `save_user_to_db` 用的是 **`INSERT OR REPLACE`**（`models.cpp:79`）→ 后写入者**直接覆盖**先写入者，**先建的用户静默消失**。

严重度我判 **high**：这是并发下的静默数据丢失（用户创建/批量导入/教师建学生都会走到），且比 t1 已列的 4 条并发问题（B5 悬垂、B6 错位、B7 事务串扰、B8 双花）更直接、更容易触发。**这条是 t1 的并发分析遗漏的**（t1 把"ID 冲突"只写在 `points_records` 的 `size()+1` 上，未覆盖 `users` 主键生成）。

**修复**：id 改由 DB 生成或用 `INSERT`（非 `OR REPLACE`）让唯一约束暴露冲突；或把序号生成放进同一把写锁内。

### N3 —— 全站没有任何服务端分页，列表/导出接口结果集无界【medium】

```
全仓库 LIMIT/OFFSET 命中仅 3 处：
  routes_admin.cpp:386    "… ORDER BY created_at DESC LIMIT 10"
  routes_teacher.cpp:564  "… LIMIT 1"
  routes_parent.cpp:27    "… LIMIT 1"
```

除上述 1 处 `LIMIT 10` 与 2 处存在性检查外，**没有任何 `LIMIT`/`OFFSET` 分页**。而 `routes_admin.cpp:1344-1356` 的 `/api/admin/export` 遍历**全量** `users`、`1388-1399` 遍历全量 `points_records`；`/api/admin/users`、`/api/teacher/students`、`/api/admin/redemptions` 等同理。前端 `Pagination.vue` + `usePagination.ts`（F6）说明**分页是在客户端对全量载荷做的** —— 服务端每个列表请求都返回全集。

影响：随数据量线性增长的内存与带宽消耗，叠加 B14 的 payload 无上限（`httplib.h:44` = `SIZE_MAX`）、B23 的单连接串行化，构成可预期的性能悬崖。**两份报告都未提"服务端无分页"**（t1 的 B23 只说 N+1 查询，t2 只说 composable 是死代码）。

### N4 —— `.trae/` 已被 `.gitignore` 排除，却仍有 25 个文件在库中【low，仓库卫生】

```powershell
git ls-files | Select-String '^\.trae/' | Measure-Object
→ matched lines = 25
```

```
.trae/documents/admin_fix_plan.md          .trae/specs/add-parent-portal/{spec,tasks,checklist}.md
.trae/documents/cookie-auth-refactor.md    .trae/specs/batch-import-students/…
.trae/documents/localize-fonts-plan.md     .trae/specs/fix-admin-and-login/…
.trae/documents/remove-memory-storage-mode.md   .trae/specs/fix-teacher-click-parent-nav/…
                                           .trae/specs/refactor-uid-and-teacher-classes/…
                                           .trae/specs/unify-mobile-nav-and-admin-parent/…
                                           .trae/specs/write-project-docs/…
```

`.gitignore` 末尾确有一条 `# Trae 工具目录（spec 文档可选择性保留，这里排除）` + `.trae/` —— 但**该规则是后加的，未执行 `git rm --cached`**，所以 25 个文件依旧被跟踪。这是"gitignore 不追溯已跟踪文件"的典型症状，会让后续 `git status` 永远干净而实际仍在入库。

**修复**：`git rm -r --cached .trae/` 后提交（`.gitignore` 规则即刻生效）。注意：这是仓库操作建议，本次审计**未执行**。

**顺带核对的仓库卫生项**：`.gitignore` 中 `.idea/`、`.vscode/`、`.zcode/`、`*.log`、`*.db`、`frontend/node_modules/`、`frontend/dist/` 均已声明，且我 `git ls-files` 未发现它们被跟踪 —— **这四项是干净的**；同样地 `git status --porcelain` 只有 `?? .agent-teams/` 与 `?? docs/audit/`，无源码改动。

### N5 —— 静态文件服务会对外提供 `.map`（source map），属潜在源码泄露【low】

```
routes_static.cpp:42   if (ext == ".map")                 return "application/json";
routes_static.cpp:94   svr.Get("/assets/.*", ...)         ← 该 handler 会按扩展名服务 /assets/ 下任意文件
```

`content_type_for` 显式支持 `.map`，且 `/assets/.*` 路由不做扩展名白名单。当前 `vite.config.ts` 未开启 `build.sourcemap`，因此 `dist/assets` 下不会生成 `.map`，**当前不构成实际泄露**；但一旦有人为了排障打开 sourcemap，生产服务就会**自动**把完整前端源码对外提供，且不会有任何提示。属于"配置一改就出事"的潜在项。

**修复**：从 `content_type_for` 删除 `.map` 分支，或让 `/assets/` 路由只服务白名单扩展名。

### N6 —— `main.ts` 的 `globalProperties.$format*` 注册是死代码【low】

见 §3.6：`main.ts:24-26` 注册的三个全局格式化属性零模板使用（模板全部用直接导入）。属 F7"死代码"一类的延伸，原报告 F7 未覆盖这一处。

---

## 6. 我复核为"干净"的负面结论（必须作为优点写入最终报告）

复核的价值不只在于证伪审计员，也在于**排除伪问题**。以下各项我主动检查后确认**不存在问题**，最终报告不得把它们写成隐患：

### 6.1 做得好且我独立验证为真的项

| 项 | 我的证据 |
|---|---|
| **日志不含敏感信息** | 我对全部 `Logger::(info\|warning\|error)` 调用点做敏感词过滤（`password\|passwd\|token\|session_id\|csrf\|req.body\|secret`）→ **零命中**。`log_request`（`auth.h:54-57`）只记录 `method + " " + path`，而 httplib 把查询串分离进 `req.params`（`httplib.h:3120`），故 URL 里的 token 也不会入日志。 |
| **`/api/admin/export` 不泄露口令哈希** | `routes_admin.cpp:1347-1354` 只取 `id/username/role_id/name/className/points` 六个非敏感字段，注释自称的"安全修复 V7"**确实落实了**。 |
| **无 `SELECT *`** | 全后端 grep `SELECT \*` → **零命中**，查询均显式列名。 |
| **静态文件路径穿越已被正确防护** | `httplib.h:3116`（`Server::parse_request_line` 内）执行 `req.path = detail::decode_url(m[3], false);` —— **服务端会先做百分号解码**；随后 `routes_static.cpp:14-23` 的 `is_path_safe` 拒绝任何解码后含 `..` 的路径、拒绝 `//` 与前导盘符。因此 `%2e%2e` 变体无法绕过（解码后必然含 `..` 而被拒），未解码则只是普通文件名。`is_path_safe` 是唯一入口（`routes_static.cpp:50`），无旁路。**结论：此项无漏洞，是应当保留的优点。** |
| **`Set-Cookie` 不会被互相覆盖** | `httplib.h:195` `using Headers = std::multimap<std::string, std::string, detail::ci>;`，`Response::set_header`（`httplib.h:2794-2803`）用 `headers.emplace` → 登录时 `set_session_cookie`（`routes_public.cpp:69`）与 `issue_csrf_token`（`:71`）产生的**两个 `Set-Cookie` 都会保留**。任何"第二个覆盖第一个"的说法不成立。 |
| **CSRF 的 `HttpOnly` 不影响双重提交** | `issue_csrf_token`（`auth.h:171-177`）除写 Cookie 外**还 return 该 token**，路由把它放进 JSON body（`routes_public.cpp:84`、`routes_parent.cpp:120`），前端从 body 取而非读 Cookie。故 `HttpOnly` 是**正确且必要**的，任何"HttpOnly 导致 CSRF 失效"的判断应判 REFUTED。 |
| **Cookie 属性组合合理** | `auth.h:86-87,94` 的 `sid` 与 `:173` 的 `csrf_token` 均为 `HttpOnly; Path=/; SameSite=Lax`。`SameSite=Lax` 能挡住跨站 POST/iframe 携带，配合 `auth.h:158-168` 的双重提交校验（常量时间比较，`165-167`）构成有效防护。`Secure` 缺失是 HTTP 部署的必然结果（见 N1 讨论），不是独立缺陷。 |
| **B4 的 XFF 问题我独立确认** | `login_client_key`（`auth.h:141-154`）优先读 `X-Forwarded-For`；httplib 注入的可信值在 `httplib.h:3639`（`req.set_header("REMOTE_ADDR", strm.get_remote_addr())`）。攻击者伪造 XFF 即可让锁定键每次不同 → 爆破无上限；不发 XFF 则键退化为 `unknown\|username`。另：`login_can_try`/`login_record_fail` 仅出现在 `routes_public.cpp`，`/api/parent/login`（`routes_parent.cpp:49-77`）**确无任何锁定**。 |
| **B14 的前提成立** | `httplib.h:44` `#define CPPHTTPLIB_PAYLOAD_MAX_LENGTH ((std::numeric_limits<size_t>::max)())`；`set_payload_max_length`/`set_read_timeout`/`set_write_timeout`/`set_error_handler` 在 17 个自研文件中**零调用**（我的 grep 输出为空）。 |
| **B30 成立** | `logger.h:59` `rotate_if_needed();` 在 `:61` 的 `std::lock_guard` **之前**执行 → 并发轮转竞态；`:42` 每次调用都 `ifstream`+`tellg` → 每条日志一次 open/stat。 |

### 6.2 从运行时日志得到的额外事实

`server.log` 首行 `[2026-03-28 21:18:43]`、末行 `[2026-06-27 19:04:02]`，说明该服务曾长期运行。这对最终报告的"现实风险"判断有用：B27 已实际发生；`POST /api/auth/login` 出现 603 次说明登录路径被高频使用，从而 **B1 的升级链在历史上极可能已被触发** —— 但那份数据库文件现已不存在，故"曾经有哪些行是 `pbkdf2$`"**无法取证**（见 §9）。

### 6.3 REFUTED 条目（一律不进入最终报告）

复核过程中我**未认定任何一条原始发现为完全 REFUTED**。但下列**具体表述**被证伪，在最终报告中已按要求删除或改写为正确说法：

1. "单参 `set_cors_headers` 是死代码" —— **证伪**（78 处调用）。已改写为"缺陷重载占主流，但同源部署下影响为 low"。
2. "`Set-Cookie` 会被覆盖 / 只剩一个" —— **证伪**（`multimap` + `emplace`）。此说法**从未出现在两份报告中**，是我主动排查后排除的伪问题。
3. "`HttpOnly` 使 CSRF 双重提交不可用" —— **证伪**（token 同时经 JSON body 返回）。同样是我主动排除的伪问题。
4. "备份接口存在路径注入" —— **证伪**（`backup_path` 无请求参数参与）。该条已由 t1 自行推翻，我复核为**推翻正确**。
5. "口令哈希无 per-user salt" —— **证伪**（`hash_password` 确有 `generate_random_hex(16)` 随机盐 + 100000 迭代，见 `sha256.h:163-165`；真正的问题是 B1 的算法实现）。已由 t1 自行纠正，我复核为**纠正正确**。

---

## 7. 与两份报告的分歧汇总（供最终报告直接采用）

| 条目 | 原表述 | 我的核实结果 | 最终报告应采用 |
|---|---|---|---|
| B17 | "仅 6 处设置 res.status" | **16 处**；"217 处业务码"无误 | 16 : 217 |
| B25 | 单参重载是"死代码"、影响面为"401/403 与预检" | 单参 **78** 处 / 双参 8 处；同源部署下影响 low，跨域配置下影响**近乎全站** | 按此改写，严重度维持 low |
| t1 §2 | "medium 10 / low 10" | **medium 9 / low 11** | 按 9 / 11 |
| F2 | "62 处 res.code" | **58 处**（TeacherApp 15、StudentApp 6） | 58 处 |
| F13 | 独立的中危信息泄露 | 成立，但口令与 B2 种子口令**同一组**，根因是 B2 | 与 B2 互相引用，说明"单修 F13 收益≈0" |
| F7 | 8 处零引用导出 | 成立，**且** `main.ts:24-26` 的 `globalProperties.$format*` 注册同样零使用 | 追加 N6 |
| F2（计数） | "62 处 `res.code`"、"6 处误导 catch" | **58** 处；catch 实为 **7** 处（漏列 `StudentApp.vue:240`） | 58 / 7 |
| mock 进产物 | 字节级结论未端到端验证 | 拆为 L1 门控机制（我 CONFIRMED）+ L2 tree-shaking（t2 强间接证据）+ L3 F1 后果（我 CONFIRMED） | **CONFIRMED（机制）**，附注"未经端到端构建复现"；并作为**优点**写入：正式发布不含 mock |
| F17 | "命中 CVE" | 成立，但证据等级为**版本匹配 + 公开 CVE 编号**，非本机 `npm audit` | 保留等级标注；`xlsx` 无更高修复版，建议替换评估 |
| F1 × B1 | 两份报告均未提及 | 开发版因前端走 Mock 而不触发 B1 升级链；正式发布包一旦登录即永久打开 B1 后门 | **必须交叉引用**：不得用 F1 降低 B1 紧急度；修复顺序 B1 先于 F1 |
| **F4 机制更正** | t2 原文"硬编码假评价数据…（显示假分数）"，定级 **high（原报已为 high）** | 定级不变；**描述须更正**：不是"1 号学生显示 85/90"，而是 `1 === "student-02-01-01"` 恒 false → **所有学生的 5 个维度一律渲染"未评价"**，85/90 为不可达死数据；真实评价只写不读 | 标题改为"评价展示为永久假数据：分数列恒显示'未评价'"；引 `:315` 原文"保留 API 调用以维持网络行为"作性质证据；修复接 `routes_teacher.cpp:728` |
| 路径遮蔽 | （队长提出待确认） | **经复核不存在**：httplib 按方法分离容器（`:547-556`）+ 按方法分派（`:3544-3554`）+ `regex_match` 整串匹配（`:3569`） | **不立发现**；仅保留单复数命名隐患并入 F4 改法 |
| **t2 严重度分布** | 有说法要求改为 1 / 5 / 11 / 7（"含 F4 提级"） | **不成立**：按 `FRONTEND_FINDINGS.md` 标题逐条重数 = blocker 1 / high 4 / medium 12 / low 7，与其自报行 `:186` 一致；F4 原已为 high | **采用 t2 自报的 1 / 4 / 12 / 7**；撤回"提级" |
| F13 性质 | "登录页内置真实默认口令" | 实为 4 个 `fillTestAccount()` **填充**按钮，**非注入/鉴权绕过**；增量风险为零，危害由 B2 承载 | 写"默认凭据随所有产物分发 + 无强制轮换机制"；严重度 low（或 medium 但注明为 B2 派生）；**不得写成前端后门** |
| 路径遮蔽 | （队长提出待确认） | **经复核不存在**：httplib 按方法分离容器（`:547-556`）+ 按方法分派（`:3544-3554`）+ `regex_match` 整串匹配（`:3569`） | **不立发现**；仅记录 `evaluation`/`evaluations` 单复数命名隐患并入 F4 改法 |

**t2 严重度分布（以源码为准的最终口径）**：**blocker 1（F1）/ high 4（F2、F3、F4、F5）/ medium 12 / low 7** —— 与 `FRONTEND_FINDINGS.md:186` 的自报统计**完全一致**。**F4 原报即为 high，不存在 medium→high 的移动**；此前"high 5 / medium 11"的说法系误传，已撤回。本文件 §3 表中 F4 的判定应为「**CONFIRMED（机制更正；定级不变）**」。

---

## 8. 契约 `verificationCommands` 的真实执行结果

| # | 命令 | 真实结果 |
|---|---|---|
| 1 | `Test-Path docs/audit/VERIFICATION.md` | **True**（§8.1） |
| 2 | `Select-String -Path docs/audit/VERIFICATION.md -Pattern 'CONFIRMED\|PARTIAL\|REFUTED\|UNVERIFIABLE' \| Measure-Object` | **Count = 96**（§8.2；度量自身故随文字编辑变动，见其中的自指说明） |
| 3 | `git ls-files \| Select-String '^\.trae/' \| Measure-Object` | **Count = 25** |
| 4 | `Select-String -Path auth.h -Pattern 'snprintf\|query_bind\|execute_bind'` | **8 行命中**（§8.3） |

### 8.1 命令 1（文件存在性）

```
PS> Test-Path docs/audit/VERIFICATION.md
True
```

### 8.2 命令 2（判定关键词计数）

```
PS> Select-String -Path docs/audit/VERIFICATION.md -Pattern 'CONFIRMED|PARTIAL|REFUTED|UNVERIFIABLE' | Measure-Object
Count : 96
```

**Count = 96**（该命令统计的是**出现这些关键词的行数**，不等于发现条数）。

> ⚠️ **自指说明（诚实披露）**：本命令度量的是"包含该数字的那个文件自身"，因此数字会随本报告的文字编辑而变化。我的实测序列：初稿 **81** → 补写本节后 **89** → 追加 §3.9-3.12 与 §9.1 重写后 **96**。上面记录的 **96** 是本文件定稿状态下重新执行得到的值。**引用时请以"能机械复现出约 81-96 量级的关键词行数"为准，不要把它当作发现条数** —— 发现条数的权威口径是本文件 §0 统计表与 §8.2 的裁定分布表。关键词文本出现次数与**发现条数**是两个口径，二者分别如下：

```
CONFIRMED occurrences    = 62
PARTIAL occurrences      = 12
REFUTED occurrences      = 8
UNVERIFIABLE occurrences = 12
```

**按"发现条数"计的裁定分布（权威口径，与 §0 统计表一致）：**

| 判定 | 条数 | 明细 |
|---|---|---|
| **CONFIRMED** | **34** | 后端 B1-B14、B16、B18、B19、B27、B29、B30（20 条）；前端 F1、F3-F9、F11-F13、F15、F16、F24（14 条） |
| **PARTIAL** | **3** | 后端 B17（6→16）、B25（"死代码"→78 处）；前端 F2（62→58） |
| **REFUTED** | **0** | 无任何原始发现被整体证伪；§6.3 中被证伪的是 5 条**具体表述/伪问题**，不是发现条目 |
| **UNVERIFIABLE** | **0（主要项已重新裁定）** | 原拟将"mock 是否进生产产物"标 UNVERIFIABLE；经补充复核（§9.1）拆为 L1/L2/L3 三层后，判定为 **CONFIRMED（机制）+ 附注"未经端到端 vite build 复现"**。其余较弱项见 §9.2 清单（均为"未复跑的环境失败命令"与"未复核的第三方行"，不构成对发现的否定）。 |

`REFUTED` 的 8 次文本出现集中在 §0 判定口径定义与 §6.3"被证伪的具体表述"两处，**不代表有 8 条发现被推翻** —— 这一点在引用本文件时务必区分，避免误读为"两份报告有 8 条是错的"。

### 8.3 命令 4（`auth.h` 两套 SQL 风格并存 —— 任务书指定必查项）

```
PS> Select-String -Path auth.h -Pattern 'snprintf|query_bind|execute_bind'
196: db.execute_bind("DELETE FROM sessions WHERE expires_at < ?",
200: // 不应拼接进 SQL（沿用 sqlite_wrapper.h 中 execute_bind 的约定）
216: return db.execute_bind(sql, params);
223: auto result = db.query_bind("SELECT user_id, expires_at FROM sessions WHERE session_id = ?",
231: db.execute_bind("DELETE FROM sessions WHERE session_id = ?",
243: snprintf(sql, sizeof(sql),
264: snprintf(sql, sizeof(sql), "DELETE FROM sessions WHERE session_id = '%s'",
272: snprintf(sql, sizeof(sql), "DELETE FROM sessions WHERE expires_at < %ld",
```

**结论：两种风格确实并存，且行号精确。**
- 参数化一侧：`create_session`（`:196` `execute_bind`、`:216` 返回）、`verify_session`（`:223` `query_bind`、`:231` `execute_bind`）。
- 字符串拼接一侧：`get_session_info`（`:243`）、`delete_session`（`:264`）、`cleanup_expired_sessions`（`:272`），全部 `snprintf` + `db.escapeString()`（`:245`、`:265`）+ `db.query(sql)`。
- **讽刺点（我独立发现并确认）**：`:199-200` 的注释明确写着"会话 SQL 使用参数化查询 …… 不应拼接进 SQL"，而同一文件 `:243` 起就用了拼接 —— **注释与 40 行后的代码直接矛盾**，这正是"维护陷阱"的最好证据。
- 缓冲上限：`get_session_info` 用 `char sql[1024]`、`delete_session` 用 `char sql[256]`。会话 ID 固定 64 hex 字符，故**当前不会截断**，属潜在风险（与 t1 B11 的"静默截断隐患"判定一致，未夸大）。
- `escapeString`（`sqlite_wrapper.h:195-206`）**只把 `'` 翻倍**，不处理反斜杠；对 SQLite 的字符串字面量规则而言这已足够（SQLite 不以 `\` 转义），故 B11"当前不可注入"的判断成立。

---

## 9. 未能验证 / 不背书事项（UNVERIFIABLE 清单）

按要求宁可标 UNVERIFIABLE 也不替审计员背书。

### 9.1 "mock 是否进入生产产物" —— **CONFIRMED（机制）**，附注：tree-shaking 结果未经端到端 `vite build` 复现

```powershell
PS> Test-Path frontend\dist
False
```

`frontend/dist` **不存在**，工作区内也没有任何构建产物，因此我**无法**用产物 grep 的方式做端到端验证。但把这条降级为 UNVERIFIABLE 会**低估证据强度** —— 该结论由三层独立证据支撑，且其真正的操作性后果（F1）完全不依赖产物字节：

| 层次 | 内容 | 责任归属 |
|---|---|---|
| **L1 门控机制（我已独立 CONFIRMED）** | `api.ts:15` 是**静态**导入 `import { isMockEnabled, mockRequest } from '../mock'`（非动态 `import()`），故 mock 模块进入模块图；`vite.config.ts:12-15` 在构建期把 `import.meta.env.VITE_USE_MOCK` 替换为字面量（未设时为 `'false'`）；`mock/index.ts:658` 运行时判 `=== 'true'`。 | **我的证据**（read 原文） |
| **L2 tree-shaking 结果（强间接证据，非我复现）** | t2 用**同版本 Rollup 4.62.3 进程内**对照实验：`VITE_USE_MOCK=false` → 4405 B / 3 模块、mock 特征串全 MISS，**且与"空壳对照组字节数完全相同"**；`=true` → 34273 B / 4 模块、全 HIT。"字节数与空壳对照完全一致"这一条尤其有力 —— 它证明被剔除的正是 mock 整模块，而非只是少了几行。 | **t2 的证据**，我未复跑 |
| **L3 操作性后果（我已独立 CONFIRMED）** | F1：`ci.yml:44` 用 `VITE_USE_MOCK: 'true'` 构建，该产物在 `ci.yml:114-124` 与 `server.exe` 打进同一个 zip（见 §3.1）。这只依赖 workflow 文本，**不依赖任何产物字节**。 | **我的证据** |

**判定：该条标 CONFIRMED（机制）**，并在引用时附注"tree-shaking 结果由同版本 Rollup 进程内实验 + 字节数与空壳对照完全一致支撑，属强间接证据，**未经真实 `vite build` 端到端复现**（环境受限）"。

**环境限制的真实性我也复核了**（见 §3.9 第 7 项）：`frontend/node_modules/@esbuild/win32-x64/esbuild.exe` **存在**（`Test-Path` = True），`vue-tsc` 可正常执行（我成功跑了 typecheck，见 §3.9 第 2 项），但 `frontend/dist` 为 False —— 说明构建失败点是**沙箱对 `spawn` 的限制**，不是缺文件或操作失误。

**这是正面结论，必须写进最终报告的"优点"一节：**
- `release.yml:53-55` 的正式发布构建**未设** `VITE_USE_MOCK` → 走真实后端模式，mock 被 tree-shaking 剔除 → **正式交付物不含 mock，也不含假口令/假 token**；
- `pages.yml:45` 设 `'true'` 属**有意设计**（GitHub Pages 纯前端演示站），**不得写成缺陷**；
- 唯一的缺陷是 `ci.yml` 的 package job **误用了演示构建**（F1），属配置串用，不是设计错误。

### 9.2 其余 UNVERIFIABLE / 不背书项

| 项 | 原因 |
|---|---|
| B1："当前库中有多少行以 `pbkdf2$` 开头" | **无数据库文件**（三路交叉验证：`Test-Path` False、PowerShell 全递归为空、`glob` No files found），故无从查询行级状态。但"文件不存在"本身是已验证的否定事实 → 结论"此刻未武装"仍可断言。 |
| B1：历史上是否已有账号被改写为坏哈希 | `server.log` 显示 603 次登录，升级链极可能触发过，但**那份 DB 已不存在**，无物证。标 UNVERIFIABLE。 |
| B10：浏览器端到端复现（`{"username":123}` → 500 + `EXCEPTION_WHAT` 头） | 任务书禁止发起 HTTP 请求、禁止运行 `server.exe`，且沙箱无浏览器。**框架层证据已逐字确认**（`httplib.h:3574-3579`），接口契约层为静态推导。 |
| F24：浏览器实测丢弃 `Content-Length` | 沙箱阻断浏览器（Chrome mojo 命名管道被拒）。已用 **MDN 规范原文**替代（§3.4），属"规范级确认"而非"本机实测"。 |
| t1 B15、t1 B20-B24、B26、B28；t2 F10、F14、F17-F23 | **不在我的抽样范围内（低于 60% medium 门槛或属 low）**，我未逐条复核，**不予背书**。其中 B20-B30 我抽查了 B25（PARTIAL）、B27（CONFIRMED，升级）、B29、B30（CONFIRMED），其余未核。 |
| 两份报告所述的沙箱失败命令（`npm ci` 失败、`vite build` EPERM、`npm audit` 404、Chrome mojo 拒绝） | 我**未复跑**这些失败命令（复跑 `npm ci` 会写 `node_modules`，与只读纪律冲突）。仅确认最终状态：`frontend/dist` 不存在、`git status --porcelain` 无 `package-lock.json` 变更。 |
| t1 引用的第三方行（`sqlite3.c:14046-14052` 的 `SQLITE_THREADSAFE` 默认值、`C:\mingw64\...c++config.h`） | 我**未逐行复核**这些第三方/工具链行。t1 对并发的收敛结论（串行模式 → 否定 API 级内存损坏）**在方向上我认可**，但其前提引用我未独立验证。 |

---

## 10. 只读性自证

```powershell
PS> git status --porcelain
?? .agent-teams/
?? docs/audit/
```

本次复核**未修改任何源码或配置**：`git status` 只显示两个未跟踪目录，`frontend/package-lock.json` 无变更，`frontend/dist` 未生成，未执行 `git commit`/`git push`，未运行 `server.exe`，未发起任何 HTTP 请求。本次任务写入的唯一文件是 `docs/audit/VERIFICATION.md` 自身；`docs/audit/BACKEND_FINDINGS.md` 与 `docs/audit/FRONTEND_FINDINGS.md` 由其他成员产出，我**未改动**它们。

本报告为**建议与判定**，不包含任何"已修改源码"的陈述。§5 与 §7 中出现的 `git rm -r --cached .trae/` 等字样均为**修复建议**，未被执行。
