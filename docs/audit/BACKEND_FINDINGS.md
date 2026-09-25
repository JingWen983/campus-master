# 校园能量站 · C++ 后端代码质量审计报告（BACKEND_FINDINGS）

- 审计人：backend-auditor（team `code-quality-audit`，任务 t1）
- 审计对象：`D:\邵敬文\comptation` 下自研 C++ 源码（17 个文件，`read` 工具累计 6,053 行，含空行/注释）
- 排除范围：vendored 第三方 `httplib.h`、`json.hpp`、`sqlite3.c/h/ext.h`、`vcpkg/`（仅在需要判定框架行为时定点阅读其行）
- 审计模式：**纯只读**。未修改/新建/删除任何源码文件，未重新编译，未运行 `server.exe`，唯一写入的文件是本报告
- 报告日期：见文件 mtime

---

## 1. 审计范围与方法

### 1.1 实际逐行阅读的自研文件（`read` 工具，行号以 `read` 输出为准）

| 文件 | 行数 | 说明 |
|---|---|---|
| `main.cpp` | 336 | 入口、schema、种子数据、线程池 |
| `auth.h` | 404 | 会话/登录锁定/CSRF/CORS/中间件 |
| `config.h` | 94 | 配置结构 + JSON 加载 |
| `config.json` | 31 | 运行时配置（thread_count=8 等） |
| `sqlite_wrapper.h` | 212 | 唯一 DB 封装（单连接） |
| `sha256.h` | 229 | SHA256 / PBKDF2 / 随机数 |
| `models.h` / `models.cpp` | 93 / 244 | 内存用户表、索引、DB 读写辅助 |
| `routes.h` | 27 | 路由注册声明 |
| `routes_public.cpp` | 420 | 登录/注册/兑换/榜单 |
| `routes_admin.cpp` | 1892 | 管理员全部接口 |
| `routes_teacher.cpp` | 1171 | 教师全部接口 |
| `routes_student.cpp` | 251 | 学生端接口 |
| `routes_parent.cpp` | 473 | 家长端接口 |
| `routes_static.cpp` | 99 | 静态资源与路径校验 |
| `logger.h` / `logger.cpp` | 92 / 7 | 日志与轮转 |
| `database.h` | 9 | 仅 include 转发 |

### 1.2 为判定“框架/库行为”而定点阅读的第三方行

- `httplib.h:44`（`CPPHTTPLIB_PAYLOAD_MAX_LENGTH`）、`1553-1568`（`get_remote_addr`）、`2955`/`3077-3079`（默认 payload 上限与 setter）、`3561-3582`（`dispatch_request` 异常处理）、`3639`（`REMOTE_ADDR` 注入）、`3666-3672`（路由失败后的响应写出）
- `json.hpp:22491-22507`（`value()` 类型不匹配抛 `type_error.302`）
- `sqlite3.c:14046-14052`（`SQLITE_THREADSAFE` 默认值 1 = 串行模式）、`sqlite3.h:10786-10797`（仅 `__wasi__` 才默认为 0）
- `README.md:249,252-263`（编译链接命令，用于确认未传 `-DSQLITE_THREADSAFE`）
- `C:\mingw64\include\c++\14.2.0\x86_64-w64-mingw32\bits\c++config.h:1693,1755`（`_GLIBCXX_USE_DEV_RANDOM`/`_GLIBCXX_USE_RANDOM_TR1` 均未定义）

### 1.3 执行过的关键命令（均为只读）

```powershell
# 文件清单与行数
Get-ChildItem -Path "D:\邵敬文\comptation" -File | Select-Object Name, Length
# 自研源码中是否存在任何互斥/原子设施
grep "mutex|lock_guard|unique_lock|std::atomic|SQLITE_THREADSAFE"  (include: *.cpp,*.h)
# 是否存在 set_payload_max_length / set_read_timeout / set_error_handler
grep "set_read_timeout|set_write_timeout|set_payload_max_length|set_error_handler|new_task_queue" (*.cpp,*.h)
# 反查种子口令哈希对应的明文（.NET SHA256 复算 22 个候选口令）
[System.Security.Cryptography.SHA256]::Create() ...  → MATCH: admin<=‘admin123’; teacher<=‘teacher123’; student<=‘student123’; parent<=‘parent123’
# 机械验证 pbkdf2_sha256 函数体是否引用 password 形参
$lines = Get-Content sha256.h; $lines[93..147] | Select-String 'password'   → 仅命中第 94 行（形参本身），函数体 95-148 行零引用
# 二进制熵源取证（不执行 server.exe，仅字节串）
[System.IO.File]::ReadAllBytes("server.exe") 中检索 urandom/rdrand/SystemFunction036/RtlGenRandom
  → 命中 "random_device: rdseed failed." / "random_device: rand_s failed." / advapi32!SystemFunction036
# 响应体与 HTTP 状态码统计
([regex]::Matches($c,'res\.status\s*=')).Count  vs  ([regex]::Matches($c,'\{"code",\s*\d+\}')).Count
# 权限码使用面统计（check_permission_middleware 的第二个实参）
grep 'check_permission_middleware\(req, res, "[a-z:_]+"'
```

> 结论可信度声明：本报告中所有“已确认”条目均来自上述源码级证据；凡属推理延伸的，均在条目中显式标注**🔶 疑似**。

---

## 2. 结论摘要（按严重度排序）

| id | 严重度 | 一句话问题 | 位置 |
|---|---|---|---|
| B1 | **blocker** | `pbkdf2_sha256()` 的 `password` 形参在函数体内从未被引用 → 派生结果只由 salt/迭代数决定 → 对任何 `pbkdf2$` 哈希**任意口令都能通过校验**，完全认证绕过 | `sha256.h:94-148`、`169-188` |
| B2 | high | 4 个默认账号以**裸 SHA256 无盐**哈希硬编码为 `admin123`/`teacher123`/`student123`/`parent123`，且首次登录会被自动“升级”为 B1 的坏哈希 | `main.cpp:250-254`、`models.cpp:17-22`、`routes_public.cpp:45-51` |
| B3 | high | `login_attempts()` 返回的函数级 `static std::map` 被 8 个工作线程无锁读写 → 数据竞争/UB、计数丢失，甚至可能崩溃 | `auth.h:107-139`、`main.cpp:306` |
| B4 | high | 登录锁定键直接信任 `X-Forwarded-For`：伪造该头即可**无限爆破**；省略该头则可反向**永久锁死任意账号**；`/api/parent/login` 完全没有锁定 | `auth.h:141-154`、`routes_parent.cpp:49-77` |
| B5 | high | 全局 `users` 向量 / `user_id_map` 等 `unordered_map` / `points_records` 被多线程无锁读写：`find_user_by_id()` 返回的 `User*` 在并发 `push_back` 后**悬垂**，跨请求写已释放内存 | `models.cpp:165-221`、`routes_public.cpp:167,325,381` |
| B6 | high | `users.erase(it)` 后仅删除被删用户的索引项，后续所有元素的内存下标全部错位 → `find_user_by_id()` **返回另一个用户**（越权/串号） | `models.cpp:164-211`、`routes_teacher.cpp:259-260`、`routes_admin.cpp:826-827` |
| B7 | high | 单条共享连接上执行 `BEGIN TRANSACTION`/`COMMIT`/`ROLLBACK`：事务是**连接级全局**的，其他线程的写入会被卷入、甚至在回滚时被一起丢弃 | `routes_admin.cpp:1453,1665,1686`、`sqlite_wrapper.h:14-210` |
| B8 | high | 兑换流程“读积分→判断→绝对值写回”非原子 + `execute_bind` 无法区分 0 行受影响 → **同一学生双花、库存超卖**；“不存在则 404”的分支系统性失效 | `routes_public.cpp:360-385`、`sqlite_wrapper.h:145-147` |
| B9 | high | `/api/teacher/*` 的**写接口完全没有班级作用域**（读取类接口有），任意教师可改/删任意学生、给任意学生加减积分 → 水平越权(IDOR) | `routes_teacher.cpp:78-96` vs `193-201,298-300,487-489` |
| B10 | high | 处理器只捕获 `json::parse_error`，其余异常被 httplib 统一转成 HTTP 500 并把异常原文写进 **`EXCEPTION_WHAT` 响应头**回显客户端 | `httplib.h:3561-3582`、`routes_public.cpp:88,174,392` 等 |
| B11 | medium | `get_session_info`/`delete_session`/`cleanup_expired_sessions` 仍用 `char[1024/256/128]`+`snprintf` 拼 SQL（同文件上方已是参数化）→ 维护陷阱 + 静默截断 | `auth.h:242-246,261-266,270-274` |
| B12 | medium | `query()/query_bind()` prepare 失败返回**空数组**，与“无数据”不可区分，调用方把 DB 错误当成业务结果（含把错误当成 403） | `sqlite_wrapper.h:53-62,151-159`、`auth.h:246`、`routes_parent.cpp:25-30` |
| B13 | medium | 备份接口判据写错：`VACUUM INTO` 成功也返回空数组，`is_null()` 恒为 false → **永远返回“备份失败”500** | `routes_admin.cpp:335-349` |
| B14 | medium | 未调用 `set_payload_max_length`（默认上限 = `SIZE_MAX`）也未设读超时 → 超大 body / 慢速连接可耗尽内存 | `httplib.h:44,2955,3077`、`main.cpp:286-306` |
| B15 | medium | `/api/admin/import` 把导出文件里 role_id≠3 的用户**静默降级为学生**；新用户不更新索引（仅靠导入结束的全量重建兜底，异常路径下内存与库分叉） | `routes_admin.cpp:1470-1475,1494-1502,1667` |
| B16 | medium | 角色/权限的增删改**只改内存不落库**，而鉴权只读内存、`/api/admin/roles` 却读库 → 功能无效、重启丢失、两处数据不一致 | `routes_admin.cpp:979-981,1044-1046,936-940`、`auth.h:280-303` |
| B17 | medium | 业务码全靠手工拼 `{"code":N}`（217 处），仅 6 处设置 `res.status` → 绝大多数业务错误以 **HTTP 200** 返回，状态码与业务码混用 | 全 `routes_*.cpp` |
| B18 | medium | 内存 `users` 与 DB 双份数据源不原子：积分用绝对值覆盖、删班级只改库、`points_records` 只增不载、CSV 导入不返回初始口令 | `models.cpp:93-97`、`routes_public.cpp:381-382`、`routes_admin.cpp:1877-1879,1389-1399`、`routes_teacher.cpp:926,959-967` |
| B19 | medium | 无统一响应封装；`/api/admin/system` 与 `/api/admin/system/config` 返回写死的假数据；5 个权限码从未被任何中间件使用 | `routes_admin.cpp:60-70,1229-1244`、`main.cpp:230-248` |
| B20 | low | 旧哈希路径用 `std::string::operator==`（非常量时间）；密码长度校验只在 2 个入口存在；`atoi` 读迭代数无上限 | `sha256.h:176,187`、`routes_public.cpp:132-136`、`routes_admin.cpp:491-492` |
| B21 | low | `std::random_device` 每次调用重建、每字节一次 `rd()`、`rd() % n` 存在模偏差（不影响安全性，属精度/性能问题） | `sha256.h:151-159,200-227` |
| B22 | low | 🔶疑似 `localtime()` 在非加锁上下文被并发调用，依赖 CRT 是否为线程局部缓冲 | `routes_teacher.cpp:554-557,646-647`、`routes_admin.cpp:1277-1285` |
| B23 | low | 单连接 + SQLite 串行模式使 8 线程无法并发用库；每请求 3~4 次会话查询（含 1 次 DELETE）、列表接口 N+1 查询 | `sqlite_wrapper.h:27-29`、`auth.h:196-197`、`routes_admin.cpp:1708-1710`、`routes_teacher.cpp:560-570` |
| B24 | low | `get_cookie_value()` 用 `find("sid=")` 裸子串匹配，`xsid=` 会被当成 `sid` | `auth.h:67-82` |
| B25 | low | `set_cors_headers(res)` 单参重载是死代码（含未使用变量），401/403 与 OPTIONS 预检均不回显 ACAO | `auth.h:22-33,329,338,350,358`、`routes_static.cpp:72-75` |
| B26 | low | 积分记录响应里 `operatorName` 硬编码 `"王老师"` | `routes_teacher.cpp:363` |
| B27 | low | 检测到旧 schema 时直接 `std::remove()` 删库重建（无备份）；删除失败路径未处理，后续 `row.value("id","")` 会抛 `type_error` 且 `main` 无捕获 | `main.cpp:60-75`、`models.cpp:63` |
| B28 | low | 声明了外键但从未 `PRAGMA foreign_keys=ON` → 外键约束全部不生效 | `main.cpp:108-211`、`sqlite_wrapper.h:19-32` |
| B29 | low | DB 层错误只写 `std::cerr`（不走 `Logger`：不落轮转文件、无时间戳、GUI 下可能不可见） | `sqlite_wrapper.h:45-48,60,132,157` |
| B30 | low | 日志轮转在加锁**之前**执行（TOCTOU/并发 rename），且每条日志都 `stat`+`open` | `logger.h:41-62` |

**统计：blocker 1 / high 9 / medium 10 / low 10，合计 30 条（其中 1 条标注为疑似）。**

---

## 3. 逐条发现详情

### B1 —— `pbkdf2_sha256()` 完全忽略 password 参数，构成认证绕过【blocker】

**位置**：`sha256.h:94-148`（函数体）、`sha256.h:162-166`（`hash_password`）、`sha256.h:169-188`（`verify_password`）；调用方 `routes_public.cpp:45-51,159`、`routes_parent.cpp:73`、`routes_admin.cpp:514,626,880,1470,1491-1492`、`routes_teacher.cpp:136,946`

**证据**（原文片段）：

```cpp
// sha256.h:94-109
inline std::string pbkdf2_sha256(const std::string& password, const std::string& salt,
                                 int iterations = 100000) {
    const int BLOCK = 64;
    unsigned char k_ipad[BLOCK] = {0};
    unsigned char k_opad[BLOCK] = {0};
    if (salt.size() > (size_t)BLOCK) {
        std::string s = sha256(salt);                    // ← HMAC 密钥取自 salt，而非 password
        std::memcpy(k_ipad, s.data(), s.size() < 32 ? s.size() : 32);
        std::memcpy(k_opad, s.data(), s.size() < 32 ? s.size() : 32);
    } else {
        std::memcpy(k_ipad, salt.data(), salt.size());    // ← 同上
        std::memcpy(k_opad, salt.data(), salt.size());
    }
    for (int i = 0; i < BLOCK; i++) { k_ipad[i] ^= 0x36; k_opad[i] ^= 0x5c; }
```

```cpp
// sha256.h:112-126
    unsigned char u[32];
    {
        std::string msg;
        msg.append((char*)k_ipad, BLOCK);
        msg.append(salt);                                 // ← 消息体也是 salt，password 缺席
        msg.push_back(0); msg.push_back(0); msg.push_back(0); msg.push_back(1); // INT(1) BE
        std::string h1 = sha256(msg);
```

```cpp
// sha256.h:129-142（迭代段同样只使用 u / k_opad，无 password）
    for (int i = 1; i < iterations; i++) {
        std::string msg;
        msg.append((char*)k_ipad, BLOCK);
        msg.append((char*)u, 32);
        std::string h1 = sha256(msg);
```

```cpp
// sha256.h:179-184（校验段）
        std::string calc = pbkdf2_sha256(password, salt, iters);
        if (calc.size() != dk.size()) return false;
        unsigned char diff = 0;
        for (size_t i = 0; i < calc.size(); i++) diff |= (unsigned char)(calc[i] ^ dk[i]);
        return diff == 0;
```

**问题**：机械核查已确认——`password` 这个标识符在 `pbkdf2_sha256` 函数体内（第 95-148 行）**出现次数为 0**，只出现在第 94 行的形参列表中：

```powershell
$lines = Get-Content -LiteralPath 'D:\邵敬文\comptation\sha256.h'
($lines[93..147] | Select-String 'password').Count   # → 1（即形参那一行）
```

