# 批次 1 设计决策裁决（BATCH1_DESIGN_DECISIONS）

- 任务：team `audit-batch0-remediation` 任务 **t35**（requirements-architect，attempt 1）
- 产物性质：**决策文档**。只新增本文件，不修改任何源码、配置或构建产物。
- 权威依据：`docs/audit/VERIFICATION.md` > `CODE_QUALITY_REVIEW.md`（§4.1、§6 批次 1、§7.1）> `CODE_QUALITY_SUMMARY.md` > `docs/audit/BATCH0_CLOSURE.md`。
- 配套契约：**`docs/audit/BATCH1_CONTRACT.md`**（判据编号 `C1-…`～`CB-…` 在本文被逐一引用）。
- 行号口径：全部由我在批次 0 落地并提交后的当前工作区用 read/grep 实测（差异见契约 §7）。
- **本文是「实现前必须定论」的 5 项决策的唯一结论**。下游实现任务（t38/t39/t40）**不得**再做设计判断；若认为某决策不可行，**必须先回报 t35**，不得自行改判（变更流程见 §7）。

---

## 决策总览

| # | 决策 | 唯一选定方案 | 被否方案 | 影响任务 |
|---|---|---|---|---|
| ① | **1-1 连接策略** | **每线程一条连接**（`SqliteDb` 保持门面，内部按线程惰性建连；约 600+ 调用点源码零改动） | (a2) 全局 `thread_local SqliteDb db;`；(b) 连接池；(c) 全局递归写锁 + 单连接 | **t38**（并约束 t39/t40） |
| ② | **锁粒度** | **一把全局 `std::mutex`** 保护全部内存容器 + 全部索引 map + `login_attempts`；**不用 `shared_mutex`**；**禁止跨锁传递裸指针** | (a) 每容器一把锁；(b) 一把 `shared_mutex`；(c) 无锁数据结构 | **t38**（约束 t39/t40） |
| ③ | **索引稳定键** | **`user_id` 字符串作稳定键**（value 不再存 `users` 下标）；越界与重复键的处置见 §3；`init_indexes()` 保持幂等 | (a) 保留下标 + 每次变更全量重建；(b) 只在 delete 时做「后缀 -1」；(c) `std::deque` 但不改索引 | **t38**（基础）+ **t39**（落地） |
| ④ | **`teacher_owns_student()` 归属语义与落点** | **判定原语放 `models.h`/`models.cpp`**，判据统一为 **`teacher_classes JOIN classes`**；**HTTP 薄包装 `require_teacher_owns_student()` 放 `auth.h`** | (a) 原语放 `auth.h`；(b) 判定用绑定班级名 ∩ `users.className` 字符串；(c) 只在中间件里做 | **t39** |
| ⑤ | **B4 客户端标识** | **默认不信任任何客户端可设头**（忽略 XFF，用 `REMOTE_ADDR`）；`trust_proxy_headers`（默认 false）+ `trusted_proxies` 白名单 + **仅取最后一跳**；**IP 维度 + 账号维度双键**；`login_attempts` 用**决策② 的同一把锁** | (a) 无条件信任 XFF（现状）；(b) 直接删掉 XFF 支持；(c) 全局封 IP；(d) 给锁定单独开一把锁 | **t40** |

---

## 决策① — 1-1 连接策略

### 候选方案

| 方案 | 形态 | 事务隔离 | 连接数 | 对既有调用点的影响 |
|---|---|---|---|---|
| **A（选定）** | `SqliteDb` 保持门面类；内部「当前线程的 `sqlite3*`」改为**线程局部惰性获取**；`db.query(...)` 等调用点**源码不变** | ✅ 天然按线程 | `thread_count + 1`（≤9） | **0 处**（约 600+ 调用点不动） |
| A2 | `thread_local SqliteDb db;`（把全局变量本身改成 `thread_local`） | ✅ | 同上 | 0 处**但语义有坑**（见否决理由） |
| B | 连接池（固定 N 条 + 借还 + 等待队列） | ✅（借还期间） | 可调 | 需为每次借还写 RAII 守卫，跨 `db.query()` 的现有写法**必须**改写 |
| C | 全局递归写锁 + 单连接 + `BEGIN/COMMIT` 同生命周期 RAII | ⚠️ 只在**持锁期间**；事务与锁必须严格同生命周期，一旦有人漏持锁即串扰 | 1 | 需逐条审查所有 DB 调用是否持锁 |

### 选定方案：**A —— 每线程一条连接（门面 + 线程局部惰性建连）**

**选择理由（逐条对应契约 §3.1 的根因 A–G）**

1. **它一次性解决两类问题**：`BEGIN/COMMIT` 的连接级串扰（契约 §3.11 伴-1）与 `sqlite3_changes64()` 的连接级污染（契约 §3.4/§3.9 的行数通道）——两者都**只有**「连接按线程隔离」能根治。方案 C 靠「锁 + 事务同生命周期」试图逼近，但它不能隔离 `sqlite3_changes64()`：即使有全局写锁，只要同一把连接上交错执行过别的语句，行数就可能被覆盖（除非把锁一路持到取行数，等于把 DB 串行化）。
2. **改动面最小**：`SqliteDb` 已是唯一 DB 封装（`sqlite_wrapper.h:14-210`），全部 ~600 处调用都写 `db.xxx(...)`。门面方案让**所有调用点保持原样**，把改动集中在 1 个文件里 —— 对「8 线程共享状态」这种跨切面改动，改动面越小、可验证性越高。
3. **线程模型已实测确定**：`httplib.h:3443-3448`（队列只建一次）+ `:371-379`/`:408-426`（worker 长驻复用）+ `:3481-3483`（Server 侧不再起线程）→ 请求 100% 落在固定的 8 个 worker 上，故「线程局部」= 「每个 worker 一条、全集 ≤ 9 条」，**上界可预知、可断言**（C1-3）。
4. **WAL 的收益此时才真正兑现**：`sqlite_wrapper.h:29` 的 WAL 让多连接并发读不互斥；单连接下 8 线程被那把递归互斥（`sqlite3.c:181405-181418`）串行化，WAL 的读并发优势完全拿不到。
5. **与 httplib 的生命周期契合**：worker 线程在 `listen()` 前创建、在 `listen()` 返回后 `shutdown()`（`httplib.h:3487`），因此线程局部连接的生命周期**严格落在服务运行期内**，不存在「连接比线程活得短」的关闭时序问题。

