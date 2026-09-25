# 批次 1 修复契约（BATCH1_CONTRACT）

- 任务：team `audit-batch0-remediation` 任务 **t35**（requirements-architect，attempt 1）
- 产物性质：**契约与判据文档**。本文件不修改任何源码、配置或构建产物。
- 权威依据（冲突时以序号靠前者为准）：
  1. 本文件 §3 的**实测 file:line**（我本次 real-tool 读取的结果）
  2. `docs/audit/VERIFICATION.md`
  3. `docs/audit/CODE_QUALITY_REVIEW.md`（§4.1 并发与数据库、§6 批次 1 路线图、§7.1 勘误表）
  4. `docs/audit/CODE_QUALITY_SUMMARY.md`
  5. `docs/audit/BATCH0_CLOSURE.md`（批次 0 交接）
- 配套决策文档：**`docs/audit/BATCH1_DESIGN_DECISIONS.md`**（5 项决策的候选/选定/理由/否决/对下游约束）。二者编号一一对应引用。
- 行号口径：本文件每个 `文件:行号` 都由我在**批次 0 全部落地并提交之后**的当前工作区用 read/grep 打开核对。与审计文档不一致处见 §8，**一律以本文件实测为准**。
- 基线（t35 执行实测）：`git status --porcelain` 仅 `?? .agent-teams/`（批次 0 的产物**已提交**，故源码行号相对批次 0 契约已发生漂移，见 §8）。

## 0. 术语与实测基线

| 术语 | 定义 |
|---|---|
| `主进程编译` | `gcc -c sqlite3.c -o sqlite3.o -O2` + 9 个 `g++ -c <src>.cpp -o <src>.o -std=c++17 -O2 -I.` + 链接 `g++ -o server.exe main.o models.o logger.o routes_static.o routes_public.o routes_admin.o routes_teacher.o routes_student.o routes_parent.o sqlite3.o -lws2_32 -lwsock32 -std=c++17 -O2 -static -static-libgcc -static-libstdc++ -lwinpthread`（与 `.github/workflows/ci.yml` 同源） |
| `无锁全局容器` | 见 §3.2 的六个 + 五个索引：`users` / `points_records` / `roles` / `permissions` / `role_permissions`（内存向量）、`login_attempts()`（函数内 static map）、`user_id_map` / `user_username_map` / `role_id_map` / `permission_id_map` / `role_permission_map`（索引 map） |
| `独立验证` | 由 verifier 在**不引用实现者结论**的前提下重跑命令或自建最小复现，见 §7.2 分类清单 |

实测基线（本机 pwsh）：

```powershell
# 自研源码中的互斥设施（排除 httplib.h/json.hpp/sqlite3.h/sqlite3ext.h）
Select-String -Path *.h -Pattern 'mutex|lock_guard|unique_lock|shared_lock|std::atomic|thread_local'
#  → logger.h:12 (#include <mutex>) / logger.h:61 (lock_guard) / logger.h:89 (static std::mutex)
Select-String -Path *.cpp -Pattern 'mutex|lock_guard|unique_lock|shared_lock|std::atomic|thread_local'
#  → logger.cpp:7 (std::mutex Logger::mutex_)
# 结论：全项目除 Logger 外零互斥设施；且 Logger 自己也保护不全（见 3.13/B30）
```

---

## 1. 范围：7 个子项 + 2 条批次 0 交接缺陷

审计 `CODE_QUALITY_REVIEW.md` §6「批次 1」列 7 条；批次 0 结项（`BATCH0_CLOSURE.md`）交出 2 条未闭合缺陷。本契约共覆盖 **9 项**，并新增 4 项**在同批必须一并处置、否则修复可被绕过**的伴生项（§3.10–3.13）。

| # | 条目 | 审计严重度 | 实测主位置 | 归属任务 |
|---|---|---|---|---|
| 1-1 | **连接策略决策**（B7/B23 的根） | 决策 | `main.cpp:22`、`main.cpp:463`、`sqlite_wrapper.h:19-32` | **t38** |
| 1-2 | **B3 + B5 + N2**（锁定 map 无锁 / 全局容器无锁+裸指针 / 并发建号重复 ID） | high×2 + high | `auth.h:111-114`；`models.cpp:14,23,26,34,50,178-182,262-277`；`models.cpp:125-173` | **t38** |
| 1-3 | **B6 彻底版**（索引改存稳定键） | high | `models.cpp:178-182,210-224,240-242,256-259,262-277` | **t39** |
| 1-4 | **B8**（兑换非原子 + `execute_bind` 无法区分 0 行） | high | `routes_public.cpp:325-412`（`:375-397`）；`sqlite_wrapper.h:127-148` | **t38** |
| 1-5 | **B9**（`/api/teacher/*` 写接口无对象级归属校验） | high | `routes_teacher.cpp:103,169,235,274,464,781,813,1036` | **t39** |
| 1-6 | **B4**（XFF 可伪造 + 家长端无锁定） | high | `auth.h:141-154`；`routes_public.cpp:29-30,39,54,60`；`routes_parent.cpp:49-77` | **t40** |
| 1-7 | **B10 + B14 + B12** | high + medium×2 | `main.cpp:442-449`；`sqlite_wrapper.h:53-92,151-188` | **t40** |
| H-① | **交接缺陷①**：删除权限/角色后不重建索引 | （批次 0 未闭合） | `routes_admin.cpp:1147`、`routes_admin.cpp:1183`（另 `:1046`、`:1111`、`:1599`、`:1629`、`:1645` 新增不重建） | **t38**（索引部分）+ **t39**（稳定键部分） |
| H-② | **交接缺陷②**：`execute_bind` 无法区分 0 行受影响 | （批次 0 未闭合） | `sqlite_wrapper.h:127-148`（`:145-147`）；受害调用点 `routes_teacher.cpp:797-803`、`routes_teacher.cpp:826-831`、`routes_public.cpp:385-388` | **t38** |
| 伴-1 | **B7 残留**：单连接上 `BEGIN/COMMIT/ROLLBACK`（只有 1-1 选定方案能解） | high | `routes_admin.cpp:1519,1734,1755` | **t38** |
| 伴-2 | **B30**：日志轮转在加锁之前（TOCTOU） | low | `logger.h:41-62`（`rotate_if_needed()` 在 `:59` 调用，锁在 `:61`） | **t40** |
| 伴-3 | **B27 变体**：删库重建路径未 fail-fast（批次 0 已修 `db.open` 失败，此路径仍静默继续） | low | `main.cpp:193-202` | **t40** |
| 伴-4 | **N2 兜底缺失**：`users.id` 无 DB 侧唯一约束兜底（`INSERT OR REPLACE` 掩盖冲突） | high（N2 的一半） | `models.cpp:89-98` | **t38** |

> **为什么把伴-1/伴-2/伴-3/伴-4 放进批次 1 而不是推迟**：前三者都是「修了 1-1/1-7 但不修它，修复可被绕过或产生新缺陷」；伴-4 是 N2 的另一半（只修 ID 生成不修写入语义，「静默覆盖」仍在）。详述见各条目「边界与反例」。

---

## 2. 批次 1 的 DAG（任务 id + 依赖方向）

```
t35 (本契约 + 决策)  ──► t36 (独立复核契约绑定 + 复现修复前失败) ──┐
                                                                  │
t35 ──────────────────────────────────────────────────────────────┴──► t38
                                                                        │
   t38 = 关键路径：1-1 连接模型 + 1-2(B3+B5+N2) 容器与索引同步
                    + 1-3 索引稳定键（基础） + 1-4(B8) 兑换原子性与行数
                    + H-① 索引重建 + H-② 行数通道 + 伴-1/伴-4
                                                                        │
                                                          ┌─────────────┴─────────────┐
                                                          ▼                           ▼
                                                        t39                         t40
   t39 = 1-5(B9 归属门禁) + 1-3(B6) 稳定键落地       t40 = 1-6(B4 XFF+家长锁定)
         + H-① 的角色/权限索引重建收口                     + 1-7(B10/B14/B12) + 伴-2/B30 + 伴-3/B27
                                                          │
                                                          └──────────► 批次 1 独立验证 / 评审 / 结项
```

**依赖方向（强制）**

| 边 | 方向 | 理由（不可颠倒的因果） |
|---|---|---|
| t35 → t36 | t36 依赖 t35 | t36 复核的对象就是本契约的绑定与判据 |
| t35 → t38 | 强 | 1-1 未定论时，1-2/1-3/1-4 的做法互斥（见决策①「为何单连接仍错误」） |
| t38 → t39 | 强 | 1-3 的稳定键必须建立在 1-2 已定下的加锁协议与容器形态之上；否则两人会在 `models.h` 上引入第二套索引语义 |
| t38 → t40 | 强 | 1-7 的 `set_error_handler` 与「错误通道」必须知道连接/事务模型（否则错误码可能被跨线程误读） |
| t39 → t40 | **弱依赖但不建议并行** | 两者都改 `auth.h`（t39 加 `require_teacher_owns_student`，t40 改 `login_attempts`/`login_client_key`）。**同文件改动必须串行**，见 §6.3 |
| t38 ≡（合并）t37 | — | t37 因 inScope 与 t38 重叠已 cancelled；**严禁再并行引入第二套事务/连接机制** |

**DAG 的硬约束（对排期）**：`t38` 是**串行关键路径**，不得再拆；`t39` 与 `t40` 可在 `t38` 完成后并行开工，但**都必须以「`t38` 已完成的 `models.h`/`auth.h` 快照」为基线，且不得同时编辑同一文件**。

---

## 3. 逐条契约

> 每条固定六段：**实测位置 / 根因 / 修复动作 / 可机械判定验收判据 / 边界与反例 / 归属任务**。
> 判据前缀 `C1-…`（连接）、`C2-…`（B3/B5/N2）、`C3-…`（B6 彻底版）、`C4-…`（B8）、`C5-…`（B9）、`C6-…`（B4）、`C7-…`（B10/B14/B12）、`CH-…`（交接）、`CB-…`（伴生）。

### 3.1 【1-1】连接策略

**实测位置**

- `main.cpp:22` `SqliteDb db;`（全局唯一实例）
- `sqlite_wrapper.h:19-32` `open()`：`sqlite3_open` + `sqlite3_busy_timeout(db_,5000)`（`:27`）+ `execute("PRAGMA journal_mode=WAL;")`（`:29`）
- `main.cpp:463` `svr.new_task_queue = [&]() { return new httplib::ThreadPool(g_config.thread_count); };`
- `config.json` `server.thread_count = 8`

**根因（框架层取证，我读了 vendored 源码，不是推理）**

1. `httplib.h:3443-3448`：`listen_internal()` 里 `std::unique_ptr<TaskQueue> task_queue(new_task_queue());` —— **整个进程生命周期只创建一次队列**。
2. `httplib.h:371-379`：`ThreadPool(n)` 构造时 `emplace_back(worker(*this))` 创建 **n 个长驻线程**；`httplib.h:408-426` 的 `worker::operator()` 是一个 `for(;;)` 循环，`fn()` 在**同一个线程**上反复执行。
3. `httplib.h:3481-3483`：所有 `process_and_close_socket(sock)` 都只 `enqueue` 到这个池；Server 侧**不另起线程、不 detach**（`grep std::thread` 在 Server 路径只有 `ThreadPool` 的成员 `threads_`）。
4. **结论**：请求 handler 100% 运行在**固定的 8 个长驻 worker 线程**上，而这 8 个线程共享 `main.cpp:22` 的**同一条 `sqlite3*`**。

**为何「有 WAL + busy_timeout 就够了」是错的（可判定论证）**

| # | 事实 | 实测依据 |
|---|---|---|
| A | WAL 只做到「读写不互斥」，**不含任何应用层临界区** | `PRAGMA journal_mode=WAL` 是数据库属性；`sqlite_wrapper.h:29` |
| B | `busy_timeout` 只把 `SQLITE_BUSY` 变成**阻塞重试**，不把多个语句变成原子 | `sqlite_wrapper.h:27`；它无法让 `SELECT` 与后续 `UPDATE` 之间有隔离 |
| C | 本项目的缺陷全是**多步序列**，不是单条语句：读-判断-写（`routes_public.cpp:375→397`）、`erase`+索引维护（两处 `erase` 各 2 步）、`BEGIN/COMMIT` 跨调用 | 见 3.4 / 3.2 |
| D | 单连接上 `BEGIN`/`COMMIT` 是**连接级**的：其他线程的语句会被卷入同一事务 | `routes_admin.cpp:1519` 与 `:1734` 之间隔着**整个导入循环**，期间任何其他请求的写入都进了这个事务；`:1755` 的 `ROLLBACK` 会把它们一起回滚 |
| E | `sqlite3_changes()` / `sqlite3_last_insert_rowid()` 是**连接级**值，任何线程都会覆盖它 | `sqlite_wrapper.h:102-108` 已有 `update()/insert()` 在用它们；H-② 的行数通道也依赖它 → 单连接下「受影响行数」可被别的线程污染 |
| F | `SQLITE_THREADSAFE` 默认 1 → `sqlite3_open`（不传 FULLMUTEX）拿到的连接**带一把递归互斥**，这**恰好掩盖**了 A–E | `sqlite3.c:14050`（`SQLITE_THREADSAFE 1`）、`sqlite3.c:22806`（`bFullMutex = SQLITE_THREADSAFE==1`）、`sqlite3.c:181367-181371`（未传 flag → `isThreadsafe = bFullMutex`）、`sqlite3.c:181405-181418`（分配递归互斥）。**它只保证单次 API 调用不撕裂，不提供任何跨调用的临界区** |
| G | 顺序化代价：8 个 worker 的全部 DB 调用被这一把递归锁**串行化**，写锁冲突时最多阻塞 `busy_timeout = 5 秒` | `sqlite_wrapper.h:27` + B |