因此派生值 `dk = F(salt, iterations)`，与口令完全无关。而 `verify_password()` 用**存储串里自带的 salt 与 iters** 重算同一个 `F`，故 `calc == dk` 恒成立 → **对任何 `pbkdf2$` 格式的哈希，任何口令（甚至空串）都校验通过**。这不是“弱哈希”，而是彻底的认证绕过。

**影响面**（凡 `password_hash` 以 `pbkdf2$` 开头的账号全部中招）：
- 自助注册学生：`routes_public.cpp:159`（`hash_password(password)`）；
- 管理员创建/编辑/重置：`routes_admin.cpp:514`、`626`、`880`、`1470`、`1491-1492`；
- 教师创建学生（含 CSV 批量导入）：`routes_teacher.cpp:136`、`946`；
- **自动升级链（最致命）**：`routes_public.cpp:45-51`

```cpp
// routes_public.cpp:45-51
            bool password_match = verify_password(password, user->password_hash);
            // 安全修复 V5：旧版无盐哈希登录成功后自动升级为 PBKDF2
            if (password_match && user->password_hash.compare(0, 7, "pbkdf2$") != 0) {
                std::string new_hash = hash_password(password);
                user->password_hash = new_hash;
                save_user_to_db(*user);
            }
```

种子账号（B2）当前是裸 SHA256，走 `sha256.h:187` 的正确分支；但管理员**只要正常登录一次**，其哈希就被就地改写成被污染的 `pbkdf2$...`，此后任意口令均可登录 `admin`。即：一次合法登录即可永久打开后门。
- 家长登录同样走 `verify_password`（`routes_parent.cpp:73`）→ 家长账号任意口令可登录。

**建议改法**：
1. 按 RFC 8018 重写 `pbkdf2_sha256`：`U1 = HMAC-SHA256(key=password, msg=salt||INT32BE(1))`，迭代 `Ui = HMAC(key=password, msg=U(i-1))`，`T = U1 xor U2 xor ...`；不要再用自拼 ipad/opad 的方式，直接写一个 `hmac_sha256(key, msg)` 原语并单独测试。
2. 立刻用 RFC 6070/官方测试向量做单元测试（`pbkdf2("password","salt",1)` 等固定向量），把“password 未参与”这类错误钉死在测试里。
3. 数据修复：现有 `pbkdf2$` 记录全部视为失效，强制走“重置密码/重新注册”流程（可用 SQL：`UPDATE users SET password_hash='' WHERE password_hash LIKE 'pbkdf2$%'`，并在 `verify_password` 中对空哈希恒返回 false）。
4. 在 `verify_password` 内加防御性断言：`calc == F(salt,iters)` 型退化可用“两次不同口令必须得到不同结果”的启动自检拦截（fail-fast，避免同类回归）。

**修复成本**：M（重写 ~60 行 + 测试向量）；若含存量数据迁移与用户通知，则 L。

---

### B2 —— 硬编码弱口令 + 无盐 SHA256 种子数据【high】

**位置**：`main.cpp:250-254`（DB 种子 SQL）、`models.cpp:17-22`（内存兜底表）、`routes_public.cpp:47-51`（自动升级）

**证据**：

```sql
-- main.cpp:250-254
            INSERT OR IGNORE INTO users (id, username, password_hash, role_id, name, className, points) VALUES
                ('admin-01', 'admin', '240be518fabd2724ddb6f04eeb1da5967448d7e831c08c8fa822809f74c720a9', 1, '管理员', '系统管理', 0),
                ('teacher-001', 'teacher', 'cde383eee8ee7a4400adf7a15f716f179a2eb97646b37e089eb8d6d04e663416', 2, '王老师', '高二(1)班', 0),
                ('student-02-01-01', 'student', '703b0a3d6ad75b649a28adde7d83c6251da457549263bc7ff45ec709b0a8448b', 3, '张同学', '高二(1)班', 150),
                ('parent-001', 'parent', '82e3edf5f5f3a46b5f94579b61817fd9a1f356adcef5ee22da3b96ef775c4860', 4, '张同学家长', '', 0);
```

```cpp
// models.cpp:16-22
// 默认用户数据（内存模式时使用，密码均为 SHA256 哈希）
vector<User> users = {
    {"admin-01", "admin", "240be518fabd2724ddb6f04eeb1da5967448d7e831c08c8fa822809f74c720a9", 1, "管理员", "系统管理", 0, ""},
    ...
```

**问题**（已用 .NET SHA256 复算确认明文）：

| 账号 | 哈希 | 明文 |
|---|---|---|
| admin | `240be518…20a9` | `admin123` |
| teacher | `cde383ee…e663416` | `teacher123` |
| student | `703b0a3d…b0a8448b` | `student123` |
| parent | `82e3edf5…c775c4860` | `parent123` |

即：4 个默认账号的口令是“用户名+123”这种可猜形式，且以**无盐 SHA256** 存储（`models.cpp:16` 的注释自己也承认）。哈希本身可被彩虹表/穷举秒破，且同一口令在不同库中哈希相同。结合 `routes_public.cpp:47-51` 的自动升级，管理员账号存在“登录一次后即被 B1 永久接管”的链路。

另注：当数据库打开失败时（`main.cpp:73-75` 仅 warning），服务会**退化为内存模式**并使用这份硬编码表继续提供服务，此时任何口令修改/新增用户都无法持久化，安全状态不可恢复（`save_user_to_db` 静默失败）。

**影响面**：`/api/auth/login`、`/api/parent/login`；管理员会话可获得全部 `system:manage`/`user:manage` 权限（`main.cpp:244-248`）。当前 `config.json:3` 与 `config.h:15` 默认只监听 `127.0.0.1`（安全修复 V14），把远程利用限制为需要改配置或同机访问。

**建议改法**：
1. 首次启动时用 `generate_random_password()` 生成强口令并**只在控制台/一次性文件输出**，不写入源码；或强制“首登必须改密”。
2. 种子数据里不要放可用口令：插入 `password_hash=''`（不可登录）+ 一次性初始化令牌，或提供 `--init-admin` 命令行参数设置口令。
3. 去掉内存兜底账号表（或让磁盘库打开失败直接退出，`exit(1)`），避免静默降级。
4. 保留向后兼容：在 `verify_password` 中对裸 SHA256 分支加日志告警，统计并清理存量弱哈希。

**修复成本**：S（改种子/初始化流程）～ M（含首登改密流程）。

---

### B3 —— `login_attempts()` 静态 std::map 被 8 线程无锁读写【high】

**位置**：`auth.h:107-139`；线程模型依据 `main.cpp:306` + `config.json:5`

**证据**：

```cpp
// auth.h:106-114
// ====== 安全修复 V8：登录失败锁定（内存计数，按用户名 + 客户端 IP 维度）======
struct LoginAttempt {
    int fails = 0;
    time_t locked_until = 0;
};
inline std::map<std::string, LoginAttempt>& login_attempts() {
    static std::map<std::string, LoginAttempt> m;
    return m;
}
```

```cpp
// auth.h:126-134
inline void login_record_fail(const std::string& key) {
    auto& m = login_attempts();
    auto& a = m[key];           // ← 并发插入会触发红黑树旋转/新节点分配
    a.fails++;
    if (a.fails >= g_config.max_login_attempts) { ... }
}
```

```cpp
// main.cpp:305-306
    // 8. 设置线程池
    svr.new_task_queue = [&]() { return new httplib::ThreadPool(g_config.thread_count); };
```

```json
// config.json:4-6
        "port": 8080,
        "thread_count": 8
```

**问题**：`httplib::Server::listen()` 用该 lambda 创建线程池，**8 个 worker 线程并发执行所有 handler**；`std::map`（非线程安全容器）被 `login_can_try`（读+遍历）、`login_record_fail`（写+插节点）、`login_record_success`（erase）同时访问，全程无 `mutex`/原子量。全项目 grep 确认除 `Logger`（`logger.h:61,89`）与 httplib 内部外**没有任何互斥设施**。

后果分三层：
1. **未定义行为（最坏是崩溃）**：`map::operator[]` 在插入时分配/链接节点并可能旋转；与另一线程的 `find`/`erase` 并发 → 读到半链接的指针 → 段错误或静默丢节点。无法通过编译器开关消除。
2. **计数丢失 / 阈值失效**：`a.fails++` 是非原子读-改-写，并发失败会合并计数，导致锁定阈值（5）实际需要更多次才触发。
3. **检查-使用竞态**：`login_can_try` 与 `login_record_fail` 之间无事务性，攻击者并发请求可越过锁定窗口。

**影响面**：`/api/auth/login`（`routes_public.cpp:29-30,39,53,59`）。触发路径：任意两个并发登录（不需要攻击者，正常用户+一次错误口令即可）→ 两个线程同时进入 `login_attempts()`。这是**必然发生**而非理论可能。

**建议改法**：
1. 最小改动：把 map 与一把 `std::mutex` 封装成一个结构体，所有访问（含 `login_can_try` 的读）在锁内完成；`login_can_try` 改为返回“允许/拒绝”结果值，避免把引用/迭代器暴露出去。
2. 更彻底：改为 `std::unordered_map<std::string, LoginAttempt>` + 分片锁，或直接把锁定计数放到 SQLite（`login_attempts(key, fails, locked_until)` 表 + `UPDATE ... SET fails=fails+1` 原子自增 + UNIQUE 索引），顺带获得持久化与上限治理（见 B4）。
3. 加清理：`login_can_try` 时惰性删除 `locked_until` 已过期且 `fails==0` 的条目，并设 `max_entries` 上限（见 B4）。

**修复成本**：S（加锁版）～ M（迁移到 DB）。

---

### B4 —— 登录锁定键信任 `X-Forwarded-For`：可绕过亦可反向锁死账号【high】

**位置**：`auth.h:141-154`；对照 `httplib.h:1553-1568,3639`

**证据**：

```cpp
// auth.h:141-154
inline std::string login_client_key(const httplib::Request& req, const std::string& username) {
    std::string ip;
    auto it = req.headers.find("X-Forwarded-For");
    if (it != req.headers.end()) {
        ip = it->second;
        auto comma = ip.find(',');
        if (comma != std::string::npos) ip = ip.substr(0, comma);
    } else {
        auto rit = req.headers.find("REMOTE_ADDR");
        if (rit != req.headers.end()) ip = rit->second;
    }
    if (ip.empty()) ip = "unknown";
    return ip + "|" + username;
}
```

```cpp
// httplib.h:3639（服务端自己注入的 REMOTE_ADDR 才是可信来源）
  req.set_header("REMOTE_ADDR", strm.get_remote_addr());
```

```cpp
// httplib.h:1553-1568（get_remote_addr 只返回 IP，不含端口，故 fallback 键是稳定的）
inline std::string get_remote_addr(socket_t sock) {
  ...
    if (!getnameinfo(..., NI_NUMERICHOST)) { return ipstr.data(); }
  return std::string();
}
```

**问题**：`X-Forwarded-For` 是**客户端可直接伪造**的请求头，服务端没有启用任何可信代理校验（也没有 `TrustedProxy` 概念）。因此锁定维度 `ip|username` 完全由攻击者挑选：
1. **锁定绕过（爆破无限次）**：每次请求发送不同的 `X-Forwarded-For: 1.2.3.<n>`，则每个键都是首次出现 → `fails` 永远停在 1 → 阈值 5 永不触发 → 可对任意账号无限次猜口令。这是对“安全修复 V8”的完全绕过。
2. **反向 DoS（锁死他人账号）**：不发送 `X-Forwarded-For` 时走 `REMOTE_ADDR`（httplib 注入，真实且稳定）→ 攻击者只要用自己 IP 对 `admin` 连续失败 5 次，即可让 `admin` 被锁定 `lockout_minutes=30` 分钟（`config.json:20`），并每 30 分钟重复一次 → 管理员无法登录。
3. **无上限/无清理**：该 map 没有任何条目上限、TTL 或淘汰逻辑，键中含攻击者可控字符串 → 伪造海量 XFF 值即可让 map 无界增长（内存耗尽 DoS），单次请求的内存成本约“键长度 + 节点开销”。
4. **家长端完全无锁定**：`/api/parent/login`（`routes_parent.cpp:49-77`）根本没有调用 `login_can_try`/`login_record_fail`（grep 确认这三函数只在 `routes_public.cpp` 出现），家长账号可被无限爆破；再叠加 B1，家长口令实际已无意义。

**影响面**：`/api/auth/login`、`/api/parent/login`。

**建议改法**：
1. 默认**不信任** XFF；仅在配置中显式声明可信反向代理（如 `"trust_proxy": true` + `"trusted_proxies": ["127.0.0.1"]`）且对端 IP 命中白名单时，才取 XFF 的**最后一个**（最靠近本机的一跳）地址，而不是第一个。
2. 统一用 `req.remote_addr` 概念（httplib 未暴露 `remote_addr` 字段，因此保留 `REMOTE_ADDR` 头的读取，但去掉 XFF 分支），或至少让 XFF 只作为“附加标签”参与键而非唯一 ip。
3. 键改为只按 **username** 计数 + 按 IP 计数两套独立维度：账号维度（防定向爆破）+ IP 维度（防撞库），避免 IP 伪造直接抹掉账号维度。
4. 给 map 加上限（如 10k 条）+ 定时清理过期条目（`locked_until < now && fails == 0`），或迁移到 SQLite 表并对 `key` 建 UNIQUE 索引 + 定期 `DELETE`。
5. 给 `/api/parent/login` 接入同一套锁定逻辑，并统一到 B3 的加锁封装中。

**修复成本**：S（去 XFF + 加账号维度）～ M（可信代理配置 + 上限治理 + 表化）。

---

### B5 —— 全局 users/索引 map/points_records 多线程无锁读写：悬垂指针与 rehash 竞态【high】

**位置**：`models.cpp:14,164-169,189-221`、`routes_public.cpp:37-51,167-170,325,381-385`、`routes_teacher.cpp:78,143,331-340,952`、`routes_admin.cpp:367,426,520,632,1263,1345,1501`、`auth.h:280-317`

**证据**：

```cpp
// models.cpp:164-169
// 安全修复 V12：索引改为存储 users 向量的下标，避免副本与原数据不一致
unordered_map<string, size_t> user_id_map;           // 用户ID到 users 下标的映射
unordered_map<string, size_t> user_username_map;     // 用户名到 users 下标的映射
...
unordered_map<int, vector<int>> role_permission_map; // 角色ID到权限ID列表的映射

// models.cpp:206-221
User* find_user_by_id(const string& user_id) {
    auto it = user_id_map.find(user_id);
    if (it != user_id_map.end() && it->second < users.size()) {
        return &users[it->second];          // ← 返回 users 元素地址
    }
    return nullptr;
}
```

```cpp
// routes_public.cpp:325-385（指针跨多次 DB 调用长期持有，随后写回）
        User* user = find_user_by_id(user_id);
        ...
            user->points -= cost;                       // 381：写 users 向量内部对象
            update_user_points_in_db(user->id, user->points);
```

```cpp
// routes_public.cpp:167-170（并发增长 users 向量）
            users.push_back(new_user);
            update_user_index(new_user);

// auth.h:355（每个受保护请求都会遍历 users）
    if (!Auth::check_permission(user_id, permission_code)) { ... }
// auth.h:281-283
        auto user_it = find_if(users.begin(), users.end(), [&](const User& u) { return u.id == user_id; });
```

**问题**：`users` 是 `std::vector<User>`，`user_id_map`/`user_username_map` 是 `std::unordered_map`，`points_records` 是 `std::vector<PointsRecord>`，三者都是**进程级全局、8 线程共享、无任何同步**。三类真实竞态：

