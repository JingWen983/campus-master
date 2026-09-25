# B1 独立验证报告（批次 1 并发 / 安全修复）

- **产物**：`docs/audit/evidence/B1_VERIFICATION.md`（本文件是本任务**唯一**写入仓库的文件）
- **验证者**：独立验证者（DP-B1 角色；不引用实现者结论，全部重新取证）
- **验证时间**：2026-09-25（本机时间）
- **被验证基线**：`git HEAD = 3417f49`，工作区**未提交**改动 12 个文件（`git diff --stat` 见 §1.3）
- **工具链**：MinGW-W64 `g++/gcc 14.2.0 (x86_64-w64-mingw32, msvcrt, posix-seh, r1)`；`pwsh 7.6.5`；`curl 8.21.0`
- **只读约束遵守情况**：未 `commit` / 未 `stash` / 未 `checkout`；未修改任何源码（`git status` 结尾复核见 §8）
- **临时产物目录**：`%TEMP%\b1verify\`（= `C:\Users\ADMINI~1\AppData\Local\Temp\b1verify\`）。按任务要求**测后已删除**；删除前的 sha256 见 §7，原始输出摘录已内联到本文件

---

## 0. 结论速览

| 项 | 判据族 | 独立结论 |
|---|---|---|
| 1-1 连接模型（每线程一条连接） | C1-1…C1-6 | **PASS**（运行期连接数 1→9→1→0，每连接 busy_timeout/WAL 均生效） |
| 1-2 B3+B5+N2 | C2-1…C2-7、伴-4 | **FAIL**（N2/伴-4 端到端静默丢失；多处仍无锁直接访问全局容器） |
| 1-3 B6 彻底版（稳定键） | C3-1…C3-7 | **FAIL**（索引**仍存下标**；仅补了身份校验这一「兜底」，H-① 未修） |
| H-② 受影响行数通道 | C4-1/C4-2/CH-6/CH-7 | **部分 PASS / 部分 FAIL**（通道本体正确；评价端点 2 个受害者未接入；`bool execute_bind` 返回表达式被改） |
| 1-4 B8 兑换原子性 | C4-4/C4-5/C4-6 | **PASS**（双花/超卖/无限库存/失败回滚 全部按判据成立） |
| 1-5 B9 对象级归属校验 | C5-1…C5-6 **FAIL**；C5-7 未独立验证；C5-8 PASS | **FAIL（完全未落地）**：原语已定义但**调用点为 0**；实测跨班写全部 200 |
| 1-6 B4 XFF + 家长端锁定 | C6-1…C6-9 | **PASS** |
| 1-7 B10+B14 | C7-1…C7-8、C7-13 | **部分 PASS / 部分 FAIL**（错误通道与上限生效；`set_read_timeout`/`set_keep_alive_max_count` 缺失） |
| 1-7 B12 错误通道 | C7-9…C7-12 | **FAIL（完全未实现）**：无 `last_error()`，编译期即可证伪 |
| H-① 索引重建 | CH-1…CH-5 | **FAIL（完全未落地）**：删权限后仍被授权（可复现） |
| 伴-1 RAII 事务（B7） | CB1-1…CB1-5 | **PASS**（CB1-5 未注入，见 §5） |
| 伴-2 日志轮转 | CB2-1…CB2-3 | **PASS**（行数守恒 4001/4001；反向对照丢 29 行） |
| 伴-2' 积分记录 id | CB2'-1…CB2'-3 | **PASS**（重启后 id=7,8,9 与库内一致） |
| 伴-3 删库重建 fail-fast | CB3-1…CB3-4 | **PASS**（删不掉时 exit code = 1） |
| 伴-4 `INSERT OR REPLACE` | C2-4/C2-5 | **FAIL（未修）** |

**一句话判断**：**批次 1 不可结项**。1-5（B9）与 H-① 属于「契约要求、但代码里根本没有调用点」，1-2 的 N2/伴-4 与 1-7 的 B12 完全未闭合，并且本批声称修好的 N2 在两条**活路径**上可被端到端复现为「静默丢失用户」。此外本批新引入的 `thread_local` 连接析构机制存在可复现的线程退出崩溃（§4）。

---

## 1. 编译与基线

### 1.1 全量编译（自建，产物全在 %TEMP%）

命令（与 `.github/workflows/ci.yml` 同源，`-o` 全部指向 `%TEMP%`，避免非 ASCII 绝对路径导致 `ld` 失败）：

```
cd D:\邵敬文\comptation
gcc -c sqlite3.c -o %TEMP%\b1verify\sqlite3.o -O2
g++ -c <9 个 .cpp> -o %TEMP%\b1verify\<name>.o -std=c++17 -O2 -I.
g++ -o %TEMP%\b1verify\b1_server.exe <10 个 .o> -lws2_32 -lwsock32 -std=c++17 -O2 ^
    -static -static-libgcc -static-libstdc++ -lwinpthread
```

原始输出（脚本 `%TEMP%\b1verify\build.ps1`）：

```
=== gcc sqlite3.c ===
sqlite3 exit=0
=== g++ main.cpp ===        main exit=0
=== g++ models.cpp ===      models exit=0
=== g++ logger.cpp ===      logger exit=0
=== g++ routes_static.cpp ===   routes_static exit=0
=== g++ routes_public.cpp ===   routes_public exit=0
=== g++ routes_admin.cpp ===    routes_admin exit=0
=== g++ routes_teacher.cpp ===  routes_teacher exit=0
=== g++ routes_student.cpp ===  routes_student exit=0
=== g++ routes_parent.cpp ===   routes_parent exit=0
=== link ===
link exit=0

Name              Length   LastWriteTime
sqlite3.o        1162258   2026/9/25 17:49:34
main.o           1035671   2026/9/25 17:49:48
models.o          219924   2026/9/25 17:49:54
logger.o            1991   2026/9/25 17:49:56
routes_static.o   593641   2026/9/25 17:50:06
routes_public.o  1137529   2026/9/25 17:50:22
routes_admin.o   1468765   2026/9/25 17:50:41
routes_teacher.o 1278038   2026/9/25 17:50:59
routes_student.o  874986   2026/9/25 17:51:11
routes_parent.o  1107504   2026/9/25 17:51:26
b1_server.exe    6114144   2026/9/25 17:51:26
```

**结论：C1-5 / C2-7 / C3-7 / C4-7 / C5-8 / C6-9 / C7-13 的「主进程编译 exit 0」部分 PASS。**

仓库根**未新增**任何 `.o` / `.exe`（复核见 §8；仓库根现存 `.o`/`.exe` 均为**先于本次验证存在**的产物，时间戳 2026/9/19，非本次生成）。

### 1.2 `tests/run_tests.ps1`

```
>>> C:\mingw64\bin\g++.exe tests/pbkdf2_rfc7914_test.cpp -o tests/pbkdf2_test_8548.exe -std=c++17 -O2 -I.    (cwd=D:\邵敬文\comptation)
<<< g++ exit code = 0

>>> D:\邵敬文\comptation\tests\pbkdf2_test_8548.exe
=== tests/pbkdf2_rfc7914_test.cpp ===
目标：以 RFC 7914 §11 标准向量机械判定 B1（PBKDF2 认证绕过）
--- [ OK ] sha256_fips180_4_primitive  (2 checks)
--- [ OK ] rfc7914_v1_t1_passwd_salt_c1  (2 checks)
--- [ OK ] rfc7914_v2_t1_Password_NaCl_c80000  (2 checks)
--- [ OK ] rfc7914_dklen64_block_counter  (2 checks)
--- [ OK ] rfc7914_dklen64_from_production  (4 checks)
--- [ OK ] b1_distinct_password_distinct_dk  (3 checks)
--- [ OK ] b1_password_len_64_vs_65  (2 checks)
--- [ OK ] b1_verify_password_wrong_password_false  (3 checks)
--- [ OK ] verify_password_malformed_fail_closed  (8 checks)
--- [ OK ] hash_password_storage_format  (6 checks)
--- [ OK ] verify_password_legacy_sha256  (2 checks)
============================================================
cases : 11 run, 11 ok, 0 failed
checks: 36 run, 36 ok, 0 failed
RESULT: PASS
<<< test exit code = 0
=== RUN_TESTS EXIT: 0 ===
```

`tests/pbkdf2_rfc7914_test.cpp` 的 SHA-256：

```
95026632E043C2D52B6F4E5D05F505A498B73051F1FD37CBDDAD9ACF79FE4ECC   ← 与任务书给定值逐字符一致
```

`tests/` 目录运行后无残留 exe（脚本按 T34 设计用后即删）。

### 1.3 可见改动面

```
$ git -c core.quotepath=false status --porcelain
 M auth.h
 M config.h
 M logger.h
 M main.cpp
 M models.cpp
 M models.h
 M routes_admin.cpp
 M routes_parent.cpp
 M routes_public.cpp
 M routes_student.cpp
 M routes_teacher.cpp
 M sqlite_wrapper.h
?? .agent-teams/
?? docs/audit/BATCH1_CONTRACT.md
?? docs/audit/BATCH1_DESIGN_DECISIONS.md
?? docs/audit/evidence/B1_T2_CONTRACT_VERIFICATION.md

$ git diff --stat
 auth.h             | 149 ++++++++++++++++------
 config.h           |  16 +++
 logger.h           |  21 ++-
 main.cpp           |  60 ++++++++-
 models.cpp         | 280 ++++++++++++++++++++++++++++++++++++----
 models.h           |  61 ++++++++-
 routes_admin.cpp   | 368 ++++++++++++++++++++++++++++++++---------------------
 routes_parent.cpp  |  38 ++++--
 routes_public.cpp  | 209 +++++++++++++++++++++-----------
 routes_student.cpp |  23 ++--
 routes_teacher.cpp | 128 +++++++++++++--------
 sqlite_wrapper.h   | 302 +++++++++++++++++++++++++++++++++++++------
 12 files changed, 1252 insertions(+), 403 deletions(-)