**因此**：`WAL + busy_timeout` 解决的是**数据库文件级**的锁竞争（磁盘层面），本项目的问题在**应用级临界区缺失**（逻辑层面）。两者不是同一层，不能互相替代。

**修复动作（唯一选定方案 = 每线程一条连接，见决策①）**

1. `SqliteDb` 保留为一个**门面类**，但把「当前线程的那条 `sqlite3*`」从数据成员改为**线程局部惰性获取**：
   - 新增线程局部持有者（唯一实例表 / `thread_local` 持有者，二者语义等价，见决策① 方案 A 与 A′）；
   - **所有既有 `db.query(...)` / `db.execute_bind(...)` 调用点保持源码不变**（约 600+ 处），门面内部转发到当前线程的连接。这是本方案相对「把全局 `db` 改成 `thread_local db`」的核心优势：不触碰任何调用点。
2. 每个 worker 线程**首次**使用 DB 时惰性建连：沿用 `sqlite_wrapper.h:19-32` 的同一套初始化（`sqlite3_open` + `busy_timeout(5000)` + `PRAGMA journal_mode=WAL`）。`busy_timeout` 是**每连接**设置，故必须在每条新连接上重设（现有代码天然满足，因为初始化集中在该函数）。
3. 建连失败**不得静默**：写 `Logger::error`；请求路径上的失败必须让该请求 fail-closed（返回 500/业务码），**不得**返回空结果冒充「无数据」。
4. `main.cpp:190-215` 的启动期建连（建表/种子/加载）继续在**主线程**使用同一门面；`:204-215` 的 fail-fast（`return 1`）保持不变。
5. `main.cpp:463` 的线程池创建**必须在**所有启动期 DB 初始化（`:190-440`）之后 —— 现状已满足，作为回归守卫写入判据。
6. 连接**不关闭**（存活到线程结束），请求代码不得持有跨线程传递的 `sqlite3*` 或 `User*`（见 3.2 判据 C2-6）。

**验收判据**

- C1-1：`grep -n 'sqlite3\*' *.h *.cpp`（排除 vendored）→ `sqlite3*` 只出现在 `sqlite_wrapper.h` 内部；没有任何自研业务文件直接持有 `sqlite3*`。
- C1-2：`grep -n 'thread_local\|sqlite3_open' sqlite_wrapper.h` → 存在线程局部获取路径，且 `sqlite3_open` 仍在**同一处**初始化（`busy_timeout` 与 WAL pragma 与其同函数）。
- C1-3（**运行期证据，必做**）：一次运行中对 8 个 worker 各触发至少一次 DB 请求（并发压测），进程内 `sqlite3_open` 的**成功次数 = worker 线程数 + 1**（主线程 + 8），由实现者打印计数（临时计数日志或 `std::cout` 计数）证明**不是** 1 次。
- C1-4：**B7 消失**：`routes_admin.cpp` 的导入端点在并发请求下不再出现「其他线程已返回 200 的写入被 ROLLBACK 回滚」。可判定形式：并发跑「导入 + 单条写入」，单条写入返回 200 后，其行必须在 DB 中真实存在（`SELECT` 校验）；修复前必须能复现丢失（作为基线证据）。
- C1-5：主进程编译 exit 0；`tests/run_tests.ps1` 全绿。
- C1-6：`config.json` 的 `server.thread_count` 保持 8（不得靠调小线程数「规避」）。

**边界与反例**

- **反例（不算修复）**：给全局容器加锁但保留单连接 → B7（连接级事务串扰）与「受影响行数被污染」仍在，C1-4 会失败。
- **反例**：把 `db` 改成 `thread_local` 全局变量（`thread_local SqliteDb db;`）—— 看似一行改动，但 `main.cpp` 的启动期初始化发生在主线程，worker 线程拿到的是**未初始化**的实例；且 `extern SqliteDb db;`（`models.h:12`）与跨 TU 的 thread_local 初始化顺序无保证。**契约要求门面方案**（决策① 方案 A），并禁止直接把 `db` 声明为 `thread_local`。
- **反例**：在每条连接上用不同的 `PRAGMA busy_timeout` 值或漏掉 WAL → 与单连接期行为不一致，属回归。
- **反例**：连接数随请求增长（每请求建连不回收）→ 违反「每线程一条」的上界；必须给出 C1-3 的计数证据。
- 连接池方案（决策① 方案 B）被否的理由见决策文档；若实现者改用方案 B，**必须重新走 t35 的决策流程**，不得自行改判。
- 本项**不**解决 B5 的悬垂指针（那是 1-2）与 N2（1-2）；也不解决 `busy_timeout = 5s` 的阻塞上限（属调优，批次 2/3）。

**归属**：**t38**（关键路径首项）。独立验证：C1-3/C1-4 **必须**由独立验证任务重跑。

---

### 3.2 【1-2】B3 + B5 + N2：无锁全局容器、裸指针悬垂、并发重复 ID

**实测位置**

```cpp
// models.cpp:14           vector<PointsRecord> points_records;          ← 无锁全局
// models.cpp:23           vector<User> users;                          ← 无锁全局（批次 0 已改为空容器，由 DB 填充）
// models.cpp:26           vector<Role> roles = {...};
// models.cpp:34           vector<Permission> permissions = {...};
// models.cpp:50           vector<RolePermission> role_permissions = {...};
// models.cpp:178-182      五个无锁索引 map（user_id_map / user_username_map /
//                          role_id_map / permission_id_map / role_permission_map）
// models.cpp:125-173      generate_user_id()：遍历内存 users 求 max_seq，返回 max_seq+1
// models.cpp:89-98        save_user_to_db()：INSERT OR REPLACE（掩盖主键冲突）
// models.cpp:262-277      find_user_by_id / find_user_by_username 返回 &users[i] 裸指针
// auth.h:111-114          login_attempts()：函数级 static std::map，无锁
// auth.h:117-139          login_can_try / login_record_fail / login_record_success
```

`login_attempts` 的调用点（全项目仅 5 处，全在登录 handler 内）：`routes_public.cpp:29,30,39,54,60`。

**根因**

1. **B3**：`static std::map` 被 8 线程无锁读写：`m[key]`（`auth.h:128`）插入会 rehash/改树结构，与并发 `find`（`:119`）/`erase`（`:138`）并发 → UB；`a.fails++`（`:129`）非原子 → 阈值失效。
2. **B5**：`find_user_by_id()` 返回 `&users[idx]`；任何 `users.push_back`（`routes_admin.cpp:571,683`、`routes_teacher.cpp:143,953`、`routes_public.cpp:174`）触发扩容后，先前取到的指针**悬垂**，而调用点会继续解引用写（如 `routes_public.cpp:396` 的 `user->points -= cost`）；索引 map 的 rehash 同样竞态。
3. **N2**：`generate_user_id()`（`models.cpp:125-173`）用**内存扫描**求 `max_seq+1`，无锁 → 两个并发建号得到同名 id；`save_user_to_db()` 用 `INSERT OR REPLACE`（`models.cpp:91`）→ **后写者静默覆盖先写者**，先建用户消失。

**修复动作**

1. **一把全局容器互斥**（`std::mutex`，非 `shared_mutex`；理由见决策②）统一保护上述**六个容器 + 五个索引 map**。
2. 新增/调整对外的**受锁 API**（`models.h` 声明）：
   - `find_user_by_id` / `find_user_by_username`：**返回值拷贝**（禁止再返回 `&users[i]`）；保留旧签名会让悬垂无法从类型上消除。
   - 需要就地改写的路径，改为「受锁的读-改-写」**一次性**接口，例如
     `bool update_user_points_by_delta(const std::string& user_id, long long delta)`，
     内部在**同一临界区**内完成 `users` 内存更新 + 索引一致性维护；
   - 建号改为「分配 ID + 插入」的**单一临界区**接口。
3. **N2 的两半都要修**：
   - (a) 序号分配（`models.cpp:125-173`）进临界区；
   - (b) `models.cpp:89-98` 的写入语义：`INSERT OR REPLACE` → **`INSERT`**，并把「唯一冲突」当作**真实失败**上报（不再替换整行）。配合 `users.id TEXT PRIMARY KEY`（`main.cpp:220`）与 `username TEXT UNIQUE`（`main.cpp:221`）让 DB 成为最终裁判。
   - (c) 可选但推荐：`save_user_to_db` 的返回值区分「插入成功 / 唯一冲突 / 其他错误」，供调用方返回 409。
4. **禁止跨锁传递裸指针/引用**：任何返回 `User*` 的函数都必须在 `models.h` 中标注「指针仅在当前临界区内有效」，而业务代码**不得**在解锁后使用。最稳做法是让所有对外接口返回值类型（`User` / `std::optional<User>`）。
5. `login_attempts()` 的并发保护见 3.6（B4），本项只要求「同一套互斥协议」；**不得**引入第二把独立锁（决策②）。

**验收判据**

- C2-1：`grep -n 'User\* find_user_by_id\|User\* find_user_by_username' models.h` → **零命中**（签名已改为值语义）。
- C2-2：`grep -n '&users\[' *.cpp *.h`（排除 vendored）→ **零命中**。
- C2-3：`grep -n 'std::mutex\|std::lock_guard\|std::unique_lock' models.cpp` → 存在**唯一**一把锁的定义与使用；`grep -c 'std::shared_mutex\|std::shared_lock' models.cpp` → **0**（与决策②一致）。
- C2-4（**运行期证据，必做**）：并发建号不复现重复 ID。最小复现方式（temp 文件需清理或标注）：直接驱动内存容器的临时程序，或对 `POST /api/admin/users` 并发打 N 次，断言 `SELECT id, COUNT(*) FROM users GROUP BY id HAVING COUNT(*)>1` → **0 行**，且用户总数 = 种子数 + N。
- C2-5（**运行期证据，必做**）：**修复前基线必须能复现**重复 ID 或被覆盖（作为「缺陷真实存在」的证据）；修复后同命令必须正常。若修复前无法复现，**必须**改用最小程序直接调用 `generate_user_id()` + `save_user_to_db()` 的两线程竞态复现，并给出原始输出，**不得**以「复现不了」为由跳过。
- C2-6：并发登录不再 UB/map 崩溃。可判定形式：并发打 `POST /api/auth/login`（含错误口令）N 次，进程不得崩、不得出现未捕获异常；并断言「同一 key 的失败计数」在修复后能被阈值正确锁定（附实测的 429 出现）。
- C2-7：主进程编译 exit 0；`tests/run_tests.ps1` 全绿。

**边界与反例**

- **反例（严重）**：只给 `login_attempts` 加锁，不给容器加锁 → B5 悬垂仍在；或在 `find_user_by_id` 内部加锁后**仍返回裸指针** → 锁释放后指针即失效，等于没修。
- **反例**：`INSERT OR REPLACE` 与并发 ID 只修一个 → 另一半仍在（这正是「N2 兜底缺失」被单独列为伴-4 的原因）。
- **反例**：把锁加在 `users.push_back` 上，但 `update_user_index()` 在锁外调用 → 另一个线程可观察到「元素已在向量、索引尚未更新」的**中间态** → 必须把「写入 + 索引维护」放进**同一临界区**（决策② 的强制约束）。
- **反例**：给 `points_records` 的 `new_record_id = points_records.size()+1`（`routes_teacher.cpp:332`）加锁，却不改「ID 由内存序号生成」的语义 → DB 侧自增 id 与返回给前端的 id 仍不一致（B18 残留）。本批只要求**在锁内**生成并**用 DB 实际 id 回给前端**（见 3.12 伴-2'）；彻底回载属批次 2。
- 索引的**稳定键**改造不在本项（属 1-3/t39）；本项只要求「下标语义在锁内保持一致 + 越界防御」（见 3.3）。
- 事务隔离不在本项（属 1-1/t38 + 3.11 伴-1）。

**归属**：**t38**。独立验证：C2-4/C2-5/C2-6 **必须**由独立验证任务重跑（C2-1/C2-2/C2-3 属符号检查，见 §7.2）。

---

### 3.3 【1-3】B6 彻底版：索引改存稳定键

**实测位置**

```cpp
// models.cpp:178-182   五个索引 map 的定义（当前：user_* 存 users 向量下标）
// models.cpp:187-194   clear_and_rebuild_user_indexes()（批次 0 引入，static，两条路径共用）
// models.cpp:210-224   init_indexes()（T24 后幂等：五个容器先 clear 再重建）
// models.cpp:240-242   rebuild_user_indexes()
// models.cpp:245-253   update_user_index()：遍历 users 线性找下标
// models.cpp:256-259   remove_user_index()
// models.cpp:262-277   find_user_by_id / find_user_by_username：下标 + 越界防御
// models.cpp:280-300   check_permission_optimized()：user → role → permission 的嵌套查找
```

批次 0 的临时修复（t9/t24/t32）已经把「erase 后不重建」和「init_indexes 非幂等」堵上；**残留问题**是索引**仍存下标**：任何改变 `users` 长度的操作（包括**别人的** `push_back`）都会让所有下标失效，只能靠「每次变更后全量重建」把窗口压到最小 —— 这是 O(n) 重建 + 一个必然存在的中间态。

**根因**：`user_id_map["X"] = i` 里的 `i` 是**易变的位置**，而 `users` 是**会被插入/删除的连续容器**。位置与身份绑定，必然错位。

**修复动作（唯一选定方案：稳定键 = 用户 id 字符串，见决策③）**