**为何 WAL + busy_timeout 已存在，单连接共享仍然错误（this is the核心论证）**

> **两者不在同一层。** WAL 与 `busy_timeout` 解决的是 **SQLite 数据库文件层**的锁竞争；本项目的缺陷是 **C++ 应用层的临界区缺失**。

| 层 | 机制 | 能解决什么 | 不能解决什么 |
|---|---|---|---|
| 文件层 | `PRAGMA journal_mode=WAL`（`sqlite_wrapper.h:29`） | 读不阻塞写、写不阻塞读 | 任何跨语句的原子性、任何跨线程的临界区 |
| 连接层 | `sqlite3_busy_timeout(db_, 5000)`（`sqlite_wrapper.h:27`） | 把 `SQLITE_BUSY` 变成「阻塞重试最多 5s」 | 让「读-判断-写」变成原子；也不改变「事务是连接级」的事实 |
| 连接层（隐式） | `SQLITE_THREADSAFE=1`（`sqlite3.c:14050`）→ `bFullMutex=1`（`:22806`）→ `sqlite3_open` 不传 flag 时连接带递归互斥（`:181367-181371`、`:181405-181418`） | **单次 API 调用不撕裂**（不会因两个线程同时 `sqlite3_step` 而堆损坏） | **跨调用的临界区**；`sqlite3_changes()`/`last_insert_rowid()` 的归属；`BEGIN/COMMIT` 的归属 |
| **应用层** | **缺失** | — | 本项目全部缺陷所在：读-判断-写、`erase`+索引、`BEGIN/COMMIT` 跨调用 |

**具体可判定的后果（每一条都有实测位置）**

1. **B8 双花/超卖**：`routes_public.cpp:358-360`（读）→ `:375`（判断）→ `:397`（写）三步之间无隔离，WAL 不提供任何屏障。
2. **B7 事务串扰**：`routes_admin.cpp:1519` 的 `BEGIN` 与 `:1734` 的 `COMMIT` 之间是**连接级**事务；`:1755` 的 `ROLLBACK` 会回滚其他线程**已返回 200** 的写入。WAL 只影响日志文件，不改变「事务绑定在连接上」这一事实。
3. **行数通道被污染**：`sqlite3_changes64()` 是**连接级**值（`sqlite_wrapper.h:102-108` 已有 `update()`/`insert()` 在用）。单连接下，「step 之后立刻取 changes」仍可能取到别线程最后一次写入的行数 → H-② 的修复（`execute_bind_affected`）**必须在 1-1 之后**才有意义（这也是 DAG 里 t35→t38 串行的原因）。
4. **B5 悬垂与容器竞态与连接无关**，由 1-2 的锁解决；但连接层如果不隔离，任何「试图用一把锁把 DB 操作包起来」的方案都会把 8 线程压成单线程（方案 C 的代价）。

### 对下游任务的约束（1-2 / 1-3 / 1-4）

| 约束 | 内容 | 对应判据 |
|---|---|---|
| **①-C1（对 1-2）** | 所有容器写入 + 索引维护必须在**同一临界区**完成；连接隔离只是让「DB 侧」不再串扰，**不能**替代容器锁 | C2-3、C2-4、C2-6 |
| **①-C2（对 1-2）** | **禁止跨锁传递裸指针/引用**；`find_user_by_*` 必须返回值拷贝 | C2-1、C2-2 |
| **①-C3（对 1-3）** | 稳定键改造必须建立在「每线程一条连接 + 一把锁」之上，**不得**再引入第二套连接/事务机制（t37 已因该风险被 cancelled） | C3-1、C3-5 |
| **①-C4（对 1-4）** | 行数通道 `execute_bind_affected` **只能在 1-1 完成后**才可依赖 `sqlite3_changes64()`；原子 SQL + RAII 事务是唯一允许的写法 | C4-1、C4-2 |
| **①-C5（通用）** | 请求代码**不得**持有跨线程传递的 `sqlite3*`；`sqlite3*` 只允许出现在 `sqlite_wrapper.h` 内部 | C1-1 |
| **①-C6（通用）** | 每条新连接都必须在建连时设置 `busy_timeout` 与 WAL pragma（它们在连接上生效，是**每连接**的） | C1-2 |
| **①-C7（通用）** | 建连失败**不得静默**；请求路径 fail-closed，启动路径 fail-fast（沿用 `main.cpp:204-215`） | C1-2、CB3-4 |

### 被否方案与否决理由

| 方案 | 否决理由 |
|---|---|
| **A2：`thread_local SqliteDb db;`** | 三处硬伤：(1) `main.cpp:190-440` 的建表/迁移/种子/加载全在**主线程**，worker 线程拿到的是**另一个未初始化实例**（或触发 `sqlite3_open` 建出第二条连接但**跳过** schema 初始化）；(2) `models.h:12` 的 `extern SqliteDb db;` 是跨 TU 引用，动态初始化次序无保证；(3) 语义上把「进程唯一的可观测状态」变成了 N 份，`g_config`/`logger` 等单例与它生命周期不一致。**门面方案等价于 A2 的收益，但没有 A2 的坑。** |
| **B：连接池** | 对当前负载是过度设计：连接数上限由「每线程一条」天然给出（≤9），池化只增加借还同步与「连接被归还时的事务/临时状态」清理成本；且现有调用点写法（`db.query()` 直接调用）**必须**改写为持守卫的形态，属大规模改动。**并且池化不解决 `sqlite3_changes64()` 的归属**（归还后不可用），需要额外的「每取一次连接一个作用域」纪律。 |
| **C：全局递归写锁 + 单连接** | (1) 把 8 线程的**全部** DB 访问压成串行，写锁冲突时每请求最多阻塞 5s（`busy_timeout`），是明确的性能悬崖；(2) 仍不能解决 `sqlite3_changes64()` 污染（除非把锁持到取行数，等于承认串行）；(3) 「事务与锁同生命周期」是**纪律**而非**机制**，任何一处漏持锁就回到 B7；(4) 与批次 1 的目标（让 WAL/`busy_timeout` 真正生效）相反。 |
| **「什么都不改，只加锁」** | 最常见也最危险：给容器加锁后 B8/B7 仍在（契约 §3.1 的 C1-4 会直接抓到），且行数通道仍被污染。 |