1. **悬垂指针（use-after-realloc）**：`find_user_by_id()` 返回 `&users[i]`；另一线程执行 `users.push_back(...)`（注册 `routes_public.cpp:167`、教师加学生 `routes_teacher.cpp:143`、管理员加用户 `routes_admin.cpp:520`、批量导入 `routes_admin.cpp:632`、数据导入 `routes_admin.cpp:1501`）触发向量扩容 → 原缓冲区释放 → `user->points -= cost`（`routes_public.cpp:381`）或 `user->password_hash = new_hash`（`routes_public.cpp:49`）**写入已释放内存** → 堆破坏/崩溃/任意数据损坏。触发条件：一次兑换或一次旧哈希登录 与 一次用户创建并发——在 8 线程下属于高概率事件。
2. **容器迭代失效**：`auth.h:281-283` 的 `find_if(users.begin(), users.end(), ...)` 在**每个受保护请求**都会执行；若同时发生 `push_back`，迭代器与被比较的引用全部失效（UB）。`routes_teacher.cpp:78`、`routes_admin.cpp:367,426,1263,1345` 等也存在遍历。
3. **unordered_map rehash 竞态**：`update_user_index()`（`models.cpp:189-197`）在**每个新用户**插入时写 `user_id_map[user.id]=i` → 可能触发 rehash（重链全部节点）；而 `find_user_by_username()`（`models.cpp:215-221`）在**每次登录**读取同一容器 → 读写并发 → UB/崩溃。
4. **`points_records` 并发 push_back**：`routes_teacher.cpp:340` 在多线程下 push_back 同一全局向量，并用 `int new_record_id = points_records.size() + 1;`（`routes_teacher.cpp:331`）生成 ID → 既可能崩溃，也会产生**重复/错误的记录 ID**（该 ID 还会回给前端 `routes_teacher.cpp:356`）。

**影响面**：登录（读密码哈希）、注册、`/api/auth/me`、`/api/user/info`、`/api/student/info`、`/api/mall/redeem`、所有教师/管理员用户类接口、权限校验（`auth.h:280-317` 两个函数都遍历全局容器）。

**建议改法**（按优先级）：
1. 引入一把全局 `std::shared_mutex user_mutex`，约定：**任何对 `users` / 两张索引 map / `points_records` 的访问都必须持锁**（读用 `shared_lock`，增删改用 `unique_lock`）；`find_user_by_id()` 不得再返回裸指针，改为返回 `std::optional<User>` 值拷贝或“按 id 的受锁访问器”（如 `with_user(id, [](User&){...})` 回调式 API），从类型上禁止悬垂。
2. 若要保持指针语义，则用 `std::deque<User>`（push_back 不使元素失效）或 `std::vector<std::unique_ptr<User>>`，让元素地址稳定；但索引 map 的并发仍需锁。
3. 积分/口令等**变更**统一走“DB 为准 + 局部重载”的短临界区，禁止在锁外长时间持有引用。
4. `points_records` 的 ID 改为由 SQLite 生成（`points_records.id` 已是 `INTEGER PRIMARY KEY AUTOINCREMENT`，见 `main.cpp:113-122`），用 `last_insert_rowid` 或直接由 DB 返回，去掉 `size()+1`。
5. 加 CI 校验：`-fsanitize=thread`（Linux CI）或 Windows 上的 Application Verifier/PageHeap 跑并发登录+注册压测，可稳定复现上述 UB。

**修复成本**：M（加锁 + 改写指针 API 的调用点，调用点集中在 6 个文件约 30 处）。

---

### B6 —— `users.erase()` 后索引错位 → `find_user_by_id()` 返回另一个用户【high】

**位置**：`models.cpp:164-166,189-211`、`routes_teacher.cpp:255-263`、`routes_admin.cpp:812-829`、`main.cpp:303`、`routes_admin.cpp:1667`

**证据**：

```cpp
// routes_teacher.cpp:257-261
            } else {
                string deleted_username = it->username;
                users.erase(it);                       // ← 后续所有元素下标 -1
                remove_user_index(student_id, deleted_username);   // ← 只删被删用户自己的两项
                delete_user_from_db(student_id);
```

```cpp
// models.cpp:200-211
void remove_user_index(const string& user_id, const string& username) {
    user_id_map.erase(user_id);
    user_username_map.erase(username);
}
User* find_user_by_id(const string& user_id) {
    auto it = user_id_map.find(user_id);
    if (it != user_id_map.end() && it->second < users.size()) {
        return &users[it->second];        // ← 下标未随 erase 修正
    }
    return nullptr;
}
```

```cpp
// routes_admin.cpp:826-827
                users.erase(user_it);
                remove_user_index(deleted_id, deleted_username);
```

**问题**：索引表里存的是**向量下标**，而下标只有在“没有发生过 erase”时才稳定。删除任意位置的用户后，被删元素之后的所有用户真实下标都减 1，但 `user_id_map`/`user_username_map` 中它们的值没有更新（`remove_user_index` 只删被删用户自己的两项，`update_user_index` 只修一个用户）。于是：
- 若错位后的下标 **仍在** `users.size()` 范围内 → `find_user_by_id(A)` 返回 `users[i]`，而 `users[i]` 现在是**排在 A 后面一位的另一个人** → 会话/权限判定/积分扣减落到错误对象上（越权读取他人数据、扣错人的积分）；
- 若超出范围 → 返回 `nullptr` → 该用户被当作“用户不存在”（`routes_public.cpp:195-199`、`routes_student.cpp:29-33`），会话仍然有效却 404。

`init_indexes()`（`models.cpp:172-186`）会在 `main.cpp:303` 与 `routes_admin.cpp:1667` 被调用，但它**不清空**三张 map（只有 `role_id_map` 等被整体覆盖），且会为**已删除用户**重新写入一个指向他人下标的值 → 错位被固化。

**影响面**：`/api/auth/me`、`/api/user/info`、`/api/student/info`、`/api/mall/redeem`（扣分对象错误）、教师/管理员用户列表操作、`update_user_points_in_db` 的入参。触发条件仅为“管理员或教师删除过任意一个用户”——这是常规运营动作。

**建议改法**：
1. **不要用下标做索引**：索引表改为 `unordered_map<string, string>`（id → 稳定键）或直接“遍历 + 比较”（用户量在此规模下可接受），彻底消灭下标耦合；或
2. 索引表改存 `User*`/稳定 ID，并改用 `std::deque`/`list`/`vector<unique_ptr>` 使元素地址不随删除变化；或
3. `erase` 后调用一个**全量重建**函数（先 `clear()` 三张 map 再重建），且所有 erase 路径（`routes_teacher.cpp:259`、`routes_admin.cpp:826`）都必须调用；`init_indexes()` 内部加 `user_id_map.clear(); user_username_map.clear();`。
4. 加回归测试：删除中间用户后，断言每个存活的 id/username 都能解析到自身。

**修复成本**：S（改成清空重建）～ M（改为稳定键/锁内访问，与 B5 一并做）。

---

### B7 —— 单条共享连接上的显式事务污染/回滚其他线程的写入【high】

**位置**：`routes_admin.cpp:1453,1665,1686`；连接模型 `sqlite_wrapper.h:14-32,208-210`

**证据**：

```cpp
// routes_admin.cpp:1451-1453
            int error_count = 0;

            db.execute("BEGIN TRANSACTION");
```

```cpp
// routes_admin.cpp:1663-1667
            db.execute("COMMIT");

            init_indexes();
```

```cpp
// routes_admin.cpp:1679-1687
        } catch (json::parse_error& e) {
            response = {{"code", 400}, {"msg", "数据格式错误"}};
        } catch (const exception& e) {
            Logger::error(string("导入失败: ") + e.what());
            response = {{"code", 500}, {"msg", "导入失败"}};
            db.execute("ROLLBACK");
        }
```

```cpp
// sqlite_wrapper.h:14-17,208-210
class SqliteDb {
public:
    SqliteDb() : db_(nullptr) {}
...
private:
    sqlite3* db_;      // ← 全进程唯一连接，8 线程共享
```

**问题**：SQLite 的事务是**连接级**的。此处 `BEGIN TRANSACTION` 打在全局唯一连接上，于是：
1. **其他线程的写入被吞进同一事务**：导入期间任何并发请求（教师加积分 `routes_teacher.cpp:343`、学生兑换 `routes_public.cpp:383`、会话写入 `auth.h:216`）都落在同一个事务里。若导入路径走到 `ROLLBACK`（`routes_admin.cpp:1686`），这些**已经向客户端返回 200 成功的写入会被一并回滚** → 用户看到“成功”，数据却消失（静默数据丢失）。
2. **`BEGIN` 失败被忽略**：若此刻另一线程已开事务（或本函数被并发调用两次），`BEGIN TRANSACTION` 返回 `SQLITE_ERROR: cannot start a transaction within a transaction`，而 `db.execute()` 的返回值**未被检查**（`routes_admin.cpp:1453`）→ 导入的写入静默挂在**别人的**事务里。
3. **`ROLLBACK` 的 catch 不完整**：只有 `catch (const exception&)` 分支回滚；`json::parse_error` 分支不回滚（虽然该异常在 `BEGIN` 之前抛出，但模式本身脆弱），任何非 `std::exception`（如 `bad_alloc`）或提前 `return` 都会让事务悬挂，长期持有写锁，后续所有写操作失败。
4. `init_indexes()`（`routes_admin.cpp:1667`）在事务提交后才执行，若中途异常，索引与数据不一致。

**影响面**：`POST /api/admin/import` 与**所有并发写接口**（`/api/teacher/points`、`/api/mall/redeem`、`/api/admin/**`、`/api/parent/student/{id}/messages`、会话创建 `auth.h:190-217`）。

**建议改法**：
1. 立刻给 `SqliteDb` 加一把**递归/全局写锁**，并规定“`BEGIN`/`COMMIT`/`ROLLBACK` 必须与锁同生命周期”，即提供 `db.with_transaction([&]{ ... })` RAII 封装（构造时 `BEGIN IMMEDIATE`，析构时按异常状态 `COMMIT`/`ROLLBACK`），从 API 上禁止手工 `execute("BEGIN")`。
2. 更正确的做法：改为**每线程一条连接**（`thread_local SqliteDb`）或一个小型连接池，让事务天然按线程隔离；此时 `busy_timeout(5000)` + WAL 才真正发挥作用（配合 `BEGIN IMMEDIATE` 减少写写冲突）。
3. `db.execute()` 在事务语句上的返回值必须检查并在失败时立即返回错误响应。
4. 导入前先做**全量校验**（如 `import_data` 结构、role_id 白名单），再开事务，减少回滚概率。

**修复成本**：M（封装 + 改造 3 处调用）～ L（连接池化）。

---

### B8 —— 兑换流程双花/超卖，且 `execute_bind` 无法识别 0 行受影响【high】

**位置**：`routes_public.cpp:325,343-352,360-385`；封装 `sqlite_wrapper.h:127-148`；同型问题 `routes_admin.cpp:245-253`、`routes_teacher.cpp:796-802,825-830,1110-1115`

**证据**：

```cpp
// routes_public.cpp:360-373
            if (user->points < cost) {
                response = {{"code", 400}, {"msg", "积分不足！"}};
                ...
            }
            bool stock_ok = true;
            if (stock == 0) {
                stock_ok = false;
            } else if (stock > 0) {
                db.execute_bind(
                    "UPDATE mall_items SET stock = stock - 1 WHERE id = ? AND stock > 0",
                    {SqliteDb::Bind((long long)item_id)});      // ← 返回值未检查
            }
```

```cpp
// routes_public.cpp:381-385
            user->points -= cost;                                  // ← 内存读-改-写
            update_user_points_in_db(user->id, user->points);       // ← 绝对值覆盖
            db.execute_bind(
                "INSERT INTO redemption_records (student_id, item_id, cost, created_at) VALUES (?, ?, ?, ?)",
                {...});
```

```cpp
// models.cpp:93-97
bool update_user_points_in_db(const string& user_id, int points) {
    return db.execute_bind(
        "UPDATE users SET points = ? WHERE id = ?",           // ← 绝对赋值，非 points = points - ?
        {SqliteDb::Bind((long long)points), SqliteDb::Bind(user_id)});
}
```

```cpp
// sqlite_wrapper.h:145-147
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;      // ← 0 行受影响同样返回 SQLITE_DONE → true
```

**问题**：
1. **双花（同一学生）**：`user->points` 是内存值，`if (user->points < cost)` 与 `user->points -= cost` 之间无锁（见 B5）。两个并发兑换各自读到 `points=100`、`cost=80`，都判定“足够”，各自算出 20 并写回 → 最终 20，但**扣了 80 得到 2 件商品**。因为 `update_user_points_in_db` 用绝对值覆盖（`models.cpp:95`），第二次写入直接吞掉第一次的语义（经典 lost update）。
2. **超卖（同一商品）**：`stock` 先被查出（`routes_public.cpp:351-352`）再判断（`367-373`）。两个线程同时读到 `stock=0`…注意：读到 `stock=1` 时两者都通过检查，随后 `UPDATE ... WHERE stock > 0` 只有一条命中；但**该语句 0 行受影响时 `execute_bind` 依然返回 true**（`sqlite_wrapper.h:147`），返回值也没被检查（`routes_public.cpp:370`）→ 第二个请求继续插入兑换记录并返回“兑换成功” → 库存 0 却卖出 2 件。
3. **系统性 404 分支失效**：同类“用返回布尔判断是否存在”的写法在多个接口存在，全部恒为 true，例如

```cpp
// routes_admin.cpp:245-253（删除不存在的商品也返回 200）
            bool ok = db.execute_bind("DELETE FROM mall_items WHERE id = ?", {...});
            if (ok) { response = {{"code", 200}, {"msg", "商品删除成功"}}; }
            else { response = {{"code", 404}, {"msg", "商品不存在"}}; }
```

`routes_teacher.cpp:796-802`（修改评价）、`825-830`（删除评价）、`1110-1115`（标记已读）同型：`WHERE id=?` 未命中时仍返回 200。

**影响面**：`/api/mall/redeem`（资金/积分正确性）、`/api/admin/mall/{id}`（DELETE/PUT）、`/api/teacher/evaluation/{id}`、`/api/teacher/parent-messages/{id}/read`。

**建议改法**：
1. `execute_bind` 增加“受影响行数”返回值（如 `int execute_bind_rows(...)` = `sqlite3_changes(db_)`，或返回 `std::pair<bool,int>`），**不要**让调用方用布尔猜存在性；同时把连接级 `sqlite3_changes` 的读取放在同一把锁内（否则又落回 B5 的竞态）。
2. 兑换改为**单条原子 SQL**：
   ```sql
   UPDATE users SET points = points - ? WHERE id = ? AND points >= ?;
   -- 受影响行数 == 1 才继续；否则返回 400 积分不足
   UPDATE mall_items SET stock = stock - 1 WHERE id = ? AND (stock > 0 OR stock = -1);
   -- 受影响行数 == 1 才插入兑换记录；否则回滚并返回 400 库存不足
   ```
   并整体放进 `BEGIN IMMEDIATE ... COMMIT`（见 B7 的 RAII 封装）。
3. 业务不变量加数据库约束/守卫：`CHECK (points >= 0)`、`CHECK (stock >= -1)`（`main.cpp:113-146` 建表处补充），让超卖在 SQLite 层被拒绝而不是被忽略。
4. 对“0 行受影响”统一规范：所有 `WHERE id=?` 的写接口必须以行数判断 404。

**修复成本**：S（兑换改原子 SQL）～ M（全站统一行数语义 + 约束补齐）。

---

### B9 —— 教师写接口缺少班级作用域：任意教师可操作任意学生【high】

**位置**：对比 `routes_teacher.cpp:60-96`（读接口有作用域）与 `193-201,251-253,298-300,487-489,118-142`（写接口无作用域）

**证据**：

```cpp
// routes_teacher.cpp:76-87（读取学生列表：用绑定班级过滤）
        // 学生列表：仅返回 className 在绑定班级集合中的学生
        json students = json::array();
        for (const auto& user : users) {
            if (user.role_id == 3) { // 学生角色
                // 若已获取绑定班级，则过滤；否则保留旧行为
                if (!bound_class_names.empty()) {
                    bool in_bound = false;
```