1. `user_id_map` / `user_username_map` 的 value 由「下标」改为**稳定键**：
   - 首选形态：`std::unordered_map<std::string, std::string>`（id → id，冗余但语义直白）**或** `std::unordered_set<std::string>`（仅判存在）+ 一个 `std::unordered_map<std::string, User>` 主存储；
   - 次选形态（改动更小）：保留 `users` 向量，但 `user_id_map` 存 `std::string user_id`，查找时再按 id 线性/二级索引定位；
   - **禁止**继续存下标。
2. **主存储建议一并改为 `std::unordered_map<std::string, User>` 或 `std::list<User>`**：前者让「按 id 取值」O(1) 且**天然免疫扩容**（这正是 B5 悬垂的根因），后者让引用/迭代器稳定。若实现者选择 `std::deque<User>` 或 `std::vector<std::unique_ptr<User>>` 也可，**但必须给出「为何引用在插入后仍有效」的一句话论证**。
3. **陈旧/越界下标与重复键的处理**（必须逐条交代）：
   - 陈旧/越界：稳定键下**不再存在下标概念**；若实现者保留任何下标，必须保留 `models.cpp:264`/`:274` 的越界防御 `it->second < users.size()` **并追加一项身份校验**：`users[it->second].id == user_id`（防「越界未触发但错位」）。
   - 重复键：`user_username_map` 的键是 `username`，DB 侧有 `UNIQUE`（`main.cpp:221`），但内存路径可能先插入重复项。要求：`update_user_index` 必须在**唯一性冲突**时给出可判定行为（返回 false / 记日志），**不得**静默用后者覆盖前者。
4. `init_indexes()` 的**幂等性**要求（沿用 T24 的做法，不得回退）：五个容器**全部 `clear()` 再重建**；连续调用两次，结果必须与调用一次完全一致（可机械断言：两次调用后 `size()` 与内容均不变）。
5. **H-① 收口（与 3.9 联动）**：`roles` / `permissions` / `role_permissions` 的**所有变更点**之后必须重建对应索引：
   - 删除：`routes_admin.cpp:1147`（`permissions.erase`）、`routes_admin.cpp:1183`（`roles.erase`）
   - 新增：`routes_admin.cpp:1046`（`roles.push_back`）、`routes_admin.cpp:1111`（`permissions.push_back`）
   - 导入：`routes_admin.cpp:1599`、`:1629`、`:1645`（`init_indexes()` 在 `:1736` 收口，**保留**）
   - 提供 `rebuild_role_indexes()` / `rebuild_permission_indexes()`（或等价的一次性 `rebuild_all_indexes()`），在**同一临界区**内调用。
6. **角色/权限索引的越界与陈旧**：`role_id_map` / `permission_id_map` 存的是**值拷贝**（`models.cpp:180-181` 的 `Role` / `Permission`），删除后不重建 → 陈旧条目仍在（`check_permission_optimized` 的 `:287-297` 会用到 `role_permission_map` 与 `permission_id_map`）→ 必须重建，且 `role_permission_map` 不得残留已删角色的权限列表。

**验收判据**

- C3-1：`grep -n 'size_t' models.h` → 索引 map 的 value 类型**不再**是 `size_t` 下标；`grep -n 'user_id_map\[' models.cpp` 的赋值右侧**不是** `i` 之类的下标变量。
- C3-2：`grep -n '&users\[' *.cpp *.h` → **零命中**（与 C2-2 同源，稳定键后自然满足）。
- C3-3（**运行期证据，必做**）：构造 ≥3 个用户，删除中间一个后逐个按 id 查询剩余用户，**取回的 `id` 必须等于查询键**；并在**修复前**用同一最小程序复现「取到错误用户 / nullptr」（作为基线证据）。
- C3-4（**幂等性，必做**）：对 `init_indexes()` 连续调用 2 次，断言第二次调用后五个索引容器的 `size()` 与全量内容（排序后 dump）与第一次调用后**完全相同**；且 `role_permission_map` 中任一角色的权限**数量不翻倍**（T24 的回归守卫）。
- C3-5：`grep -n 'rebuild_user_indexes\|rebuild_role_indexes\|rebuild_permission_indexes\|init_indexes' *.cpp` → 5 个用户变更点（`routes_admin.cpp:571,683,886`、`routes_teacher.cpp:143,258`、`routes_public.cpp:174`）+ 5 个角色/权限变更点（`routes_admin.cpp:1046,1111,1147,1183` + `:1736`）**全部**在变更后处于「索引已同步」状态。**逐点列清单**是验收形式（不得只写「已统一处理」）。
- C3-6：**H-① 的可判定复现**：先 `POST /api/admin/permissions` 新增一个 id>12 的权限，再 `DELETE /api/admin/permissions` 删除它，随后调用一个依赖 `check_permission_optimized` 的端点，断言**不因陈旧条目而返回错误授权**；并断言 `role_id_map`/`permission_id_map` 中已不含被删项（可用临时只读探针打印）。
- C3-7：主进程编译 exit 0；`tests/run_tests.ps1` 全绿。

**边界与反例**

- **反例（最易踩）**：把索引改成稳定键，但**保留返回 `User*`** → 悬垂仍在（C2-1/C2-2 会失败）。
- **反例**：删除后调用 `init_indexes()` 之外**不**重建 → 与批次 0 的 B6 同型，C3-6 会失败。
- **反例**：`init_indexes()` 被改成「只 clear 用户部分」→ 回退 T24 的幂等修复，C3-4 会失败。
- **反例**：为了「稳定」把 `users` 换成 `std::map<std::string, User>`，但**不改** `roles.back().id + 1`（`routes_admin.cpp:1045`）这类「靠容器顺序推 id」的代码 → 顺序语义被破坏。要求实现者逐一核对所有「依赖容器顺序」的写法（`roles.back()`、`points_records.size()+1`、`users.size()+1`）并给出替换方案。
- 本项**不**解决：`points_records` 从不回载（B18/N5，批次 2）、角色/权限的 DB 持久化（B16，批次 2）、`/api/teacher/*` 的归属校验（B9，1-5）。
- 本项**不得**引入 `shared_mutex`（决策②：统一一把互斥锁）。

**归属**：**t39**（基础形态由 t38 提供，见 DAG 说明）。独立验证：C3-3/C3-4/C3-6 **必须**由独立验证任务重跑。

---

### 3.4 【1-4】B8：兑换非原子 + 行数不可判定

**实测位置**

```cpp
// routes_public.cpp:325   svr.Post("/api/mall/redeem", ...)
// routes_public.cpp:340   User* user = find_user_by_id(user_id);      ← 裸指针（B5）
// routes_public.cpp:358-360  SELECT cost, stock FROM mall_items ...   ← 读
// routes_public.cpp:375   if (user->points < cost)                    ← 判断（内存值）
// routes_public.cpp:381-388  stock_ok / UPDATE stock = stock - 1      ← 写（结果未检查）
// routes_public.cpp:396-397  user->points -= cost; update_user_points_in_db(...)  ← 写（绝对值覆盖）
// routes_public.cpp:398-400  INSERT redemption_records
// sqlite_wrapper.h:127-148   execute_bind：return rc == SQLITE_DONE;   ← 0 行受影响也 true
// sqlite_wrapper.h:102-108   update()/insert()：已在用 sqlite3_changes / last_insert_rowid（连接级）
```

**根因（两个独立缺陷叠加）**

1. **读-判断-写非原子**：`SELECT`（`:358`）与 `UPDATE`（`:397`）之间无任何隔离；8 线程下同一学生的两个兑换请求可同时通过 `:375` 的余额判断 → **双花**。库存同理：`:384` 的 `stock > 0` 与 `:386` 的减法之间可被另一线程插空 → 负库存或**无故扣库存**。
2. **行数不可判定（H-②）**：`execute_bind`（`:145-147`）只判 `SQLITE_DONE`。`UPDATE ... WHERE id=? AND stock>0` 命中 0 行时 `sqlite3_step` 仍返回 `SQLITE_DONE` → `true`。因此：
   - `routes_public.cpp:385-388` 的扣库存**结果被丢弃**（`stock_ok` 只看 `:381-383` 的**陈旧**判断）；
   - `routes_teacher.cpp:797-803`（修改评价）与 `:826-831`（删除评价）的 `else { code 404 }` 分支**恒不可达** —— 这是同一 H-② 的第二个真实受害者。

**修复动作**

1. **`sqlite_wrapper.h` 增加「受影响行数」通道**（H-② 的唯一解法）：
   - 保留 `bool execute_bind(const std::string&, const std::vector<Bind>& = {})` 的**签名与语义**（`sqlite3_step == SQLITE_DONE`），以零回归方式保住既有 600+ 调用点；
   - **新增** `int execute_bind_affected(const std::string& sql, const std::vector<Bind>& params, int& affected_rows)`：
     - 返回 `0` = 语句成功执行（`SQLITE_DONE`）；返回 `非 0` = 失败（`SQLITE_ERROR` / prepare 失败）；
     - 成功时 `affected_rows = sqlite3_changes64(db_)`（**必须紧接 `sqlite3_step` 之后、且在同一线程的连接上**才不被其他线程污染 —— 这正是 1-1 的收益）。
2. **兑换改为单条原子 SQL**（判决依据 = 受影响行数）：
   - 扣积分：`UPDATE users SET points = points - ? WHERE id = ? AND points >= ?` → 继续条件 `affected_rows == 1`；否则返回「积分不足」。
   - 扣库存：`UPDATE mall_items SET stock = stock - 1 WHERE id = ? AND stock > 0` → 继续条件 `affected_rows == 1`；否则返回「库存不足」。
   - `stock = -1` 的「无限库存」语义（`main.cpp` 的 `mall_items.stock INTEGER DEFAULT -1`）必须保留：SQL 需写成 `WHERE id = ? AND (stock > 0 OR stock = -1)`，并把 `stock = -1` 视为不扣减（`stock = CASE WHEN stock > 0 THEN stock - 1 ELSE stock END`）。
3. **事务包裹**：三段写入（扣积分、扣库存、插兑换记录）必须在**同一事务**内，用 1-1 建立的 **RAII 事务守卫**（`BEGIN IMMEDIATE` → `COMMIT` / 异常 `ROLLBACK`），确保任意一步失败不留半成品。**禁止**裸 `db.execute("BEGIN")`（伴-1 的根因，也是「异常路径漏 ROLLBACK」的来源）。
4. **内存态一致性**：扣减成功后**必须**用 DB 的真实结果同步内存中的 `User`（在同一临界区内），不得再用 `user->points -= cost` 写内存再覆盖 DB。
5. 补 `CHECK` 约束（可选但推荐，且**必须在同一迁移里**做）：`CHECK (points >= 0)`。注意 SQLite 不支持给已有表加 `CHECK`（需重建表），故**不强制**；若不做，必须在报告中说明并把它列入批次 2。

**验收判据**

- C4-1：`grep -n 'execute_bind_affected' sqlite_wrapper.h` 存在定义；`grep -n 'execute_bind' sqlite_wrapper.h` 的 `bool` 版本**签名未变**（`git diff` 只增不删）。
- C4-2（**行数语义，必做**）：用临时最小程序直接调 `execute_bind_affected`，断言
  - `UPDATE ... WHERE id = <存在的 id>` → `affected_rows == 1`；
  - `UPDATE ... WHERE id = <不存在的 id>` → 返回 0 且 `affected_rows == 0`；
  - `DELETE ... WHERE id = <不存在>` → 返回 0 且 `affected_rows == 0`。
- C4-3（**H-② 的真实受害者，必做**）：`PUT /api/teacher/evaluation/<不存在的 id>` 与 `DELETE /api/teacher/evaluation/<不存在的 id>` 现在必须返回 **404**（修复前必然返回 200）。给出修复前的 200 原始响应作为基线证据。
- C4-4（**双花，必做**）：对同一学生（余额 P、商品花费 C 且 P ≥ C 但 P < 2C）**并发两次** `POST /api/mall/redeem`，断言：成功次数 ≤ ⌊P/C⌋，最终 `points >= 0`，且 `redemption_records` 行数 == 成功次数。给出修复前的「两次都成功 / points 变负」原始证据。
- C4-5（超卖）：库存 S 的商品并发下单 S+K 次，断言成功次数 == S（或 `stock == -1` 时不受限），`stock` 不为负。
- C4-6：失败路径不留半成品：构造「积分扣成功但库存不足」的场景，断言**积分未被扣**（事务回滚生效）。
- C4-7：主进程编译 exit 0；`tests/run_tests.ps1` 全绿。

**边界与反例**

- **反例**：只把 `:397` 改成「先读再写」加个锁，但**不**用原子 SQL → 跨进程/跨语句仍有窗口；且锁不能跨到 DB 层，仍可能被别处的写入覆盖。契约要求**原子 SQL + 行数判据**。
- **反例**：`execute_bind_affected` 内部先 `sqlite3_changes64` 再 `sqlite3_finalize` 的顺序若颠倒，仍能取到值（changes 在 finalize 前有效），但**必须**在实现里明确顺序；若在 1-1 未完成前实现，`changes64` 会被其他线程覆盖 → 故本项**依赖 1-1**。
- **反例**：把 `stock = -1` 当成 `stock == 0` 处理 → 无限库存商品全部「库存不足」，属功能回归。C4-5 必须包含该分支。
- **反例**：用 `execute()`（`sqlite_wrapper.h:41-51`）代替 `execute_bind_affected` → `sqlite3_exec` 不返回行数；`update()`（`:102-108`）虽返回 `sqlite3_changes()` 但**不接受参数**（SQL 拼接），不得为了行数而放弃参数化。
- 本项**不**处理 F2（HTTP 500 静默）与 B17（业务码统一），属批次 2。
- 本项**不**解决 B18（`points_records` 内存副本与 DB 双源）—— 但**必须**把返回给前端的 `record.id` 从「内存序号」改为「DB 自增 id」（见 3.12 伴-2'），否则并发下该 id 与 DB 完全脱钩。