---

## 决策② — 锁粒度

### 候选方案

| 方案 | 形态 | 死锁风险 | 读并发 | 改动面 |
|---|---|---|---|---|
| **乙（选定）** | **一把全局 `std::mutex`** 保护「六个容器 + 五个索引 map + `login_attempts`」 | **零**（单锁无顺序问题；禁止嵌套） | 读也被串行（临界区极短） | 小 |
| 甲 | 每容器/每索引一把 `std::mutex` | 需要严格锁顺序规则；`check_permission_optimized` 天然要跨 3 个容器 | 略好 | 大 |
| 丙 | 一把 `std::shared_mutex`（读 `shared_lock` / 写 `unique_lock`） | `shared_mutex` **不可重入**，升级（read→write）会自死锁；嵌套读也要小心 | 好 | 中 |
| 丁 | 无锁数据结构 / 每线程副本 | 正确性证明成本极高 | 最好 | 极大 |

### 选定方案：**乙 —— 一把全局 `std::mutex`（非 `shared_mutex`）**

**理由**

1. **本项目所有并发缺陷的临界区都极短**：`find_user_by_id` 一次哈希查找、`update_user_index` 一次线性扫描、`login_record_fail` 一次 `map` 更新。用共享读锁换来的吞吐，在 8 线程、请求里还有 DB 往返的现实下**不可测量**；而 `shared_mutex` 的**不可重入**会引入一类新的、很难排查的自死锁。
2. **单锁消灭了锁顺序问题**：`check_permission_optimized`（`models.cpp:280-300`）依次读 `user_id_map` → `role_permission_map` → `permission_id_map`；`Auth::check_permission`（`auth.h:280-303`）依次读 `users` → `role_permissions` → `permissions`。多锁方案必须为这些嵌套读定义全局顺序并逐处遵守；单锁下**顺序问题不存在**（前提是禁止嵌套获取）。
3. **`login_attempts` 并入同一把锁**：`auth.h:111-114` 的 static map 与用户/会话数据在同一条请求路径上被访问（登录：`find_user_by_username` + `login_*`），分开加锁就需要「跨两个锁的顺序规则」；并入同一把锁虽然把两个本可独立的热点合并，但登录路径本来就是热点、临界区极短。
4. **与 1-1 的分工清晰**：连接隔离解决「DB 侧」，这把锁解决「内存侧」；两者互不嵌套（见下方规则），因此**没有跨锁死锁的可能**。

### 强制规则（必须写进实现注释与报告）

1. **R1 — 单一锁**：整个内存并发状态只有一把互斥锁。**禁止**新增第二把 `std::mutex`/`std::shared_mutex`/`std::recursive_mutex`（`Logger::mutex_` 是既有且独立的例外，见 R6）。
2. **R2 — 禁止嵌套获取**：同一线程**不得**在持锁状态下再次获取同一把锁（`std::mutex` 不可重入）。因此所有「可能被别的加锁函数调用」的内部实现必须**不带锁**，由最外层加锁一次。命名约定：`*_locked()`（内部无锁）/ 公开 API（负责加锁）。
3. **R3 — 禁止跨锁传递引用**：不得把 `User*`/`User&`/容器迭代器/索引 map 的引用带出临界区。`find_user_by_*` 必须返回值拷贝。**这条是 R2 之外的独立约束**：即便重入被禁止，返回裸指针仍等于「锁外使用锁内对象」。
4. **R4 — 临界区内禁止 I/O**：不得在持锁时调用 `db.*`、`Logger::*`（其内部有 `ofstream` 与自旋）、`std::cout`、`std::filesystem`。正确形态是「锁内取出需要的值拷贝 → 解锁 → 做 I/O」；需要「读-改-写」复合操作时，把**写**也带进锁内，但 DB 只在锁外做（见 R5）。
5. **R5 — 「内存 + DB」写入的顺序**：先做 DB 写入（原子 SQL，见 1-4），再用 DB 的真实结果（`affected_rows` / `last_insert_rowid`）更新内存 —— 两段**分别**加锁，中间不留「内存已改、DB 未改」的持久窗口。若要求强一致，则在锁内做 DB **不做**（R4 禁止），因此**契约选择「DB 为先、内存为后」**，并规定：内存更新失败（例如 id 不存在）必须记 `Logger::error` 并让响应如实反映。
6. **R6 — 与 Logger 的关系**：`Logger::mutex_`（`logger.h:89`）是独立锁。**禁止**在持有状态锁时调用 `Logger::*`（避免「状态锁 → 日志锁」的方向）。若必须在锁内记日志，改为「锁内收集字符串 → 锁外记」。反向（日志锁内取状态锁）也不得存在。
7. **R7 — 与 DB 连接的关系**：连接（线程局部）不是共享资源，**不需要**状态锁保护；`sqlite3*` 的创建是线程局部惰性的，也**不得**在状态锁内创建连接（避免持锁做 I/O）。
8. **R8 — 锁的可见性**：状态锁与其守卫类型只允许在 `models.cpp` 内部匿名命名空间或 `models.h` 暴露的最小接口中使用；业务文件（`routes_*.cpp`）**不得**直接 `lock()`，只能调用受锁 API。判据：`grep -n 'mutex' routes_*.cpp` → 零命中（若需要，改为在 `models.h` 提供受锁包装）。

### 对下游任务的约束（1-2 / 1-3 / 1-4）

| 约束 | 内容 | 对应判据 |
|---|---|---|
| **②-C1（1-2）** | 容器写入与其索引维护必须在**同一临界区**；否则存在「元素已入向量、索引未更新」的中间态 | C2-3、C2-4 |
| **②-C2（1-2）** | `find_user_by_id`/`find_user_by_username` 改为值返回；所有调用点必须在锁外使用拷贝 | C2-1、C2-2 |
| **②-C3（1-3）** | 稳定键改造**不引入新锁**；`init_indexes()` 的幂等性在锁内保证 | C3-1、C3-4 |
| **②-C4（1-4）** | 兑换的「内存扣分」与「DB 扣分」按 R5 顺序执行；不得用状态锁包住 DB 事务 | C4-4、C4-6 |
| **②-C5（1-6/B4）** | `login_attempts` 用决策② 的**同一把锁**（不新开锁）；三个函数（`can_try`/`record_fail`/`record_success`）各自原子，或合并为一次带锁调用 | C6-6 |
| **②-C6（通用）** | 任何新增的共享可变状态都必须纳入这把锁，并更新 `models.h` 的注释说明「哪些数据受保护」 | C2-3 |