```

与任务描述一致：12 个 ` M`，未跟踪项仅上述 4 个（均先于本次验证存在）。

---

## 2. 判据总表（逐条 pass / fail）

> 「符号检查」= 只查 `grep`/签名；「运行期」= 我实际跑出来的行为。按契约 §4.2，符号检查**不单独**作为通过依据。

| 编号 | 判据摘要 | 我的结论 | 证据 |
|---|---|---|---|
| C1-1 | `sqlite3*` 只出现在 sqlite_wrapper.h | PASS | §3.1 |
| C1-2 | 线程局部惰性建连 + 同函数内 busy_timeout/WAL | PASS | §3.1 |
| C1-3 | 运行期连接数 = 线程数 + 1 | PASS（1→9→1→0） | §3.1 |
| C1-4 | 并发导入不吞别的写入 | PASS（并入 CB1-3） | §3.10 |
| C1-5 | 主进程编译 exit 0 + 测试全绿 | PASS | §1.1/§1.2 |
| C1-6 | `thread_count` 保持 8 | PASS | `config.json` 未改 |
| C2-1 | 旧 `User* find_user_by_*` 签名零命中 | PASS（仅注释命中） | §3.2 |
| C2-2 | `&users[` 零命中 | **FAIL（字面）**：models.cpp:250/266 命中；结论见 §3.2 | §3.2 |
| C2-3 | 唯一一把锁、无 shared_mutex | PASS（recursive_mutex 18 处引用、shared_* 0） | §3.2 |
| C2-4 | 并发建号无重复 ID，用户数 = 种子+N | **FAIL**（8 并发注册 → 库内仅 1 个用户） | §3.2 |
| C2-5 | 修复前可复现、修复后同口径正常 | **FAIL**（现状即可复现静默丢失） | §3.2 |
| C2-6 | 并发登录不崩且锁定生效 | PASS（50 并发 → 10×401 / 40×429，进程存活） | §3.6 |
| C2-7 | 编译 + 测试 | PASS | §1 |
| C3-1 | 索引 value 不再是下标 | **FAIL**（仍为 `unordered_map<string, size_t>`） | §3.3 |
| C3-2 | `&users[` 零命中 | **FAIL（字面）** | §3.3 |
| C3-3 | 删中间用户后按 id 取回正确用户 | PASS（+ 反向对照见 §3.3） | §3.3 |
| C3-4 | `init_indexes()` 幂等 | PASS（两次 dump IDENTICAL） | §3.3 |
| C3-5 | 10 个变更点逐点列清单 + 索引同步 | **FAIL**（2 处建号 push_back 在锁外、角色/权限 4 处无重建） | §3.2/§3.9 |
| C3-6 | 删权限后无陈旧授权 | **FAIL** | §3.9 |
| C3-7 | 编译 + 测试 | PASS | §1 |
| C4-1 | `execute_bind_affected` 存在、bool 版签名不变 | PASS（签名不变） | §3.4 |
| CH-7 | `bool execute_bind` 签名**与返回表达式**零改动 | **FAIL**（返回表达式已改为委托调用） | §3.4 |
| C4-2 | affected_rows 语义 1 / 0 / 失败 | PASS（1 / 0 / 0、prepare 失败 rc=1） | §3.4 |
| C4-3 | 不存在评价 → 404 | **FAIL**（PUT/DELETE 均 200「成功」） | §3.4 |
| CH-6 | H-② 4 个业务受害者全部改用行数判据 | **FAIL**（评价侧 2 个未改） | §3.4 |
| （H-② 端到端） | 改密/重置口令 0 行可判别 | PASS（改为 code 500；HTTP 仍 200） | §3.4 |
| C4-4 | 双花不可复现 | PASS（3 并发 → 成功 1、points 0、records 1） | §3.5 |
| C4-5 | 超卖不可复现（含 stock=-1） | PASS（5 并发 → 成功 2、stock 0、points 98；-1 → 成功 3） | §3.5 |
| C4-6 | 失败不留半成品 | PASS（item4 净扣库存 == 成功兑换数 == 2） | §3.5 |
| C5-1 | 归属原语 + 包装 + 调用点 ≥ 8 | **FAIL**（调用点 = 0） | §3.7 |
| C5-2 | `bound_class_names.empty()` 零命中 | **FAIL**（routes_teacher.cpp:81 命中） | §3.7 |
| C5-3 | 不再用 className 字符串比较 | **FAIL**（routes_teacher.cpp:84 命中） | §3.7 |
| C5-4 | 跨班 8 个写端点全部被拒 | **FAIL**（实测 PUT/DELETE/建号/回复留言 全部 200） | §3.7 |
| C5-5 | 未绑定教师看不到全校 | **FAIL**（返回全校 4 名学生） | §3.7 |
| C5-6 | 建号/改班级双向校验 | **FAIL**（可把学生建到别人的班） | §3.7 |
| C5-7 | 家长端 `check_same_parent` / `GET /api/parent/*` 不受影响（回归） | **未独立验证**（家长登录正常且锁定正常＝PASS；`routes_parent.cpp` 的 `check_same_parent` 与家长读接口本次**未逐一重跑**） | §5 |
| C5-8 | 编译 + 测试 | PASS | §1 |
| C6-1 | `trust_proxy_headers` 默认 false | PASS | §3.6 |
| C6-2 | XFF 读取受门控、无 X-Real-IP | PASS | §3.6 |
| C6-3 | 伪造 XFF 不得换键（第 6 次 429） | PASS（4/5/6 次均 429） | §3.6 |
| C6-4 | 账号维度锁定 | PASS | §3.6 |
| C6-5 | 家长端锁定 | PASS（第 4 次起 429） | §3.6 |
| C6-6 | ≥50 并发登录不崩、锁定不被绕过 | PASS | §3.6 |
| C6-7 | 锁定表 ≤ 上限 | PASS（11000 键 → size 恰为 10000） | §3.6 |
| C6-8 | 锁定到期恢复 | PASS（白盒模拟到期；见 §5 限制） | §3.6 |
| C6-9 | 编译 + 测试 | PASS | §1 |
| C7-1 | 类型错误请求 → JSON 体、无 EXCEPTION_WHAT | PASS（HTTP 500 + `{"code":500,...}`） | §3.8 |
| C7-2 | EXCEPTION_WHAT 显式删除 + error handler 注册 | **信号检查 FAIL**（`EXCEPTION_WHAT` 仅出现在注释；无 `headers.erase`）；行为上无泄露 | §3.8 |
| C7-3 | 4xx/5xx 结构化 JSON 生效 | PASS（404/413/500 均为 JSON） | §3.8 |
| C7-4 | 未把 500 伪装成 200 | PASS（实测 500） | §3.8 |
| C7-5 | 三个 setter 各 ≥1 处 | **FAIL**（只有 `set_payload_max_length`） | §3.8 |
| C7-6 | >1MiB → 413 + JSON 体 | PASS | §3.8 |
| C7-7 | 正常请求仍 200 | PASS | §3.6/§3.8 |
| C7-8 | 慢连接不阻塞其他请求 | **部分 PASS**：1 个慢连接不阻塞（49 ms）；8 个慢连接会把正常请求拖 9684 ms | §3.8 |
| C7-9 | `last_error()` 成员存在 | **FAIL**（编译期证明不存在） | §3.8 |
| C7-10 | 区分「SQL 失败」与「无数据」 | **FAIL**（两者都是空 `json::array()`） | §3.8 |
| C7-11 | DB 层错误不再走 `std::cerr` | **FAIL**（10 处 `std::cerr` 仍在） | §3.8 |
| C7-12 | 鉴权路径 DB 错误 → 500 而非 401 | **FAIL**（通道不存在，无从接入） | §3.8 |
| C7-13 | 编译 + 测试 | PASS | §1 |
| CH-1 | 4 个角色/权限变更点后有重建 | **FAIL**（0 处有重建） | §3.9 |
| CH-2 | 删权限后 map 无陈旧项 | **FAIL**（探针：陈旧项仍在，且仍授权） | §3.9 |
| CH-3 | 删角色后 map 与权限列表无残余 | **FAIL** | §3.9 |
| CH-4 | 编译 + 测试 | PASS | §1 |
| CB1-1 | 无裸 BEGIN/COMMIT/ROLLBACK | PASS（仅注释命中） | §3.10 |
| CB1-2 | RAII 守卫存在且被使用 | PASS（routes_admin.cpp:1563、routes_public.cpp:386） | §3.10 |
| CB1-3 | 并发导入不吞别的写入 | PASS（并发写入 survived=1） | §3.10 |
| CB1-4 | 导入失败不留半成品 | PASS（half_products=0） | §3.10 |
| CB1-5 | COMMIT 失败路径可达 | **未独立验证**（未注入） | §5 |
| CB2-1 | 轮转在锁内调用 | PASS（logger.h:65 锁 → :67 rotate） | §3.11 |
| CB2-2 | 并发轮转不丢行 | PASS（4001/4001；对照 3972/4001） | §3.11 |
| CB2-3 | 单线程轮转行为一致 | PASS（13 个分文件） | §3.11 |
| CB2'-1 | 无 `points_records.size()+1` | PASS（仅注释命中） | §3.12 |
| CB2'-2 | record.id 与 DB 一致且重启不重号 | PASS（重启后 7,8,9 == DB id） | §3.12 |
| CB2'-3 | INSERT 返回值被检查 | **部分 PASS**（`rc==0 && affected==1` 已检查；失败分支未注入） | §5 |
| CB3-1 | 两个失败分支 return 1 | PASS | §3.13 |
| CB3-2 | 删除失败 → 非 0 退出 | PASS（exit code = 1） | §3.13 |
| CB3-3 | 正常库启动不受影响 | PASS（重建后 schema=TEXT 并正常监听） | §3.13 |
| CB3-4 | 无「未开库仍 listen」路径 | PASS（return 1 位于 :204/:210/:225/:449，均在 listen 之前） | §3.13 |

---

## 3. 逐条取证

### 3.1 【1-1】连接模型（C1-x）

**符号证据**（自研源码，排除 vendored）：`sqlite3*` 只出现在 `sqlite_wrapper.h`（构造函数内 8 处局部 `conn()`、`ThreadConn::handle`、`open_current_thread`）。`thread_local` / `sqlite3_open` / `busy_timeout` / `journal_mode` 集中在同一处：

```
sqlite_wrapper.h:386: static thread_local ThreadConn c;
sqlite_wrapper.h:401: int rc = sqlite3_open(db_path_.c_str(), &h);
sqlite_wrapper.h:413: sqlite3_busy_timeout(h, 5000);           // 写锁冲突时最多阻塞 5s 重试
sqlite_wrapper.h:415: sqlite3_exec(h, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &err);
```

**运行期证据（探针 `b1probe_misc conn`，产物 `%TEMP%\b1verify\b1probe_misc.exe`）**：

```
[probe] mode=conn
[probe] after main-thread open: live_connections=1 (expect 1)
[probe] with 8 worker threads alive: live_connections=9 (expect 9)
[probe]   thread0: rows=1 busy_timeout=5000 journal_mode=wal
[probe]   thread1: rows=1 busy_timeout=5000 journal_mode=wal
[probe]   thread2: rows=1 busy_timeout=5000 journal_mode=wal
[probe]   thread3: rows=1 busy_timeout=5000 journal_mode=wal
[probe]   thread4: rows=1 busy_timeout=5000 journal_mode=wal
[probe]   thread5: rows=1 busy_timeout=5000 journal_mode=wal
[probe]   thread6: rows=1 busy_timeout=5000 journal_mode=wal
[probe]   thread7: rows=1 busy_timeout=5000 journal_mode=wal
---- exit: -1073740940 ----     ← ★ 见 §4.4：进程在线程退出阶段崩溃
```

- 连接数 1 → 9 → （join 后应回到 1）→ 0：**「每线程一条 + 惰性建连」成立**。
- 每条新连接都读到 `busy_timeout=5000` 与 `journal_mode=wal`：**①-C6 要求成立**。
- ★ 但 `after all workers joined` 那一行**没有打印出来**，进程以 `0xC0000374 (STATUS_HEAP_CORRUPTION)` 结束；`conn` 连跑 3 次全部复现。这是**新缺陷**，定位见 §4.4。

**C1-1 / C1-2 / C1-3 → PASS**（C1-3 的「9」这一步已实测；线程退出阶段的崩溃单列为 §4.4，不算作 C1-3 失败，但属于连接模型的实现缺陷）。

---

### 3.2 【1-2】B3 + B5 + N2（C2-x、伴-4）

**符号证据**

```
models.h:133:  // 旧的 `User* find_user_by_id` / `find_user_by_username` 已**删除**：
models.cpp:450: // 使用索引查找用户（批次 1 / B5：旧的 `User* find_user_by_id` 已**删除**，
→ 旧签名零「代码」命中（仅注释），C2-1 PASS

models.cpp:250: return &users[it->second];
models.cpp:266: return &users[it->second];
→ C2-2 字面 FAIL（但见下方「实质」说明）

std::recursive_mutex 计数 = 18（models.cpp）；std::shared_mutex|shared_lock 计数 = 0
→ C2-3 PASS
```

C2-2 字面 FAIL 的**实质**：这两处是 `find_user_record_locked()` / `find_user_record_by_username_locked()` 的内部返回值（`const User*`），调用方在同一临界区内**立即拷贝**（`find_user_by_id_copy`）或**立即用引用**（`with_user_record`），指针不外泄；契约要消除的是「跨锁存活的裸指针」，这一点做到了。**但** `with_user_record` 把 `User&` 交给回调且回调内可执行 DB I/O 甚至 `users.erase`，若回调触发 `users` 扩容/删除后再触碰该引用即悬垂——目前 6 处回调逐一看过均未在 erase 后继续使用引用（routes_admin.cpp:892-911 用 `deleted_id` 而非 `u`）。故判：**字面 FAIL、实质可接受**。

**端到端 FAIL：N2 静默丢失（C2-4 / C2-5 / 伴-4）**

隔离实例（`%TEMP%\b1verify\srv`，端口 18099，测试数据库 `b1test.db`）上并发打**公开注册端点**（`routes_public.cpp:157-186` 走的是 `find_user_by_username_copy` → `generate_user_id` → **锁外 `users.push_back`** → `save_user_to_db`＝`INSERT OR REPLACE`）：

```
### 8 并发 POST /api/auth/register（同一 grade_code/class_code）
reg 1 : {"code":200,"data":["user_id","student-09-N-01"],"msg":"注册成功"}
reg 2 : {"code":200,"data":["user_id","student-09-N-01"],"msg":"注册成功"}
reg 3 : {"code":200,"data":["user_id","student-09-N-01"],"msg":"注册成功"}
reg 4 : {"code":200,"data":["user_id","student-09-N-01"],"msg":"注册成功"}
reg 5 : {"code":200,"data":["user_id","student-09-N-01"],"msg":"注册成功"}
reg 6 : {"code":200,"data":["user_id","student-09-N-01"],"msg":"注册成功"}
reg 7 : {"code":200,"data":["user_id","student-09-N-01"],"msg":"注册成功"}
reg 8 : {"code":200,"data":["user_id","student-09-N-01"],"msg":"注册成功"}
server alive after concurrent registration? True
duplicate ids in DB:            ← SELECT id,COUNT(*) FROM users GROUP BY id HAVING COUNT(*)>1 → 0 行
registered usernames present:
  ROW{registered=1}             ← ★ 8 个请求全部 200，库内只剩 1 个用户，7 个被静默覆盖
  ROW{id=student-09-N-01, username=n2probe_1790330597_6}
```

同一实例上并发打**教师建学生端点**（`routes_teacher.cpp:130-146`：`generate_user_id` → `hash_password` → **锁外 `users.push_back`**）：

```
tstu 1 : {"code":200,"data":{"id":"student-02-01-07",...},"msg":"学生添加成功"}
tstu 2 : {"code":200,"data":{"id":"student-02-01-07",...},"msg":"学生添加成功"}
tstu 3 : {"code":200,"data":{"id":"student-02-01-07",...},"msg":"学生添加成功"}
tstu 4 : {"code":200,"data":{"id":"student-02-01-07",...},"msg":"学生添加成功"}
tstu 5 : {"code":200,"data":{"id":"student-02-01-07",...},"msg":"学生添加成功"}
tstu 6 : {"code":200,"data":{"id":"student-02-01-07",...},"msg":"学生添加成功"}
created students:
  ROW{id=student-02-01-07, username=n2t_1790330597_6}   ← 6 个请求全部 200，库内只剩 1 行
```

**根因量化（探针 `b1probe_conc`）**——`generate_user_id()` 与插入之间隔着的真实窗口是一次 PBKDF2：

```
[probe] hash_password avg = 112.95 ms  (this is the real width of the id-allocation window)

# ids-gap：generate_user_id（持锁）→ PBKDF2 窗口 → add_user_record（也持锁），两次持锁不构成一个原子步骤
[probe] allocate_calls=40 distinct_ids=5 duplicate_allocations=35
[probe] users_in_container=43 duplicate_user_ids=35
[probe] N2 atomicity -> VIOLATED (generate+insert are not one atomic step)

# ids-logical：8 线程 × 10 次「分配 id → PBKDF2 → 记录」
[probe] allocate_calls=80 distinct_ids=1 duplicate_allocations=79
[probe] N2 id-allocation race -> YES (same id handed to >1 request)
```

**结论**：
- **C2-4 FAIL**，并且**契约 C2-4 给出的 SQL 判据本身不足**：`GROUP BY id HAVING COUNT(*)>1` 返回 **0 行**（因为 `INSERT OR REPLACE` 把重复 id 合并成了一行），会**假通过**；正确判据必须是「用户数 == 种子数 + N」（实测 1 ≠ 8）。
- **C2-5 FAIL**：不是「修复前可复现」，而是**现状即可复现**。
- **伴-4 FAIL**：`models.cpp:94` 仍是 `INSERT OR REPLACE INTO users ...`（契约要求改 `INSERT` 并把唯一冲突当真实失败上报）。
- 正确的原子接口 `add_user_record()` **存在且被 admin 路径使用**（routes_admin.cpp:563、routes_admin.cpp:1621、routes_teacher.cpp:966 导入路径），但**注册**与**教师建学生**这两条活路径没接。

**新增 FAIL：多处仍无锁直接访问全局容器**（违反契约 R1「所有访问必须经受锁入口」）：

```
routes_admin.cpp:454:  {"totalUsers", users.size()},
routes_admin.cpp:904:  for (auto it = users.begin(); it != users.end(); ++it) {      ← 无锁（但被 with_user_record 包着，实际持锁）
routes_admin.cpp:905:      if (it->id == deleted_id) { users.erase(it); break; }
routes_admin.cpp:1589: for (size_t k = 0; k < users.size(); k++) {                  ← 无锁
routes_admin.cpp:1728: auto existing = find_if(points_records.begin(), ...)         ← 无锁
routes_admin.cpp:1742: points_records.push_back(new_record);                        ← 无锁
routes_teacher.cpp:143: users.push_back(new_user);                                  ← ★ 无锁
routes_teacher.cpp:193: auto student_it = find_if(users.begin(), users.end(), ...)  ← ★ 无锁
routes_teacher.cpp:251: auto it = find_if(users.begin(), users.end(), ...)          ← ★ 无锁
routes_teacher.cpp:258: users.erase(it);                                            ← ★ 无锁
routes_teacher.cpp:510: auto it = find_if(users.begin(), users.end(), ...)          ← ★ 无锁
routes_public.cpp:183:  users.push_back(new_user);                                  ← ★ 无锁
```

这些无锁访问与「持锁遍历」并发即 UB，**已实测到崩溃**（探针 `b1probe_conc ids-race`，8 线程 × 100 次，3 次运行）：

```
run1 exit=3        :: ... heartbeat pushed=200 | terminate called after throwing an instance of 'std::out_of_range'
                      what():  basic_string::substr: __pos (which is 14) > this->size() (which is 0)
run2 exit=3        :: （同上）
run3 exit=-1073741819 :: ACCESS_VIOLATION
```

（该异常来自 `generate_user_id()` 在持锁遍历 `users` 时读到被并发 `push_back` 破坏的 `std::string`，属内存竞争的直接后果。）

**结论：1-2 判 FAIL。**

---

### 3.3 【1-3】B6 彻底版：索引稳定键（C3-x）

**C3-1 FAIL**——索引**仍然存下标**：

```
models.cpp:214: unordered_map<string, size_t> user_id_map;           // 用户ID到 users 下标的映射
models.cpp:215: unordered_map<string, size_t> user_username_map;     // 用户名到 users 下标的映射
```

（契约 §3.3 第 1 条明文「**禁止**继续存下标」。`models.h` **没有声明**这两个 map，所以「在 models.h 里 grep `size_t`」这条字面检查会**空过**——这是判据本身的缺口。）

实际落地的是契约 §3.3 第 3 条的**兜底**分支（保留下标 + 身份校验）：

```
models.cpp:245: if (users[it->second].id != user_id) {
models.cpp:246:     Logger::warning("用户索引身份校验失败（陈旧下标）： 查找=" + user_id + ...
```

**C3-2 FAIL（字面，同 §3.2）**。

**C3-3 PASS**（探针 `b1probe_misc b6` / `b6-control`）：

```
########## b6（当前实现：erase → rebuild_user_indexes → 身份校验） ##########
[probe] mode=b6 users=3
[probe]   find_user_by_id_copy("A") -> found=1 returned_id=A  OK
[probe]   find_user_by_id_copy("C") -> found=1 returned_id=C  OK
[probe]   find_user_by_id_copy("D") -> found=1 returned_id=D  OK
[probe] init_indexes idempotent: IDENTICAL (user_id_map=3 perm12=12)

########## b6-control（反向副本：erase 后只摘键、不重建＝批次 0 之前的 B6 写法） ##########
[probe] mode=b6-control users=3
[probe]   find_user_by_id_copy("A") -> found=1 returned_id=A  OK
[probe]   find_user_by_id_copy("C") -> found=0 returned_id=-  OK     ← 身份校验把「错位取到别人」变成「查不到」
[probe]   find_user_by_id_copy("D") -> found=0 returned_id=-  OK
```

**有价值的诚实结论**：身份校验确实把 B6 的失效模式从「取到**另一个用户**」（越权/串号）降级为「查不到」。反向副本已不能取到错误用户，因此 C3-3 的「取回 id == 查询键」成立；**代价**是索引一旦陈旧，相关用户在重启前会「消失」。因此这一条**不能**用来说明「1-3 已完成」——契约要求的稳定键改造**没有做**。

**C3-4 PASS**：`init_indexes()` 连续两次调用后 `user_id_map` + `role_permission_map` 的 dump **IDENTICAL**，`role_permission_map[1].size()` 未翻倍（=12）。

**C3-5 FAIL**：见 §3.2 的无锁 `push_back` 清单与 §3.9 的角色/权限 4 个变更点。

---

### 3.4 【H-②】受影响行数通道（C4-1/C4-2/CH-6/CH-7）

**C4-1 PASS**（签名未变）但 **CH-7 FAIL**（返回表达式被改）。`git diff` 原文：

```
      bool execute_bind(const std::string& sql, const std::vector<Bind>& params = {}) {
  -        if (!db_) return false;
  +        int affected = 0;
  +        return execute_bind_affected(sql, params, affected) == 0;
  +    }
  +
  +    int execute_bind_affected(const std::string& sql, const std::vector<Bind>& params, int& affected_rows) {
```

契约 CH-7 要求「对 `bool execute_bind` 的**签名与返回表达式**为零改动（只允许新增函数）」。签名未变、语义等价（我在 §3.4 的 case4/case5 验证了旧行为保持），但**返回表达式确实被改写**，字面判 FAIL。

**C4-2 PASS**（探针 `b1probe_h2`，直接调库层）：

```
[case0] insert: affected=1
[case1] UPDATE existing  : rc=0 affected=1  (expect rc=0 affected=1)
[case2] UPDATE nonexistent: rc=0 affected=0  (expect rc=0 affected=0)
[case3] DELETE nonexistent: rc=0 affected=0  (expect rc=0 affected=0)
[case4] OLD bool execute_bind, 0-row UPDATE: returns true  <-- H-2 受害者分支恒不可达
[case5] OLD bool execute_bind, 0-row DELETE: returns true  <-- H-2 受害者分支恒不可达
[case6] prepare failure: rc=1 affected=0  (expect rc!=0)
```

case4/case5 说明：**旧 `bool execute_bind` 的语义被原样保留**（0 行也返回 true），这正是评价端点两个 `else { code 404 }` 分支恒不可达的原因；契约要求保留该语义（不动 600+ 调用点），因此这两条不是缺陷，而是下一段 CH-6 FAIL 的成因。

**CH-6 / C4-3 FAIL**——两个评价受害者**没有**改用行数判据（`routes_teacher.cpp:819` / `:848` 仍是 `bool execute_bind`），端到端实测：

```
--- PUT /api/teacher/evaluation/999999 (期望 404) : HTTP 200
    body: {"code":200,"msg":"评价修改成功"}
--- DELETE /api/teacher/evaluation/999999 (期望 404) : HTTP 200
    body: {"code":200,"msg":"评价删除成功"}
```

**H-② 端到端 PASS（但 HTTP 状态仍为 200）**：外部先删除目标行，再走改密/重置口令：

```
>>> 外部删除该行: DELETE FROM users WHERE id='student-02-01-03'
rc=0 changes=1
  ROW{remaining=0}
--- reset-password (row deleted externally, 修复前为 200) : HTTP 200
    body: {"code":500,"msg":"密码重置失败"}         ← 调用方**可以**从 body code 判别失败
...
--- change-password (DB row deleted externally; 修复前 200 成功) : HTTP 500
    body: {"code":500,"msg":"密码保存失败，请重试"}
```

**注意**：`reset-password` 只改业务码、**未改 HTTP 状态**（仍是 200）；`change-password` 两者都改（500）。这是既有的 B17 问题（HTTP 状态与业务码不统一），本批未处理，且会让「只看 HTTP 状态」的调用方判错。

---

### 3.5 【1-4】B8 兑换原子性（C4-4/C4-5/C4-6）

每次用**全新**学生（避免残留干扰），商品与余额现造。原始响应：

**双花（余额 150，商品 cost=150）**

```
    before:   ROW{points=150}
    resp: {"code":200,"data":{"remain_points":0},"msg":"兑换成功！"}
    resp: {"code":400,"msg":"积分不足！"}
    resp: {"code":400,"msg":"积分不足！"}
    concurrent=3 successes=1
    after :   ROW{points=0}
    records:  ROW{n=1}
```

**超卖（stock=2, cost=1，5 并发）**

```
    item id=11 stock=2 cost=1
    resp: {"code":200,"data":{"remain_points":99},"msg":"兑换成功！"}
    resp: {"code":200,"data":{"remain_points":98},"msg":"兑换成功！"}
    resp: {"code":400,"msg":"商品库存不足"}
    resp: {"code":400,"msg":"商品库存不足"}
    resp: {"code":400,"msg":"商品库存不足"}
    concurrent=5 successes=2
    after :   ROW{stock=0}   ROW{points=98}   ROW{n=2}
```

**无限库存（stock=-1）分支**

```
    item id=12 stock=-1
    resp: ... remain_points 97 / 96 / 95 ...  "兑换成功！" ×3
    concurrent=3 successes=3
    after :   ROW{stock=-1}   ROW{points=95}
```

**C4-6（失败不留半成品）PASS**：`CASE 4` 起始 stock=20，全程只有 **2 次成功**兑换（1 次手工对照 + 1 次 3 并发测试中的 1 次），终态实测：

```
  ROW{id=4, name=体育用品, cost=150, stock=18}     ← 净扣 2，与成功次数一致
  ROW{redemption_rows_item4=2}
```

即在 3 并发那轮里，2 个失败请求**已经先扣了库存**、随后在 `with_user_record` 内因积分不足失败**提前 return**，由 `SqliteDb::Transaction` 析构回滚——净影响为 0。契约原文设想的场景（「扣积分成功但库存不足」）在本实现里不存在（顺序是**先扣库存后扣积分**），但判据「失败路径不留半成品」**成立**。

**结论：C4-4 / C4-5 / C4-6 → PASS。**

---

### 3.6 【1-6】B4 XFF 信任边界 + 家长端锁定（C6-x）

**符号**

```
config.h:43: bool trust_proxy_headers = false;
config.h:46: std::vector<std::string> trusted_proxies;
auth.h:209: auto it = req.headers.find("X-Forwarded-For");      ← 唯一命中，位于 proxy_trusted 分支内
（X-Real-IP：零命中）
routes_public.cpp:32-68   ip_key/acct_key 的 can_try / record_fail ×2 / record_success ×2
routes_parent.cpp:67-97   家长登录同构的 6 个调用点
auth.h:130: inline constexpr size_t kMaxLoginAttemptEntries = 10000;
```

**反向副本对照（探针 `b1probe_xff`）——证明「修复前该判据必然失败」**

```
=== (a) REAL impl, trust_proxy_headers=false (default) ===
  XFF=1.1.1.1   -> client_ip=127.0.0.1  ip_key=ip|127.0.0.1           client_key=127.0.0.1|admin
  XFF=2.2.2.2   -> client_ip=127.0.0.1  ip_key=ip|127.0.0.1           client_key=127.0.0.1|admin
  ... 6 个不同 XFF 全部得到同一个键
=== (b) REAL impl, trust_proxy_headers=true + trusted_proxies={127.0.0.1} ===
  XFF='9.9.9.9, 1.1.1.1  , 10.0.0.1' -> client_ip=10.0.0.1 (last hop)
  (b2) REMOTE_ADDR 不在白名单: XFF='1.1.1.1, 2.2.2.2' -> client_ip=127.0.0.1 (must be REMOTE_ADDR)
=== (c) REVERSE COPY: pre-fix logic (unconditional XFF, first hop) ===
  XFF=1.1.1.1   -> OLD client_key=1.1.1.1|admin
  XFF=2.2.2.2   -> OLD client_key=2.2.2.2|admin
  ... 6 个不同 XFF 得到 6 个不同键
```

**C6-3/C6-4 端到端（隔离实例 L1，端口 18101，max_login_attempts=3）**

```
    attempt 1 XFF=1.1.1.1 -> HTTP 200  {"code":401,"msg":"用户名或密码错误"}
    attempt 2 XFF=2.2.2.2 -> HTTP 200  {"code":401,"msg":"用户名或密码错误"}
    attempt 3 XFF=3.3.3.3 -> HTTP 200  {"code":401,"msg":"用户名或密码错误"}
    attempt 4 XFF=4.4.4.4 -> HTTP 429  {"code":429,"msg":"登录尝试过于频繁，请稍后再试"}
    attempt 5 XFF=5.5.5.5 -> HTTP 429  {"code":429,"msg":"登录尝试过于频繁，请稍后再试"}
    attempt 6 XFF=6.6.6.6 -> HTTP 429  {"code":429,"msg":"登录尝试过于频繁，请稍后再试"}
--- 锁定后用**正确**口令也应被拒 ---
    correct password while locked -> HTTP 429  {"code":429,"msg":"登录尝试过于频繁，请稍后再试"}
```

（401 的业务码配 HTTP 200，是既有 B17 问题；锁定用 429 且同时设置 `res.status = 429`。）

**C6-5 家长端（实例 L2，端口 18102）**

```
    parent attempt 1..3 -> 401（业务码）"家长密码错误"
    parent attempt 4..6 -> HTTP 429
    correct parent password while locked -> HTTP 429
```

**C6-6 并发（实例 L3，端口 18103，50 并发错误口令 + 每次不同 XFF）**

```
    HTTP code distribution over 50 concurrent wrong-password logins:
      401x10  429x40
    server alive after 50 concurrent wrong logins? True
    stderr.txt: (空)
    correct admin password after burst -> HTTP 429
```

**C6-7 / C6-8 白盒（探针 `b1probe_auth`，直接调用 auth.h 的内联接口）**

```
[probe] fed 11000 distinct keys; login_attempts().size()=10000 (cap=10000)
[probe] C6-7 upper bound -> HOLD

[probe] after 1000 concurrent fails: entry_present=1 locked_until_in_future=1 fails_left=1
[probe] login_can_try(acct|race) = false (expect false = locked)
[probe] process survived concurrent counting -> yes

[probe] after 3 fails: can_try=false (expect false)
[probe] after simulating expiry: entry_present=1 locked_until<=now=1 can_try=true (expect true)
[probe] after cleanup (4h idle, unlocked): entry_present=0 (expect 0)
[probe] locked entry survives cleanup: present=1 (expect 1)
```

**结论：1-6 全部 PASS。**
（已知残余风险，**非本批失败**：锁定键是 `ip|真实IP` + `acct|用户名` 两个维度，而后者只清前者——3 次失败即把该 IP 上**所有**账号锁 30 分钟，NAT/本机环境下会连坐；进程重启清零；`reset-password` 不参与计数。契约 §8 已声明属批次 2。实测依据：L1/L3 里「正确口令也返回 429」。）

---

### 3.7 【1-5】B9 对象级归属校验（C5-x）——**完全未落地**

**符号证据（决定性的）**：归属原语**只有定义，没有任何调用点**：

```
models.h:172:  bool teacher_owns_student(const string& teacher_id, const string& student_id);
models.h:174:  bool teacher_owns_class_name(const string& teacher_id, const string& class_name);
models.cpp:495: bool teacher_owns_class_name(...) { ... }
models.cpp:512: bool teacher_owns_student(...) { ... }
→ 在全部 *.h/*.cpp 中，teacher_owns_* / require_teacher_owns_student 的命中只有上面 4 行；routes_teacher.cpp 零命中

routes_teacher.cpp:81:  if (!bound_class_names.empty()) {         ← C5-2 要求零命中
routes_teacher.cpp:84:      if (cn == user.className) { in_bound = true; break; }   ← C5-3 要求零命中
```

**端到端（教师 A = teacher-001 绑定「高二(1)班」；目标 = student-01-B-01，属「B1班B」，由 teacher-002 拥有）**

```
--- PUT /api/teacher/students/student-01-B-01 (cross-class edit) : HTTP 200
    body: {"code":200,"data":{"className":"B1班B","id":"student-01-B-01","name":"HACKED-BY-T1",...},"msg":"学生信息更新成功"}
--- DELETE /api/teacher/students {id=student-01-B-01} (cross-class delete) : HTTP 200
    body: {"code":200,"msg":"学生删除成功"}                      ← 真的把别的班的学生删掉了
--- POST /api/teacher/points {studentId=student-01-B-01, +777} : HTTP 200
    body: {"code":404,"msg":"学生不存在"}                        ← 只因上一步已把该学生删掉
--- POST /api/teacher/evaluation {studentId=student-01-B-01} : HTTP 200
    body: {"code":404,"msg":"学生不存在"}                        ← 同上（非归属校验所致）
--- POST /api/teacher/students {className=B1班B} (other teacher class) : HTTP 200
    body: {...,"data":{"className":"B1班B","id":"student-01-B-01",...},"msg":"学生添加成功"}
    inserted parent_messages id=1 for student-01-B-01
--- POST /api/teacher/parent-messages/1/reply (cross-class) : HTTP 200     ← ★ 任务书点名要覆盖的 handler
    body: {"code":200,"msg":"回复成功"}
--- PUT /api/teacher/evaluation/999999 (期望 404) : HTTP 200  {"code":200,"msg":"评价修改成功"}
--- DELETE /api/teacher/evaluation/999999 (期望 404) : HTTP 200  {"code":200,"msg":"评价删除成功"}
```

**正向对照（本班操作确实成功，证明不是「全都坏」）**

```
--- POST /api/teacher/points {studentId=student-02-01-01, +5} (own class) : HTTP 200
    body: {"code":200,"data":{"points":155,"record":{"id":1,...}},"msg":"积分操作成功"}
--- POST /api/teacher/students {className=高二(1)班} (own class) : HTTP 200
    body: {"code":200,"data":{"className":"高二(1)班","id":"student-02-01-02",...},"msg":"学生添加成功"}
```

**C5-5**（无绑定教师）：admin 建 `probe_teacher_unbound`（无 `teacher_classes` 记录），登录后：

```
--- GET /api/teacher/students (未绑定教师) — 学生条目数=4 (契约要求 0) : HTTP 200
    body: {"code":200,"data":[
      {"className":"高二(1)班","id":"student-02-01-01",...},
      {"className":"B1班A","id":"student-01-A-01",...},
      {"className":"B1班B","id":"student-01-B-01",...},
      {"className":"高二(1)班","id":"student-02-01-02",...}]}
```

**结论：C5-1/C5-2/C5-3/C5-4/C5-5/C5-6 全部 FAIL；1-5 属于“只写了函数、没接线”。**
（另：`teacher_owns_student()` 的 SQL 没有契约 §3.5 第 2 条要求的 `AND u.role_id = 3` 约束——即便接线，判定范围也与契约不一致。`teacher_owns_evaluation` 未定义。）

---

### 3.8 【1-7】B10 + B14 + B12（C7-x）

**B10（类型错误 → 有 JSON 体、无 EXCEPTION_WHAT）PASS**

```
--- POST /api/auth/login {"username":123} : HTTP 500
    body: {"code":500,"msg":"服务器内部错误"}
    headers raw:
      HTTP/1.1 500 Internal Server Error
      Access-Control-Allow-Headers: Content-Type, Authorization, X-CSRF-Token
      Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS
      Content-Length: 42
      Content-Type: application/json
    EXCEPTION_WHAT present? False
    body contains internal path/line/exception? False
```

**C7-2 字面 FAIL**：`EXCEPTION_WHAT` 在 `main.cpp` 里**只出现在注释**（:485/:486），没有契约要求的 `res.headers.erase("EXCEPTION_WHAT")`。由于 httplib 写响应头时显式跳过该头（`httplib.h:1981`），**行为上无泄露**，因此这只是「字面未落实」。

**C7-3 PASS**（4xx/5xx 全都是结构化 JSON）

```
--- GET /api/definitely-not-a-route : HTTP 404  {"code":404,"msg":"资源不存在"}
--- DELETE /api/auth/login (405?) : HTTP 404  {"code":404,"msg":"资源不存在"}   ← httplib 对未匹配方法返回 404，仍是结构化 JSON
```

**C7-5 FAIL**（三个 setter 只落了 1 个）

```
main.cpp:457: svr.set_logger(...)
main.cpp:480: svr.set_payload_max_length(1 * 1024 * 1024);
main.cpp:494: svr.set_error_handler(...)
→ set_read_timeout：零命中    set_keep_alive_max_count：零命中
```

**C7-6 PASS**（>1 MiB → 413 + JSON；响应含 curl 的 `100 Continue` 中间响应，原文如下）

```
    body size = 1228830 bytes
HTTP/1.1 100 Continue
HTTP/1.1 413 Payload Too Large
Content-Length: 36
Content-Type: application/json

{"code":413,"msg":"请求体过大"}
    EXCEPTION_WHAT present? False
```

**C7-8 部分 PASS + 新缺陷**

```
################ C7-8 慢连接：只发部分 body 后不动 ################
    1 个卡住连接存在时，正常请求: HTTP 200  耗时 49 ms
    卡住连接在 4942 ms 后被对端关闭（Read 返回 64）      ← 靠 httplib 默认 CPPHTTPLIB_READ_TIMEOUT_SECOND=5
    造 8 个卡住连接（= thread_count），再测正常请求延迟
    8 个卡住连接存在时，正常请求: HTTP 200  耗时 9684 ms   ← ★ 8 个 worker 全被慢连接占住
    清理后正常请求: HTTP 200  耗时 19 ms
```

httplib 默认值依据：

```
httplib.h:24: #define CPPHTTPLIB_KEEPALIVE_MAX_COUNT 5
httplib.h:28: #define CPPHTTPLIB_READ_TIMEOUT_SECOND 5
httplib.h:44: #define CPPHTTPLIB_PAYLOAD_MAX_LENGTH ((std::numeric_limits<size_t>::max)())
httplib.h:732: time_t read_timeout_sec_ = CPPHTTPLIB_READ_TIMEOUT_SECOND;
```

契约只要求「一个卡住的连接不阻塞其他请求」——**该形态成立**（49 ms），且 5 秒后被关闭；但**8 个慢连接会把合法请求拖延 ~9.7 秒**，而契约 1-7 的修复动作原本要求显式设 `set_read_timeout` 与 `set_keep_alive_max_count`（未做）。列为新缺陷 §4.3。

**B12（C7-9…C7-12）FAIL——完全未实现，编译期即可证伪**

我写了一个只引用 `db.last_error()` 的探针，编译直接失败：

```
C:\...\b1probe_b12.cpp:6:24: error: 'class SqliteDb' has no member named 'last_error'
    6 |     std::string e = db.last_error();   // C7-9 要求的成员/访问器
      |                        ^~~~~~~~~~
```

运行期对照（两种查询都无法区分）：

```
[case7] query(bad table) : empty=1
[case8] query(no rows)   : empty=1
[case9] distinguishable via API? -> NO: both are json::array() and there is no last_error() member (B12 NOT implemented)
```

且 DB 层错误仍走 `std::cerr`（C7-11 FAIL），共 10 处：

```
sqlite_wrapper.h:66/98/114/219/249/298/304/316/403/426: std::cerr << ...
```

`auth.h:324-342` 的 `get_session_info()` 仍用 `query()` + `snprintf` 拼串，空结果一律当「会话无效」→ 401，C7-12 FAIL。

---

### 3.9 【H-①】索引重建（CH-x）——**完全未落地**

**符号证据：4 个变更点之后都没有重建**（我逐点看了后 15 行）

```
routes_admin.cpp:1084: roles.push_back(Role{new_id, name, description});        ← 之后无 init_indexes/rebuild
routes_admin.cpp:1149: permissions.push_back(Permission{new_id, name, code, description});  ← 之后无重建
routes_admin.cpp:1185: permissions.erase(it);                                   ← 之后无重建
routes_admin.cpp:1221: roles.erase(it);                                         ← 之后无重建
（只有导入端点在 :1811 收口调用 init_indexes();）
```

**运行期证据（探针 `b1probe_misc h1` / `h1-authz`，复刻同一个全局容器的同一个写点）**

```
[probe] --- 复刻 POST /api/admin/permissions (routes_admin.cpp:1148-1149) ---
[probe] after push_back id=2 : permission_id_map.count=0 (index NOT rebuilt -> missing)
[probe] after grant+init_indexes: permission_id_map.count=1 ; role_permission_map[1] size=2

########## h1-authz ##########
[probe] check_permission_optimized(admin-01,'b1:probe') BEFORE delete = TRUE
[probe] 复刻 DELETE /api/admin/permissions (routes_admin.cpp:1185: erase, 之后无重建)
[probe]   permissions.size=1 (该权限已删除)
[probe]   permission_id_map.count(2)=1  <-- 陈旧条目仍在
[probe] check_permission_optimized(admin-01,'b1:probe') AFTER delete = TRUE  <-- 已删权限仍授权
[probe] 若端点调用 init_indexes() 重建后 = false (permission_id_map.count=0)

[probe] --- 复刻 POST/DELETE /api/admin/roles (routes_admin.cpp:1084 / :1221) ---
[probe] after roles.push_back id=5 : role_id_map.count=0 (missing)
[probe] after init_indexes        : role_id_map.count=1
[probe] 复刻 roles.erase（端点之后无重建）: role_id_map.count=1  <-- 陈旧条目仍在
```

**HTTP 侧（任务书要求的 H-① 端到端）**

```
--- GET /api/admin/statistics (改密后, 未删角色) : HTTP 200
--- POST /api/admin/roles (new role) : HTTP 200      new role id = 5
--- DELETE /api/admin/roles (probe role) : HTTP 200
--- GET /api/admin/statistics (删角色后, 同一既有会话) : HTTP 200
    >>> 判据要求「被拒」，实测状态码 = 200
```

**我判定该判据本身是「未闭合的」——必须写明**：任务书给的判据是「管理员删掉一个角色后，用同一会话访问 `/api/admin/statistics` → 应被拒（修复前 200）」。这在因果上不成立：`/api/admin/statistics` 的守卫是 `statistics:view`（权限 id 7），而**被删的是另一个角色**，管理员（role 1）自身权限不受影响；H-① 的语义是「索引陈旧」，不是「删角色会撤销其他角色的授权」。因此无论修不修 H-①，管理员删一个**非内置角色**后访问统计都应当（也确实是）**200**。我按任务书原样执行并对结果如实标注：**这个判据无法区分修复前后**。

**H-① 真正的可复现后果在函数级（上面的 h1-authz）**：只要某角色被授予过该权限，删权限后 `check_permission_optimized` **仍然授权**。这个后果在**活的 HTTP 路径上观察不到**，因为端点守卫用的 7 个权限码 id 全在 1–7、而内置权限 `id<=12` 不可删（`routes_admin.cpp:1175`），且没有任何端点用自定义权限码——与任务书的提示一致，我按「潜在缺陷（函数级已复现，端点级不可达）」定性。

（顺带纠正任务书的一处细节：`Auth::check_permission` 并不是「读向量」，它在 `auth.h:369-371` 委托给 `models.cpp:458` 的 `check_permission_optimized()`，后者读的是 `role_permission_map` / `permission_id_map` **两个索引 map**——正因为如此，陈旧条目才会造成「已删权限仍授权」。）

**结论：CH-1 / CH-2 / CH-3 → FAIL。**

---

### 3.10 【伴-1】RAII 事务（CB1-x）

**符号**

```
routes_admin.cpp 中 BEGIN TRANSACTION / execute("COMMIT") / execute("ROLLBACK") → 仅注释命中（:1558、:1825）
routes_admin.cpp:1563: SqliteDb::Transaction txn(db);
routes_public.cpp:386: SqliteDb::Transaction txn(db);
sqlite_wrapper.h:293:  class Transaction {   ← BEGIN IMMEDIATE + 析构未 commit 即 ROLLBACK
```

**运行期（并发「导入必然失败」+ 单条写入）**

注入方式：import 的第 2 个用户 `role_id` 传字符串 → `json::type_error` 在**写完第 1 个用户之后**抛出 → 走 `catch(exception)` → RAII 回滚。

```
      import  HTTP {"code":500,"msg":"导入失败"}200
      points  HTTP {"code":200,"data":{"points":15,"record":{"id":10,...,"reason":"b1-isolation-1790331157"}},"msg":"积分操作成功"}200
    CB1-4：导入失败是否留下半成品（imp-a/imp-b 应为 0 行）:
  ROW{half_products=0}                         ← PASS：无半成品
    CB1-3：并发单条写入是否存活于 DB:
  ROW{survived=1}                              ← PASS：并发写入未被回滚
  ROW{points=15}
    服务端日志尾部:
      [2026-09-25 18:12:36] [ERROR] 导入失败: [json.exception.type_error.302] type must be number, but is string
      [2026-09-25 18:12:36] [INFO] /api/admin/import responded with status 200
      [2026-09-25 18:12:36] [INFO] /api/teacher/points responded with status 200
```

**CB1-1 / CB1-2 / CB1-3 / CB1-4 → PASS**（CB1-5 未注入，见 §5）。
注：`res.status` 仍为 200（业务码 500），属既有 B17 问题。另注：`json::parse_error` 分支（`routes_admin.cpp:1823`）的「漏回滚」其实已不存在——`json::parse`（:1536）发生在 `Transaction` 构造（:1563）**之前**，该分支根本没有事务可回滚；注释（:1825）措辞不准确，但行为正确。

---

### 3.11 【伴-2】日志轮转（CB2-x）

**符号**

```
logger.h:65:  std::lock_guard<std::mutex> lock(mutex_);
logger.h:67:  rotate_if_needed_locked();                 ← 调用点在锁**之后**
logger.h:93-96: 公开入口 rotate_if_needed() 也自行加锁后走同一实现
```

**运行期（max_size=16 KiB，max_files=60 足够大以守恒行数；8 线程 × 500 + 1）**

```
########## logs（当前实现） ##########
[probe] wrote=4001 lines; files_found=13; total_lines_in_files=4001; lost=0
[probe] line-conservation -> HOLD

########## logs-control（反向副本：轮转在锁外、append 在锁内＝修复前的 :59/:61） ##########
[probe] wrote=4001 lines; files_found=22; total_lines_in_files=3972; lost=29
[probe] line-conservation -> VIOLATED
```

**CB2-1 / CB2-2 / CB2-3 → PASS**（反向对照确实丢 29 行，说明该判据「能失败」，不是恒真）。

---

### 3.12 【伴-2'】积分记录 id（CB2'-x）

**符号**：`routes_teacher.cpp` 中 `points_records.size() + 1` 零命中（仅注释 :353）；改用 `db.last_insert_rowid()`（:363）。

**运行期（关键：重启服务，使内存 `points_records` 清空，而库内已有 6 条记录）**

```
    DB before:   ROW{n=6}   ROW{m=6}
--- login teacher-001 : HTTP 200
    attempt 1 -> HTTP 200 record.id=7
    attempt 2 -> HTTP 200 record.id=8
    attempt 3 -> HTTP 200 record.id=9
    returned ids: 7,8,9  (修复前 points_records.size()+1 会返回 1,2,3)
    DB rows:
        ROW{id=9, reason=b1-recordid-3}
        ROW{id=8, reason=b1-recordid-2}
        ROW{id=7, reason=b1-recordid-1}
```

**CB2'-1 / CB2'-2 → PASS**（重启后不重号，且与库内 id 一致）。CB2'-3 部分（见 §5）。

---

### 3.13 【伴-3】删库重建 fail-fast（CB3-x）

我写了一个 `b1lock.exe`：以 `FILE_SHARE_READ|FILE_SHARE_WRITE`（**不含** `FILE_SHARE_DELETE`）持有旧库文件——这样 SQLite 仍能正常打开它，而 `std::remove`（DeleteFile）会以 sharing violation 失败。

**对照组（无人占用）→ 正常删库重建并监听**

```
      [INFO] SQLite 数据库连接成功
      [WARNING] 检测到旧 schema（users.id 为 INTEGER），删除数据库文件并重建为新 schema (TEXT 主键)
      [INFO] 旧 schema 数据库已删除并重建成功
      [INFO] 数据库表结构初始化成功
      [INFO] 首启检测：users 表为空，已创建 4 个种子账号（随机强口令 + 强制首登改密）
    重建后的 schema（id 应为 TEXT）:
        ROW{cid=0, name=id, type=TEXT, notnull=0, dflt_value=<NULL>, pk=1}
```

**实验组（文件被持有、拒绝删除）→ 必须非 0 退出**

```
    b1lock pid=29296: holding ...\oldschema\old.db for 45 s with delete-sharing denied
    exited_within_60s=True
    EXIT CODE = 1  (fail-fast 要求非 0)
    --- stdout ---
      [INFO] SQLite 数据库连接成功
      [WARNING] 检测到旧 schema（users.id 为 INTEGER），删除数据库文件并重建为新 schema (TEXT 主键)
      [ERROR] 删除旧数据库文件失败，拒绝继续启动（fail-fast）: old.db
    --- stderr ---
      [FATAL] 无法删除旧 schema 数据库文件: old.db
              请检查文件占用/权限后重试（退出码 1）。
    old.db 是否仍存在: True
    端口 18106 是否仍可服务（应不可，进程已退出）: 连接失败（预期）
```

`return 1` 位置：`main.cpp:204`（删除失败）、`:210`（重开失败）、`:225`（首次 open 失败）、`:449`（建表失败），全部在 `svr.listen()`（:553）之前。

**CB3-1 / CB3-2 / CB3-3 / CB3-4 → PASS。**

---

## 4. 我发现的缺陷（含不在清单内的）

### 4.1 【高】N2 / 伴-4 仍在两条活路径上造成**静默丢失用户**
`routes_teacher.cpp:130-146`（教师建学生）与 `routes_public.cpp:157-186`（自助注册）都没有用本批新增的原子接口 `add_user_record()`：`generate_user_id()` 返回后到 `users.push_back` 之间隔着 **PBKDF2（实测均值 112.95 ms）**，任何并发请求都会拿到**同一个 id**；随后 `save_user_to_db()`（`models.cpp:94`，仍是 `INSERT OR REPLACE`）把前者**静默覆盖**。
证据：8 并发注册 → 8 个 200、库内只剩 1 个用户；6 并发建学生 → 6 个 200、库内只剩 1 行。这是**用户数据丢失**，不是「潜在的」竞态。

### 4.2 【中】契约里 C2-4 的 SQL 判据不可靠（会假通过）
`SELECT id, COUNT(*) FROM users GROUP BY id HAVING COUNT(*)>1` 在本缺陷下返回 **0 行**（`INSERT OR REPLACE` 把冲突行合并），因此按该判据会得出「无重复 id」的结论。正确判据必须是**行数/用户名数守恒**（种子数 + N）。我在 §3.2 同时报告了两种口径的结果。

### 4.3 【中】慢连接可耗尽全部 worker（1-7 未设 keep-alive/read 上限）
8 个只发一半 body 就不再发送的连接（= `thread_count`）会让一个**合法**请求等待 **9684 ms**（对照：1 个慢连接时 49 ms）。成因是 `main.cpp` 未调用契约要求的 `set_read_timeout()` / `set_keep_alive_max_count()`，只能靠 httplib 默认的 5 秒读超时兜底。

### 4.4 【中】新连接模型的线程退出崩溃：`thread_local` 析构里调 `sqlite3_close`
`sqlite_wrapper.h:377-383` 的 `ThreadConn::~ThreadConn()` 在 `sqlite3_close()` 后清理。最小 A/B（同工具链、同静态链接参数、8 线程）证明**正是这一步**导致崩溃：

| 探针形态 | 结果 |
|---|---|
| `plain512` / `plain4096`：thread_local 平凡 `char[N]` + 用户析构（不 close） | dtor 8/8，exit 0 |
| `string`：thread_local 含 `std::string`（什么都不做） | **0xC0000374** |
| `sqlite-local`：线程体内 open/exec/close（非 TLS 析构） | 8/8，exit 0 |
| `product-like-noopen`：**与产品同形的 ThreadConn**，不 open | 8/8，exit 0 |
| `product-like-noclose`：同形，open 但**析构里不 close** | 8/8，exit 0 |
| **`product-like`：同形，open + 析构里 close** | **0xC0000005** |
| `product-like-query`：同上再加 `sqlite3_exec` | **0xC0000005** |

`b1probe_misc conn` 独立复现同类崩溃（`0xC0000374`，3/3 次）。
**影响面**：不是请求处理路径（HTTP 压测期间服务未崩），而是**线程退出阶段**——即 worker 线程结束/优雅关停时；连接（`sqlite3_close`）失败或 TLS 清理异常会让进程以异常码结束、`atexit` 之后的清理不保证执行。
**诚实限制**：这是**对产品机制的最小复现**（探针只含该模式，不含产品代码），我**没能**端到端触发真实服务的优雅关停（隐藏窗口进程收不到 Ctrl+C，`taskkill` 不跑析构），因此不作为「产品关停必崩」的端到端结论，但该 pattern 与产品逐字相同，值得在关停路径上专项复验。

### 4.5 【中】H-①：删除权限/角色后索引陈旧，且**已删权限仍被授权**
函数级已复现（§3.9）：`permissions.erase` 之后 `permission_id_map` 仍含该 id，`check_permission_optimized()` 对已删权限码返回 **true**；调用 `init_indexes()` 后才变 false。角色侧同理。活 HTTP 路径不可达（内置权限不可删、无端点用自定义权限码），属**潜在缺陷**，但契约把它列为必做项。

### 4.6 【中】`g_state_mutex` 与 SQLite 写锁的**加锁顺序倒置**（代码级，未端到端复现）
- `routes_public.cpp:386` 先 `SqliteDb::Transaction txn(db)`（`BEGIN IMMEDIATE`，占住 DB 写锁），**之后**:423 才 `with_user_record(...)`（取 `g_state_mutex`）；
- 反向顺序存在于多处：`with_user_record` 回调里做 DB 写，即**持 `g_state_mutex` 再去拿 DB 写锁**——`routes_teacher.cpp:311-322`（`update_user_points_in_db`）、`routes_admin.cpp:970-984`（reset-password）、`routes_public.cpp:423-431`、`:579-592`（change-password）。
两条路径交叉时：A 持 DB 写锁等 `g_state_mutex`（无超时），B 持 `g_state_mutex` 等 DB 写锁（`busy_timeout=5000`）→ 至少 5 秒的停顿，且 B 的写会以 `SQLITE_BUSY` 失败（返回 500）。契约决策② 的 R7 明确要求该锁不得与其它锁互套。**未端到端复现**（需要精确时序），仅作代码级风险 + 建议。

### 4.7 【低】注册接口的 `data` 被序列化成**数组**而不是对象
`routes_public.cpp:188`：`response = {{"code",200},{"msg","注册成功"},{"data",{"user_id",new_id}}};` —— nlohmann 把 `{"user_id", new_id}` 当成**两元素数组**。实测原文：

```
reg 1 : {"code":200,"data":["user_id","student-09-N-01"],"msg":"注册成功"}
```

前端若按 `data.user_id` 取值会拿到 `undefined`。既存缺陷（本批未改这两行），验证中发现。

### 4.8 【低】HTTP 状态码与业务码长期不一致（B17）
多处 handler 只写业务码不设 `res.status`：登录失败 → HTTP 200 + `{"code":401}`；`reset-password` 0 行 → HTTP 200 + `{"code":500}`；`import` 失败 → HTTP 200 + `{"code":500}`。而 429/500（change-password）会正确设置状态。任何以 HTTP 状态为准的调用方/代理/监控都会判错。

### 4.9 【低】`permissions.back().id + 1` / `roles.back().id + 1` 的 id 复用
`routes_admin.cpp:1148`、`:1083`。删除 id=13 后，下一次新增又会拿到 **13**；与「删后不重建索引」叠加时，陈旧 map 条目会指向**新**权限对象但保留**旧** code（探针里复现了 `id=2` 的复用）。契约 §3.8 的「反例」已点名此写法。

### 4.10 【低，仓库卫生】仓库根残留旧构建产物
`b1_server.exe`、`server.exe`、9 个 `.o`、`server.log`（5.1 MB）存在于仓库根，时间戳均为 **2026/9/19**（先于本次验证），已被 `.gitignore` 覆盖故不出现在 `git status` 中。本次验证**没有**新增任何仓库内产物，但建议清理以免误用旧二进制。

### 4.11 【低】`teacher_owns_student()` 的判定 SQL 与契约不一致
`models.cpp:516-521` 缺少契约 §3.5 第 2 条要求的 `AND u.role_id = 3`；`teacher_owns_evaluation` / `require_teacher_owns_student` 未定义。即便 1-5 接线，口径也与契约不符。

---

## 5. 未独立验证 / 无法端到端复现的项（逐条标注）

1. **CB1-5（COMMIT 失败 → `Logger::error` 且不回 200）**：未注入 COMMIT 失败（需要让 `COMMIT` 在事务已关闭/被顶掉的状态下执行）。**未独立验证**。
2. **CB2'-3 的失败分支**：`rc == 0 && affected == 1` 的判断代码存在，但「INSERT 失败」分支未注入。**仅符号核对**。
3. **C7-12（鉴权路径 DB 错误 → 500 而非 401）**：因 B12（`last_error()`）完全不存在，**无从接入，无法验证**。
4. **C6-8 的真实时间到期**：我用白盒方式把 `locked_until` 拨到过去来验证「到期后可恢复」（并要求真实配置下不等待 30 分钟）。属于**模拟到期**，不是真实等待。
5. **C4-6 的原始设想场景**（「积分扣成功但库存不足」）**在本实现里不存在**（顺序是先扣库存后扣积分）。我用「库存先扣成功、积分随后失败 → RAII 回滚 → 净扣库存 == 成功次数」完成了等价判据的端到端验证，但**没有**、也无法构造契约原文设想的那个顺序。
6. **4.4 的 thread_local 崩溃**：为**最小复现**（探针只含该 pattern），**未能**在真实服务进程上触发「优雅关停 → worker 退出」路径（无法向隐藏窗口进程发送 Ctrl+C；强杀不执行析构）。因此不作为「服务关停必崩」的端到端结论。
7. **4.6 的加锁顺序倒置**：**代码级判定，未端到端复现** 5 秒停顿/BUSY 失败（需要精确的并发时序注入）。
8. **C1-4 / CB1-3 的「大文件导入」形态**：我用「必然失败的导入（type_error 注入）+ 并发单条写入」验证了「写入不被回滚 + 无半成品」，**没有**用真实的大批量成功导入做压力时序测试。
9. **1-5 的「删权限」一半**：按任务书要求只按**潜在缺陷**核对（函数级已复现，见 §3.9 的 h1-authz），**不**写成端点级已复现。
10. **H-① 的 HTTP 判据本身不可判定**：如 §3.9 所述，「删角色后管理员访问 statistics 应被拒」在因果上不成立，**无法区分修复前后**。我按原样执行并给出 200 的实测，同时明确标注这是**判据缺陷**而非实现通过/失败。
11. **CI / GitHub Actions**：未运行（本机无 CI 端到端条件）。
12. **前端 / 全链路 UI**：未测试；所有结论均来自后端 HTTP + 直连 DB + 进程内探针。

---

## 6. 对「批次 1 是否可结项」的判断

**结论：不可结项（FAIL）。** 理由按严重度排序：

1. **1-5（B9）根本没有落地**。`teacher_owns_student` / `teacher_owns_class_name` 只有定义、**调用点为 0**，`bound_class_names.empty()` 兜底与 className 字符串比较都还在。端到端实测：跨班 **改学生信息、删学生、把学生建到别人的班、回复别人班家长的留言** 全部 `HTTP 200`，未绑定班级的教师能看到**全校**学生。这是本批最高的越权项，也是契约 §3.5 判 C5-4/C5-5/C5-6 必做重跑的三条——全部 FAIL。
2. **1-2 的 N2 / 伴-4 未闭合，且可造成用户数据丢失**。8 并发注册 → 库内只剩 1 个用户（其余 7 个被 `INSERT OR REPLACE` 静默覆盖）。契约 C2-4 自带的 SQL 判据会**假通过**。
3. **H-① 未落地**，且函数级可复现「已删权限仍授权」。
4. **1-7 的 B12 完全未实现**（`last_error()` 不存在，编译期即证伪），`set_read_timeout` / `set_keep_alive_max_count` 缺失；H-② 的两个评价受害者未接入行数判据（`PUT/DELETE evaluation/999999` 仍返回 200「成功」）。
5. **本批新引入的连接模型存在线程退出崩溃**（§4.4，最小 A/B 已定位到「TLS 析构里 `sqlite3_close`」）。
6. 另有未闭合的加锁顺序倒置风险、慢连接耗尽 worker、全局容器仍有 6+ 处无锁直接访问。

**可以结项的部分（建议按子项分别记账，而不是整批结项）**：1-1 连接模型（除析构崩溃）、1-4（B8）兑换原子性、1-6（B4）XFF/家长端锁定、H-② 的通道本体与改密/重置口令接入、伴-1、伴-2、伴-2'、伴-3。这些项我都拿到了可失败对照的正向运行证据。

**建议的最小返工单**：
1. 把 8 个教师写端点接上归属校验（含 `POST/PUT` 的目标 `className` 双向校验、未绑定教师 fail-closed、删除 `bound_class_names.empty()` 兜底）；
2. 注册与教师建学生改用 `add_user_record()`，`save_user_to_db` 的 `INSERT OR REPLACE` 改 `INSERT` 并把唯一冲突当失败；C2-4 的判据改成**行数守恒**；
3. 4 个角色/权限变更点补 `init_indexes()`（并在删除时同步清理 `role_permissions` 源向量）；`roles.back()+1` 改 `max+1`；
4. B12 补齐（`last_error_` + 鉴权 fail-closed + `std::cerr` → `Logger::error`）；评价两个端点改 `execute_bind_affected`；补 `set_read_timeout` / `set_keep_alive_max_count`；
5. 处理 6+ 处无锁全局容器访问，或明确记录为「已知残留」并在契约里划出边界；
6. 评估 `ThreadConn` 的关闭时机（例如不依赖 `thread_local` 析构，或延后到线程池 `shutdown()` 后由主线程统一关闭），并复验关停路径；
7. 修 H-① 的 HTTP 判据（改成函数级/探针级可判定的形式）。

---

## 7. 临时产物与 sha256（删除前记录）

临时根目录：`%TEMP%\b1verify\`（`C:\Users\ADMINI~1\AppData\Local\Temp\b1verify\`）。
**按任务要求测后已整目录删除**；下表是删除前的 sha256，便于任何复验者核对「我告称的产物」与「重建的产物」是否同源。

### 7.1 主构建产物（全量编译）

| 文件 | sha256 | 字节 |
|---|---|---|
| `sqlite3.o` | `BB53966DDDC310387FB40650619D99E4CE167588D0D82D4B75CE59FBF2F14C0F` | 1162258 |
| `main.o` | `F6873F6C20A5945BE0A13D16A8116B9A26456321F2A4C1DD7D68A7940203B289` | 1035671 |
| `models.o` | `2A7DD03CC1AD2792BD32DACD18C76757666E3138614C625E0D84E48EF29988B3` | 219924 |
| `logger.o` | `30BBE578A1E79D9C534003E7ABE0E087B4FE249E090CA3FFD03C330BF65D451C` | 1991 |
| `routes_static.o` | `B0533D7A9E549CE354D8EC442BE426A58588368606BA9396BEAC226765EE63E1` | 593641 |
| `routes_public.o` | `4D50190DFDB3E7C723A70FA791FC49C3D723E34A60520548DC6FFFC0CD1B0630` | 1137529 |
| `routes_admin.o` | `D6CF784151F85565530EC9C154E001457EE79913EEC1EA7684C4BEA6D72C2D31` | 1468765 |
| `routes_teacher.o` | `59F1291CFD145AE9D0D7464C828730F2B831C8AA2C4B0E0CA91F16D6F34C16B8` | 1278038 |
| `routes_student.o` | `D296C00C05BE3D00BA9EC3E1834CA8070CEA68D9EFF3A4B2489993AF4FCDBD3A` | 874986 |
| `routes_parent.o` | `C3A7BD0B25B0615CDD1295CA04BCC16597887D9B75F27E922A056E954A4B02E9` | 1107504 |
| **`b1_server.exe`** | **`5E70695F2CE4CA6DB00614AD0E2045E0E32B12E39E072BFB30A2314A66193336`** | 6114144 |

### 7.2 探针 / 工具源码与二进制

| 文件 | 类型 | sha256 |
|---|---|---|
| `build.ps1` | 全量编译脚本 | `92F23C93C4B85CA15641EFC36DEEDEF90B6FFD16ACDAECA81C96DF0812122D48` |
| `statics.ps1` | 静态判据取证 | `946C6D52378A748F53A95F31530EE48A5E756EB1AE3FD996861496C34EAB0428` |
| `http_helpers.ps1` | HTTP 辅助 | `FCDD8B100BDC0206391D37FE5125DF57FDE7893F34E1C9A3C93088D705FD265C` |
| `srvlib.ps1` | 隔离实例启停 | `9697071D2632148988F259B0CA0A83D2A89044CB853A0AAC31BEDB1B30F48A36` |
| `start_server.ps1` | 主实例首启 | `51AFCD9731E66613497F07C48148CDE0AED556E0CBCC48816177863B5A58DD6B` |
| `stage1.ps1` | 管理员流程 + 1-7 + H-① | `BC283D48E8DD71AE51AA46BBC3FBE1F707782A2416B5DCAEAF9403B3EA945A90` |
| `stage2.ps1` | 1-5 越权 / H-② | `8FF4531050EB184C4567BEF453B6C63620C8B8C62810081E7D14441FDEB58F67` |
| `stage3.ps1` | 1-4 兑换并发 | `1E94E1BF0C17BEB461CEDE71B94DFA15B11175E41A2FE55A23193E4E754306C8` |
| `stage4.ps1` | 伴-2' + N2 端到端 | `1F51E5EA240F86879159C9BF80F4AD59B9DAE8F94DA55B170D689899251C7B8C` |
| `stage5.ps1` | 1-6 锁定 | `CD2D8B01902C2D4CD794DDC45D93D30607EF20392862AB68CCA205B9DD6E47E9` |
| `stage6.ps1` | 伴-3 fail-fast | `DC8836186078D4896B4F22305739C26D9BC4435899790E0628578E41FA4B8936` |
| `stage7.ps1` | C7-8 + CB1-3/4 | `1F37C708CB5570DA4416E735B3BB87FC992EE20A43ADBBD1546A2A39390EFE63` |
| `b1probe_conc.cpp` | 并发 RMW / 建号 源码 | `20770E0B0739E425BBF2B5BFE1CD02B15C2635CEE0C60EFE71845948D5948AB4` |
| `b1probe_conc.exe` | 同上二进制 | `DCCDBA45E44221FC0251A52BFE665339613862502D031DF06667C116AC5E226F` |
| `b1probe_h2.cpp` | H-② 通道源码 | `FE9CB71A2381473D4D1FD78C3DD15C02F8210901548A554139508AC3FED15400` |
| `b1probe_h2.exe` | 同上二进制 | `B977246477F5F9A62D15613FBB893B9AF0032490FC2E07DBD20E1B68E2AB9BBB` |
| `b1probe_b12.cpp` | B12 证伪源码（**编译失败**） | `C6C657197819D12D115DC1CFED3F2D82808E2D8D4F91B579C836E85254F8DB0F` |
| `b1probe_xff.cpp` | XFF 键 A/B 源码 | `FF7A1FD580E4AAF64F7B844D2003C81A53B44FCE64501B2DDB0E1677AEDB6720` |
| `b1probe_xff.exe` | 同上二进制 | `06B262C92CFA582AD2BEF086A9AE8C703615BFFFB7EA2DB6D7CB0B6CDE27BAA7` |
| `b1probe_misc.cpp` | 连接/H-①/B6/日志源码 | `E574B93694B0AF238B68A43D50CF2C67A583044A4A5739347C310B6D97B54B49` |
| `b1probe_misc.exe` | 同上二进制 | `C98A936B523D967D6A35061219571A0356C9EB0B21C1898DFBFF683F785D9A7F` |
| `b1probe_tls.cpp` | TLS 析构崩溃定位源码 | `9F96096DD2DD2644C6D0676B1F9BC41575B68C4F81CE3874C38C438190D5B102` |
| `b1probe_tls.exe` | 同上二进制 | `7EC100934FA92983A49AB65502E61B28DF1A0EBC886054572A02289597B659B1` |
| `b1probe_auth.cpp` | 锁定表白盒源码 | `02E780CE9E5E3FE318E997F5F26A07AA67ED01BBBA4D1CE7FC363A9F0D08C9A0` |
| `b1probe_auth.exe` | 同上二进制 | `5ECD587D5F0C5E3D6F00067AA3F125FD410D1F8ED24F1C05A1BF4F677EA14330` |
| `b1db_exec.cpp` / `.exe` | DB 查询/写入小工具 | `BBE202BEA2AB09D776E4893A8BEC2F60CAAB5731B15088D16FA8C508E75EF7B6` / `0C8199A32686DAD67D90B499CA78651A5C0985C7B5E59722EA4B607D617D208B` |
| `b1lock.cpp` / `.exe` | 「拒绝删除共享」句柄持有器 | `5B1DD319929FC9C6DB1B3EEF33B5C97029827A8DDE31D2AAC7217D12FCF3417D` / `20442FDF23FDA1B56D182B4823CECAD0557D2B604EEB617A092522CB2D29E597` |

### 7.3 隔离实例（全部已停止，目录随临时根一起删除）

| 实例 | 目录 | 端口 | 用途 |
|---|---|---|---|
| 主实例 | `%TEMP%\b1verify\srv` | 18099 | 主流程、1-5、H-① HTTP、H-②、1-4、N2、伴-2'、C7-8、CB1-3/4 |
| L1 | `%TEMP%\b1verify\lockA` | 18101 | 1-6 普通登录锁定（6 个不同 XFF） |
| L2 | `%TEMP%\b1verify\lockB` | 18102 | 1-6 家长端登录锁定 |
| L3 | `%TEMP%\b1verify\lockC` | 18103 | C2-6 50 并发错误登录 |
| 伴-3 对照 | `%TEMP%\b1verify\oldschema-ok` | 18105 | 旧 schema 正常重建路径 |
| 伴-3 实验 | `%TEMP%\b1verify\oldschema` | 18106 | 删不掉 → fail-fast |

---

## 8. 清理与只读性复核

```
$ git -c core.quotepath=false status --porcelain
 M auth.h
 M config.h
 M logger.h
 M main.cpp
 M models.cpp
 M models.h
 M routes_admin.cpp
 M routes_parent.cpp
 M routes_public.cpp
 M routes_student.cpp
 M routes_teacher.cpp
 M sqlite_wrapper.h
?? .agent-teams/
?? docs/audit/BATCH1_CONTRACT.md
?? docs/audit/BATCH1_DESIGN_DECISIONS.md
?? docs/audit/evidence/B1_T2_CONTRACT_VERIFICATION.md
?? docs/audit/evidence/B1_VERIFICATION.md      ← 本文件（唯一新增）
```

- 仍是**同样 12 个 ` M`**，未提交、未 stash、未 checkout；未修改任何源码。
- 仓库根**未新增** `.o`/`.exe`；`tests/` 无残留 exe；仓库根既存产物未被触碰（时间戳仍为 2026/9/19）。
- 我启动的全部服务进程已终止（`Get-Process b1_server,b1lock,server` → 空）。
- `%TEMP%\b1verify\` 整目录已删除。

### 未竟事项（明确声明，避免误读为「已验证通过」）

- §5 的 12 条「未独立验证 / 无法端到端复现」项**不得**被读作通过。
- §4.4 的 thread_local 崩溃与 §4.6 的加锁顺序倒置，前者是最小复现、后者是代码级判定，均**未**在真实服务上端到端触发。
- §3.9 的 H-① HTTP 判据被判定为**不可判定**；其函数级后果已复现，端点级不可达。