**归属**：**t38**。独立验证：C4-2/C4-3/C4-4/C4-5/C4-6 **必须**由独立验证任务重跑。

---

### 3.5 【1-5】B9：`/api/teacher/*` 写接口缺对象级归属校验

**实测位置（写接口，全部只有角色级 `check_permission_middleware`）**

| 端点 | 实测路由行 | 归属校验所需参数 | 现状 |
|---|---|---|---|
| `POST /api/teacher/students` | `routes_teacher.cpp:103`（`className` 取自 body `:118`） | 目标 `className` / `class_code` | ❌ 并且 `className` **由请求体任意指定** → 可把学生建到任意班级 |
| `PUT /api/teacher/students/<id>` | `routes_teacher.cpp:169`（`student_it` 查找 `:193`，改 `className` `:204`） | 目标学生 id | ❌ 可改任意学生，且可把学生改到任意班级 |
| `DELETE /api/teacher/students` | `routes_teacher.cpp:235`（`users.erase` `:258`） | 目标学生 id | ❌ 可删任意学生 |
| `POST /api/teacher/points` | `routes_teacher.cpp:274`（`it` `:299`，扣减 `:320`） | 目标学生 id | ❌ 可给任意学生加减积分 |
| `POST /api/teacher/evaluation` | `routes_teacher.cpp:464`（`it` `:488`） | 目标学生 id | ❌ 可给任意学生打分 |
| `PUT /api/teacher/evaluation/<id>` | `routes_teacher.cpp:781`（`UPDATE` `:797`） | 目标评价 id → student_id | ❌ 可改任意评价 |
| `DELETE /api/teacher/evaluation/<id>` | `routes_teacher.cpp:813`（`DELETE` `:826`） | 目标评价 id → student_id | ❌ 可删任意评价 |
| `POST /api/teacher/messages/<id>/reply` | `routes_teacher.cpp:1036`（取 `student_id` `:1065`） | 目标留言 → student_id | ❌ 可回复任意学生的留言 |

**已具备作用域的读接口（作为「正确范本」，不得改动其语义）**：`routes_teacher.cpp:31`（my-classes JOIN）、`:67`（绑定班级名）、`:396`、`:564`、`:578`、`:587`、`:624`、`:747`、`:1003`、`:1152`；以及家长端的 `routes_parent.cpp:25-30` `check_same_parent()`。

**根因**：鉴权确实在**服务端**做（`auth.h:344-388` 的 `check_permission_middleware`），但粒度只到**角色级**（`main.cpp:385-389` 的 `role_permissions` 种子把 `student:manage`（permission id 3）授予 role=2 全体教师，见 `main.cpp:387`）。**缺失的是对象级 ownership 校验**。注意措辞：不是「校验只在前端做」，而是「服务端只做角色校验、缺 ownership 校验」（`VERIFICATION.md` §2.5）。

**附带事实（必须一并修，否则归属校验可被绕过）**

- `POST /api/teacher/students` 的 `className`（`:118`）与 `PUT /api/teacher/students/<id>` 的 `className`（`:204`）都**直接采信请求体**。即使加了「必须是自己的班级」校验，也必须**对 `className` 本身**做归属校验（改学生班级 = 把学生移出/移入自己的班级，两个方向都要判）。

**修复动作（唯一选定方案，见决策④）**

1. 新增归属原语：

   ```cpp
   // models.h
   // 教师是否「拥有」该学生。判定依据统一为 teacher_classes JOIN classes（不用 className 字符串）。
   bool teacher_owns_student(const std::string& teacher_id, const std::string& student_id);
   // 教师是否拥有该班级（用于建号/改班级的目标班级校验）。
   bool teacher_owns_class(const std::string& teacher_id, const std::string& class_name);
   // 教师是否拥有该评价（按评价 id 反查 student_id 再判归属）。
   bool teacher_owns_evaluation(const std::string& teacher_id, int eval_id);
   ```

2. 判定 SQL 统一为**已占主导的形态**（同一子查询在 `routes_teacher.cpp` 出现 9 次）：

   ```sql
   SELECT 1 FROM teacher_classes tc
     JOIN classes c ON tc.class_id = c.id
     JOIN users   u ON u.className = c.name
    WHERE tc.teacher_id = ? AND u.id = ? AND u.role_id = 3
    LIMIT 1
   ```

   **禁止**在 C++ 侧用「绑定班级名集合 ∩ 学生 className 字符串」做判定（`routes_teacher.cpp:81-87` 的写法）：它把「班级」当成自由文本，而 `classes.name` 有 `UNIQUE`（`main.cpp:405` 附近）但 `users.className` **没有外键约束**，字符串一旦不一致就静默失配。统一到 JOIN 后，`className` 只作为**连接键**使用。
3. **HTTP 层薄包装**（放 `auth.h`，与 `require_csrf`（`auth.h:180-187`）同构）：

   ```cpp
   // auth.h
   // 归属校验失败 → 403 + 可读 msg，返回 false
   inline bool require_teacher_owns_student(const httplib::Request& req, httplib::Response& res,
                                            const std::string& student_id);
   ```

4. 在 8 个写 handler 的 `check_permission_middleware` **之后、任何读写之前**调用；`PUT /api/teacher/students/<id>` 还要**额外交验目标 `className`**（`teacher_owns_class`），并在响应中区分「学生不存在(404)」与「无权操作(403)」。
5. **未绑定任何班级的教师**（`teacher_classes` 无行）必须得到**拒绝**（403 或空集合），**不得**保留 `routes_teacher.cpp:80-87` 的 `if (!bound_class_names.empty())` 兜底 —— 那条分支让未绑定教师看到**全校**学生。同一「保留旧行为」的漏洞在 `:1152` 附近也要一并核对。
6. 读接口的**同一兜底**必须同步收紧（否则 B9 只修了一半）：`routes_teacher.cpp:78-96` 的 `if (!bound_class_names.empty())` 与 `:1152` 附近。

**验收判据**

- C5-1：`grep -n 'teacher_owns_student\|teacher_owns_class\|teacher_owns_evaluation\|require_teacher_owns_student' *.h *.cpp` → 定义 1 处（`models.h`/`models.cpp`）+ 包装 1 处（`auth.h`）+ **调用点 ≥ 8**（与 §3.5 表格的 8 个端点一一对应）。
- C5-2：`grep -n 'bound_class_names.empty()' routes_teacher.cpp` → **零命中**（兜底已消除）。
- C5-3：`grep -n "className == cn\|cn == user.className" routes_teacher.cpp` → **零命中**（判定不再用字符串比较）。
- C5-4（**水平越权，必做**）：以「教师 A（绑定班 1）」的会话，对「教师 B（绑定班 2）」的学生执行 8 个写端点，**全部**必须返回 403（或 404，但必须与分析一致且可判定）；给出**修复前全部返回 200** 的原始证据。
- C5-5（未绑定班级）：创建一个**无任何** `teacher_classes` 记录的教师账号，`GET /api/teacher/students` 必须返回**空数组**（不是全校），所有写端点必须 403。
- C5-6（建号/改班级）：教师 A 尝试 `POST /api/teacher/students` 且 `className` 属于班 2 → 403；属于班 1 → 200。同法覆盖 `PUT` 把学生改到班 2。
- C5-7：家长端 `check_same_parent`（`routes_parent.cpp:25-30`）不受影响（回归）；`GET /api/parent/*` 仍正常。
- C5-8：主进程编译 exit 0；`tests/run_tests.ps1` 全绿。

**边界与反例**

- **反例**：只在 `POST /api/teacher/points` 加校验，其余 7 个不加 → C5-1 的调用点计数不足，直接判 fail。
- **反例**：用「请求体里的 `className` 是否在我的班级列表里」判归属，但**不校验** `student_id` 实际属于哪个班 → 可用「学生 A 的 id + 我自己的班级名」拼出通过校验的请求（`PUT /api/teacher/students/<id>` 的 `className` 会被写入 DB，学生被"搬"进我的班）。必须**双向**校验（学生当前所属班 + 目标班）。
- **反例**：把 403 改成静默返回空数据 → 前端表现为「操作成功但无变化」，是假功能；必须显式 403。
- **反例**：用 `className` 字符串比较作为「性能优化」的快速路径 → 与 C5-3 冲突。
- **迁移风险（必须在报告里写明）**：若生产库中 `teacher_classes` 的绑定不完整（例如教师账号是直接写库创建的，未走 `routes_admin.cpp:587-596` / `:797-807` 的绑定写入），升级后该教师将**失去**原本可见的学生。这是**有意的收紧**；实现者必须给出「如何为存量教师补绑定」的运维 SQL 建议（例如按 `classes.head_teacher` 匹配），并把它写进批次 1 结项报告的残余风险。
- 本项**不**改 `student:manage` 的授权映射（`main.cpp:385-389`），也不引入新的权限码（批次 2 的 B19）。

**归属**：**t39**。独立验证：C5-4/C5-5/C5-6 **必须**由独立验证任务重跑。

---

### 3.6 【1-6】B4：XFF 信任边界与家长端锁定

**实测位置**

```cpp
// auth.h:141-154   login_client_key()：无条件取 X-Forwarded-For 的第一个值；无该头才退回 REMOTE_ADDR
// auth.h:111-114   login_attempts()：static std::map，无锁
// auth.h:117-123   login_can_try()
// auth.h:126-134   login_record_fail()（a.fails++ 非原子）
// auth.h:137-139   login_record_success()
// routes_public.cpp:29,30,39,54,60   仅有的 5 个锁定调用点（全在 /api/auth/login）
// routes_parent.cpp:49-77  /api/parent/login：find_user_by_username(:65) + verify_password(:73)，无任何锁定
// config.h:22-36   现有 security 配置项（无任何代理/信任配置）
```

**根因**

1. **XFF 可伪造** → 每个请求换一个 `X-Forwarded-For` 就得到一个全新的锁定键 → 爆破无限。
2. **键会反向锁死**：不发该头时退回 `REMOTE_ADDR`；若两者都拿不到（见下）则键退化为 `unknown|<username>` → **共享一个键**，攻击者可以故意失败 5 次把任意账号锁 30 分钟（`config.json` 的 `security.max_login_attempts=5` / `lockout_minutes=30`）。
3. **家长端完全无锁定**（`routes_parent.cpp:49-77`）→ 同一字典攻击面。

**必须纠正的一处审计事实（我读了 vendored 源码）**：`CODE_QUALITY_SUMMARY.md` §五 B10 行与 `CODE_QUALITY_REVIEW.md` 称「httplib **从不设** `REMOTE_ADDR` → 键退化为 `unknown|<user>`」。**实测相反**：

```
httplib.h:3639   req.set_header("REMOTE_ADDR", strm.get_remote_addr());
```

即 httplib **确实**注入 `REMOTE_ADDR`（每请求由 `Server::routing` 前置设置）。因此 B4 的**失效模式是 (1)+(3)**（XFF 可伪造 + 家长端无锁定），而**不是** (2)。本契约据此调整：判据必须同时覆盖「伪造 XFF 不能换键」与「家长端有锁定」；`unknown|<user>` 的退化路径**仍要防御**（当 `get_remote_addr()` 返回空时可能发生），但不得再把它写成「httplib 从不设置」。

**修复动作（唯一选定方案，见决策⑤）**

1. `config.h` 新增显式信任开关（新增字段，不得改动既有字段语义）：

   ```cpp
   // config.h  ServerConfig
   bool trust_proxy_headers = false;         // 默认 false：不信任任何客户端可伪造的头
   std::vector<std::string> trusted_proxies; // 可信代理地址/网段白名单（默认空）
   ```

   同步在 `load_config()`（`config.h:62-71` 的 `security` 段）读取 `security.trust_proxy_headers` 与 `security.trusted_proxies`；`config.json` 增加对应两行（**默认关闭**）。
2. 重写 `login_client_key()`（`auth.h:141-154`）：
   - `trust_proxy_headers == false`（默认）→ **只用 `REMOTE_ADDR`**，**完全忽略** `X-Forwarded-For`（含 `X-Real-IP` 等任何客户端可设头）。
   - `trust_proxy_headers == true` → 仅当 `REMOTE_ADDR` **命中** `trusted_proxies` 白名单时才采信 XFF，且取**最后一跳**（`rfind(',')` 之后那段，因为它离我们最近、由我们的可信代理写入）；不得取第一段。
   - 兜底：地址为空时使用**固定常量** `"unknown"`（保持现状语义），并**必须**在日志中记录一次（便于发现代理配置缺失）。
   - **键的维度**：按决策⑤ 增加**账号维度**的第二个键（`"acct|" + username`），两键**同时**计数与判定；这样即使攻击者能换 IP，也无法绕过账号维度的累计失败。
3. **并发保护**：`login_attempts()` 的 `static std::map` 必须受**与容器同一把锁**（决策②）或**专用互斥**保护（二选一，必须在报告写明并保持全局唯一方案）。同时：
   - 增加**上限与清理**：map 条目数上限（例如 10000）+ 过期条目清理（`locked_until` 早于 now 且 `fails == 0` 的条目可删），防止无界增长（内存 DoS）。
   - `login_can_try` / `login_record_fail` / `login_record_success` 的三个步骤必须在**同一临界区**内原子地完成（或合并为一次带锁调用），否则「检查-计数」仍有竞态。
4. **家长端接入同一套**：`routes_parent.cpp:49-77` 的 `/api/parent/login` 增加与 `routes_public.cpp:29-60` **完全同构**的 4 个调用点（`login_client_key` / `login_can_try` / 失败 `login_record_fail` ×2 / 成功 `login_record_success`）。
5. **不得**删除锁定机制，也不得把阈值调大来「绕过」判据。