### 被否方案与否决理由

| 方案 | 否决理由 |
|---|---|
| 甲（每容器一把锁） | 需要为 `check_permission_optimized` / `Auth::check_permission` / 登录路径定义并遵守全局锁顺序（users → role_permissions → permissions → login_attempts），任何一处违反就是死锁；本项目的收益（读并发）在 8 线程 + DB 往返下不可测量。**复杂度换不到可测量的收益。** |
| 丙（`shared_mutex`） | 不可重入：一旦某读路径在持 `shared_lock` 时调用另一读路径（本项目大量存在，如 `check_permission_optimized` 内调 `find_user_by_id`），若实现为 `shared_lock` 则**自死锁**；而 `shared_mutex` 的升级/降级语义在不同实现上有坑。为规避这些坑要写更多代码，比单锁更危险。 |
| 丁（无锁） | 需要为「向量 + 两张索引 map + 五个索引 map + 一个 Attempt map」建立多写者一致性协议；本项目 0 测试文件、8 线程、且正确性缺陷刚刚集中爆发，选无锁是**把风险从可见的竞态换成不可见的内存序错误**。 |
| **「不引入锁，靠连接隔离 + 原子 SQL 解决一切」** | 只能解决 DB 侧（B8/B7）；B5 的悬垂（`routes_public.cpp:340` 的 `User*` 在并发 `push_back` 后失效）、B3 的 `static std::map`、B6 的索引错位都是**内存侧**，必须靠锁 + 值语义解决。 |

---

## 决策③ — 索引稳定键

### 候选方案

| 方案 | 形态 | 变更成本 | 正确性论证 | 残留风险 |
|---|---|---|---|---|
| **C（选定）** | 索引 value 改为**稳定键（`user_id` 字符串）**；建议主存储同时改为「按 id 索引的映射」 | 中（`models.cpp` 内部 + `models.h` 接口） | 短：位置与身份解耦，插入/删除不再影响任何键 | 主存储改造的连带影响（遍历顺序、`generate_user_id` 的顺序依赖） |
| A | 保留下标 + **每次变更后全量重建**（批次 0 现状的办法扩展到全部变更点） | 小 | 中：必须证明「每个变更点都重建」且「重建期间无读者观察到中间态」 | 变更点遗漏即回归（H-① 就是这么来的）；O(n) 重建；中间态靠锁收敛 |
| B | 只在 delete 时做「后缀下标 -1」 | 小 | 长（易错）：需遍历两个 map、逐值比较、处理重复键 | 任何新变更点都要照抄这套逻辑 |
| D | 主存储改 `std::deque<User>` / `vector<unique_ptr<User>>`（引用稳定），索引仍存下标 | 中 | 中：解决**引用悬垂**，但**不解决下标错位**（deque 的 `erase` 依然前移） | 仍需要「变更后重建」 |

### 选定方案：**C —— `user_id` 作稳定键**

**选定形态（写死，不给实现者留歧义）**

```cpp
// models.h（示意，最终签名由实现者按代码风格定，但语义不得变）
// 1) 主存储：按 id 取值 O(1)，天然免疫扩容/删除引起的位置变化
extern std::unordered_map<std::string, User> users;   // 或保留 vector<User> + 二级稳定索引（见下）
// 2) 索引：value 不再是下标
extern std::unordered_map<std::string, std::string> user_id_map;      // id -> id（稳定键）
extern std::unordered_map<std::string, std::string> user_username_map;// username -> id
```

- **首选**：主存储直接改成 `std::unordered_map<std::string, User>`（或 `std::map` 以获得稳定遍历顺序）。这样 `find_user_by_id` 是 O(1)、**不存在扩容导致的引用失效**（B5 的根因被从类型上消除），且「下标」概念整体消失。
- **次选**（改动更小、也必须被接受）：保留 `std::vector<User> users`，但把两张用户索引 map 的 value 改为**用户 id 字符串**；查找时再用一个「id → 位置」的瞬时映射或在锁内线性定位。
  - 若选次选，**必须**保留 `models.cpp:264`/`:274` 的越界防御，并**追加身份校验** `users[it->second].id == user_id`（防「越界未触发但错位」）。
- **禁止**：索引 value 继续是 `size_t` 下标（判据 C3-1）。
- **遍历顺序的影响必须评估**：现有代码存在依赖容器顺序的写法（`roles.back().id + 1`、`points_records.size() + 1`、`users.size() + 1`）。改用 `unordered_map` 或改变容器后，**必须**逐处替换为「由 DB 生成」或「max(id)+1」，并在报告中列出被替换的位置。

**理由（为什么不在本批继续用方案 A）**

1. **方案 A 的正确性依赖「穷举所有变更点」**，而这正是批次 0 失手的地方：批次 0 修了用户侧两个 `erase`，却漏了角色/权限的四个端点（契约 §3.8 的 H-①）。凡是「靠人记住每个变更点」的方案，都会在下一次新增端点时再次失手。
2. **方案 A 无法消除中间态**：即使每次变更后重建，`users.push_back(...)` 与 `rebuild_user_indexes()` 之间仍存在一段「索引与数据不一致」的窗口，必须靠决策② 的锁把窗口关闭 —— 而一旦依赖锁，就不如直接把索引做成**无需重建**的形态，让正确性不再依赖锁的覆盖完整性。
3. **方案 C 与「不返回裸指针」（决策②/C2-1）协同**：`unordered_map<string, User>` 的引用在插入后**不会**失效（rehash 只影响迭代器），即使实现者日后不慎返回引用，也不会出现「写已释放内存」这种堆破坏级别的事故。
4. **与 1-2 同批做最经济**：1-2 已经要改 `models.cpp` 的锁与接口（`find_user_by_*` 的返回值语义），此时一并把索引键改掉，是同一片代码的两次相邻改动；分成两批反而要做两次接口迁移。