```cpp
// routes_teacher.cpp:193-201（编辑学生：全校范围内查找，无班级过滤）
            auto student_it = find_if(users.begin(), users.end(), [&](const User& u) {
                return u.id == student_id && u.role_id == 3;
            });
            if (student_it == users.end()) { ... }
            student_it->name = name;
            student_it->className = className;    // ← 还能把别班学生改到自己班
```

```cpp
// routes_teacher.cpp:297-300（积分操作：同样无班级过滤）
            auto it = find_if(users.begin(), users.end(), [&](const User& u) {
                return u.id == student_id && u.role_id == 3;
            });
```

```cpp
// routes_teacher.cpp:118-142（新增学生：className 完全来自请求体）
            string className = req_json.value("className", "");
            ...
            User new_user = { new_id, studentId, hash_password(default_password),
                              3, name, className, points, studentId };
```

**问题**：鉴权只做到**角色级**（`check_permission_middleware(req,res,"student:manage")`，`main.cpp:246` 把该权限授予 role=2 的全体教师），而**对象级授权（ownership）只在一部分读接口实现**（`routes_teacher.cpp:64-74` 通过 `teacher_classes` 求班级集合）。所有变更类接口都没有这层过滤，因此任何教师账号（含默认 `teacher/teacher123`，以及管理员新建的每个教师）可以：
1. `PUT /api/teacher/students/{id}` 修改**任意班级**学生的姓名/班级，并把 `points` 直接设成任意值（`routes_teacher.cpp:203-212`，`points` 参数字段直接写库）；
2. `DELETE /api/teacher/students` 删除任意学生（`routes_teacher.cpp:251-263`）；
3. `POST /api/teacher/points` 给任意学生加减积分（`routes_teacher.cpp:298-343`）；
4. `POST /api/teacher/evaluation` 给任意学生打分（`routes_teacher.cpp:487-489`）；
5. `GET /api/teacher/students` 在教师**未绑定任何班级**时返回全校学生（`routes_teacher.cpp:81` 的 `if (!bound_class_names.empty())` 把过滤整体跳过，注释亦自承“否则保留旧行为”）。

**影响面**：所有 `/api/teacher/**` 写接口 + 学生列表读接口；影响面覆盖全校学生数据完整性。

**建议改法**：
1. 抽出统一的授权原语并**在每个教师接口入口调用**：
   ```cpp
   // 返回 true 表示 target_student_id 属于该教师绑定班级
   bool teacher_owns_student(const std::string& teacher_id, const std::string& student_id);
   // 或按班级：bool teacher_owns_class(const std::string& teacher_id, const std::string& class_name);
   ```
   实现用一条 SQL：`SELECT 1 FROM users u JOIN teacher_classes tc ON tc.class_id = c.id JOIN classes c ON c.name = u.className WHERE u.id=? AND tc.teacher_id=? LIMIT 1`。
2. 对 `PUT/DELETE /api/teacher/students/{id}`、`POST /api/teacher/points`、`POST /api/teacher/evaluation` 补上该检查，未命中返回 403；新增学生时校验 `className ∈ 绑定班级`。
3. 修掉 `!bound_class_names.empty()` 的兜底：未绑定班级的教师应返回空列表（或 403），不能退化为“可以看到全校”。
4. 把“读接口有作用域、写接口没有”的不对称写进代码审查清单，避免回归。

**修复成本**：M（1 个原语 + 5~6 处调用 + 测试）。

---

### B10 —— 未捕获异常被 httplib 转成 500 并把异常原文写进响应头【high】

**位置**：`httplib.h:3561-3582`（框架行为）+ `httplib.h:3666-3672`（响应写出）；触发点 `routes_public.cpp:88,174,392`、`routes_teacher.cpp:161,368,529`、`routes_admin.cpp:183,316,1785,1832`

**证据**：

```cpp
// httplib.h:3561-3581（本仓库 vendored 版本）
inline bool Server::dispatch_request(Request &req, Response &res,
                                     Handlers &handlers) {
  try {
    for (const auto &x : handlers) {
      ...
        handler(req, res);
        return true;
      }
    }
  } catch (const std::exception &ex) {
    res.status = 500;
    res.set_header("EXCEPTION_WHAT", ex.what());     // ← 异常原文进入响应头
  } catch (...) {
    res.status = 500;
    res.set_header("EXCEPTION_WHAT", "UNKNOWN");
  }
  return false;
}
```

```cpp
// routes_public.cpp:17-24（只捕 json::parse_error，但用了强类型 value()）
        try {
            auto req_json = json::parse(req.body);
            string username = req_json.value("username", "");
            ...
        } catch (json::parse_error& e) {              // 88 行：type_error 不在捕获范围
            response = {{"code", 400}, {"msg", "请求数据格式错误"}};
        }
```

```cpp
// json.hpp:22491-22501（value() 在类型不匹配时抛异常，而不是返回默认值）
    ValueType value(const typename object_t::key_type& key, const ValueType& default_value) const
    {
        if (JSON_HEDLEY_LIKELY(is_object()))
        {
            const auto it = find(key);
            if (it != end())
            {
                return it->template get<ValueType>();     // ← 类型不符 → type_error.302
            }
```

**问题**：这些 handler 只捕获 `json::parse_error`，但 `req_json.value("username", "")` 在键存在而**类型不符**时抛 `json::type_error`（json.hpp:22491-22501 已确认），不会被捕获。异常冒到 `dispatch_request` → 框架设 `status=500` 并把 `ex.what()` 写入 **`EXCEPTION_WHAT` 响应头**（`httplib.h:3576`），空 body 返回（`httplib.h:3666-3672` 走 `write_response`）。

可复现路径（无需认证）：`POST /api/auth/login`，body `{"username":123,"password":"x"}` → 客户端收到 `HTTP/1.1 500` + 响应头 `EXCEPTION_WHAT: [json.exception.type_error.302] type must be string, but is number`。

危害：
1. **信息泄露**：异常原文可能包含 nlohmann 的类型/键名细节，或 SQLite/内部路径信息（各 handler 里未捕获的 `db` 层异常同样会走到这里）。项目里多处注释自称“安全修复 V15：不向客户端回显内部异常细节”，但这条通道绕过了所有手工 catch。
2. **接口契约破坏**：前端按 `res.json()` 解析统一 `{code,msg,data}`，遇到空 body 的 500 会抛 `SyntaxError`，表现为“网络错误”，掩盖真实原因；同时 HTTP 500 与业务码体系（B17）进一步错配。
3. 触发面广：`routes_public.cpp:88,174,392`、`routes_teacher.cpp:161,368,529`、`routes_admin.cpp:183,316,1785,1832` 等**仅捕获 json::parse_error 的 handler** 全部受影响（grep 出的完整清单见 §1.3 命令）。

**建议改法**：
1. 每个 handler 的捕获列表补齐 `catch (const std::exception& e)`（并 `catch (...)`），统一返回 `{"code":500,"msg":"服务器内部错误"}`，细节只写 `Logger::error`。可抽一个宏/包装器 `handle(fn)` 或把 body 解析统一为 `parse_body(req, res, out)` 帮助函数，一次修完。
2. 或者在 `main.cpp` 给 httplib 注册全局异常处理器（本仓库 httplib 版本没有 `set_exception_handler`，`grep` 确认只有 `dispatch_request` 的 catch），因此更可行的是**在路由层统一包装**：`register_*_routes` 里用一层 wrapper lambda 把 `handler` 包住（`try { h(req,res); } catch (...) { 500 + 日志 }`）。
3. 无论选哪种，都要求：**任何响应不得包含异常原文**。可在 CI 里加一条冒烟用例断言响应头中不存在 `EXCEPTION_WHAT`。
4. 升级 vendored httplib 到带 `set_exception_handler` 的版本，并在启动时注册脱敏 handler。

**修复成本**：S（加统一 wrapper）～ M（逐 handler 规范化 + 冒烟测试）。

---

### B11 —— 会话 SQL 仍用 `snprintf` 拼接 + `escapeString`：非当前可利用，但属维护陷阱与静默失败【medium】

**位置**：`auth.h:242-246`、`261-266`、`270-274`；对照同文件已参数化的 `auth.h:196-216`（`execute_bind`）与 `223-232`（`query_bind`）

**证据**：

```cpp
// auth.h:240-247
inline bool get_session_info(const std::string& session_id, std::string& user_id, int& role_id,
                             bool& is_parent, std::string& student_id) {
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "SELECT user_id, role_id, is_parent, student_id FROM sessions WHERE session_id = '%s' AND expires_at > %ld",
        db.escapeString(session_id).c_str(), static_cast<long>(time(nullptr)));
    auto result = db.query(sql);
    if (result.empty()) return false;
```

```cpp
// auth.h:261-267
inline bool delete_session(const std::string& session_id) {
    if (session_id.empty()) return false;
    char sql[256];
    snprintf(sql, sizeof(sql), "DELETE FROM sessions WHERE session_id = '%s'",
             db.escapeString(session_id).c_str());
    return db.execute(sql);
}
```

```cpp
// auth.h:270-275
inline void cleanup_expired_sessions() {
    char sql[128];
    snprintf(sql, sizeof(sql), "DELETE FROM sessions WHERE expires_at < %ld",
             static_cast<long>(time(nullptr)));
    db.execute(sql);
}
```

```cpp
// auth.h:199-203（对照：同文件的登录路径已经参数化，并有明确注释）
    // 会话 SQL 使用参数化查询：session_id / user_id 来自 Cookie，属用户可控输入，
    // 不应拼接进 SQL（沿用 sqlite_wrapper.h 中 execute_bind 的约定）
    const char* sql =
        "INSERT OR REPLACE INTO sessions (session_id, user_id, role_id, created_at, expires_at, is_parent, student_id) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)";
```

**问题**（对队长线索 1 的核实结论：**不构成本次可直接利用的注入，但确实是真实隐患**，理由必须说清）：
1. **为什么现在无法注入**：`session_id` 被单引号包裹且经过 `db.escapeString()`（`sqlite_wrapper.h:195-206`）把 `'` 翻倍为 `''`，在 SQLite 中这是正确的字符串字面量转义；`static_cast<long>` 与配置整数不是用户输入。因此现有三个调用点**不可注入**。
2. **为什么仍是隐患**：
   - **模板污染**：同一文件内 190-237 行已迁移到 `execute_bind/query_bind`（注释还写着“不应拼接进 SQL”），242-275 行却保留旧模式。这是最容易被后续开发者复制粘贴的“范例”，而 `escapeString` 是**可选的denylist 兜底**（`sqlite_wrapper.h:194` 注释自称“新代码请用 execute_bind”），任何人漏调一次就是完整 SQLi。风险不是“当前可被利用”，而是“下一次修改极易变成可利用”。
   - **静默截断**：`snprintf` 返回值未检查，超长 `session_id`（攻击者可用 Cookie 发任意长度字符串）会被截断成非法 SQL → `sqlite3_prepare_v2` 失败 → `query()` 返回**空数组**（`sqlite_wrapper.h:58-62`）→ `get_session_info` 返回 `false`，被上层当作“权限不足/未登录”（`auth.h:347`）而不是“内部错误”。`delete_session` 同样静默变成 no-op（登出失效但返回 200）。缓冲区尺寸 1024/256/128 与字段长度之间没有任何断言或校验。
   - **类型细节**：`static_cast<long>` 在 Windows 上是 32 位，`expires_at` 由 `create_session` 以 `long long` 写入（`auth.h:208-209`），2038 年后比较会失真（低危，顺带修正）。
3. **建议改法**：
   ```cpp
   // get_session_info
   auto result = db.query_bind(
       "SELECT user_id, role_id, is_parent, student_id FROM sessions WHERE session_id = ? AND expires_at > ?",
       {SqliteDb::Bind(session_id), SqliteDb::Bind(static_cast<long long>(time(nullptr)))});
   // delete_session
   return db.execute_bind("DELETE FROM sessions WHERE session_id = ?", {SqliteDb::Bind(session_id)});
   // cleanup_expired_sessions
   db.execute_bind("DELETE FROM sessions WHERE expires_at < ?", {SqliteDb::Bind(static_cast<long long>(time(nullptr)))});
   ```
4. 顺手删除 `escapeString()` 这一公开 API（或降级为 `private`），并在 code review / clang-tidy 中禁止在自研代码里出现 `snprintf`+SQL 的组合（可加 CI 正则：`snprintf\([^)]*sql` → 报错）。
5. 在会话入口对 `session_id` 做格式校验（长度必须 = 64 且 `[0-9a-f]`，与 `generate_session_id()` 的产出对齐），一举免疫截断与未来拼接风险。

**影响面**：`get_session_info`（被 `auth.h:347,392` 与 `routes_public.cpp:205` 调用）、`delete_session`（`routes_public.cpp:102`、`routes_parent.cpp:141`）、`cleanup_expired_sessions`（`routes_public.cpp:62`、`routes_parent.cpp:80`）。

**修复成本**：S（3 处替换 + 1 处校验）。

---

### B12 —— `query()/query_bind()` 把 prepare 失败伪装成“空结果”【medium】

**位置**：`sqlite_wrapper.h:53-62`、`151-159`；具体受害调用点 `auth.h:246`、`routes_parent.cpp:25-30`、`routes_teacher.cpp:701-714`、`routes_public.cpp:343-350`、`routes_admin.cpp:1708-1711`

**证据**：

```cpp
// sqlite_wrapper.h:53-62
    json query(const std::string& sql) {
        json result = json::array();
        if (!db_) return result;

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "查询失败: " << sqlite3_errmsg(db_) << std::endl;
            return result;                 // ← 与“查到 0 行”返回的对象完全相同
        }
```

```cpp
// auth.h:246-247（DB 错误 → 被当成“会话无效”）
    auto result = db.query(sql);
    if (result.empty()) return false;
```

```cpp
// routes_parent.cpp:25-30（DB 错误 → 被当成“无权访问”，返回 403）
static bool check_same_parent(const std::string& parent_id, const std::string& target_student_id) {
    auto result = db.query_bind(
        "SELECT 1 FROM parent_students WHERE parent_id = ? AND student_id = ? LIMIT 1",
        {SqliteDb::Bind(parent_id), SqliteDb::Bind(target_student_id)});
    return !result.empty();
}
```

```cpp
// routes_teacher.cpp:701-714（DB 错误 → 图表静默显示 0，HTTP 仍 200）
        json excellentResult = db.query("SELECT COUNT(*) as count FROM evaluations WHERE score >= 90");
        int excellentCount = excellentResult.empty() ? 0 : excellentResult[0].value("count", 0);
```

**问题**：API 契约把“执行失败”和“结果为空”压成同一个值（`json::array()`），调用方无法区分，于是**系统级错误被降级为业务语义**。具体后果清单：
- `auth.h:246` → SQL 失败被报告为“会话无效/权限不足”（401/403），掩盖真实故障；
- `routes_parent.cpp:25-30` → 数据库故障被报告为“无权访问该学生信息”（403），误导排查方向，且家长正常请求被拒；
- `routes_public.cpp:343-350` → 商品查询失败被报告为“商品不存在或已下架”（404）；
- `routes_teacher.cpp:584-591,701-714`、`routes_admin.cpp:1708-1711,375-376` → 统计数字静默变 0 且返回 200，前端展示错误数据（无任何告警）；
- 反向案例：`routes_admin.cpp:341-347` 依赖 `is_null()` 判成功，结果恒为“失败”（见 B13）。
由于 `query()/query_bind()` 内部把错误信息只写到 `std::cerr`（见 B29），线上几乎不可能发现。

**建议改法**：
1. 让 `query/query_bind` 返回携带状态的结构，例如 `struct QueryResult { bool ok; std::string error; json rows; }`，或在失败时**抛出** `std::runtime_error`（配合 B10 的统一 wrapper 转成 500），或返回 `std::optional<json>` 并把错误码/消息通过 `out` 参数抛出；
2. 调用方一律写 `if (!res.ok) return internal_error(res);`，禁止再把 `empty()` 当错误判据；
3. 保留 `rows().empty()` 作为“无数据”的唯一含义，语义分离；
4. 在 `db` 层失败时通过 `Logger::error` 落地日志（含 SQL 与 `sqlite3_errmsg`），并加计数器便于监控。