**验收判据**

- C6-1：`config.h` 存在 `trust_proxy_headers`（默认 `false`）；`config.json` 的 `security` 段默认关闭。
- C6-2：`grep -n 'X-Forwarded-For' auth.h` → 命中处**必须**位于 `trust_proxy_headers` 判断的分支内；`grep -n 'X-Real-IP' *.h *.cpp` → 若新增则同样受门控，若不新增则零命中。
- C6-3（**伪造 XFF 不得换键，必做**）：默认配置下，用**同一个**真实 IP、5 次**不同** `X-Forwarded-For` 值的错误口令登录，第 6 次必须被锁定（429）。给出修复前「每次换 XFF 都得到 401、永不 429」的原始证据。
- C6-4（**账号维度，必做**）：用**不同** XFF + 不同真实表现（若无法伪造真实 IP，则用账号维度键证明）连续失败 5 次 → 目标账号必须被锁定。给出 429 及其响应体。
- C6-5（**家长端，必做**）：`POST /api/parent/login` 错误口令连续 5 次 → 第 6 次 429（修复前恒 401）。
- C6-6（**并发安全，必做**）：并发发起 ≥ 50 次错误口令登录（同一账号），进程不得崩溃；结束后该账号必须处于锁定态，且失败计数不得出现「超过阈值后仍可继续尝试」。
- C6-7（**内存上界**）：用 ≥ 1000 个不同 key 触发 `login_record_fail`，断言 map 规模不超过配置上限（或过期条目被清理）；给出实测的规模输出。
- C6-8：锁定到期后可恢复：把 `lockout_minutes` 临时设为极小值（或直接等待）后断言恢复 401；临时配置改动必须还原并给出对照。
- C6-9：主进程编译 exit 0；`tests/run_tests.ps1` 全绿。

**边界与反例**

- **反例**：只把「取第一段」改成「取最后一段」但仍无条件信任 XFF → C6-3 仍失败（攻击者直接把 XFF 设成任意单值）。
- **反例**：把 `trust_proxy_headers` 默认设为 true → 等于默认开洞，违反 C6-1。
- **反例**：只在 `/api/auth/login` 加账号维度键，`/api/parent/login` 不加 → C6-5 失败。
- **反例**：用「按 IP 阻断所有请求」代替锁定 → 会把 NAT 后的正常用户一起封禁；契约要求的是**账号 + IP 双维度计数**，不是全局封 IP。
- **反例**：清理过期条目时把正在锁定的条目删掉 → 锁定被解除；清理条件必须排除 `locked_until > now`。
- **已知未闭合（必须写入结项报告）**：锁定状态仍在**内存**中，进程重启即清零；`/api/admin/users/reset-password`（`routes_admin.cpp`，无锁定调用点）不参与锁定计数。二者属**批次 2**（「锁定持久化 + 管理端点限流」），本项不修但必须**显式列出**。
- 本项**不**解决 B9 的归属问题（1-5）与 B10/B14/B12（1-7）。

**归属**：**t40**。独立验证：C6-3/C6-4/C6-5/C6-6/C6-8 **必须**由独立验证任务重跑。

---

### 3.7 【1-7】B10 + B14 + B12

#### 3.7.1 B10 —— 异常原文进响应头【**审计结论部分被推翻，见下**】

**实测位置**

```cpp
// httplib.h:3561-3582  Server::dispatch_request
//   3564: try { for (handlers) { if (regex_match) { handler(req,res); return true; } } }
//   3574: } catch (const std::exception &ex) { res.status = 500; res.set_header("EXCEPTION_WHAT", ex.what()); }
// httplib.h:1976-1997  detail::write_headers
//   1981:     if (x.first == "EXCEPTION_WHAT") { continue; }   ← 发响应时被显式跳过
// routes_public.cpp:20    auto req_json = json::parse(req.body);   ← 体
// routes_public.cpp:21-22 req_json.value("username","") / ("password","")  ← 类型不符抛 type_error
// routes_public.cpp:92    } catch (json::parse_error& e) { ... }   ← 只捕 parse_error
// main.cpp:442-449        已注册 set_logger，未注册任何 error/exception handler
```

**审计结论的纠正（关键）**：审计称「异常原文进响应头**回显客户端**」。实测：`httplib.h:1981` 在**写响应头时显式跳过** `EXCEPTION_WHAT`，因此该头**永远不会到达客户端**。

- **主缺陷改判**：B10 的实质是**可用性缺陷** —— 客户端用 `{"username":123,"password":"x"}` 这类**类型不符但 JSON 合法**的请求体即可触发 `json::type_error.302` 冒泡 → **HTTP 500 且响应体为空**（`dispatch_request` 的 catch 不设置 `set_content`）。而前端 `api.ts` 只对 401/403/429 做提示 → 用户看到「无反应」（F2 的放大器）。**不是信息泄露**。
- **次要缺陷（仍成立）**：`ex.what()` 被写入一个**内部响应头**，并被 httplib 的**默认 logger** 打印；若有人后续注册的 handler 或代理把 `res.headers` 整体透出（例如某些自定义 `mapper` / 反代日志），泄露面才会出现。另外 `res.headers` 里的异常文本也会被 `log_response` 之外的其他日志路径读到（取决于实现）。**契约按「防御性删除 + 明确记录」处理，不得再声称"已在响应头泄露到客户端"。**

**修复动作**

1. **输入校验前置**（治本）：`routes_public.cpp` 的登录与注册 handler 在 `value()` 之前，必须用**类型安全**的读取（`contains()` + `is_string()`，或 `json::find`）替代会抛异常的强类型 `value()`；类型不符 → `{"code":400,"msg":"请求数据格式错误"}`，**不得** 500。**范围 = 全部 handler 的入口解析**，至少覆盖 `routes_public.cpp`（登录/注册/改密）、`routes_teacher.cpp`、`routes_admin.cpp`、`routes_parent.cpp` 中所有 `req_json.value(` 的直接调用点。
2. **全局兜底**：在 `main.cpp` 注册 `svr.set_error_handler`（`httplib.h:476` 与 `:3128-3132` 证明：**任何 `res.status >= 400` 的响应**都会经过它）：
   - 统一输出 JSON 体 `{"code":<status 或业务码>,"msg":"<可读文本>"}`，`Content-Type: application/json`；
   - **必须显式删除内部头**：`res.headers.erase("EXCEPTION_WHAT")`（`httplib.h:195` 的 `Headers` 支持 erase）；
   - 把异常原文**只写 Logger**（服务端可观测），不得回显。
   - **B14 联动（必须同批）**：413 响应同样走该 handler，因此设了 payload 上限之后前端不会收到空体。
3. **不得**用 `catch (...)` 在路由注册处一层层包（侵入式）；用 httplib 的 handler 机制（已存在且在正确的位置）。
4. 说明：本版本 httplib **没有** `set_exception_handler`（`grep` 仅命中 `set_error_handler`），故不存在「注册全局异常处理器」这一选项；`set_error_handler` 通过「catch 已把 status 置 500」间接覆盖异常场景，**这是本版本唯一可用的机制**。

**验收判据**

- C7-1（**可判定，必做**）：`POST /api/auth/login` 体 `{"username":123,"password":"x"}` → 响应必须是 `400` 或 `500`，但**必须带 JSON 体**且 `Content-Type: application/json`；且响应头集合中**不含** `EXCEPTION_WHAT`。给出原始响应头 dump。
- C7-2：`grep -n 'EXCEPTION_WHAT' main.cpp` → 命中（显式删除）；`grep -n 'set_error_handler' main.cpp` → 命中。
- C7-3：`:476`/`:3128-3132` 的机制已被实际使用（给出 `main.cpp` 中的注册行与一处实测的 404/413/500 响应的 JSON 体）。
- C7-4：**不得**为了让 C7-1 通过而把 `res.status` 从 500 改成 200（那是把异常伪装成成功）；若实现者选择「类型错误 → 400」，必须证明这是**输入校验**的结果（前置校验代码存在），而不是 catch 里改状态码。

**边界与反例**

- **反例**：只在登录 handler 加校验，其余 handler 仍用 `value()` → 同一 500+空体的缺陷仍在别处；C7-2/C7-3 之外的判据要求实现者**列出**所有 `req_json.value(` 调用点及其处置。
- **反例**：把 `EXCEPTION_WHAT` 改成别的头名继续用 → 仍被 `:1981` 跳过（无害）但仍是死代码；契约要求**删除**。
- **反例**：用「全局 try/catch 包住 handler」但**不改** httplib → 不可能（handler 是 lambda，注册进 `handlers` 容器）。

#### 3.7.2 B14 —— payload 无上限、无读超时

**实测位置**

```cpp
// httplib.h:43-44      #define CPPHTTPLIB_PAYLOAD_MAX_LENGTH ((std::numeric_limits<size_t>::max)())
// httplib.h:2955       Server 构造：payload_max_length_(CPPHTTPLIB_PAYLOAD_MAX_LENGTH)
// httplib.h:3072-3078  set_read_timeout / set_payload_max_length 的实现（存在且可用）
// httplib.h:1955-1971  read_content：len > payload_max_length → skip + 413
// httplib.h:1648       status_message(413) = "Payload Too Large"
// main.cpp:442-449     服务器创建处：未调用任何 setter（全项目 grep 零命中）
```

**修复动作**：在 `main.cpp` 的 `httplib::Server svr;`（`:443`）之后、`listen()`（`:497`）之前设置：

```cpp
svr.set_payload_max_length(<正整数上限>);   // 取值由实现者定，但必须是正整数且满足 C7-6/C7-7 两个判据
svr.set_read_timeout(<正秒数>, 0);          // 必须是正数（0 的语义不明确，禁止）；usec 传 0
svr.set_keep_alive_max_count(<正上限>);     // 必须是正整数，用于防止连接堆积
```

**验收判据**

- C7-5：`grep -n 'set_payload_max_length\|set_read_timeout\|set_keep_alive_max_count' main.cpp` → **至少各 1 处命中**，且位于 `svr` 构造与 `listen()` 之间。
- C7-6（**运行期，必做**）：发送大于上限的请求体 → 得到 **413**，且响应体是 JSON（C7-1 的 error handler 生效）。给出实测的响应码与体。
- C7-7：正常大小的请求（例如 `POST /api/auth/login`）仍然 200（不得把上限设得过小导致功能回归）。
- C7-8：慢速连接（只发部分 body 后不动）在超时后被关闭，服务仍可继续服务其他请求（给出「一个卡住的连接不阻塞其他请求」的证据）。

**边界与反例**

- **反例**：只设 `set_payload_max_length` 而不注册 error handler → 413 响应体为空/纯文本，前端 `response.json()` 抛错（退化到「服务器响应格式错误」）。二者必须同批。
- **反例**：把读超时设成 0 → 可能表示「无超时」或「立即超时」，两种解读都会产生缺陷；必须显式给正数并实测 C7-8。
- **反例**：靠 `config.json` 加一个「不设上限」的开关 → 等于把 C7-6 变成可关闭项；契约要求**上限始终生效**，只允许通过配置调整**数值**。

#### 3.7.3 B12 —— `query()/query_bind()` 把 DB 错误伪装成「无数据」

**实测位置**

```cpp
// sqlite_wrapper.h:53-92    query()：prepare 失败 → std::cerr + return result（json::array()）
// sqlite_wrapper.h:59-62    └─ 与「无数据」不可区分
// sqlite_wrapper.h:151-188  query_bind()：同样在 :156-159 早退返回空数组
// sqlite_wrapper.h:41-51    execute()：错误只走 std::cerr（B29）
// auth.h:240-258            get_session_info()：用 query()，空结果被当成「会话无效」→ 401
// auth.h:246                └─ 同一处还有 snprintf 拼接（B11，批次 2）
```

**修复动作**

1. 给 `SqliteDb` 增加**独立的错误通道**，不改动 `query()`/`query_bind()` 的返回类型（零调用点回归）：数据成员 `mutable std::string last_error_;` + `last_error()` 访问器；在**每个** prepare/step 失败分支写入 `sqlite3_errmsg(db_)`（并清空在每次成功调用开始时）。
2. **调用方策略（只改最关键的 fail-closed 点，不做 600 处改造）**：
   - 鉴权路径：`auth.h:240-258` 的 `get_session_info()` 在 `result.empty()` 且 `db.last_error()` 非空时必须**区分**「SQL 失败」与「无此会话」——SQL 失败应返回 `false` 并让中间件给出 **500**（而非 401），避免「DB 坏了 → 所有人被当成未登录」；
   - 权限路径：任何把 DB 错误当「403 权限不足」的路径必须改为 500（`sqlite_wrapper.h:53-62` 的注释已经把这列为已知后果）。
3. 把 `std::cerr` 的 DB 层错误改为 `Logger::error`（B29），使错误进日志文件并带时间戳。
4. **不得**把 `query()` 改成抛异常（会改变 600+ 调用点的控制流）；**不得**改变 `query_bind()` 成功时的返回形状。

**验收判据**

- C7-9：`grep -n 'last_error' sqlite_wrapper.h` → 存在成员与访问器；`grep -n 'last_error_' sqlite_wrapper.h` 的写入点覆盖 prepare 失败与（若有）step 失败。
- C7-10（**可判定，必做**）：构造一次**必然失败**的查询（例如对不存在的表 `SELECT * FROM __no_such_table__`），断言 `q.empty() == true` **且** `db.last_error()` **非空**；再跑一次成功但无行的查询（`SELECT ... WHERE 1=0`），断言 `q.empty() == true` **且** `db.last_error()` **为空**。两者行为必须可区分。
- C7-11：`grep -n 'std::cerr' sqlite_wrapper.h` → DB 层错误不再走 `std::cerr`（改为 `Logger::error`），或给出为何保留的明确理由。
- C7-12：鉴权路径的 fail-closed：把一个会失败的查询注入 `get_session_info` 路径（临时探针即可），断言返回 500 而不是 401；临时探针必须清理或标注。
- C7-13：主进程编译 exit 0；`tests/run_tests.ps1` 全绿。