### 陈旧/越界下标与重复键的处理（必须逐条交代）

| 情形 | 方案 C 下的处置 | 若实现者选次选（保留下标）则必须做 |
|---|---|---|
| **陈旧下标**（已删用户的键仍指向旧位置） | 概念消失：稳定键下「键 = 身份」。若用户被删，**明确要求**删除其键（`user_id_map.erase(id)` / `user_username_map.erase(username)`），并在同一临界区完成 | 保留 `remove_*_index` + 同一临界区内的重建；并断言「重建后 map.size() == users.size()」 |
| **越界下标**（`i >= users.size()`） | 概念消失 | **必须**保留越界判断，并追加 `users[i].id == key` 的身份校验；任一不满足即视为「未找到」并**记录一次 `Logger::warning`**（便于发现残留陈旧键） |
| **重复键** | `user_username_map` 的键是 `username`：DB 侧 `username TEXT UNIQUE`（`main.cpp:221`）保证唯一；内存路径必须在**插入前**检查重复，冲突时返回失败（`save_user_to_db` 已是 `INSERT`，见 1-2 的 N2 兜底） | 同左 |
| **`role_id_map` / `permission_id_map` 的值** | 这两个 map 存的是**值拷贝**（`Role` / `Permission`），删除后**必须**重建（H-①）。稳定键方案不影响它们，但**同一临界区**的重建纪律一致 | 同左 |
| **`role_permission_map` 的累积** | `init_indexes()` **必须**先 `clear()` 再重建（T24 的幂等性，不得回退） | 同左 |

### `init_indexes()` 的幂等性要求（强制，不得回退）

1. 五个索引容器（`user_id_map` / `user_username_map` / `role_id_map` / `permission_id_map` / `role_permission_map`）在重建前**全部** `clear()`。
2. **连续调用两次**与调用一次的结果**完全一致**（可机械断言：两次调用后的 `size()` 与排序 dump 相同）。
3. 任一角色的权限数量**不得**在重复调用后翻倍（这是 T24 修的缺陷，回归守卫 = 契约判据 C3-4）。
4. 重建必须在**同一临界区**内完成（决策② 的 R1/R2）。

### 对下游任务的约束（1-2 / 1-3 / 1-4）

| 约束 | 内容 |
|---|---|
| **③-C1（1-2）** | 1-2 完成前不得落地稳定键（否则两套索引语义并存）；1-2 的锁协议必须对「稳定键」与「下标」两种形态都成立（即锁的覆盖范围不依赖具体键形态） |
| **③-C2（1-3）** | 稳定键落地必须**同时**完成：接口语义（不返回裸指针）、越界/陈旧键处置、`init_indexes()` 幂等、5+5 个变更点清单（契约 C3-5） |
| **③-C3（1-3）** | 必须评估并替换所有「依赖容器顺序」的代码（`roles.back().id + 1`、`*_records.size() + 1`、`users.size() + 1`），逐处列出 |
| **③-C4（1-4）** | 1-4 的原子 SQL 不依赖容器形态，但**内存同步**（把 DB 结果写回内存）必须通过新的受锁 API 完成，不得直接 `users[i].points = ...` |
| **③-C5（通用）** | 索引键形态一旦确定，`models.h` 必须写明「哪些 map 是索引、其键与值的语义、在哪些操作后需要重建」 |

### 被否方案与否决理由

| 方案 | 否决理由 |
|---|---|
| A（保留下标 + 每次重建） | 正确性依赖「穷举变更点」（批次 0 已在这上面失手，见 H-①）；无法消除中间态；O(n) 重建；**且必须靠锁兜底**，与「让正确性不依赖锁的覆盖完整性」背道而驰 |
| B（后缀 -1） | 正确性论证最长且最易错（要处理重复键、跨两个 map、部分失败）；对新增变更点不友好 |
| D（`deque`/`unique_ptr` 但索引仍存下标） | 只解决引用悬垂（B5 的一半），**不解决**下标错位（B6 本身）；`deque::erase` 依然使后续位置前移。可作为方案 C 主存储形态的一个实现选项，但**不能替代**稳定键 |
| 「用 `std::map<std::string,User>` 但保持现有 `roles.back()` 等顺序依赖」 | 会静默改变 ID 生成语义（`admin-01`/`teacher-001` 的序号来源），属功能回归；必须先替换顺序依赖（③-C3） |

---

## 决策④ — `teacher_owns_student()` 的归属语义与落点

### 候选方案

| 方案 | 落点 | 判定依据 | 复用性 | 回归风险 |
|---|---|---|---|---|
| **乙（选定）** | 判定原语 → `models.h`/`models.cpp`；HTTP 薄包装 → `auth.h` | `teacher_classes JOIN classes JOIN users` | 高（读/写路径都能用） | 低（与既有 9 处 SQL 同构） |
| 甲 | 全部放 `auth.h` | 同左 | 中 | 中：`auth.h` 是 header-only，把业务 SQL 放进去会让「鉴权层依赖业务表结构」，且 `auth.h` 已被 t40 占用（同文件串行风险） |
| 丙 | 判定用「绑定班级名集合 ∩ `users.className`」字符串比较（把 `routes_teacher.cpp:64-87` 的逻辑抽成函数） | `SELECT c.name ...` + C++ 侧字符串比对 | 中 | **高**：`users.className` 无外键约束、`classes.name` 有 UNIQUE 但两者靠自由文本连接；不一致即静默失配（这正是 B9 当下「读接口有作用域但可能漏」的形态） |
| 丁 | 只在中间件里做（扩展 `check_permission_middleware`） | — | — | 中间件是「角色级」的通用入口，且**拿不到目标对象 id**（目标 id 在 body/路径里，`req.matches` 只有路径段）；把它做成对象级会误伤大量无关端点 |

### 选定方案：**乙**

**落点（写死）**

```cpp
// models.h —— 判定原语（纯数据谓词，无 HTTP 依赖）
bool teacher_owns_student(const std::string& teacher_id, const std::string& student_id);
bool teacher_owns_class(const std::string& teacher_id, const std::string& class_name);
bool teacher_owns_evaluation(const std::string& teacher_id, int eval_id);

// auth.h —— HTTP 薄包装（失败时写 403 响应并返回 false，与 require_csrf 同构）
inline bool require_teacher_owns_student(const httplib::Request& req, httplib::Response& res,
                                         const std::string& student_id);
inline bool require_teacher_owns_class(const httplib::Request& req, httplib::Response& res,
                                       const std::string& class_name);
```