**修复成本**：M（封装签名 + 全量调用点约 60 处的语义调整，可分文件推进）。

---

### B13 —— 备份接口判据写错：成功也返回“备份失败”【medium】

**位置**：`routes_admin.cpp:335-349`；对照 `routes_admin.cpp:282-283`、`376`（同文件用 `empty()` 判空）

**证据**：

```cpp
// routes_admin.cpp:335-347
        string backup_path = "campus_system_backup_" + get_current_time();
        for (auto& c : backup_path) {
            if (c == ' ' || c == ':') c = '_';
        }
        backup_path += ".db";

        json backup_result = db.query("VACUUM INTO '" + backup_path + "'");
        if (backup_result.is_null()) {
            response = {{"code", 200}, {"msg", "备份成功"}, {"data", {{"path", backup_path}}}};
            Logger::info("数据库备份成功: " + backup_path);
        } else {
            response = {{"code", 500}, {"msg", "备份失败"}};
        }
```

**问题**（对队长线索 6 的核实结论：**路径用户不可控，但该接口存在另一个确定的逻辑错误**）：
1. **路径来源不可控**：`backup_path` 完全由 `get_current_time()`（`sha256.h:191-197`，格式 `%Y-%m-%d %H:%M`）+ 固定前缀 + `':'/' '→'_'` 替换生成，**没有任何请求参数参与**。因此不存在“用户可控路径注入/写入任意位置”的问题，线索 6 的注入前提**不成立**。路径为相对 CWD 的 `campus_system_backup_YYYY-MM-DD_HH_MM.db`。另注：静态资源只从 `frontend/dist` 读取（`routes_static.cpp:10,51`），该备份文件不在服务目录内，不会被 HTTP 下载。
2. **判据必然走 else 分支**：`VACUUM INTO` 不产生结果行，`db.query()` 成功时返回 `json::array()`（`sqlite_wrapper.h:54,88-91`）；`json::array().is_null()` 为 **false**。于是无论备份成功还是失败，都会返回 `{"code":500,"msg":"备份失败"}`（HTTP 200，见 B17）。**功能实际可用但被恒报为失败**，且成功路径的日志（`routes_admin.cpp:344`）永不执行，运维无法确认备份状态。
3. 附加缺陷：`VACUUM INTO` 在目标文件已存在时会报错，而路径只精确到分钟 → 同分钟内二次备份必然失败；备份文件无轮转/清理（`backupConfig.retentionDays: 7` 只是 `routes_admin.cpp:1240-1243` 里的写死展示值，无实现）；返回值未检查 `backup_result` 之外的任何东西（也未校验目标文件真的生成）。
4. 若仍保留字符串拼接，路径虽不可控但风格上仍是“拼接 SQL”，建议一并参数化：`db.execute_bind("VACUUM INTO ?", {SqliteDb::Bind(backup_path)})`（SQLite 支持 `VACUUM INTO` 带表达式参数）。

**影响面**：`POST /api/admin/system/backup`（管理员）。

**建议改法**：
1. 用 `db.execute_bind("VACUUM INTO ?", {SqliteDb::Bind(backup_path)})` 并**检查返回值**：
   ```cpp
   if (db.execute_bind("VACUUM INTO ?", {SqliteDb::Bind(backup_path)}) && file_exists(backup_path)) { 200 } else { 500 + Logger::error }
   ```
2. 路径加入秒级或唯一时间戳（`%Y-%m-%d_%H-%M-%S`）避免同分钟冲突；按 `retentionDays` 实现清理；把备份放在 `backups/` 子目录。
3. 顺手修掉 `routes_admin.cpp:1229-1244` 的假配置（见 B19），让 `/api/admin/system/config` 返回真实 `g_config` 值。

**修复成本**：S。

---

### B14 —— 未限制请求体大小与读取超时：内存/慢速连接 DoS【medium】

**位置**：`main.cpp:286-306`（无任何 Server 参数设置）；框架默认 `httplib.h:44,2955,3077-3079`

**证据**：

```cpp
// httplib.h:44
#define CPPHTTPLIB_PAYLOAD_MAX_LENGTH ((std::numeric_limits<size_t>::max)())
```

```cpp
// httplib.h:2955,3077-3079
      payload_max_length_(CPPHTTPLIB_PAYLOAD_MAX_LENGTH), is_running_(false),
...
inline void Server::set_payload_max_length(size_t length) {
  payload_max_length_ = length;
}
```

```cpp
// main.cpp:285-306（创建 Server 后只设置了 logger 与线程池：295-300 行注册路由，306 行设线程池）
    httplib::Server svr;

    // 5. 设置请求日志
    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        log_request(req);
        log_response(req.path, res.status);
    });
    ...
    svr.new_task_queue = [&]() { return new httplib::ThreadPool(g_config.thread_count); };
```

**问题**：全项目 grep 确认**没有**调用 `set_payload_max_length`、`set_read_timeout`、`set_keep_alive_max_count`、`set_error_handler`（唯一的 Server 级设置为 `main.cpp:306` 的线程池）。默认上限是 `SIZE_MAX`，即「无上限」，而 httplib 会把整个 body 读进内存并交给 `json::parse`/multipart 解析：
1. 攻击者发送超大 `Content-Length` body（或慢速发送 + 长连接）→ 8 个 worker 全部阻塞在读取与解析上（无限内存增长），服务整体不可用；
2. `/api/admin/import`（`routes_admin.cpp:1432`）与 CSV 导入（`routes_teacher.cpp:857-864`，multipart 文件同样全量进内存）是放大点，JSON 解析还会额外占用数倍内存；
3. 未设读超时 → Slowloris 型连接可长期占用 8 个线程中的若干个。

注：本机部署（`config.h:15` 默认 127.0.0.1）降低了外部风险，但只要 `host` 被改成 `0.0.0.0`（配置文件可改）即可被远程利用；且未认证接口（`/api/auth/login`、`/api/auth/register`）同样受影响。

**建议改法**：
```cpp
svr.set_payload_max_length(2 * 1024 * 1024);   // 按业务上限设定（导入/CSV 另给出更合理的上限）
svr.set_read_timeout(10, 0);
svr.set_write_timeout(10, 0);
svr.set_keep_alive_max_count(100);
svr.set_idle_interval(0, 100000);              // 视需要
```
并在 handler 内对关键字段再做长度上限校验（如 `content`、`csv_data`、`reason`、评价 `comment`），因为 payload 上限是最后一道闸，不是唯一一道。

**影响面**：全站所有接口。

**修复成本**：S。

---

### B15 —— `/api/admin/import` 静默降级角色且不更新索引【medium】

**位置**：`routes_admin.cpp:1464-1505`、`1667`；相关 `routes_admin.cpp:612-634`（批量导入学生反而有去重与索引更新）

**证据**：

```cpp
// routes_admin.cpp:1464-1480（overwrite 分支：role_id 被强制改成 3）
                    if (existing != users.end()) {
                        if (mode == "overwrite") {
                            existing->username = username;
                            std::string plain = user_json.value("password", "");
                            if (!plain.empty()) {
                                existing->password_hash = hash_password(plain);
                            }
                            // 安全修复 V1：导入时 role_id 仅允许学生；提升角色需在专门接口
                            int role = user_json.value("role_id", 3);
                            if (role != 3) role = 3;              // ← 管理员/教师/家长被降级为学生
                            existing->role_id = role;
```

```cpp
// routes_admin.cpp:1484-1502（new 分支：push_back 后未调用 update_user_index）
                        users.push_back(new_user);
                        save_user_to_db(new_user);
                        imported_count++;
```

**问题**：
1. **角色降级（数据破坏）**：`/api/admin/export`（`routes_admin.cpp:1344-1356`）导出的用户带有真实 `role_id`；用这份文件 `mode=overwrite` 导回时，所有 `role_id != 3` 的用户（管理员、教师、家长）被**静默改成学生**。导出→导入这一“备份恢复”动作会破坏整个权限体系，且响应只报 `{"code":200,"msg":"导入完成"}`，无任何提示。管理员也不再有管理员账号。（这是 V1“防提权”修复的过度修正：应“拒绝/跳过”而非“篡改”。）
2. **导入的新用户不进索引**：新分支 `users.push_back()`（1501）没有配套 `update_user_index(new_user)`，而 `user_id_map`/`user_username_map` 只在 `init_indexes()`（1667，导入结束后）与 `update_user_index()` 中维护——但 `init_indexes()` 是导入**成功结束时**才调用；此外它不 `clear()`（见 B6），且**该函数内部一次性重建**确实能让新用户可登录，所以主路径最终可恢复。真正的问题是：若导入因异常走到 `catch`（`routes_admin.cpp:1682-1686`）→ `init_indexes()` 不执行，而事务已 `ROLLBACK`（DB 侧回滚）却**内存里已 push_back 的用户不会撤销** → 内存与 DB 分叉（用户能登录但 DB 无记录，重启后消失）。
3. `error_count` 恒为 0（`routes_admin.cpp:1451,1675`），从不递增 → 导入报告没有失败计数。

**影响面**：`POST /api/admin/import`；间接影响全部依赖角色判定的接口。

**建议改法**：
1. 角色按白名单处理：`role` 不在 {1,2,3,4} 或与导入者权限不符时**跳过该用户并计入 failed 列表**，绝不改写为 3；管理员自身角色永远不因导入变化。
2. 导入改为“先校验全部记录 → 再开事务写入 → 提交后一次性 `init_indexes()`（先 clear）”；异常路径要同时回滚内存（可用“先在临时容器构造，成功后一次性 swap”）而不是只回滚 DB。
3. `catch` 分支补 `users` 的一致性恢复（或使用 `db.execute("ROLLBACK")` + 从 DB 重新 `load_users_from_db()` + `init_indexes()`）。
4. `error_count` 真正累加并进入响应。
5. 增加 dry-run 模式（`"dry_run": true`）供管理员预检。

**修复成本**：M。

---

### B16 —— 角色/权限管理只改内存不落库，鉴权与展示各读一处【medium】

**位置**：`routes_admin.cpp:958-988,1014-1054,1056-1090,1092-1126,1128-1167,1169-1218,936-940`；鉴权 `auth.h:280-303`；DB 种子 `main.cpp:244-248`；内存种子 `models.cpp:49-54`

**证据**：

```cpp
// routes_admin.cpp:978-981（新增角色：只 push_back 到内存）
                int new_id = roles.empty() ? 1 : roles.back().id + 1;
                roles.push_back(Role{new_id, name, description});
                response = {{"code", 200}, {"msg", "角色添加成功"}};
```

```cpp
// routes_admin.cpp:936-940（角色列表里的权限却从数据库读）
            json perms_result = db.query_bind(
                "SELECT p.id, p.name, p.code FROM permissions p "
                "INNER JOIN role_permissions rp ON p.id = rp.permission_id "
                "WHERE rp.role_id = ?",
                {SqliteDb::Bind((long long)role.id)});
```

```cpp
// auth.h:291-300（鉴权只用内存里的 role_permissions）
        for (const auto& rp : role_permissions) {
            if (rp.role_id == role_id) {
                auto perm_it = find_if(permissions.begin(), permissions.end(), [&](const Permission& p) {
                    return p.id == rp.permission_id && p.code == permission_code;
                });
```

**问题**：存在**两份互相独立的 RBAC 数据**——DB 里的 `roles/permissions/role_permissions`（只在 `main.cpp:244-248` 初始化，且**从未被读回内存**；`load_users_from_db()` 只加载 `users`，见 `models.cpp:58-74`）与内存里的 `roles/permissions/role_permissions`（`models.cpp:25-54` 硬编码）。而：
- 鉴权只读**内存**（`auth.h:280-303`）；
- `/api/admin/roles` 的权限列表却读**DB**（`routes_admin.cpp:936-940`）。

于是新增/编辑/删除角色与权限（`routes_admin.cpp:980,1045,1081,1117,1158,1207`）只改内存：**重启即丢失**、**对鉴权无任何影响**、**对 `/api/admin/roles` 输出也无影响**（它读库）——管理员看到的界面与实际生效的权限完全脱节。同时**没有任何接口可以给角色分配权限**（`role_permissions` 只能在导入时被追加，`routes_admin.cpp:1576`），所以“权限管理”功能整体不可用；`role_permissions.push_back`（1576）同样不落库。

**影响面**：`/api/admin/roles*`、`/api/admin/permissions*`、所有依赖 `check_permission`/`check_permission_optimized` 的接口。

**建议改法**：
1. 明确**单一权威源**：推荐 DB 为权威——启动时把 `roles/permissions/role_permissions` 从 DB 读入内存（新增 `load_rbac_from_db()`），所有增删改同时写库并刷新内存（或直接改为每次按需查库 + 缓存失效）。
2. 补上缺失的“角色-权限分配”接口（`POST /api/admin/roles/{id}/permissions`），并写库。
3. 若短期内不实现持久化，则**必须**把接口返回改为“未实现/只读”，并在 UI 上禁用，避免管理员误以为已生效（当前行为属于静默失败）。
4. 加测试：新增权限后立即用该权限访问对应接口，断言生效；重启后再次断言。

**修复成本**：M。

---

### B17 —— HTTP 状态码与业务 code 混用，且无统一响应封装【medium】

**位置**：全 `routes_*.cpp`（统计见下表）

**证据**（机械统计，命令见 §1.3）：

| 文件 | `res.status =` 次数 | `{"code",N}` 字面量次数 | `res.set_content` 次数 |
|---|---|---|---|
| `auth.h` | 8 | 7 | 8 |
| `routes_public.cpp` | 1 | 32 | 29 |
| `routes_admin.cpp` | 1 | 90 | 46 |
| `routes_teacher.cpp` | 3 | 50 | 35 |
| `routes_student.cpp` | 1 | 8 | 11 |
| `routes_parent.cpp` | **0** | 30 | 20 |
| `routes_static.cpp` | 2 | 0 | 2 |

```cpp
// routes_public.cpp:22-25（业务 400，HTTP 仍是 200）
            if (username.empty() || password.empty()) {
                response = {{"code", 400}, {"msg", "用户名和密码不能为空"}};
                res.set_content(response.dump(), "application/json");
                return;
            }
```

```cpp
// routes_public.cpp:30-34（同一个文件里又设置了状态码 —— 风格不一致）
            if (!login_can_try(lk)) {
                response = {{"code", 429}, {"msg", "登录尝试过于频繁，请稍后再试"}};
                res.status = 429;
```

```cpp
// auth.h:327-331（中间件里则设置了状态码）
        res.status = 401;
        json response = {{"code", 401}, {"msg", "未登录"}};
```

**问题**：
1. **217 处**手工拼装的 `{"code",N}` 响应体（外加大量多行 `response = {...}` 形式，未计入）只有 **6 处**（routes_*）设置了 HTTP 状态码。即：绝大多数“用户名密码错误”“权限不足”“商品不存在”都以 **HTTP 200** 返回。前端若用 `fetch().ok`/axios 拦截器按状态码分流，会全部走成功分支；反之中间件返回的 401/403 又会跳进错误分支 —— 两种风格并存，必然出现“有的 401 弹登录框、有的不弹”。
2. **无统一 helper**：`res.set_content(response.dump(), "application/json") + set_cors_headers` 这组三行在每个分支重复；`routes_admin.cpp` 出现了两套风格（`json response = {{"code",200},...}` 与 `json response; response["code"]=200;`，如 `routes_teacher.cpp:773-776`、`routes_student.cpp:51-53,86-88,144-146`），`routes_admin.cpp:519-553` 甚至存在“先赋值响应但未 return，最后统一 `set_content`”与“中途 `set_content`+return”混用，导致同一个响应体可能被写两次（`res.set_content` 覆盖，最后一次生效，容易写出“错误信息被后写覆盖”的 bug）。
3. 响应体字段命名不统一：`studentId`/`student_id`/`className`/`class_name`/`cost`/`price`（`routes_student.cpp:187` 用 `price`）等混用，前端需要大量适配。