**边界与反例**

- **反例**：把 `query()` 的失败返回值从空数组改成 `json(nullptr)` → 会破坏所有 `for (const auto& row : result)` 的调用点（对 null 迭代是 UB/json 抛异常）。**禁止**。
- **反例**：只加 `last_error()` 而没有任何调用方用它 → 通道存在但无效；C7-12 要求至少鉴权路径真的用上。
- **反例**：让 `last_error()` 保存 `std::cerr` 已打印的文本 → 必须保存 `sqlite3_errmsg(db_)` 的原文。
- 本项**不**处理 B17（16 处 `res.status` vs 217 处业务码的统一）、F2（前端 5xx 兜底），属批次 2。

**归属**：**t40**。独立验证：C7-1/C7-6/C7-8/C7-10/C7-12 **必须**由独立验证任务重跑。

---

### 3.8 【H-①】交接缺陷①：删权限/角色不重建索引

**实测位置**

```cpp
// routes_admin.cpp:1123  svr.Delete("/api/admin/permissions", ...)
// routes_admin.cpp:1147      permissions.erase(it);          ← 之后无任何索引重建
// routes_admin.cpp:1159  svr.Delete("/api/admin/roles", ...)
// routes_admin.cpp:1183      roles.erase(it);                ← 之后无任何索引重建
// routes_admin.cpp:1007  svr.Post("/api/admin/roles", ...)
// routes_admin.cpp:1046      roles.push_back(Role{...});      ← 之后无任何索引重建
// routes_admin.cpp:1081  svr.Post("/api/admin/permissions", ...)
// routes_admin.cpp:1111      permissions.push_back(Permission{...});  ← 之后无任何索引重建
// routes_admin.cpp:1736      init_indexes();                  ← 仅导入端点收口
// models.cpp:180-181     role_id_map / permission_id_map 存的是值拷贝 → 删除后陈旧条目仍在
```

**根因**：`role_id_map` / `permission_id_map` / `role_permission_map`（`models.cpp:180-182`）**只在 `init_indexes()`（`:210-224`）里建立**。删除/新增角色或权限的 4 个端点从未调用重建 → 索引停在旧快照：已删权限仍在 `permission_id_map`（`models.cpp:293-294` 的 `perm_it->second.code == permission_code` 仍会命中），新加的权限在 map 中缺失（`check_permission_optimized` 会漏判）。批次 0 只修了**用户**索引（`rebuild_user_indexes()`），漏了这条。

**修复动作**：见 3.3 第 5 条的 5 个调用点；实现上推荐「一次 `rebuild_all_indexes()`（= `init_indexes()`）在同一临界区内调用」。删除后**必须**同时清掉 `role_permission_map` 中该角色的权限列表（`init_indexes()` 的 `clear()` 已覆盖，前提是**真的调用它**）。

**验收判据**

- CH-1：`grep -n 'permissions.erase\|roles.erase\|roles.push_back\|permissions.push_back\|role_permissions.push_back' routes_admin.cpp` 的**每一个**命中行之后 15 行内，必须出现 `init_indexes()` / `rebuild_all_indexes()` / `rebuild_role_indexes()` / `rebuild_permission_indexes()` 之一。逐点列清单（5 点）。
- CH-2（**可判定运行证据，必做**）：
  1. 登录管理员 → `POST /api/admin/permissions` 新增权限（id 由 `routes_admin.cpp:1110` 附近的 `permissions.back().id + 1` 生成）；
  2. `DELETE /api/admin/permissions` 删除它；
  3. 断言 `permission_id_map` 中已无该 id（临时只读探针打印 map 的 key 集合）；
  4. 断言此时 `check_permission_optimized` 的行为与「无该权限」一致。
  给出修复前「陈旧条目仍在」的原始输出。
- CH-3（**角色侧同法**）：新增角色 → 删除角色 → 断言 `role_id_map` 与 `role_permission_map` 中均无该角色。
- CH-4：主进程编译 exit 0；`tests/run_tests.ps1` 全绿。
- CH-5：**必须**在报告中声明：本次改动**只**修「索引同步」，**不**修「角色/权限增删只改内存不落库」（B16）与「导入静默降级角色」（B15）——那两条属批次 2。

**边界与反例**

- **反例**：只在 `DELETE` 后重建，不在 `POST` 后重建 → 新权限在 map 中缺失；CH-1 的逐点清单会抓到。
- **反例**：重建时调用**非幂等**的旧式 `init_indexes()`（即回退 T24 的 `clear()`）→ `role_permission_map` 重复累积（23/46/69…）。C3-4 是这条的回归守卫。
- **反例**：新增权限时 `permissions.back().id + 1`（`routes_admin.cpp:1045`/`:1110` 的模式）在**删除后**会**重用**已删 id → 与稳定键/唯一性假设冲突。必须改为「max(id)+1」并（推荐）断言不与既有 id 冲突。

---

### 3.9 【H-②】交接缺陷②：`execute_bind` 无法区分 0 行受影响

**实测位置**：`sqlite_wrapper.h:127-148`（`bool execute_bind(...)`，`:145-147` `return rc == SQLITE_DONE;`）。
**受害调用点（我实测确认，非穷举推测）**：

| 调用点 | 期望语义 | 现状后果 |
|---|---|---|
| `routes_public.cpp:385-388` | 扣库存必须命中 1 行 | 0 行也返回 true → **无故扣库存成功**（B8 的一半） |
| `routes_teacher.cpp:797-803` | `else { code 404 }` | **恒不可达** → 修改不存在的评价也返回 200 |
| `routes_teacher.cpp:826-831` | `else { code 404 }` | **恒不可达** → 删除不存在的评价也返回 200 |
| `routes_admin.cpp:951-952`（`/api/admin/users/reset-password` 的改口令 UPDATE） | 必须命中 1 行 | 0 行也成功（用户已不存在时静默「成功」） |
| `models.cpp:100-104` `delete_user_from_db` | 由调用方先查存在性 | 尚可（调用方已判存在），但语义应改为行数 |

**根因**：`sqlite3_step` 对「语法合法但影响 0 行」的语句返回 `SQLITE_DONE`，与「影响 N 行」**不可区分**；必须在 `sqlite3_step` 之后用 `sqlite3_changes64()` 取行数 —— 而该值是**连接级**的，单连接下会被其他线程覆盖（→ 依赖 1-1）。

**修复动作**：见 3.4 第 1 条（新增 `execute_bind_affected`）。**另外**要求：

- 对上述 5 个受害者调用点逐个改用行数判据，并给出「0 行 → 正确响应」的实测（C4-3 覆盖评价侧；库存侧由 C4-4/C4-5 覆盖）。
- **不要**把 `bool execute_bind` 的返回语义改成 `affected > 0`（会静默改变 600+ 调用点的行为，属高危回归）。

**验收判据**：见 C4-2 / C4-3（用于评价侧）、C4-4 / C4-5（用于库存侧），另加：

- CH-6：`grep -n 'execute_bind_affected' routes_public.cpp routes_teacher.cpp routes_admin.cpp` → 至少覆盖上表 4 个业务调用点。
- CH-7：`git diff sqlite_wrapper.h` 对 `bool execute_bind` 的**签名与返回表达式**为零改动（只允许新增函数）。

**边界与反例**

- **反例**：新函数返回 `int` 表示行数并让 0/负值都表示错误 → 调用方无法区分「0 行」与「失败」；契约要求**通过出参返回行数**，返回值只表达成功/失败（见 3.4 第 1 条）。
- **反例**：在单连接未改前就依赖 `sqlite3_changes64()` 判行数 → 并发下取到别线程的行数；**本项严格依赖 1-1**（DAG 已强制 t35→t38 串行）。

---

### 3.10 【伴-1】B7 残留：单连接上的 `BEGIN/COMMIT/ROLLBACK`

**实测位置**

```cpp
// routes_admin.cpp:1519   db.execute("BEGIN TRANSACTION");     ← 返回码未检查
// routes_admin.cpp:1734   db.execute("COMMIT");                ← 返回码未检查
// routes_admin.cpp:1755   db.execute("ROLLBACK");              ← 只在 catch(exception) 里；json::parse_error 分支(:1748) 不回滚
// sqlite_wrapper.h:14-32  BEGIN/COMMIT 走 sqlite3_exec，同一 db_
```

**根因**：事务是**连接级**的。`:1519` 与 `:1734` 之间隔着整个导入循环（数百行、多次 DB 调用），期间**其他线程**在同一连接上的语句全部被卷入该事务；`:1755` 的 `ROLLBACK` 会连它们**已返回 200** 的写入一起丢弃。此外 `:1748` 的 `json::parse_error` 分支**不回滚** → 半成品持久化。

**修复动作**

1. 1-1 完成后，事务天然按线程隔离；仍须把裸 `db.execute("BEGIN")` 换成**RAII 事务守卫**（`SqliteDb` 内新增，析构时若未 commit 则 rollback）：

   ```cpp
   class SqliteDb {
   public:
       // RAII：构造时 BEGIN IMMEDIATE，析构时未 commit 则 ROLLBACK
       class Transaction { public: explicit Transaction(SqliteDb&); ~Transaction(); bool commit(); bool active() const; ... };
   };
   ```

2. **必须检查 BEGIN/COMMIT 的返回码**：BEGIN 失败 → 立即 fail；COMMIT 失败 → 必须记录 `Logger::error` 并回滚，**不得**返回 200。
3. `:1748` 的 `json::parse_error` 分支必须同样回滚（RAII 自动解决）。
4. 使用 `BEGIN IMMEDIATE`（而非 `BEGIN`/`BEGIN DEFERRED`）以避免「升级锁失败」的经典死锁路径。

**验收判据**

- CB1-1：`grep -n 'BEGIN TRANSACTION\|db.execute("COMMIT")\|db.execute("ROLLBACK")' routes_admin.cpp` → **零命中**（全部改为 RAII）。
- CB1-2：`grep -n 'class Transaction\|begin_transaction\|Transaction trans' sqlite_wrapper.h routes_admin.cpp` → 守卫存在且被导入端点使用。
- CB1-3（**跨线程隔离，必做**）：并发「导入（大文件）」+「单条写入」，断言单条写入落库且**不被**回滚（C1-4 的配套证据）。
- CB1-4：构造一次**导入失败**（`json::parse_error` 与 SQL 错误各一次），断言 DB 无任何半成品（对比导入前后行数）。
- CB1-5：COMMIT 失败路径可达：给出至少一次 `Logger::error` 的实测（例如临时让 COMMIT 作用于已关闭事务）。

**边界与反例**

- **反例**：只把 `BEGIN` 换成 `BEGIN IMMEDIATE` 而不换 RAII → `:1748` 的漏回滚仍在，CB1-4 失败。
- **反例**：RAII 析构里吞掉 ROLLBACK 失败 → 必须 `Logger::error`。
- **反例**：把事务放大成「所有请求一个事务」→ 与 1-1 的隔离目标相反。

---

### 3.11 【伴-2 / 伴-2'】B30（日志轮转 TOCTOU）+ 积分记录 ID 与 DB 脱钩

**B30 实测位置**

```cpp
// logger.h:41-56   rotate_if_needed()：ifstream 取 size → 一连串 std::rename
// logger.h:58-62   log()：先 rotate_if_needed()（:59），再加锁（:61）
// logger.h:89       static std::mutex mutex_;
```

**根因**：轮转的 `stat` + N 次 `rename` 发生在**加锁之前**；两个线程可同时在轮转 → rename 交错 → 日志文件丢失/错乱。这是全项目**唯一**已有锁的地方，却保护不全。

**修复动作**：把 `rotate_if_needed()` 移进锁内（或让 `log()` 先加锁再调用它），使「判大小 + 滚动」成为一个临界区。这是本批最容易的一处改动（surgical）。

**B30 验收判据**

- CB2-1：`logger.h` 中 `rotate_if_needed()` 的调用点位于 `std::lock_guard<std::mutex>` **之后**（给出改动后 file:line）。
- CB2-2（**并发证据，必做**）：把 `log_max_size` 临时设成极小值（例如 1 KB），并发写 ≥ 2000 条日志，断言：不丢行（总行数 == 写入数，允许轮转后的分文件计数）、`server.log*` 文件数不超过 `log_max_files` + 1、无 rename 异常/文件缺失。配置改动必须还原。
- CB2-3：单线程下轮转行为与修复前一致（`server.log` → `server.log.1` → …）。

**边界与反例**

- **反例**：只把 `mutex_` 改成 `recursive_mutex` 而不改结构 → TOCTOU 仍在。
- **反例**：把轮转改成「每次写日志都 stat + open」（现状即是）—— 本项**不**要求性能优化（属批次 3）。

**伴-2' 实测位置（与 N2/H-② 同源，必须在 t38 一并处置）**

```cpp
// routes_teacher.cpp:332   int new_record_id = points_records.size() + 1;   ← 内存序号
// routes_teacher.cpp:341   points_records.push_back(new_record);
// routes_teacher.cpp:344-349 INSERT INTO points_records (...)              ← 不指定 id → DB 自增
// routes_teacher.cpp:357   {"id", new_record_id}                           ← 把内存序号回给前端
// routes_admin.cpp:1664-1671 导入路径同样按外部 id 覆盖
```