**为什么判定原语放 `models` 而不是 `auth`**

1. **分层**：`models` 拥有「数据是什么、谁与谁有关系」，`auth` 拥有「HTTP 层的准入与拒绝形态」。`auth.h:180-187` 的 `require_csrf` 已经是这个模式的范本（薄包装 + 写响应 + 返回 bool）。
2. **可复用**：读路径（`routes_teacher.cpp:52-100`、`:376-413`、`:727-777`、`:987-1034`）也需要同一谓词；放在 `auth` 会迫使读路径依赖 `httplib::Request`，或把 SQL 复制两份。
3. **`auth.h` 的并发编辑风险**：`auth.h` 是 t39（本决策）与 t40（决策⑤）的**唯一冲突文件**（契约 §6.2）。把业务 SQL 塞进 `auth.h` 会显著扩大冲突面。

**判定依据：统一为 `teacher_classes JOIN classes`（不用 `className` 字符串）**

已占主导的形态（同一子查询在 `routes_teacher.cpp` 出现 9 次）：

```sql
SELECT 1 FROM teacher_classes tc
  JOIN classes c ON tc.class_id = c.id
  JOIN users   u ON u.className = c.name
 WHERE tc.teacher_id = ? AND u.id = ? AND u.role_id = 3
 LIMIT 1
```

- **现状是两者混用**：`routes_teacher.cpp:31`/`:67`/`:396`/`:747`/`:1003`/`:1152` 用 JOIN，而 `:81-87`、`:563-566` 用「先取班级名集合、再与 `user.className` 比字符串」。**契约要求统一到 JOIN**，判据 `C5-3`（`className` 不再参与 C++ 侧比较）。
- `:563-566` 是 **N+1**（每个学生一次查询）——统一到 JOIN 时顺手可以改成一次查询；**契约不强制**做 N+1 优化（属批次 2 的 N3），但**强制**不得再新增字符串比较。

**归属语义的精确定义（必须一致，避免歧义）**

> `teacher_owns_student(t, s)` 为真 **当且仅当**：存在 `tc.teacher_id = t`，使 `classes[tc.class_id].name == users[s].className` 且 `users[s].role_id = 3`。

- **不含** `classes.head_teacher == 教师姓名` 的判定：`classes.head_teacher` 是**自由文本姓名**（`main.cpp` 的种子为「王老师」），不是教师 id，用它判归属会引入「同名教师」与「改姓名即失权」两类问题。
- **不含**家长关系（那是 `routes_parent.cpp:25-30` 的 `check_same_parent`，独立）。
- **管理员**不受此谓词限制：`/api/admin/users` 等端点继续只做角色级校验（`user:manage`）。

**迁移风险（必须写入结项报告）**

1. **升级即收紧**：若生产库中某些教师账号的 `teacher_classes` 绑定**不完整或缺失**（例如通过直接写库创建、或早期版本未写绑定），升级后该教师将**看不到任何学生**、且所有写操作 403。
   - 代码侧缓解：`routes_teacher.cpp` 的 `my-classes`（`:11-49`）会返回空数组，前端表现为「无班级」——**必须**在报告与 Release Notes 中提示「请先在管理端为该教师勾选班级」。建议同时给出运维 SQL（例如按 `classes.head_teacher` 匹配教师姓名补绑定）作为**可选**措施，但**不得**把它做成自动降级（那就是把 B9 又打开）。
2. **`users.className` 与 `classes.name` 的文本一致性**：JOIN 依赖两者相等。当前 `users.className` 无外键约束，且 `PUT /api/teacher/students/<id>`（`routes_teacher.cpp:204`）可写入任意 `className`。**本批要求**：写入前必须通过 `teacher_owns_class`（目标班）校验，从源头阻止「写进一个不存在的班级名」。**存量脏数据的清理**列入批次 2（需先加外键/CHECK，属 B28 范畴）。
3. **未绑定教师的行为变化**：`routes_teacher.cpp:80-87` 的「未绑定则保留旧行为（看全校）」被删除 → 未绑定教师看到**空列表**。这是**有意的收紧**，必须在报告与前端提示中说明。
4. **`teacher_owns_evaluation`** 需要按评价 id 反查 `student_id` 再判归属（一次 JOIN 查询即可）；`PUT/DELETE /api/teacher/evaluation/<id>` 必须先校验再执行（并配合行数判据返回 404/403 的**正确区分**）。

### 对下游任务的约束（1-5 与相邻）

| 约束 | 内容 |
|---|---|
| **④-C1（1-5）** | 8 个写端点全部接入；**逐点列出** file:line（契约 C5-1） |
| **④-C2（1-5）** | 删除 `bound_class_names.empty()` 兜底（`routes_teacher.cpp:80-87`、`:1152` 附近），读接口一并收紧（C5-2） |
| **④-C3（1-5）** | `POST /api/teacher/students` 与 `PUT /api/teacher/students/<id>` 必须**双向**校验班级（当前所属班 + 目标班），且目标班必须存在（C5-6） |
| **④-C4（1-3）** | 归属判定只依赖 DB（`teacher_classes`/`classes`/`users` 表），**不依赖**内存容器形态 → 与决策③ 的稳定键改造**解耦**；但若实现者想在内存态缓存绑定关系，必须纳入决策② 的同一把锁 |
| **④-C5（1-4）** | `PUT/DELETE /api/teacher/evaluation/<id>` 的 404/403 区分必须与行数判据配合（C4-3 + C5-4），不得用「执行成功」当「已授权」 |
| **④-C6（通用）** | 403 必须显式返回（不得静默返回空数据冒充成功）；错误文案不得泄露对象是否存在之外的信息 |

### 被否方案与否决理由