**建议改法**：
1. 抽出统一封装（放 `auth.h` 或新建 `http_util.h`）：
   ```cpp
   inline void send_json(httplib::Response& res, const httplib::Request& req, int status,
                         int code, const std::string& msg, json data = nullptr) {
       res.status = status;                       // HTTP 状态与业务码同源
       set_cors_headers(req, res);
       json body{{"code", code}, {"msg", msg}};
       if (!data.is_null()) body["data"] = data;
       res.set_content(body.dump(), "application/json");
   }
   ```
   约定：业务错误 → 400/401/403/404/409/429；系统错误 → 500；成功 → 200。
2. 全量替换 217 处字面量为 `send_json(...)`，消灭“重复 set_content”。
3. 统一字段命名为下划线或小驼峰之一，并在 `docs/` 里维护接口契约表；前端与后端共用该契约（可作为后续独立任务）。

**修复成本**：M（机械替换量大但模式单一，可分批按文件推进）。

---

### B18 —— 内存与 DB 双份数据源不原子，导致积分/班级/记录不一致【medium】

**位置**：`models.cpp:93-97`、`routes_public.cpp:381-382`、`routes_teacher.cpp:203-212,326,331-340`、`routes_admin.cpp:1877-1879,1388-1399`、`main.cpp:113-122`（`points_records` 建表）

**证据**：

```cpp
// models.cpp:93-97（积分用绝对值覆盖）
bool update_user_points_in_db(const string& user_id, int points) {
    return db.execute_bind(
        "UPDATE users SET points = ? WHERE id = ?",
        {SqliteDb::Bind((long long)points), SqliteDb::Bind(user_id)});
}
```

```cpp
// routes_admin.cpp:1874-1879（删班级只改库，不改内存 users）
            if (ok) {
                if (!class_name.empty()) {
                    // 安全修复 V2：参数化更新学生班级
                    db.execute_bind(
                        "UPDATE users SET className='' WHERE className=?",
                        {SqliteDb::Bind(class_name)});
                }
```

```cpp
// routes_teacher.cpp:331-340（内存记录用 size()+1 生成 ID，且只增不载）
            int new_record_id = points_records.size() + 1;
            PointsRecord new_record = { new_record_id, student_id, ... };
            points_records.push_back(new_record);
```

```cpp
// routes_admin.cpp:1388-1399（导出用了内存 points_records，其余表却查库）
        json points_records_json = json::array();
        for (const auto& record : points_records) {
            ...
        }
        export_data["points_records"] = points_records_json;
```

**问题**：`users`（内存）与 `users`（DB）双写，`points_records`（内存）与 `points_records`（DB）双份，但两者**没有一致性协议**：
1. **积分覆盖式写入**：`update_user_points_in_db` 传绝对值为参数，任何并发/重复调用都会丢更新（配合 B8 变成双花）。
2. **删班级后内存不刷**：`routes_admin.cpp:1877` 只更新 DB，内存 `users[i].className` 仍是旧值，而 `/api/teacher/students`（`routes_teacher.cpp:78-96`）、`/api/admin/dashboard`（`routes_admin.cpp:367-373`）、`/api/student/info`（`routes_student.cpp:37-49`）等**读内存** → 前端仍显示学生属于已删除班级，且 `check_permission`（`auth.h:280-303`）依赖内存 `role_id`（DB 被外部工具/导入改动后不会同步）。
3. **`points_records` 只增不载**：进程启动时从不从 DB 读取（`load_users_from_db` 只加载 users），因此 `/api/admin/export` 的 `points_records` 只包含**本次运行期间**产生的记录（重启即空），而同一响应里的 `evaluations/mall_items/redemption_records` 是查库的全量 —— 导出的“备份”本身不完整，用它恢复会丢积分历史。
4. **`new_record_id = size()+1`** 与 DB 自增 ID（`main.cpp:114`）无关，前端拿到的 `record.id` 与库中 id 不一致，且并发下重复（见 B5）。另外 `routes_teacher.cpp:363` 的 `operatorName` 是写死的 `"王老师"`（见 B26）。
5. **CSV 导入不返回初始口令**：`routes_teacher.cpp:926` 用 `generate_random_password()` 生成口令，但响应（`routes_teacher.cpp:959-967`）只返回成功/失败计数，**不含口令** → 该批学生账号无人知道口令，无法登录（对比单条新增学生会返回 `initial_password`，`routes_teacher.cpp:157`）。这是 V13“不再硬编码 123456”修复后遗留的功能缺口。

**建议改法**：
1. 积分一律用**相对更新**：`UPDATE users SET points = points + ? WHERE id = ?`（扣分加 `AND points + ? >= 0` 守卫），内存对象只作展示缓存并在写库成功后用 DB 值刷新。
2. 引入“单一权威源”原则：写操作先落库、再统一刷新受影响的内存对象（封装 `apply_user_update(...)`），避免散落的“改内存 + 改库”两段式代码；长期方案是去掉内存用户表，改为按请求查库 + LRU 缓存。
3. 删班级/改班级后同步刷新内存中受影响学生的 `className`（或改为查库）。
4. `points_records` 增加启动加载（`load_points_records_from_db()`）或让导出直接查库；`new_record_id` 改由 DB 生成（`INTEGER PRIMARY KEY AUTOINCREMENT`）。
5. CSV 批量导入的响应补充 `initial_password`（未提供密码时生成的），或要求导入方必须提供密码；二者择一但必须在文档中明确。

**修复成本**：M（相对更新 + 刷新封装 + 导入响应）；L（彻底去内存表）。

---

### B19 —— 无统一封装的管理端假数据与从未生效的权限码【medium】

**位置**：`routes_admin.cpp:52-72`、`1229-1244`；权限码使用面统计（§1.3 命令输出）；`main.cpp:230-248`

**证据**：

```cpp
// routes_admin.cpp:60-70（系统信息写死）
        json response = {
            {"code", 200},
            {"msg", "success"},
            {"data", {
                {"system_name", "校园文明能量站"},
                {"version", "1.0.0"},
                {"admin_count", 1},        // ← 固定 1，与真实用户数无关
                {"teacher_count", 1},
                {"student_count", 2}
            }}
        };
```

```cpp
// routes_admin.cpp:1235-1243（"配置"接口返回写死的常量，PUT 版本 301-321 什么都不做）
            {"securityConfig", {
                {"loginTimeout", 30},
                {"minPasswordLength", 6},
                {"enableCaptcha", false}
            }},
            {"backupConfig", {
                {"frequency", "daily"},
                {"retentionDays", 7}
            }}
```

```cpp
// routes_admin.cpp:301-321（PUT /api/admin/system/config 只回“保存成功”，注释自承“这里可以保存配置”）
            auto req_json = json::parse(req.body);
            // 这里可以保存配置到数据库或文件
            response = {{"code", 200}, {"msg", "配置保存成功"}};
```

**问题**：
1. `GET /api/admin/system` 的计数是硬编码的假数据，与真实 `users` 完全脱钩（同文件 `routes_admin.cpp:362-373` 已经在算真实计数，两处不一致）；前端两个页面会显示互相矛盾的数字。
2. `GET /api/admin/system/config` 返回的“安全配置”全部是常量，**与真实运行的 `g_config`（`config.h:13-37`）无关**；`PUT` 版本解析请求后什么都不做却回复“保存成功” → 管理员以为改动了 `loginTimeout`/`minPasswordLength`/备份保留策略，实际**没有生效**（例如 `minPasswordLength` 在真实代码里是分散的 6 位判断，见 B20；`retentionDays` 无实现，见 B13）。
3. **权限码空转**：`main.cpp:230-242` 定义 12 个权限码并授予各角色（`main.cpp:244-248`），但 grep 显示 `check_permission_middleware` 只用到 7 个：`system:manage`、`user:manage`、`student:manage`、`points:manage`、`evaluation:manage`、`statistics:view`、`mall:manage`。**从未被任何路由使用的 5 个**：`parent:manage`(8)、`message:manage`(9)、`class:manage`(10)、`redemption:manage`(11)、`data:export`(12)（例如 `/api/admin/export` 用的是 `system:manage`，`/api/admin/classes` 用的是 `user:manage`，家长留言接口用的是 `student:manage`）。角色 2（教师）被授予的 `class:manage`/`redemption:manage`/`message:manage` 因此毫无作用，而 `role_permissions` 的实际含义与文档不符。

**影响面**：`/api/admin/system`、`/api/admin/system/config`（GET/PUT）、权限矩阵整体可维护性。

**建议改法**：
1. `GET /api/admin/system` 改为复用真实统计（把 `routes_admin.cpp:362-373` 的计数逻辑抽成函数并两处共用）。
2. `GET /api/admin/system/config` 返回 `g_config` 的真实字段（端口、线程数、会话时长、锁定阈值、CSRF 开关、CORS 白名单等）；`PUT` 要么实现持久化+热更新（写入 `config.json` 并加锁重载），要么明确返回 501/400“不支持运行时修改”，不要谎报成功。
3. 权限码与路由建立**显式映射表**（代码里一张 `route → permission_code` 表，或至少在 `docs/` 中列出并加测试），把未使用的权限码真正接到对应接口（`class:manage` → `/api/admin/classes*`，`data:export` → `/api/admin/export`，`parent:manage` → 家长账号管理，`redemption:manage` → 兑换记录查看，`message:manage` → 留言接口），否则从权限表中删除以免误导。

**修复成本**：S（假数据/权限码映射）～ M（配置热更新）。

---

### B20 —— 口令比较非常量时间；密码强度校验不一致；迭代次数无上限【low】

**位置**：`sha256.h:170-188`、`routes_public.cpp:131-136`、`routes_admin.cpp:491-492`、`routes_teacher.cpp:926`、`routes_admin.cpp:862-871`

**证据**：

```cpp
// sha256.h:186-187（旧哈希走非常量时间比较）
    // 旧版兼容：无盐 SHA256
    return sha256(password) == hash;
```

```cpp
// sha256.h:176（迭代数来自存储串，未做上下界校验）
        int iters = std::atoi(hash.c_str() + 7);
```

```cpp
// routes_public.cpp:131-136（只有注册入口做长度校验）
            // 安全修复 V8：密码最小长度校验
            if (password.length() < 6) {
```

```cpp
// routes_admin.cpp:491-492（管理员建用户：只校验非空）
            if (username.empty() || name.empty() || password.empty()) {
```

**问题**：
1. 旧哈希分支用 `std::string::operator==`（逐字节短路），理论上存在比较时序侧信道；虽然远程测量的噪声很大，但项目已经在别处（`verify_password` 的 pbkdf2 分支 `sha256.h:181-184`、CSRF 比较 `auth.h:164-167`）刻意实现了常量时间比较，属**风格不一致**，修复成本极低。
2. 密码长度下限只在自助注册（`routes_public.cpp:132`）实现；管理员创建用户（`routes_admin.cpp:491`）、SQL/JSON 导入（`routes_admin.cpp:1490-1492`）、教师 CSV 导入（`routes_teacher.cpp:926` 直接采用 CSV 里的口令）、管理员重置密码（`routes_admin.cpp:866` 只在非自动生成时才校验）均无下限 → 可以创建 1 字符口令的账号（配合 B2/B1 影响放大）。
3. `atoi(hash.c_str()+7)` 无上限：若 DB 中的哈希被写入超大迭代数（例如通过 B16 的导入路径或直接改库），校验会变成 CPU 炸弹（配合 B14 可放大为 DoS）。
4. 值得肯定的是：`hash_password`（`sha256.h:162-166`）确实做了 per-user 随机盐（`generate_random_hex(16)`）与 100000 次迭代 —— **因此“全项目无 per-user salt”的说法不成立**，问题只在算法实现错误（B1）与上述细节。

**建议改法**：
1. 旧分支改为常量时间比较（复用 `sha256.h:182-184` 的写法或抽 `constant_time_equal(a,b)`）。
2. 统一在**所有**设置口令的入口调用同一个 `validate_password_strength()`（长度 ≥ 8、含字母数字、拒绝用户名/学号等），放在服务端而非各 handler。
3. `iters` 限幅：`iters = std::clamp(iters, 10000, 1000000)`，非法值直接判失败。
4. 顺带：`hash_password` 建议把迭代数提升到 210k 以上（OWASP 2023 对 PBKDF2-SHA256 的建议）并存入字符串（已经是这个格式，易于升级）。

**修复成本**：S。

---

### B21 —— CSPRNG 使用细节：重复构造 device、逐字节取样、模偏差【low】

**位置**：`sha256.h:150-159`、`200-227`

**证据**：

```cpp
// sha256.h:150-159
// 安全修复 V4：CSPRNG —— 使用 std::random_device 生成随机字节
inline std::string generate_random_hex(int bytes) {
    std::random_device rd;
    std::ostringstream oss;
    for (int i = 0; i < bytes; i++) {
        unsigned char b = (unsigned char)(rd() & 0xFF);
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    }
    return oss.str();
}
```

```cpp
// sha256.h:206-216（模偏差 + 逐字符调用 rd()）
    std::random_device rd;
    std::uniform_int_distribution<int> len_dist(0, 4);
    int length = 8 + len_dist(rd);

    std::string password;
    password += uppercase[rd() % uppercase.length()];
    password += lowercase[rd() % lowercase.length()];
```

**问题**（安全性 OK，属质量/性能问题，故列 low）：
1. 安全结论（已取证）：`std::random_device` 在本工具链上**确实**是密码学安全源。证据链：`C:\mingw64\...\bits\c++config.h:1693,1755` 显示 `_GLIBCXX_USE_DEV_RANDOM`/`_GLIBCXX_USE_RANDOM_TR1` 均未定义 → 走 Windows 专用回退实现；`server.exe` 二进制中存在 `"random_device: rdseed failed."`/`"random_device: rand_s failed."` 字符串与 `advapi32!SystemFunction036`（= `RtlGenRandom`）导入 → 使用 rdseed/rdrand/`rand_s` 链路。因此 `generate_session_id()`（`auth.h:102-104`，32 字节 = 256 位）具备足够熵。
2. 缺陷：每次调用都**新建** `random_device`（会重新探测/打开底层源），并为**每个字节**调用一次 `rd()` 后只取低 8 位（浪费 24 位熵、每次调用数十次系统调用）；`rd() % n` 引入极小模偏差（n=62 时偏差 10^-7 量级，实践中无影响）。
3. `generate_random_password` 的 Fisher–Yates 用 `rd() % length`，同样是模偏差；且长度仅 8–12 位。

**建议改法**：
```cpp
inline const std::random_device& rng_device() { static std::random_device rd; return rd; }  // 静态复用
// 或更好：用一个 CSPRNG 引擎持有 256 位种子：
// static std::mt19937_64 gen(seed_from_random_device());
// 取字节时一次性使用 rd() 返回的 32 位（4 字节/次调用），并用 std::uniform_int_distribution 消除模偏差
```
（注意：`std::mt19937_64` 只适合与随机种子配合的会话 ID 场景，密钥类材料建议继续直接用 `random_device`/`BCryptGenRandom`。）

**影响面**：`auth.h:102-104`（会话 ID）、`auth.h:172`（CSRF token）、`sha256.h:163`（盐）、`sha256.h:200`（随机口令）。

**修复成本**：S。

---

### B22 —— 🔶疑似：`localtime()` 在无锁上下文并发调用【low】

**位置**：`routes_teacher.cpp:554-557`、`routes_teacher.cpp:646-647`、`routes_admin.cpp:1277-1285`；对照 `logger.h:72-75`（在锁内）

**证据**：

```cpp
// routes_teacher.cpp:554-557
        time_t now = time(nullptr);
        tm* today_tm = localtime(&now);
        char today_str[16];
        strftime(today_str, sizeof(today_str), "%Y-%m-%d", today_tm);
```

```cpp
// routes_admin.cpp:1277-1285
        time_t now = time(nullptr);
        tm* tm_info = localtime(&now);
        ...
            tm* target_tm = localtime(&target_time);
```