**根因**：返回给前端的 `record.id` 与 DB 实际 id 不是一回事（重启后从 1 重号）；并发下 `points_records.size()` 也非原子。批次 0 未闭合（B18/N5 属批次 2）。

**本批最小要求（不做整表回载）**：插入后用 **`sqlite3_last_insert_rowid()`**（或等价的「插入后取行数/id」通道）把**DB 真实 id** 回给前端；内存 `points_records` 的条目 id 也用同一值。这样「ID 重号 + 与 DB 脱钩」在前端可见层面被消除。

**伴-2' 验收判据**

- CB2'-1：`grep -n 'points_records.size() + 1' routes_teacher.cpp` → **零命中**。
- CB2'-2（**可判定，必做**）：连续两次 `POST /api/teacher/points`，断言返回的 `record.id` **递增且与实际 DB 行的 `SELECT id` 一致**；重启服务后再次操作，`record.id` **不与历史 id 冲突**（给出 DB 侧对照输出）。
- CB2'-3：`INSERT` 的返回值（行数/id）被检查，失败时返回非 200 并 `Logger::error`。

**边界与反例**

- **反例**：只把 `size()+1` 换成 `rand()` 生成 id → 与 DB 完全脱钩。
- **反例**：宣称「已修 B18」→ B18 的**回载缺失**（重启后 `/api/admin/export` 导出空数组）本批**不修**，必须如实标注为批次 2。

---

### 3.12 【伴-3】B27 变体：删库重建路径未 fail-fast

**实测位置**

```cpp
// main.cpp:193-202
//   193: if (users_table_is_old_schema()) {
//   194:     Logger::warning("检测到旧 schema（users.id 为 INTEGER），删除数据库文件并重建为新 schema (TEXT 主键)");
//   195:     db.close();
//   196:     if (std::remove(g_config.db_path.c_str()) != 0) { Logger::error("删除旧数据库文件失败: " + ...); }   ← 不 return
//   199:     if (!db.open(g_config.db_path)) { Logger::error("重新打开数据库失败"); }                              ← 不 return
//   202: }
```

**根因**：批次 0 把 `db.open` 失败改成了 fail-fast（`main.cpp:204-215` 的 `return 1`），但**删库重建这条路径**里的两处失败（删除失败、重开失败）仍只打日志就继续 → 服务可能带着**未打开或陈旧 schema** 的 DB 启动，且 `main.cpp:217` 的 `if (db.isOpen())` 会把整段初始化**跳过**，最终以「所有接口都失败」的状态对外服务（批次 0 已经为「表结构初始化失败」加了 fail-fast，`:433-439`，这里漏了）。

**修复动作**：`main.cpp:196-201` 的两处失败改为 `Logger::error` + `std::cerr` + `return 1`（与 `:204-215`、`:433-439` 的既有风格一致）。

**验收判据**

- CB3-1：`main.cpp` 中删库重建块内的两个失败分支都以 `return 1`（或等价 fail-fast）结束。
- CB3-2（**可判定，必做**）：构造「删除失败」场景（例如把 `campus_system.db` 置为被占用/只读），断言进程**非 0 退出**且日志有 `Logger::error`；修复前会继续启动。
- CB3-3：正常库（`users.id` 已是 TEXT）启动路径不受影响（回归）：进程正常监听，`users_table_is_old_schema()` 返回 false。
- CB3-4：`main.cpp` 的 `if (db.isOpen())`（`:217`）之后**不得**存在「DB 未打开但继续走到 listen」的路径；给出 `grep` 证明所有失败分支都在 `listen()`（`:497`）之前 `return`。

**边界与反例**

- **反例**：只在重开失败处 return、不在删除失败处 return → CB3-2 只覆盖一半。
- **反例**：把删库重建整段删掉（保留旧 schema 服务）→ 与 `main.cpp:35` 的 INTEGER schema 假设冲突，属功能回归；契约要求 fail-fast 而非删除逻辑。
- 说明：这条**不属于** 1-7 的原始范围，但与 1-7 的「不静默降级」是同一条原则，且批次 0 的 V18 修复已铺好风格；**若 t40 判定容量不足，可在报告中标注为 defer 并把判据完整移交批次 2 —— 但不得静默略过**。

---

## 4. 必须由独立验证任务重跑 / 只允许符号检查 的分类清单

> **分类原则**：只有「能通过运行/请求/文件系统观察到行为差异」的判据才计入「必须重跑」；纯 `grep`/签名类的符号检查**不得单独作为通过依据**（它们只能证明「代码被改了」，不能证明「行为对了」）。

### 4.1 必须由独立验证任务重跑（禁止只查符号）

| 编号 | 判据 | 为什么不能只查符号 | 需要的独立方法 |
|---|---|---|---|
| C1-3 | 连接数 = 线程数 + 1 | 计数是运行期事实 | 自建并发压测 + 进程内计数输出 |
| C1-4 | B7 事务串扰消失 | 需要并发时序 | 并发导入 + 单条写入，断言写入存活 |
| C2-4 | 并发建号无重复 ID | 竞态只在运行时出现 | 并发建号 + `GROUP BY ... HAVING COUNT(*)>1` |
| C2-5 | 修复前基线可复现 | 基线证据必须自证 | 独立构造竞态 |
| C2-6 | 并发登录不崩且锁定生效 | UB/竞态只能观测 | 并发登录（含错误口令） |
| C3-3 | 删除中间用户后按 id 取回正确用户 | 需运行内存容器 | 最小程序或接口序列 + 修复前对照 |
| C3-4 | `init_indexes()` 幂等（重复调用不翻倍） | 需运行并 dump | 连续调用 2 次 + 排序 dump 比对 |
| C3-6 | 删权限后不留陈旧授权 | 需运行 + 探针 | 新增→删除→读 map |
| C4-2 | `affected_rows` 语义（1/0/失败） | 需运行 SQLite | 最小程序直接调用 |
| C4-3 | 不存在评价返回 404（修复前 200） | 需 HTTP | 修复前后各跑一次 |
| C4-4 | 双花不可复现（成功次数 ≤ 余额/单价） | 需并发 + 账实核对 | 并发兑换 + DB 终态核对 |
| C4-5 | 超卖不可复现（含 `stock=-1` 分支） | 同上 | 并发兑换 + 库存核对 |
| C4-6 | 失败不留半成品 | 需事务回滚观测 | 构造「积分成功/库存失败」 |
| C5-4 | 8 个写端点跨教师 403（修复前 200） | 需真实会话 + HTTP | 双教师双班级会话对打 |
| C5-5 | 未绑定教师看不到全校 | 需真实会话 | 新建无绑定教师账号 |
| C5-6 | 建号/改班级的 `className` 归属校验 | 需真实会话 + 请求体 | 双向（当前班/目标班） |
| C6-3 | 伪造 XFF 不得换键 | 需真实 HTTP + 头 | 同 IP 换 XFF 打 6 次 |
| C6-4 | 账号维度锁定 | 同 | 换 XFF 打 6 次 |
| C6-5 | 家长端锁定（修复前恒 401） | 同 | `/api/parent/login` 打 6 次 |
| C6-6 | 并发登录不崩、锁定不被绕过 | 并发 | ≥50 并发错误登录 |
| C6-8 | 锁定到期恢复 | 需时间/配置改动 | 临时改 `lockout_minutes` 并还原 |
| C7-1 | 类型错误请求 → 有 JSON 体且无 `EXCEPTION_WHAT` | 需抓响应头 | 原始响应 dump |
| C7-6 | 超大请求体 → 413 + JSON 体 | 需真实请求 | 原始响应 dump |
| C7-8 | 慢连接不阻塞其他请求 | 需时序 | 挂起连接 + 并行正常请求 |
| C7-10 | `last_error()` 能区分「SQL 失败」与「无数据」 | 需运行 | 两种查询对照 |
| C7-12 | 鉴权路径 DB 错误 → 500 而非 401 | 需注入失败 | 临时探针（清理/标注） |
| CH-2 | 删权限后 map 无陈旧项 | 需运行 + 探针 | 新增→删除→读 map |
| CH-3 | 删角色后 map 与权限列表无残余 | 同 | 同上 |
| CH-6 | 4 个受害者调用点改用行数判据 | 需运行 | 与 C4-3 合并 |
| CB1-3 | 并发导入不吞别的写入 | 并发 | 与 C1-4 合并 |
| CB1-4 | 两种导入失败都不留半成品 | 需失败注入 | 行数对比 |
| CB2-2 | 并发日志不丢行、不超轮转上界 | 并发 + 文件系统 | 并发写 2000 条 + 文件清点 |
| CB2'-2 | `record.id` 与 DB 实际 id 一致且不重号 | 需运行 + DB 对照 | 两次操作 + 重启 + `SELECT id` |
| CB3-2 | 删库重建失败 → 非 0 退出 | 需失败注入 | 只读/占用文件 |

### 4.2 允许只做机械/符号检查（但**不得**单独宣告条目通过）

| 编号 | 判据 | 附加要求 |
|---|---|---|
| C1-1 / C1-2 | `sqlite3*` 不外泄、线程局部获取存在 | 必须与 C1-3 组合 |
| C2-1 / C2-2 / C2-3 | 签名改值语义、无 `&users[`、唯一一把锁 | 必须与 C2-4/C2-6 组合 |
| C3-1 / C3-2 / C3-5 | 索引键类型、无下标、5+5 个变更点清单 | 变更点清单必须逐点列出 file:line |
| C4-1 / CH-7 | `execute_bind_affected` 存在、`bool` 版本零改动 | 必须与 C4-2 组合 |
| C5-1 / C5-2 / C5-3 | 调用点 ≥8、无 `bound_class_names.empty()`、无字符串比较 | 必须与 C5-4/C5-5 组合 |
| C6-1 / C6-2 | 配置开关默认关闭、XFF 读取受门控 | 必须与 C6-3/C6-5 组合 |
| C7-2 / C7-3 / C7-5 | error handler 注册、setter 调用、`EXCEPTION_WHAT` 删除 | 必须与 C7-1/C7-6 组合 |
| C7-9 / C7-11 | `last_error()` 存在、`cerr` 已迁移 | 必须与 C7-10/C7-12 组合 |
| CH-1 | 5 个变更点后有重建调用 | 必须与 CH-2/CH-3 组合 |
| CB1-1 / CB1-2 | 无裸 BEGIN/COMMIT、RAII 守卫存在 | 必须与 CB1-3/CB1-4 组合 |
| CB2-1 / CB2-3 | 轮转在锁内、单线程行为一致 | 必须与 CB2-2 组合 |
| CB2'-1 / CB2'-3 | 无 `size()+1`、返回值被检查 | 必须与 CB2'-2 组合 |
| CB3-1 / CB3-4 | fail-fast 分支、无「未开库仍 listen」路径 | 必须与 CB3-2 组合 |
| C1-5 / C2-7 / C3-7 / C4-7 / C5-8 / C6-9 / C7-13 | 编译 exit 0 + `tests/run_tests.ps1` 全绿 | 这是**回归门禁**，不是功能证明 |

### 4.3 各批次的独立验证任务必须遵守

1. **不得**引用实现者的转述；每个「必须重跑」判据都要有**独立终端输出/响应 dump**。
2. **修复前基线**：C2-5、C3-3、C4-3、C4-4、C5-4、C6-3、C6-5、CH-2、CH-3、CB3-2 都要求「修复前可复现的失败/错误行为」作为对照；若某条确实无法在修复前复现，**必须**改用最小程序直接驱动内部函数复现，并**明确标注方法**，不得跳过。
3. **临时产物**（临时 .exe/.db/配置改动/探针）必须清理或标注路径；配置改动必须给出还原后的对照。
4. 不得为了让门禁通过而弱化断言、放宽比较、跳过用例。

---

## 5. 各条目 → 任务号映射（计账用）

| 条目 | 落地任务 | 独立验证任务 | 说明 |
|---|---|---|---|
| 1-1 连接策略 | **t38** | 独立验证（C1-3/C1-4） | 关键路径首项；决策① 已定论 |
| 1-2 B3+B5+N2 | **t38** | 独立验证（C2-4/5/6） | 与 1-1 同任务，保证「锁 + 连接」一体 |
| 1-3 B6 彻底版 | **t39** | 独立验证（C3-3/4/6） | 基础形态由 t38 提供 |
| 1-4 B8 | **t38** | 独立验证（C4-2..6） | 原子 SQL 依赖 1-1 的行数通道 |
| 1-5 B9 | **t39** | 独立验证（C5-4/5/6） | 决策④ 已定论 |
| 1-6 B4 | **t40** | 独立验证（C6-3/4/5/6/8） | 决策⑤ 已定论 |
| 1-7 B10+B14+B12 | **t40** | 独立验证（C7-1/6/8/10/12） | B10 结论已按 vendored 源码纠正 |
| H-① 删权限/角色索引 | **t38**（索引重建）+ **t39**（稳定键收口） | 独立验证（CH-1/2/3） | 两阶段：t38 先堵漏，t39 再改语义 |
| H-② `execute_bind` 行数 | **t38** | 独立验证（C4-2/C4-3/CH-6/CH-7） | 与 1-4 同任务（同一文件同一根因） |
| 伴-1 B7 | **t38** | 独立验证（CB1-1..5） | 与 1-1 同任务（RAII 依赖连接隔离） |
| 伴-2 B30 | **t40** | 独立验证（CB2-1..3） | 独立可做 |
| 伴-2' 积分记录 id | **t38** | 独立验证（CB2'-1..3） | 与 N2 同源 |
| 伴-3 B27 变体 | **t40** | 独立验证（CB3-1..4） | 可 defer，但必须显式记账 |
| 伴-4 N2 兜底 | **t38** | 独立验证（C2-4/5） | `INSERT OR REPLACE` → `INSERT` |