| 方案 | 否决理由 |
|---|---|
| 甲（全放 `auth.h`） | `auth.h` 是 header-only 且已被 t40 占用（同文件串行）；把业务表结构 SQL 放进鉴权层会使「鉴权」与「数据模型」耦合，读路径复用困难（要么复制 SQL，要么让读路径依赖 `httplib::Request`） |
| 丙（字符串比较） | `users.className` **无外键约束**、`classes.name` 虽 UNIQUE 但两者靠自由文本连接 → 任何拼写/前后空格差异即静默失配；且该写法**已经**与实际 JOIN 混用（当前两种写法并存正是 B9 拖到批次 1 的原因之一）。**禁止**继续扩散 |
| 丁（只在中间件里做对象级） | 中间件拿不到目标对象 id（目标 id 在 body 或 `req.matches` 中，且各端点形态不同）；把对象级校验做进通用中间件会误伤无关端点，并迫使每个端点都改中间件签名 |
| 「用 `classes.head_teacher` 判归属」 | 字段是**姓名文本**不是教师 id；同名教师、改姓名、管理员代管等情形都会出错；且与既有 `teacher_classes` 关联表的设计意图冲突 |

---

## 决策⑤ — B4 客户端标识（XFF 信任边界）与 `login_attempts` 并发保护

### 必须先纠正的一处事实（审计结论被 vendored 源码推翻）

审计在 `CODE_QUALITY_SUMMARY.md` §五 B10 行与 `CODE_QUALITY_REVIEW.md` 中称：

> 「httplib **从不设** `REMOTE_ADDR` → 键退化为 `unknown|<user>`，可伪造绕过」

**实测相反**：

```
httplib.h:3639   req.set_header("REMOTE_ADDR", strm.get_remote_addr());
```

httplib 的 `Server::routing` 在分发前**确实**注入 `REMOTE_ADDR`。因此 `auth.h:148-151` 的 `REMOTE_ADDR` 回退分支**是可达的**，B4 的实际失效模式是：

1. **XFF 可伪造**（`auth.h:143-147` 无条件取 XFF 的**第一段**）→ 每个请求换一个值即换一个锁定键 → 爆破无限；
2. **家长端完全无锁定**（`routes_parent.cpp:49-77`）；
3. `unknown|<user>` 退化**仍可能**在 `get_remote_addr()` 返回空时发生（须防御，但不得再把它写成「框架从不设置」）。

本决策据此调整判据：重点是 (1)+(2)，并保留 (3) 的兜底。

### 候选方案

| 方案 | 形态 | 抗伪造 | 抗反向锁死 | 破坏性 |
|---|---|---|---|---|
| **丁（选定）** | `trust_proxy_headers`（默认 **false**）+ `trusted_proxies` 白名单；默认**忽略** XFF 用 `REMOTE_ADDR`；开启且命中白名单时取 **XFF 最后一跳**；**IP 维度 + 账号维度双键** | 高 | 高（账号维度独立计数） | 低（默认行为对单机部署**无变化**） |
| 甲（现状） | 无条件信任 XFF 第一段 | **无** | 低 | 无 |
| 乙 | 直接**删除** XFF 支持 | 高 | 低 | 中（反向代理/负载均衡部署下所有请求的 IP 都变成代理 IP → 全局共享一个键 → 一人失败锁所有人） |
| 丙 | 全局封 IP（如 iptables/计数封禁） | 中 | 低 | 高（NAT 后的正常用户被连坐） |
| 戊 | 用 `X-Real-IP` 替代 XFF | 无 | 低 | 中（同样是客户端可设头） |

### 选定方案：**丁**

**为什么「信任边界」必须是显式配置而不是「取最后一跳」就够**

- 取最后一跳能防「客户端直接伪造多段 XFF」，但**不能**防「客户端直接把 XFF 设成任意单值」：单机直连部署下，攻击者发 `X-Forwarded-For: 1.2.3.4` 就能让服务端把 1.2.3.4 当成客户端 IP。**唯一正确的边界是「只有当我们确信请求来自我们自己的代理时才解析该头」**，即：默认关闭 + 白名单。
- 默认关闭对**当前部署无影响**：`config.json` 的部署形态是「后端直接 serve 前端 dist、同源」（`routes_static.cpp`），不存在前置代理；因此默认忽略 XFF 不改变任何现有行为（`REMOTE_ADDR` 即真实客户端）。

**选定形态（写死）**

```cpp
// config.h  ServerConfig（新增字段；不得改动既有字段语义）
bool trust_proxy_headers = false;          // 默认 false：忽略一切客户端可设的地址头
std::vector<std::string> trusted_proxies;  // 可信代理地址/网段白名单；为空时即使开关为 true 也不采信 XFF
// load_config()：在 security 段读取
//   config.trust_proxy_headers = cfg["security"].value("trust_proxy_headers", false);
//   config.trusted_proxies = cfg["security"]["trusted_proxies"].get<std::vector<std::string>>()（存在时）
```

```cpp
// auth.h  login_client_key() 的新语义
// 1) 取客户端地址 remote = req.get_header_value("REMOTE_ADDR")
// 2) if (trust_proxy_headers && remote 命中 trusted_proxies) {
//        std::string xff = req.get_header_value("X-Forwarded-For");
//        auto pos = xff.rfind(',');            // 取「最后一跳」= 最靠近我们的可信代理写入的那段
//        remote = (pos == npos) ? trim(xff) : trim(xff.substr(pos + 1));
//    }
// 3) if (remote.empty()) remote = "unknown";   // 保留兜底；首次命中时记一次 Logger::warning
// 4) 返回两个键（或由调用方分别计数）：
//      ip_key   = remote + "|" + username
//      acct_key = "acct|" + username
```

**双键语义（必须实现）**：`login_can_try` / `login_record_fail` / `login_record_success` 必须对 **ip_key 与 acct_key 同时**生效 —— 任一被锁定即拒绝。这样：
- 攻击者换 IP → `ip_key` 变了，但 `acct_key` 累积（目标账号必被锁）；
- 攻击者用**别人的** XFF 反向锁死某账号 → 仍会发生（账号维度的固有代价），但**只能锁自己索要的那个账号**，且锁定期与阈值可配置；这是「以可用性换抗爆破」的**有意取舍**，必须在报告中写明（业界通行做法）。
- 反向锁死的缓解（本批不做、列入批次 2）：把「同 IP 的失败」与「跨 IP 的失败」用不同阈值；或引入指数退避而非硬锁定。