**问题**：`localtime()` 返回指向共享 `struct tm` 的指针（C 标准明确其可能使用静态缓冲）。这些调用发生在 httplib 的 8 个 worker 线程中且**没有任何锁**；与之对比，`Logger::log` 里的 `localtime`（`logger.h:73`）被 `std::mutex`（`logger.h:61`）保护。若运行库的 `localtime` 使用进程级共享缓冲，则并发调用会产生**时间字段串扰**（偶发的错误日期/月份，影响“今日积分统计”与 7 日趋势图），构成数据竞争。
**为什么标为疑似**：Windows UCRT/MSVCRT 的 `localtime` 在多数版本里使用线程局部缓冲，实际串扰概率低；我无法在不运行程序的前提下确证本二进制所用 CRT 的行为。但“不该依赖实现细节”这一点是确定的。

**建议改法**：统一改用 `localtime_s(&tm_buf, &t)`（MSVC/MinGW 均可用）或 `gmtime_r`/`localtime_r`（POSIX），并在项目内抽一个 `std::tm safe_localtime(std::time_t)` 帮助函数；顺带把 `routes_teacher.cpp:646-661` 的月份计算（手写跨年回退）也搬进该函数。

**影响面**：教师工作台统计、教师统计图、管理员统计的趋势图。

**修复成本**：S。

---

### B23 —— 单连接串行化 + 重复会话查询 + N+1 查询（性能）【low】

**位置**：`sqlite_wrapper.h:14-32,27-29`（注释与实际不符）、`auth.h:196-197`、`auth.h:323-364`、`routes_admin.cpp:1704-1712`、`routes_teacher.cpp:560-570`

**证据**：

```cpp
// sqlite_wrapper.h:25-29
        // 并发修复：服务器为多线程（ThreadPool 共享本连接），
        // 无 busy_timeout 时并发写会立即返回 SQLITE_BUSY（database is locked）
        sqlite3_busy_timeout(db_, 5000);
        // WAL 模式：读写不互斥，进一步降低并发下的锁冲突
        execute("PRAGMA journal_mode=WAL;");
```

```cpp
// routes_admin.cpp:1704-1711（列表里对每行再发一条 COUNT 查询 → N+1）
        json result = db.query("SELECT id, name, grade, grade_code, class_code, head_teacher, description FROM classes ORDER BY id");
        for (const auto& row : result) {
            string class_name = row.value("name", "");
            json count_result = db.query_bind(
                "SELECT COUNT(*) as cnt FROM users WHERE className=? AND role_id=3",
                {SqliteDb::Bind(class_name)});
```

```cpp
// auth.h:334-347（每个受保护请求至少 2 次会话查询，verify_session 里还可能 1 次 DELETE）
    std::string user_id = verify_session(session_id);
    ...
    if (!get_session_info(session_id, user_id, role_id, is_parent, student_id) || is_parent) {
```

**问题**：
1. **注释与实际不符**（这是本轮线索 4 的关键判断）：`sqlite3_busy_timeout` + WAL 只能缓解**多连接之间**的锁冲突。本进程只有**一条**连接（`sqlite_wrapper.h:209`，全局单例 `main.cpp:22`），线程间不存在 `SQLITE_BUSY`；真正的串行化来自 SQLite 的**连接级互斥**（`SQLITE_THREADSAFE` 默认 1，见 `sqlite3.c:14046-14052`；`README.md:249` 的编译命令未传 `-DSQLITE_THREADSAFE`）。因此“加了 busy_timeout 就解决并发”的判断是错的：它既不解决 B7/B8 的语义竞态，也（在单连接下）不解决 B5 的容器竞态。
2. **吞吐**：所有 DB 调用被一把内部互斥串行化，8 个 worker 线程在 DB 上排队；WAL 在此模型下收益有限（单连接无法真正并行读）。
3. **重复会话查询**：每个受保护请求先 `verify_session`（1 次 SELECT，可能 1 次 DELETE）再 `get_session_info`（1 次 SELECT，且是 B11 的拼接版本），`check_permission_middleware`（`auth.h:323-364`）之后 handler 往往**再查一遍** `verify_session`（如 `routes_teacher.cpp:14` 与 `18-19`、`routes_student.cpp:15` 与 `19-20`）→ 单请求 3~4 次会话查询。
4. **登录时的全表清理**：每次登录都执行 `DELETE FROM sessions WHERE expires_at < now`（`auth.h:196-197`），若索引未命中会全表扫描（`idx_sessions_expires_at` 已建，`main.cpp:217`，风险可控）。
5. **N+1**：班级列表（`routes_admin.cpp:1708`）、教师工作台（`routes_teacher.cpp:560-570` 对**每个学生**发一条 JOIN 查询）随数据量线性放大。

**建议改法**：
1. 改为**每线程一条连接**（`thread_local`）或连接池（`N = thread_count`），配合 WAL + `busy_timeout` + `BEGIN IMMEDIATE`，让 WAL 真正生效并获得并行读；
2. 会话校验合并为一次 JOIN 查询（一次拿到 `user_id/role_id/is_parent/student_id/permissions`），并在同一请求内**缓存**（把结果放进线程局部或请求上下文对象），去掉重复的 `verify_session`；
3. N+1 改为一条聚合 SQL（`LEFT JOIN` + `GROUP BY`，或 `IN (...)` 批量查询）；
4. 修正 `sqlite_wrapper.h:25-28` 的注释，避免误导后续维护者（重要：这条注释正是“已知问题被掩盖”的典型）。

**影响面**：全站性能；并发压测下最明显的是教师/管理员统计页与班级管理页。

**修复成本**：M（thread_local 连接 + 归并查询）。

---

### B24 —— `get_cookie_value()` 裸子串匹配导致 Cookie 名混淆【low】

**位置**：`auth.h:67-82`

**证据**：

```cpp
// auth.h:71-81
    std::string cookie_header = it->second;
    std::string search = name + "=";
    size_t pos = cookie_header.find(search);      // ← 未要求出现在 Cookie 名边界
    if (pos == std::string::npos) return "";
    pos += search.size();
    size_t end = cookie_header.find(';', pos);
```

**问题**：只做子串查找，不校验前缀是 `; ` 或串首。若请求同时带有 `xsid=AAA; sid=BBB`（或任何以 `sid=` 结尾的 cookie 名，如 `mysid=`），`find("sid=")` 会命中 `xsid=` 内部的位置并返回 `AAA`。会话 ID 因此可能取自错误的 Cookie。在当前自研代码里没有设置其他 `*sid` cookie，所以**不可被远程直接利用**；但一旦引入同名后缀 cookie（或与前端/其他中间件共享域），会话解析就会静默错乱，且攻击者若能写同域 Cookie（如通过子域 XSS）可借此做会话固定/覆盖。

**建议改法**：按 `';'` 切分再对每段 `trim` 后判断 `segment.compare(0, name.size(), name) == 0 && segment[name.size()] == '='`，或使用现成的 cookie 解析器；同时拒绝值中含 NUL/超过长度上限（见 B11 的 session_id 格式校验）。

**影响面**：所有依赖 `get_cookie_value(req,"sid")`/`"csrf_token"` 的接口。

**修复成本**：S。

---

### B25 —— `set_cors_headers(res)` 单参重载是死代码；401/403 与预检不回显 ACAO【low】

**位置**：`auth.h:21-33`、`auth.h:329,338,350,358`、`routes_static.cpp:72-75`

**证据**：

```cpp
// auth.h:21-33
// 安全修复 V9：CORS 头设置 —— 仅在请求 Origin 命中白名单时回显
inline void set_cors_headers(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-CSRF-Token");
    if (g_config.cors_allowed_origins.empty()) {
        // 未配置白名单：不回显 Origin，禁止跨域凭据
        return;
    }
    // 没有 Origin 头视为同源，不回显
    auto it = res.headers.find("Origin");          // ← 未使用变量：永远查不到，函数到此结束
    // httplib Response 通常没有 Origin；尝试从 Request 读取由路由层注入
    // 此处保留默认不回显；具体 Origin 回显在 set_cors_headers(req, res) 重载中处理
}
```

```cpp
// routes_static.cpp:72-75（OPTIONS 预检用单参版本 → 无 ACAO，跨域预检必然失败）
    svr.Options(R"(.*)", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        res.status = 200;
    });
```

```cpp
// auth.h:327-331（中间件里的 401 也用单参版本）
        res.status = 401;
        json response = {{"code", 401}, {"msg", "未登录"}};
        set_cors_headers(res);
```

**问题**：
1. 单参重载里的 `auto it = res.headers.find("Origin");` 是**未使用变量**（编译器会告警），且整个函数在 `cors_allowed_origins` 非空时**什么也没做**就直接结束——注释自己承认“此处保留默认不回显”。这是典型的**半成品死代码**，容易让人误以为 401/403 也带了 ACAO。
2. 中间件的 401/403 响应（`auth.h:329,338,350,358`）不带 `Access-Control-Allow-Origin`，浏览器端（跨域部署时）**读不到响应体**，前端无法区分“未登录”与“网络错误”。
3. `routes_static.cpp:72` 的 OPTIONS 预检同样不回显 ACAO → 只要真跨域（例如前端 dev server 直连 8080、或前后端分域部署），**所有带自定义头（`X-CSRF-Token`）的请求预检都会失败**。当前 `config.json:23-25` 白名单为空，所以只能在同源部署下工作；若按 `config.h:28-29` 的意图配置白名单，预检仍会失败——**配置了也修不好**。

**建议改法**：
1. 删除单参重载（或改为 `delete`），强制所有调用点传入 `req`，让“需要 Origin 上下文”由类型系统保证；OPTIONS 与中间件统一改用 `set_cors_headers(req, res)`。
2. 预检响应补 `Access-Control-Max-Age`，并对 OPTIONS 直接返回 204。
3. 加一条集成测试：配置白名单后，从不同 Origin 发 OPTIONS + 带凭据的 GET，断言 `Access-Control-Allow-Origin`/`-Credentials` 存在。

**影响面**：所有跨域场景；本机同源部署下无明显表现（因此列为 low）。

**修复成本**：S。

---

### B26 —— 响应中硬编码操作人姓名【low】

**位置**：`routes_teacher.cpp:363`

**证据**：

```cpp
// routes_teacher.cpp:355-364
                    {"record", {
                        {"id", new_record_id},
                        {"studentId", student_id},
                        {"studentName", it->name},
                        {"className", it->className},
                        {"points", type == "add" ? points : -points},
                        {"reason", reason},
                        {"time", get_current_time()},
                        {"operatorName", "王老师"}          // ← 与真实操作者无关
                    }}
```

**问题**：`POST /api/teacher/points` 的响应把操作人写成常量 `"王老师"`（同文件 `routes_teacher.cpp:329-330` 已经取到了真实的 `current_user_id`）。任何教师操作积分记录，前端展示的操作人都是“王老师” → 操作审计信息错误（若前端直接用该字段渲染，还会与 `/api/teacher/points/records` 查库得到的真实 `operator_name` 不一致，如 `routes_teacher.cpp:407`）。

**建议改法**：用 `find_user_by_id(current_user_id)` 取真实姓名（或直接回 `operator_id` 让前端展示），并与 `points_records.operator_id` 落库值保持一致。

**修复成本**：S。

---

### B27 —— 检测到旧 schema 时直接删除数据库文件（无备份/无失败处理）【low】

**位置**：`main.cpp:60-75`、`models.cpp:62-63`

**证据**：

```cpp
// main.cpp:63-72
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
```

```cpp
// models.cpp:62-63
        User u;
        u.id = row.value("id", "");        // 旧 schema 下 id 为 INTEGER → type_error
```

**问题**：
1. **破坏性迁移**：检测到旧 schema 就直接 `std::remove()` 删库（用户、积分、兑换、留言全部丢失），没有先备份、没有二次确认、没有迁移脚本。对教学/竞赛项目尚可接受，但这是**静默数据销毁**，任何误判（例如 `users_table_is_old_schema()` 在特殊表结构下返回 true）都会造成不可恢复的损失。
2. **删除失败路径的后果**：`std::remove` 失败（文件被其他进程占用/权限不足）时只记一条 error 然后**继续**（`main.cpp:69` 重开同一文件）→ 旧 schema 仍在 → `load_users_from_db()` 的 `row.value("id","")`（`models.cpp:63`）遇到 INTEGER 的 id 会抛 `type_error`，而 `main()`（`main.cpp:277`）**没有 try/catch** → `std::terminate` → 进程启动即崩溃，且没有可读的失败原因（只有 stderr 的 terminate 信息）。
3. 该自动迁移还会在**每次启动**检查一次（`main.cpp:60-63`），且删除后不重建 WAL/journal 等伴生文件（`-wal`/`-shm` 可能残留，导致新库首次打开时读入旧 WAL，存在数据错乱风险）。

**建议改法**：
1. 改为“备份后迁移”：先 `std::filesystem::rename(db_path, db_path + ".bak")`（Windows 上 rename 可避免占用问题），再建新库并从备份**迁移数据**（`INSERT INTO users_new SELECT CAST(id AS TEXT), ... FROM users_old`）。
2. `load_users_from_db()` 内部对 `id` 做类型自适应（`row["id"].is_number_integer() ? std::to_string(...) : row.value("id","")`），并在 `main()` 顶层包一层 `try/catch`，把异常转成可读日志 + 非零退出码。
3. 迁移前要求显式开关（如 `--migrate-old-schema`），默认只告警不删库。

**影响面**：服务启动路径。

**修复成本**：S（加备份 + try/catch）～ M（实现真正迁移）。

---

### B28 —— 声明了外键但未开启 `PRAGMA foreign_keys`：约束全部不生效【low】

**位置**：`main.cpp:108-111,120-121,133-135,154-155,175,185-186,200`；`sqlite_wrapper.h:19-32`

**证据**：

```sql
-- main.cpp:104-111
            CREATE TABLE IF NOT EXISTS role_permissions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                role_id INTEGER NOT NULL,
                permission_id INTEGER NOT NULL,
                FOREIGN KEY (role_id) REFERENCES roles(id),
                FOREIGN KEY (permission_id) REFERENCES permissions(id),
                UNIQUE(role_id, permission_id)
            );
```

```cpp
// sqlite_wrapper.h:19-32（打开连接后只设置 busy_timeout 与 journal_mode，无 foreign_keys）
        sqlite3_busy_timeout(db_, 5000);
        execute("PRAGMA journal_mode=WAL;");
```

**问题**：SQLite 默认 `foreign_keys=OFF`（**每个连接**都需显式开启），本仓库全文 grep 无 `foreign_keys`。因此 `main.cpp` 里声明的所有 `FOREIGN KEY`（users/points_records/evaluations/redemption_records/teacher_classes/parent_students/parent_messages）**完全不生效**：
- 可以插入不存在学生的积分记录（数据脏）；
- 删除用户/班级后 `teacher_classes`、`parent_students`、`parent_messages` 会留下孤儿行（`routes_admin.cpp:816-823` 只手工清理了 `teacher_classes`/`parent_students`，没有清理 `parent_messages`/`points_records`/`evaluations`/`redemption_records`）；
- `routes_admin.cpp:1871-1879` 删除班级时 `classes.id` 被删而 `teacher_classes.class_id` 悬空。
项目在业务层做了部分补偿，但补齐的只是个别表，整体依赖不成立。

**建议改法**：
1. 在 `SqliteDb::open()` 里加 `execute("PRAGMA foreign_keys=ON;")`，并确认 `sqlite3_open` 未启用 `SQLITE_DEFAULT_FOREIGN_KEYS` 之外的假设；
2. 开启前先清理历史脏数据（`DELETE FROM ... WHERE ... NOT IN (SELECT id FROM ...)`），否则开启后写入会开始报错；
3. 或在业务层为每张表补齐级联删除（更明确但更易漏）；建议 1+2。
4. 同时把 `ON DELETE CASCADE/SET NULL` 写进建表语句（`main.cpp:104-211`），让删除语义由 schema 表达。

**修复成本**：S（开 PRAGMA + 清脏数据）。

---