---

## 6. 交付与协作纪律

### 6.1 只读性

- 本契约与 `BATCH1_DESIGN_DECISIONS.md` 只**新增文档**：`git status --porcelain` 仅出现两份新文档与既有未跟踪项（`?? .agent-teams/`）。
- 未修改任何 `*.cpp` / `*.h` / `config.json` / `.github/**` / `frontend/**`；未执行 `git commit` / `git push`。

### 6.2 同文件并发改动（必须遵守）

批次 1 的四个任务在同一批文件上高度重叠。**约束**：

| 文件 | t38 | t39 | t40 |
|---|---|---|---|
| `sqlite_wrapper.h` | 连接门面 + 行数通道 + RAII 事务 | — | 错误通道（`last_error_`） |
| `models.h` / `models.cpp` | 锁、受锁 API、稳定键基础、H-① 重建 | 稳定键落地、归属原语 | — |
| `auth.h` | （仅随锁协议） | `require_teacher_owns_student` | `login_attempts` 加锁、`login_client_key` |
| `routes_*.cpp` | 兑换、删除/新增路径 | 8 个教师写端点 | 登录/家长登录、导入端点 |
| `main.cpp` | 线程池与启动顺序守卫 | — | setter、error handler、fail-fast |
| `logger.h` | — | — | 轮转加锁 |

**规则**：`t39` 与 `t40` **不得并行编辑同一文件**（`auth.h` 是两者唯一冲突点）。执行顺序建议：`t38` → `t39` → `t40`（严格串行最稳），或 `t38` → `t39` 与 `t40` 并行但**先约定 `auth.h` 的归属**（推荐 t40 后做，因为 t39 的 `auth.h` 改动更小）。

### 6.3 不得跨越的红线

1. **不得**引入第二套连接或事务机制（t37 已因该风险被 cancelled）。
2. **不得**为了通过判据而调大 `busy_timeout`、调小 `thread_count`、关闭 WAL、或把锁定阈值改大。
3. **不得**回退批次 0 的任何既有修复（尤其 `sha256.h`、`must_change_password`、`rebuild_user_indexes`、`init_indexes` 的幂等性、`cookie_secure` 复位、`.trae` 取消跟踪）。
4. **不得**在未更新本契约的情况下改变任何决策结论（见决策文档末尾的「变更流程」）。

---

## 7. 与审计文档的行号差异记录（逐条）

> 口径：批次 0 已提交（`git status` 干净、仅 `?? .agent-teams/`），因此**所有批次 0 契约里的行号都已漂移**。下表以「审计/批次 0 契约的说法」对「本次实测」逐条记录。**一律采用本次实测值。**

| # | 来源 | 原表述 | 本次实测 | 处理 |
|---|---|---|---|---|
| 1 | 审计 | B1 影响面 `sha256.h:94-148`、升级链 `routes_public.cpp:45-51` | 批次 0 已修：升级链**已删除**（现 `routes_public.cpp:46-52` 为注释说明） | 采用现状；批次 1 不再涉及 |
| 2 | 批次 0 契约 | B2 种子 `main.cpp:250-254` | 已重写为 `seed_default_users_if_empty()`（`main.cpp:83-152`） | 采用现状 |
| 3 | 批次 0 契约 | `models.cpp:17-22` 兜底表 | **已删除**；`users` 定义在 `models.cpp:23`（注释块 `:16-22`） | 采用现状 |
| 4 | 批次 0 契约 | `models.cpp:164-221` 索引区 | 现为 `models.cpp:175-300` | 采用现状 |
| 5 | 批次 0 契约 | `models.cpp:206-211` `find_user_by_id` | 现为 `models.cpp:262-268` | 采用现状 |
| 6 | 批次 0 契约 | `models.cpp:172-186` `init_indexes` | 现为 `models.cpp:210-224`（T24 已幂等化） | 采用现状 |
| 7 | 批次 0 契约 | `routes_admin.cpp:826-827` 删除用户 | 现为 `routes_admin.cpp:886`（`users.erase`）+ `:889`（`rebuild_user_indexes()`） | 采用现状 |
| 8 | 批次 0 契约 | `routes_teacher.cpp:259-260` 删除学生 | 现为 `routes_teacher.cpp:258`（`erase`）+ `:262`（`rebuild_user_indexes()`） | 采用现状 |
| 9 | 批次 0 契约 | `auth.h:355` / `:392` 门禁接入点 | 现为 `auth.h:344-388`（`check_permission_middleware`，`:346` 处调用 `enforce_password_change`）与 `:393-429`（`check_parent_auth_middleware`，`:395` 处调用） | 采用现状 |
| 10 | 批次 0 契约 | `main.cpp:463` 线程池 | **实测一致**（`main.cpp:463`） | 无差异 |
| 11 | 批次 0 契约 | `main.cpp:22` 全局 `db` | **实测一致**（`main.cpp:22`） | 无差异 |
| 12 | 审计 §3.3 / `CODE_QUALITY_SUMMARY.md:173` | N1「`main.cpp:312-315`」 | 现为 `main.cpp:469-479`（含批次 0 补的 `cookie_secure=false` 于 `:477`） | 采用现状 |
| 13 | **审计事实纠正（重要）** | `CODE_QUALITY_SUMMARY.md` §五 B10 行称「httplib **从不设** `REMOTE_ADDR`」 | **实测相反**：`httplib.h:3639` `req.set_header("REMOTE_ADDR", strm.get_remote_addr());` | **采用实测**；B4 的失效模式改为「XFF 可伪造 + 家长端无锁定」，见 3.6 |
| 14 | **审计事实纠正（重要）** | B10 称「异常原文进响应头**回显客户端**」 | **实测相反**：`httplib.h:1981` 写响应头时显式 `continue` 跳过 `EXCEPTION_WHAT` → **不会到达客户端**；真实主缺陷是「500 + 空体」的可用性问题 | **采用实测**；B10 的主判据改为 C7-1（有 JSON 体、无该头），见 3.7.1 |
| 15 | 审计 | B13 备份判据 `routes_admin.cpp:335-349` | 现为 `routes_admin.cpp:323-350`（批次 0 已修判据） | 批次 1 不涉及 |
| 16 | 审计 | B6 `models.cpp:206-211` + `routes_teacher.cpp:259` | 用户侧已修（T24/T32）；**角色/权限侧未修**：`routes_admin.cpp:1147`、`:1183` | H-① 就是这条的剩余部分，见 3.8 |
| 17 | 审计 §三 | `execute_bind` 行数 `sqlite_wrapper.h:145-147` | **实测一致**（`sqlite_wrapper.h:145-147`） | 无差异 |
| 18 | 任务书锚点 | 「`auth.h:111-114` login_attempts」 | **实测一致** | 无差异 |
| 19 | 任务书锚点 | 「`main.cpp:463`」 | **实测一致** | 无差异 |
| 20 | 任务书锚点 | 「`models.cpp:14 points_records / :23 users / :26 roles / :34 permissions / :50 role_permissions`」 | **全部实测一致** | 无差异 |
| 21 | 任务书锚点 | 「`routes_parent.cpp:27` LIMIT 1」 | 现为 `routes_parent.cpp:27`（`check_same_parent` 内） | 无差异 |
| 22 | 任务书锚点 | 「`config.h:84`」指 `cookie_secure` | **实测一致**（`config.h:84`：`if (config.https_enabled) config.cookie_secure = true;`） | 无差异 |
| 23 | 任务书锚点 | 「`config.h:31` `cookie_secure = false`」 | **实测一致** | 无差异 |
| 24 | 任务书锚点 | 「`sqlite_wrapper.h:20` / `:27` / `:29`」 | **实测一致** | 无差异 |
| 25 | 任务书锚点 | 「`auth.h:141-149` login_client_key」 | 实测 `auth.h:141-154`（`:149` 是 `REMOTE_ADDR` 回退分支） | 采用 `:141-154` |
| 26 | 任务书锚点 | 「`models.cpp:187 clear_and_rebuild_user_indexes / :210 init_indexes / :240 rebuild_user_indexes / :245 update_user_index / :256 remove_user_index / :262 find_user_by_id / :280 check_permission_optimized`」 | **全部实测一致** | 无差异 |

**框架层实测事实（本契约新增的确定性依据，均来自我本次读取 vendored 源码）**

| 事实 | 证据 |
|---|---|
| `TaskQueue` 只在 `listen_internal()` 创建一次；worker 长驻复用 | `httplib.h:3443-3448`、`:371-379`、`:408-426` |
| Server 侧无额外线程/detach | `httplib.h:3481-3483`；`grep 'std::thread'` 仅命中 `ThreadPool::threads_`（`:432`）与 `CPPHTTPLIB_THREAD_POOL_COUNT`（`:53`） |
| `EXCEPTION_WHAT` 写响应时被跳过 | `httplib.h:1981` |
| 异常被 `dispatch_request` 捕获并置 500（不设 body） | `httplib.h:3564-3582` |
| `REMOTE_ADDR` 由框架注入 | `httplib.h:3639` |
| `set_error_handler` 在 `write_response` 中对 `status>=400` 生效 | `httplib.h:476`、`:3128-3132` |
| **无** `set_exception_handler` | `grep` 仅命中 `set_error_handler` |
| payload 上限默认 `SIZE_MAX`，且超出时 `skip + 413` | `httplib.h:43-44`、`:2955`、`:1955-1971`、`:1648` |
| `set_payload_max_length` / `set_read_timeout` / `set_keep_alive_max_count` 可用 | `httplib.h:3072-3078`、`:3068-3070` |
| `SQLITE_THREADSAFE` 默认 1；`bFullMutex = (SQLITE_THREADSAFE==1)`；`sqlite3_open` 不传 flag → 连接带递归互斥 | `sqlite3.c:14050`、`:22806`、`:181367-181371`、`:181405-181418` |
| 自研源码除 `Logger` 外零互斥设施 | `grep`（§0 基线） |

---

## 8. 未闭环 / 明确移交后续批次的事项（必须与结论同读）

1. **B5 的裸指针语义彻底消除**：本批只要求「不返回 `User*`」；`routes_public.cpp:340` 等调用点的重写若涉及更大的重构，须在报告中说明并保证不引入新裸指针。
2. **B18 / N5（`points_records` 从不回载、导出空数组）**：本批只修 `record.id` 与 DB 一致（伴-2'）；**整表回载**属批次 2。
3. **B16（角色/权限增改不落库）、B15（导入静默降级角色）、B19（假数据与死权限码）**：本批只修索引同步（H-①）；持久化与清理属批次 2。
4. **B17（16 : 217 的错误语义）与 F2（前端 5xx 兜底）**：本批只统一 `set_error_handler` 的输出形状；**全站统一响应封装**属批次 2。
5. **B11（`auth.h:240-275` 三处 `snprintf` 拼接）**：本批只要求 `get_session_info` 的错误语义（C7-12）；**参数化迁移**属批次 2。
6. **B4 的锁定持久化**：进程重启即清零；管理端点（`reset-password`）不参与锁定计数。属批次 2。
7. **B23（单连接串行化导致的 3~4 次会话查询 / N+1）**：1-1 解决连接层，但**查询次数**未优化；属批次 2（N3）。
8. **B30 之外：锁定 map 的上限与清理**（3.6 第 3 条）本批要求实现，但**过期条目的内存回收策略**（LRU 等）属批次 3。
9. **`CHECK (points >= 0)` 约束**：SQLite 不支持对已有表加 CHECK（需重建表）；本批**不强制**，未做则列入批次 2。
10. **CI 无法端到端运行**：所有涉及 GitHub Actions 的判据（批次 1 无新增）仍为「静态核对」；本批所有运行期判据都在本机 pwsh 可复现。
11. **`busy_timeout = 5000` 的阻塞上限**：高并发下仍可能整请求阻塞 5 秒；属调优（批次 3）。

---

## 9. 措辞纪律（防误读，必须遵守）

以下写法**禁止**出现在任何批次 1 的任务报告、验证报告或结项文档中：

- ❌「B10 已修复客户端可见的异常原文泄露」 → ✅「审计的『响应头回显客户端』结论被 vendored 源码推翻（`httplib.h:1981`）；本批修的是『500 + 空响应体』这一可用性缺陷」
- ❌「B4 的根因是 httplib 从不设 REMOTE_ADDR」 → ✅「`httplib.h:3639` 确实注入 `REMOTE_ADDR`；根因是 XFF 可伪造 + 家长端无锁定」
- ❌「N2 只修 ID 生成就够了」 → ✅「必须同时把 `INSERT OR REPLACE` 改成 `INSERT`，让唯一约束暴露冲突」
- ❌「B6 已在批次 0 修完」 → ✅「用户侧已修；角色/权限侧的索引同步（H-①）未修」
- ❌「`execute_bind` 已能判行数」（未改前） → ✅「必须新增 `execute_bind_affected`；不得改变既有 `bool` 版本的返回语义」
- ❌「加锁就解决了并发」 → ✅「WAL + busy_timeout 在单连接下不提供应用级临界区；必须同时改连接模型（1-1）」
- ❌「B9 是前端越权」/「鉴权只在前端做」 → ✅「服务端只做角色级鉴权，缺对象级 ownership 校验」
- ❌「CI 已端到端验证」 → ✅「静态核对，未经端到端复现」
- ❌「所有判据已通过」而只跑了 `grep` → ✅「§4.2 的符号检查不能单独宣告通过；必须附 §4.1 的运行期判据」
- ❌ 把技术取值写成「建议 X」并当成判据 → ✅ 判据必须是「必须是正整数 + 必须满足 C*-… 某判据」这类**可判定**形式；「建议/推荐」只允许出现在非判据的旁注里