**`login_attempts` 的并发保护方案（与决策② 对齐）**

- **采用决策② 的同一把锁**（不新开锁），满足「全项目只有一把状态锁」的纪律（决策② 的 R1）。
- 三个函数各自**原子**完成；**推荐**合并为一次带锁的复合调用，以消除「`login_can_try` 返回 true → 别的线程把它锁上 → 我们仍然尝试」的窗口：
  ```cpp
  // auth.h（推荐形态）
  enum class LoginAttemptResult { Allowed, Locked };
  LoginAttemptResult login_begin_attempt(const std::string& ip_key, const std::string& acct_key);
  void login_record_fail(const std::string& ip_key, const std::string& acct_key);
  void login_record_success(const std::string& ip_key, const std::string& acct_key);
  ```
- **上限与清理**（防内存 DoS）：条目数上限（建议 10000）与过期清理（仅清 `locked_until <= now` 的条目；**绝不得**清理仍处于锁定期的条目）。
- **家长端接入**：`routes_parent.cpp:49-77` 增加与 `routes_public.cpp:29-60` **完全同构**的调用点（4 个）。

### 对下游任务的约束

| 约束 | 内容 |
|---|---|
| **⑤-C1（1-6）** | 默认 `trust_proxy_headers = false`；`config.json` 默认关闭；XFF 解析必须位于该判断之内 |
| **⑤-C2（1-6）** | 双键（IP + 账号）同时计数与判定；三处（`/api/auth/login`、`/api/parent/login`）全部接入 |
| **⑤-C3（1-6）** | `login_attempts` 使用决策② 的同一把锁；不得新增第二把锁；条目数上限 + 过期清理（不清理锁定中的条目） |
| **⑤-C4（1-6）** | 开启代理信任时只取**最后一跳**；`trusted_proxies` 为空时**即使开关为 true** 也不采信 XFF（白名单是必要条件） |
| **⑤-C5（1-6）** | 必须记录「`REMOTE_ADDR` 为空」这一退化情形（`Logger::warning`），便于发现代理配置缺失 |
| **⑤-C6（通用）** | 本项**不**引入数据库表；锁定状态的内存性质与重启清零必须在报告中如实标注（批次 2 的持久化） |
| **⑤-C7（与 1-7 的顺序）** | `auth.h` 同时被 t39（决策④ 的包装函数）与 t40（本决策）修改 → **串行执行**，见契约 §6.2 |

### 被否方案与否决理由

| 方案 | 否决理由 |
|---|---|
| 甲（现状：无条件信任 XFF 第一段） | 换一个 XFF 即换一个键 → 锁定形同虚设；这是 B4 的主要失效模式 |
| 乙（删除 XFF 支持） | 反代/负载均衡部署下 `REMOTE_ADDR` 全是代理地址 → 所有用户共享一个键 → **一人失败锁全场**。不是「更安全」，而是把爆破防护变成拒绝服务 |
| 丙（全局封 IP） | NAT / 校园网出口 IP 共享极普遍 → 连坐正常用户；且不解决「目标账号被反复爆破」 |
| 戊（用 `X-Real-IP`） | 同为客户端可设头，问题原样存在；只是换了个名字 |
| 「只取最后一跳但无条件信任」 | 见上文：单机直连部署下仍可被任意单值伪造 |
| 「给锁定单独开一把锁」 | 违反决策② 的 R1（单一锁）；且登录路径同时访问用户容器与 locking map，两把锁需要顺序规则 |
| 「把锁定放到 `sessions`/新表里」 | 是更彻底的方案，但引入 schema 迁移与每请求 DB 往返；本批取内存 + 单一锁 + 上限清理；**持久化列入批次 2** |

---

## §6 与批次 0 既有决策的对应关系（避免与 BATCH0_DESIGN_DECISIONS.md 的 5 项混淆）

`docs/audit/BATCH0_DESIGN_DECISIONS.md` 的 5 项决策是**批次 0 的**（B1 存量处置 / B2 落库形态 / F13 处置 / B6 取向 / …）。本文件是**批次 1 的** 5 项，二者互不替代。对应关系：

| 批次 0 决策 | 内容 | 在批次 1 中的落点 |
|---|---|---|
| ① B1 存量 `pbkdf2$` 处置 | 视为失效 + 删除自动升级链 | 已由 T7 落地；批次 1 **不涉及**（`routes_public.cpp:46-52` 已是注释说明） |
| ② B2 落库形态 | `users.must_change_password` 单列 + 双中间件门禁 | 已由 T7 落地；批次 1 **必须不得回退**（契约 §6.3 红线 3） |
| ③ F13 处置 | 登录页按钮收进 mock 门控 | 已由 T22 落地；批次 1 不涉及 |
| ④ B6 取向 | 批次 0 取「erase 后 clear + 重建」；**彻底版留批次 1-3** | **本文件决策③** 就是那个彻底版 |
| （批次 0 无第五项） | — | 本文件决策①②⑤ 是批次 1 新增的决策 |

---

## §7 变更流程（强制）

1. 任何实现者若发现某项决策与源码现状冲突（例如 `models.h` 的接口形态受限），**必须先回报 t35** 并说明冲突点与建议替代，**不得自行改判**。
2. 决策变更必须：更新本文件对应小节（含被否方案与理由）+ 同步更新 `BATCH1_CONTRACT.md` 的相关判据编号 + 在变更记录中写明「谁在何时因何改」。
3. **不得**以「实现更方便」为由删除任一判据；可以调整判据的**形式**（更可判定），但必须保持**同一语义**。
4. 本文件与 `BATCH1_CONTRACT.md` 一旦被下游引用（t36 起），其判据编号 `C1-…`～`CB-…` 即为稳定标识；新增判据使用新编号，不得重编号既有编号。

---

## §8 只读性自证

- 本次只新增 `docs/audit/BATCH1_CONTRACT.md` 与 `docs/audit/BATCH1_DESIGN_DECISIONS.md` 两份文档。
- **未修改**任何 `*.cpp` / `*.h` / `config.json` / `.github/**` / `frontend/**`；未执行 `git commit` / `git push` / `git rm`。
- 判据：`git status --porcelain` 的输出仅包含两份新文档与既有未跟踪项（`?? .agent-teams/`）。