### B29 —— DB 层错误只写 `std::cerr`，不进入 Logger【low】

**位置**：`sqlite_wrapper.h:45-48,60,131-133,156-158`；对照 `main.cpp:53`（`Logger::init`）

**证据**：

```cpp
// sqlite_wrapper.h:41-51
    bool execute(const std::string& sql) {
        if (!db_) return false;
        char* errMsg = nullptr;
        int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL错误: " << errMsg << std::endl;
            sqlite3_free(errMsg);
            return false;
        }
```

```cpp
// sqlite_wrapper.h:58-61
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "查询失败: " << sqlite3_errmsg(db_) << std::endl;
```

**问题**：所有 SQL 失败都只输出到 `std::cerr`：
- 不进入 `Logger` 的文件（`server.log`）与轮转，运维事后无法检索（“为什么昨天的统计是 0”无法回答）；
- 没有时间戳、没有请求/trace 上下文、没有 SQL 语句本身；
- Windows GUI/服务方式启动（或 stdout 重定向）时 stderr 可能完全丢失；
- 与 B12 叠加：错误既没被上报也没被调用方感知，成为**完全静默的失败**。

**建议改法**：`SqliteDb` 依赖 `logger.h`（或注入一个 `std::function<void(const std::string&)>` 日志回调，避免头文件循环依赖），把 `std::cerr` 全部替换为 `Logger::error("SQL 失败: " + errmsg + " | sql=" + sql)`；同时返回错误信息给调用方（见 B12）；对高频失败加计数器/告警。

**影响面**：全部 DB 操作的可观测性。

**修复成本**：S。

---

### B30 —— 日志轮转在加锁前执行（TOCTOU/并发 rename），且每条日志都 stat+open【low】

**位置**：`logger.h:41-62`

**证据**：

```cpp
// logger.h:41-56
    static void rotate_if_needed() {
        std::ifstream ifs(log_file_, std::ios::ate | std::ios::binary);
        if (!ifs) return;
        size_t size = ifs.tellg();
        ifs.close();

        if (size < log_max_size_) return;

        for (int i = log_max_files_ - 1; i >= 1; i--) {
            std::string old_name = log_file_ + "." + std::to_string(i);
            std::string new_name = log_file_ + "." + std::to_string(i + 1);
            std::rename(old_name.c_str(), new_name.c_str());
        }
        std::rename(log_file_.c_str(), (log_file_ + ".1").c_str());
    }
```

```cpp
// logger.h:58-62
    static void log(Level level, const std::string& message) {
        rotate_if_needed();                    // ← 在锁之外

        std::lock_guard<std::mutex> lock(mutex_);
```

**问题**：
1. `rotate_if_needed()` 在获取 `mutex_` **之前**调用（`logger.h:59` vs `61`），因此轮转逻辑本身不受锁保护：两个线程可同时判定“需要轮转”并同时 rename（第二次 rename 目标已存在，Windows 上直接失败/覆盖语义不定），或在 AB 线程重命名时 C 线程正准备写入；`tellg()` 的失败（`ifs.tellg()` 返回 -1 赋给 `size_t` → 巨大值）会导致**每写一条日志都触发一次轮转**。
2. 性能：每条日志都要 `open+stat+close` 两次（rotate 的 `ifstream` + append 的 `ofstream`），而日志在整个请求路径上被高频调用（`main.cpp:289-292` 每请求 2 条、`routes_*` 中还有额外 `Logger::info`）。
3. `log_max_files_` 的循环把 `.4`→`.5` 也重命名，最终保留 `max_files+1` 个文件（与配置 `max_files: 5` 的语义略有出入，属细节）。

**建议改法**：
1. 把 `rotate_if_needed()` 移入锁内（`logger.h:59-61` 交换顺序），或改为独立的“轮转锁”。
2. `tellg()` 结果判负：`auto sz = ifs.tellg(); size_t size = (sz < 0) ? 0 : static_cast<size_t>(sz);`
3. 用常驻的 `std::ofstream`（`Logger::init` 打开、`log` 复用）+ 维护的写入字节计数来替代“每条日志 stat”；写入失败时再重开。
4. 轮转改为基于时间（按天）或按计数触发，并在轮转时先 `flush/close` 当前流。

**影响面**：日志可靠性/性能（`server.log` 已 5MB，说明确有使用）。

**修复成本**：S。

---

## 4. 做得好的地方（均有源码依据）

1. **会话 ID 是真正的 CSPRNG**：`auth.h:102-104` → `sha256.h:151-159`，32 字节 = 64 hex 字符（256 位熵）。取证链：`c++config.h:1693,1755`（`_GLIBCXX_USE_DEV_RANDOM`/`RANDOM_TR1` 未定义）→ `server.exe` 内含 `"random_device: rdseed failed."`/`"rand_s failed."` 与 `advapi32!SystemFunction036`（`RtlGenRandom`）。**这一条比“看起来像随机”更实质**：MinGW 上 `std::random_device` 常被误认为会退化成 LCG，此处已证伪。
2. **会话 SQL 的主体已参数化**：`auth.h:196-217`（`execute_bind`）、`auth.h:223-232`（`query_bind`）并配有明确注释；`models.cpp:77-97` 的全部用户写操作、`routes_*.cpp` 中大量查询都用 `?` 绑定（如 `routes_public.cpp:272-274,343-345`、`routes_parent.cpp:26-28,92-96,397-401`、`routes_teacher.cpp:28-34,388-397`、`routes_admin.cpp:594-596,1708-1710,1862-1864`）。
3. **`sqlite_wrapper.h:127-148` 的 `Bind` 变体设计**：`INT/DBL/STR/NUL` 四型 + `string` 用 `sqlite3_bind_text(..., SQLITE_TRANSIENT)`（免悬垂），`long long` 重载避免了大整数被截断。
4. **CSRF 采用双重提交 Cookie + 常量时间比较**：`auth.h:158-168`（`csrf_check`，第 164-167 行逐字节 XOR 累积比较）、`auth.h:171-177`（`issue_csrf_token`，CSPRNG 16 字节）、`auth.h:180-187`（`require_csrf` 对 GET/HEAD/OPTIONS 豁免），并在大量写接口接线（`routes_public.cpp:99,315`、`routes_teacher.cpp:111,176,243,281,471,787,819,847,1039,1104`、`routes_admin.cpp:153,198,239,308,331,477,567,677,796,846,967,1023,1064,1100,1136,1177,1427,1748,1800,1851`）。
5. **Cookie 安全属性**：`auth.h:85-97`，`HttpOnly` + `SameSite=Lax` + 可选 `Secure`（`config.h:31,84` 在 HTTPS 时自动开启）。
6. **CORS 采用白名单反射而非通配**：`auth.h:36-51`（命中白名单才回显 `Origin`，并加 `Vary: Origin`、`Allow-Credentials: true`），空白名单时保守不回显（`auth.h:39`）。
7. **静态资源路径穿越防护**：`routes_static.cpp:14-23`（拒绝含 `..`、拒绝 `//` 起手、拒绝盘符），`routes_static.cpp:26-44` 用精确扩展名匹配 Content-Type（避免 `.js` 命中 `.json`），只从固定的 `frontend/dist` 子目录读取（`routes_static.cpp:10,47-57`）。
8. **权限校验做了参数化与“家长不得通过普通中间件”的隔离**：`auth.h:343-353`（`is_parent` 时直接 403），`auth.h:369-402`（家长专用中间件要求 `is_parent`），家长侧对象级授权有统一原语 `routes_parent.cpp:25-30`（`check_same_parent`）并在 6 个接口全部调用（`routes_parent.cpp:195,245,290,341,389,438`）——这是**正确实现对象级授权的范例**，可作为 B9 的改造模板。
9. **敏感字段导出已脱敏**：`routes_admin.cpp:1344-1355`（`// 安全修复 V7：导出接口不得泄露口令哈希`，只导出 id/username/role_id/name/className/points），同样 `routes_admin.cpp:1467-1470` 拒绝客户端提供的哈希、只接受明文并服务端哈希。
10. **防提权修复确实到位（除过度修正外）**：`routes_public.cpp:121-123`（开放注册强制 role_id=3）、`routes_admin.cpp:1472-1475`（导入拒绝非学生角色）、`routes_admin.cpp:803-811`（不能删除管理员用户）、`routes_admin.cpp:1071-1072,1107-1108`（内置权限/角色不可删除）。
11. **异常细节不落响应体的意识**：`routes_teacher.cpp:970-973`、`routes_admin.cpp:1679-1686`（`// 安全修复 V15：不向客户端回显内部异常细节`，异常只进 `Logger`）——意识是对的，只是覆盖面不足（见 B10）。
12. **SHA256 实现本身正确**：`sha256.h:12-89`（含填充长度计算 `blocks = (len+9+63)/64` 与 64 位大端长度域），我用 .NET SHA256 复算 `admin123`/`teacher123`/`student123`/`parent123` 与其种子哈希**逐字节一致**，可反向验证实现无误。
13. **有些修复是有针对性的、带原因注释的**：例如 `routes_public.cpp:301-303`（显式处理 `image_url` 为 NULL 以免 `value()` 抛 `type_error`）、`routes_teacher.cpp:38-43`（NULL 安全取值）、`sqlite_wrapper.h:25-27`（为并发引入 `busy_timeout`）、`routes_admin.cpp:1706`（参数化改造）——说明作者对“类型/NULL/注入”已有清晰的问题意识。
14. **WAL 与索引**：`sqlite_wrapper.h:27-29`（`busy_timeout=5000` + `journal_mode=WAL`，方向正确）、`main.cpp:203,216-222`（为 `parent_messages.student_id`、`sessions.user_id/expires_at`、`users.username/role_id`、`points_records.student_id`、`evaluations.student_id` 建索引），配合 `sqlite_wrapper.h:195-206` 的转义兜底，基础设施并不“野”。
15. **`Logger` 有轮转与级别**（`logger.h:30-83`）、`logger.cpp:4-7` 静态成员定义规范；`models.cpp:1-5` 还记录了 `windows.h` 与 `std::byte` 二义性的包含顺序坑，属有价值的工程注释。

---

## 5. 未验证 / 存疑事项（明确区分）

**已确认（有源码级证据，可直接复现判读）**：B1、B2、B3、B4、B5、B6、B7、B8、B9、B10、B11（结论为“当前不可注入但属维护陷阱”）、B12、B13、B14、B15、B16、B17、B18、B19、B20、B21、B23、B24、B25、B26、B27、B28、B29、B30。

**标注为疑似（🔶）**：
- **B22 `localtime` 并发竞争**：取决于此二进制所链接 CRT 是否为线程局部缓冲，我未运行程序故无法确证；建议按“有则修”处理（改 `localtime_s`）。

**未验证（本轮范围外或需要运行/数据才能确认）**：
1. **运行时行为均未实测**：为遵守“只读审计、不运行 server.exe”的约束，没有发起任何 HTTP 请求；B1 的“任意口令可登录”是**静态推导**（`password` 形参零引用是机械事实，推导链完整），但未做端到端验证。若需要，最小验证方式：用 `INSERT` 造一个 `pbkdf2$100000$<salt>$<F(salt,iters)>` 用户，再用错误口令登录，观察是否返回 200。
2. **`config.json` 之外是否有环境覆盖**：未发现环境变量/命令行覆写配置的代码路径（`main.cpp:50` 只读 `config.json`），但若部署侧有外部脚本修改该文件（如把 `host` 改为 `0.0.0.0`），B4/B14 的远程可利用性会显著上升。
3. **实际数据库文件状态**：工作区顶层未发现 `campus_system.db`（可能已被 `.gitignore`/清理，或从未生成），因此**无法核对库内现有 `password_hash` 是否为 `pbkdf2$` 格式**，也无法确认 B1 的存量受影响账号数量；`server.log`（5MB）未逐行审计（只作为存在性证据）。
4. **前端如何处理 HTTP 状态码/字段命名**：B17 的后端侧结论已确认，但“前端因此出现哪一类具体故障”需要前端审计（frontend-auditor）交叉确认。
5. **`sqlite3.o` 实测编译选项**：依据 `README.md:249` 的编译命令与 `sqlite3.c:14046-14052` 的默认值推断为 `SQLITE_THREADSAFE=1`（串行模式）；未通过 `PRAGMA compile_options` 实测（需运行程序）。
6. **httplib 版本号**：`httplib.h:44` 附近未取到 `CPPHTTPLIB_VERSION`（grep 未命中），因此 B10/B14 的框架行为以**本仓库实际代码**为准（已逐行引用），而非某个上游版本。
7. **并发问题的实际复现概率**：B5/B7/B8 的竞态窗口大小未做压测量化；理论上 8 线程 + 数百毫秒级 DB 操作窗口足够大，但未实测统计。

---

## 6. 建议的改进优先级（严重度 × 修复成本）

### 批次 0（立刻，阻断级，当日）
| 顺序 | 条目 | 理由 | 成本 |
|---|---|---|---|
| 1 | **B1** | 任意口令登录任何 pbkdf2 账号；一次正常登录即永久开后门 | M |
| 2 | **B2** | 默认口令即管理员入口；与 B1 叠加成完整攻击链 | S |
| 3 | **B3 + B4** | 登录锁定同时“可绕过”与“可武器化”；并发 UB 可能直接崩溃服务 | S~M |

> 批次 0 的三项建议一并做：修正 PBKDF2 → 强制口令重置 → 锁定逻辑加锁 + 去 XFF 信任 + 家长登录接入。

### 批次 1（高优先，1 周内，数据正确性与越权）
| 顺序 | 条目 | 理由 | 成本 |
|---|---|---|---|
| 4 | **B8** | 会造成真实资损（双花/超卖），修法确定（原子 SQL + 行数语义） | S~M |
| 5 | **B5 + B6** | 影响所有请求的稳定性与身份正确性；B6 会直接串号 | M |
| 6 | **B9** | 教师可改全校学生数据；有现成模板（`check_same_parent`） | M |
| 7 | **B7** | 全局事务会静默吞掉已返回成功的写入 | M |
| 8 | **B10** | 无认证即可触发 500 + 异常文本泄露 | S~M |

### 批次 2（中优先，2~4 周，健壮性与契约）
| 顺序 | 条目 | 理由 | 成本 |
|---|---|---|---|
| 9 | **B12 + B29** | 错误可见性是后续所有排查的前提 | M |
| 10 | **B17 + B19** | 统一响应与权限映射，降低后续改动风险 | M |
| 11 | **B13 + B14 + B11** | 低成本高收益（备份判据、请求上限、会话 SQL） | S |
| 12 | **B15 + B16 + B18** | 数据源一致性（导入/ RBAC / 双写） | M |
| 13 | **B21 + B20 + B24 + B25 + B26** | 一批 S 级小修，建议合入同一 PR“低垂果实” | S |

### 批次 3（低优先，与重构合并）
- **B23**（thread_local 连接 + 会话归并 + N+1）建议与 B5/B7 的重构**同批完成**，因为都要动 `sqlite_wrapper.h` 与 `auth.h` 的访问模式；单独做收益有限、返工成本高。
- **B22**（`localtime`）、**B27**（破坏性迁移）、**B28**（外键）、**B30**（日志轮转）为独立小修，可随时插入。

### 建议的落地顺序原则
1. **先修“不可见”→“可见”**：本报告中大量问题之所以危险，是因为失败被静默化（B12/B29/B10 的空白 catch、`std::cerr`、`empty()` 语义混用）。先把错误变成响亮，再改逻辑。
2. **再修“正确性”→“性能”**：B23 的并发模型改造与 B5/B7 同源，避免两次重构。
3. **最后做契约统一**：B17/B19 的机械替换量大但风险低，适合在有回归测试后成批执行。
4. **每批都加“不变量测试”**：积分不得为负、库存不得超卖、删除用户后索引仍自洽、任意错误口令不得登录 —— 这四条断言可覆盖本报告约 60% 的 high/blocker 条目。

---

*报告完。所有引用行号均已用 `read` 工具重新打开对应文件核对原文；机械统计类证据的执行命令见 §1.3。*
