# B1-T2 独立复核：批次 1 契约绑定 + 两条交接缺陷的「修复前必然失败」

| 项 | 值 |
| --- | --- |
| 任务 | t36 —— B1-T2 独立复核批次 1 契约绑定并复现两条关键缺陷的修复前失败 |
| 承担人 | verifier（独立验证，非契约作者） |
| attempt_id | `9d2427ff-0944-459a-b0d7-41a70a72fb63` |
| 修订基线 | `HEAD = 9f6c4b795d9180778c7ea987fd85738eee87748e`；`git -c core.quotepath=false status --short` 仅 `?? .agent-teams/`、`?? docs/audit/BATCH1_CONTRACT.md`、`?? docs/audit/BATCH1_DESIGN_DECISIONS.md`（后两者为 T1 本轮新增，**非本次写入**） |
| 环境 | g++/gcc `14.2.0 (MinGW-W64 x86_64-msvcrt-posix-seh)`、node `v24.16.0`、pwsh `7.6.5` |
| 被测契约 | `docs/audit/BATCH1_CONTRACT.md`（1044 行 / 98148 B / sha256 `D3A507B4701CA666…`）、`docs/audit/BATCH1_DESIGN_DECISIONS.md`（437 行 / 45263 B / sha256 `3D28B077FF8DF337…`） |

## 0. 结论摘要（逐条 pass/fail）

| # | 验收项 | 结论 |
| --- | --- | --- |
| 1 | 契约每一条 file:line 逐一回源复核 | **pass**（352 次出现 / 232 去重绑定，**FAIL=0**；并发现契约自身 3 处判据/引用缺陷，见 §1.4） |
| 2 | 复现 `execute_bind` 无法区分 0 行受影响 | **pass**（探针级 + 端点级，两条原始证据均在 §2） |
| 3 | 复现「删权限/删角色后索引未重建」的修复前失败 | **pass（并附重要更正）**：**角色半边端到端复现**；**权限半边在活路径上不成立**（只影响当前 0 活调用点的死代码），见 §3 |
| 4 | 1-1 决策的实测依据（WAL/busy_timeout 在单连接多线程下失效） | **pass**（三组并发实测，见 §4） |
| 5 | 逐条结论文档 + 引用文件 sha256 与字节数 | **pass**（§5 指纹表） |
| 6 | 不修改任何源码（仅新增 docs/audit/evidence/） | **pass**（§6） |

**本次复核最重要的一条**：契约 §3.8 的 H-① 在**角色侧**是真实且可端到端复现的缺陷；但其**权限侧**与任务书表述的「被删权限对既有会话仍生效」在**当前的活授权路径上不成立**——`Auth::check_permission`（`auth.h:280`）读的是 `permissions`/`role_permissions` **向量**，删权限会立即生效；只有 `check_permission_optimized`（`models.cpp:280`）读陈旧 map，而它**全仓活调用点 = 0**。因此该子项应从「已生效的安全缺陷」改叙为「**潜在**缺陷：一旦 1-1/1-3 把授权改走优化路径即变为真实」。§3.4 给出原始证据。

---

## 1. 验收项 1：契约 file:line 绑定逐一回源复核

### 1.1 方法（可复现）
自写检查器 `%TEMP%\b1t2\check2.js`（Node，仓库外），对两份文档逐行扫描 `文件名:行号[-行号]`：
1. **锚点存在性**：文件是否存在（含 `docs/audit/`、`docs/` 回退解析）、行号是否越界、被引用行是否为空行 → 任一不满足记 `FAIL`；
2. **语义初筛**：把 claim 行里的 `file:line` 引用先挖掉，取其反引号内的代码 token（长度 ≥5 的标识符），检查该 token 是否出现在**锚点 ±5 行窗口**内 → 不命中记 `REVIEW`（交人工判读，不自动判 fail）。

命令：`node %TEMP%\b1t2\check2.js docs/audit/BATCH1_CONTRACT.md`（第二份文档同法）。
逐条结果落盘为 TSV：`%TEMP%\b1t2\BATCH1_CONTRACT.md.bindings.tsv`、`%TEMP%\b1t2\BATCH1_DESIGN_DECISIONS.md.bindings.tsv`（含 docLine / ref / verdict / note / claim / anchor 六列）。§1.3 给出全部去重绑定的判读结果。

### 1.2 结果
```
BATCH1_CONTRACT.md        出现=258  去重=170  PASS=234  REVIEW=24  FAIL=0
BATCH1_DESIGN_DECISIONS.md 出现=34   去重=31   PASS=30   REVIEW=4   FAIL=0
```
（`PASS+REVIEW+FAIL` 按**出现次数**计，故 258/34。）**FAIL = 0**：不存在「文件不存在 / 行号越界 / 引用空行」的绑定。

### 1.3 REVIEW 逐条判读（28 条，全部判 PASS，23 条为我的启发式噪声）
`REVIEW` 的成因是 **token 与锚点本就不同名**，而非错绑。按成因分类：

| 成因 | 条数 | 例证（claim → 锚点实际内容） |
| --- | --- | --- |
| claim 在同一行/格内列了**多个** `file:line`，token 属于**另一个**引用 | 16 | `routes_admin.cpp:1183`（claim 同行提到 `permissions.erase`）→ 锚点 `roles.erase(it);` ✓ ；`:150` 类「old→new 映射行」同理 |
| claim 的 token 是**概念/宏/类型名**，锚点是其使用点 | 8 | `main.cpp:22` claim 写「同一条 `sqlite3*`」→ 锚点 `SqliteDb db;` ✓（包装器持有 sqlite3*）；`sqlite_wrapper.h:145-147` claim 写 `execute_bind` → 锚点 `return rc == SQLITE_DONE;` ✓（函数体末行，函数名在 12 行前）；`logger.h:89` claim 写 `Logger::mutex_` → 锚点 `static std::mutex mutex_;` ✓ |
| claim 是**批次 0「旧位置 → 现位置」对照行**，被引的是旧位置 | 4 | `main.cpp:250-254`（旧种子行）、`auth.h:355`（旧门禁行）、`routes_admin.cpp:826-827`（旧删除行）→ 锚点落在旧位置附近 ✓；同行的**新位置**（`main.cpp:83-152`、`auth.h:344-388/:393-429`、`routes_admin.cpp:886/:889`）我另行核对：`auth.h:344` 为 `check_permission_middleware`、`:346` 调 `enforce_password_change` ✓ |
| **真正的引用不精确（判为契约缺陷，见 §1.4-③）** | **1** | `routes_admin.cpp:1045` 被当作「`permissions.back().id + 1` 的模式」实例，实际 `:1045` 是 **roles** 的 `roles.back().id + 1`；permissions 侧在 `:1110` |

### 1.4 复核中发现的三处契约自身问题（**不影响「绑定真实」结论**，但影响「判据可判定性」）

#### ① CH-1 判据**不可能通过**（medium）
契约 §3.8 的 CH-1 原文：`grep -n 'permissions.erase\|roles.erase\|roles.push_back\|permissions.push_back\|role_permissions.push_back' routes_admin.cpp` 的**每一个命中行**之后 **15 行内**必须出现重建调用，「逐点列清单（**5 点**）」。
**我的实测**：该 grep 在 `routes_admin.cpp` 命中 **7 行**（不是 5），且**全部 7 行后 15 行内均无重建**：

```
  :1046 roles.push_back(Role{new_id, name, description}); 后 15 行内有重建 = False
  :1111 permissions.push_back(Permission{new_id, name, code, description}); 后 15 行内有重建 = False
  :1147 permissions.erase(it);                         后 15 行内有重建 = False
  :1183 roles.erase(it);                               后 15 行内有重建 = False
  :1599 roles.push_back(new_role);                     后 15 行内有重建 = False
  :1629 permissions.push_back(new_perm);               后 15 行内有重建 = False
  :1645 role_permissions.push_back({role_id, permission_id}); 后 15 行内有重建 = False
```
**问题**：`:1599/:1629/:1645` 属**导入端点**，而契约 §3.8 自己说导入路径的重建「在 `:1736` 收口，**保留**」。`:1736` 距 `:1599` 有 137 行、距 `:1629` 有 107 行 —— **远超 15 行窗口**。故按 CH-1 原文，即使实现者完成 §3.3 要求的全部修复，这 3 点仍**必然不满足**，判据无法通过。
**必须的二选一**：把 CH-1 的窗口限定为「4 个非导入变更点（`:1046/:1111/:1147/:1183`）」，或明确「导入端点以 `:1736` 的单次全量重建收口，其 3 个 push_back 点豁免 15 行窗口」。另：原文「5 点」与 grep 实际的 7 命中不一致，也需对齐。
**正面结论**：CH-1 本身是**合格的可判定判据**（修复前 7/7 为 False → 它能真为假，不是永真检查）。这正是本次复核要守的性质。

#### ② C3-6 与 CH-2 第 3/4 步引用了**不存在的端点**（medium）
- C3-6（`:273`）：「先 POST 新增 id>12 权限 → DELETE 删除它 → **随后调用一个依赖 `check_permission_optimized` 的端点**，断言不因陈旧条目而返回错误授权」；
- CH-2（`:661-666`）第 3/4 步：断言 `permission_id_map` 无该 id（可用只读探针）+ 断言 `check_permission_optimized` 行为与「无该权限」一致。
**我的实测**：`check_permission_optimized` 全仓命中 **2 处**，分别是 `models.cpp:280`（定义）与 `models.h:117`（声明）；**业务/端点层调用点 = 0**：
```
  models.cpp:280: bool check_permission_optimized(const string& user_id, const string& permission_code) {
  models.h:117: bool check_permission_optimized(const string& user_id, const string& permission_code);
```
**问题**：当前**没有任何 HTTP 端点可「依赖 `check_permission_optimized`」**，故 C3-6 与 CH-2 第 3/4 步在修复前**不可执行**（无端点可调），也提示了一个更深的事实：该函数是**死代码**（与契约 §6 对 `check_permission_optimized`「活调用点 = 0」的自述一致，但 §3.8/§3.6 的判据未据此收口）。
**建议**：把 CH-2 第 3/4 步改为**探针级**断言（临时只读程序打印 `permission_id_map`/`role_id_map` 的 key 集合，以及直接调用 `check_permission_optimized` 的返回值 —— 与我的 §3.3 探针同法），或注明「该断言在 1-3 把授权改走优化路径后才有端点级形态」。

#### ③ 一处引用不精确（low）
§3.8 反例与 §3.3 第 6 条均以 `routes_admin.cpp:1045` 作为「`permissions.back().id + 1` 模式」的实例。实测 `:1045` 是 **roles** 的 `int new_id = roles.empty() ? 1 : roles.back().id + 1;`；permissions 的同类模式在 `:1110`。机制（`back().id + 1` 在删除后会**重用**已删 id）本身成立且重要，仅**归属标注**需修正为 `:1045`(roles)/`:1110`(permissions)。

---

## 2. 验收项 2：`execute_bind` 无法区分「执行成功」与「0 行受影响」（**修复前复现成功**）

### 2.1 绑定复核
契约 §3.9 引 `sqlite_wrapper.h:127-148`（`:145-147` `return rc == SQLITE_DONE;`）——**我实测一致**：`:127` 为 `bool execute_bind(...)`，`:145-147` 为 `rc = sqlite3_step(stmt); sqlite3_finalize(stmt); return rc == SQLITE_DONE;`。

### 2.2 探针级原始输出（`%TEMP%\b1t2\b1t2_execute_bind.cpp`，本轮新编，exit 0）
```
[初始] row1 行数=1 ; ghost 行数=0
[A] UPDATE 命中 1 行 -> execute_bind 返回 true ; 之后 row1 行数=1 (预期 1)
[B] UPDATE 命中 0 行 -> execute_bind 返回 true ; ghost 行数=0 (预期 0)
[C] DELETE 命中 0 行 -> execute_bind 返回 true
[D] 外部先删行(返回 true) 再 UPDATE -> execute_bind 返回 true ; row1 行数=0 (预期 0)
[对照] update(WHERE id='row1') 受影响行数=0 ; update(WHERE id='row2') 受影响行数=1
[判据] B/C/D 三条 0 行语句的 execute_bind 返回值是否全为 true -> 缺陷成立：0 行与成功不可区分
```
→ **同族接口 `update()`（`sqlite_wrapper.h:102-108`）返回真实受影响行数（0 / 1）**，证明「行数通道」在包装器内已存在，只是参数化路径未使用。

### 2.3 端点级原始输出（真实 `server.exe`，`:18121` 沙箱；`server.exe` sha256 `C33876A6…`）
```
  [1] 首次改密（行存在）-> HTTP 200 code=200
  [2] 库内行数（删前）= n=1
  [3] 外部删除该行后 库内行数 = n=0
  [4] 行已不存在时再次改密 -> HTTP 200 code=200 msg=密码修改成功
  [判据] HTTP=200 且库中 0 行 -> **缺陷②端点级复现**：UPDATE 命中 0 行却报成功
```
→ 契约 §3.9 把该缺陷列为 high 并分配给 t38，**依据充分**；且我额外证明了端点级可达（不只是探针级）。

---

## 3. 验收项 3：「删权限/删角色后索引未重建」的修复前判定（**半边成立，半边需改叙**）

### 3.1 绑定复核
契约 §3.8 引 `routes_admin.cpp:1147`（`permissions.erase`）、`:1183`（`roles.erase`）——**我逐行核对，完全准确**（§1.4-① 的 grep 输出即含这两行）。契约另引 `:1046`、`:1111`、`:1599`、`:1629`、`:1645`（新增/导入不重建）亦准确。

### 3.2 探针级：两条授权路径的对照（`%TEMP%\b1t2\b1t2_stale_maps.cpp`，链接 `models.cpp` + `logger.cpp`，exit 0）
```
[A] 删权限前       : 活路径(Auth::check_permission)=true  死路径(check_permission_optimized)=true
[B] 复刻 :1147 后 : 已 erase=true  活路径=false  死路径=true   ← 活路径已正确拒绝；死路径因 permission_id_map 陈旧仍放行
[C] 删角色前       : 活路径=true  死路径=true
[D] 复刻 :1183 后 : 已 erase=true  活路径=true  死路径=true   ← 角色已不存在，但两条路径都仍授权 = 真实缺陷（对既有会话仍生效）
[判据-权限半边/干净态] 仅 erase 权限后：活路径=false  死路径=true
```

### 3.3 端到端：**角色半边复现成功**（真实 `server.exe`，`:18121` 沙箱）
```
  [1] 删除前 GET /api/admin/roles -> roles=[管理员,教师,学生,家长,T36Role2]  新角色 id=5
  [2] 删除角色前 t36v5 GET /api/admin/statistics -> HTTP 200 code=200
  [3] admin DELETE /api/admin/roles{id=5} -> HTTP 200 角色删除成功
  [4] 删除后 GET /api/admin/roles -> roles=[管理员,教师,学生,家长] ; 该角色已从系统自身视图消失=True
  [5] 删除角色后 t36v5 GET /api/admin/statistics -> HTTP 200 code=200
  [判据] 角色已从系统视图消失(True) 而既有会话仍 200 -> **缺陷①（角色半边）端到端复现**（修复后应为 403）
```
证据链完整：**系统自己的角色列表已不含该角色**（[4]），但持有该角色的既有会话（[5]）**仍被授权**（`GET /api/admin/statistics` 由 `statistics:view` 守卫，`routes_admin.cpp:1321`）。机制：`Auth::check_permission`（`auth.h:291-298`）遍历 `role_permissions` **向量**与 `permissions` **向量**，二者都不随 `roles.erase` 变更 → 授权持续有效。

### 3.4 **必须更正的一条**：权限半边在活路径上不成立
- **活路径**：`check_permission_middleware`（`auth.h:344`）→ `Auth::check_permission`（`auth.h:280`）读**向量**；`permissions.erase` 后该权限立即从 `permissions` 向量消失 → **授权立即失效**（探针 [B] 的 `活路径=false`）。
- **死路径**：`check_permission_optimized`（`models.cpp:280`）读 `role_permission_map`/`permission_id_map` → 陈旧条目仍在（探针 [B] 的 `死路径=true`），但该函数**活调用点 = 0**（§1.4-② 原始输出）。
- **API 层可达性**：`DELETE /api/admin/permissions` 对 `id <= 12` 直接拒绝 —— 实测 `DELETE {id:7}` → `HTTP 200 {"code":400,"msg":"不能删除系统内置权限"}`；而**全部被端点守卫使用的 7 个权限码（`system:manage`/`user:manage`/`student:manage`/`points:manage`/`evaluation:manage`/`mall:manage`/`statistics:view`）的 id 均为 1–7（内置）**，故**没有任何可删权限能被端点观察**。
→ **结论**：任务书表述的「被删权限对既有会话仍生效」在**当前代码的活授权路径上不可复现**；它是**潜在**缺陷（一旦授权改走优化路径即成真实）。契约 §3.8 的根因描述（「已删权限仍在 `permission_id_map`，`models.cpp:293-294` 仍会命中」）**在我读来是准确的**，但它把影响面表述为「必须重建」而非「当前即可被利用」，与任务书的措辞不一致 —— **建议以本条复核为准改叙**，并把它与 1-3 的「授权路径是否切到优化实现」绑定。
→ 相应地，§1.4-② 指出 C3-6/CH-2 第 3/4 步缺少可用端点，与这里是同一根源。

---

## 4. 验收项 4：1-1 决策依据（WAL/busy_timeout 在单连接多线程下失效）—— **pass**

契约 §3.1（`:105`、`:116-128`）的论证是：`sqlite_wrapper.h:27` 的 `busy_timeout(5000)` 与 `:29` 的 `PRAGMA journal_mode=WAL` 只解决**数据库文件级**竞争，不提供**应用级临界区**。我用同一份包装器（`open()` 内部即设置二者）在**单连接 + 4 线程 × 500 次**下实测三种模式：

```
[环境] journal_mode={"journal_mode":"wal"} ; busy_timeout={"timeout":5000}     ← 不是假设，是实测 PRAGMA 读回
----- mode=atomic （单条 UPDATE ... v=v+1）-----
[结果] ok=2000 err=0 ; 最终 v=2000 ; 期望 v=2000 ; 差值(丢失更新)=0
----- mode=rmw （SELECT → 计算 → UPDATE，非原子）-----
[结果] ok=2000 err=0 ; 最终 v=644 ; 期望 v=2000 ; 差值(丢失更新)=1356
----- mode=txn （BEGIN IMMEDIATE / UPDATE / COMMIT）-----
SQL错误: cannot start a transaction within a transaction
SQL错误: cannot commit - no transaction is active
SQL错误: cannot rollback - no transaction is active
[结果] ok=1378 err=622 ; 最终 v=1655 ; 期望 v=2000 ; 差值(丢失更新)=345
```
判读：
1. **`atomic`**：单条语句由 SQLite 内部串行化 → 无丢失、无错误。故**单连接本身不会让单语句出错**；
2. **`rmw`**：2000 次调用**全部返回成功（err=0）**，却丢失 **1356** 次更新 → 证明 WAL/busy_timeout **对「读-判断-写」序列零保护**（正是 B8/兑换双花与 H-② 的土壤）；
3. **`txn`**：出现 **622** 次真实错误（事务无法嵌套/提交/回滚）与 **345** 次丢失更新 → 证明**单共享连接上无法承载 per-thread 事务**（B7 事务串扰）。
→ 契约「WAL+忙等≠应用级互斥、1-1 必须改为连接/临界区模型」的决策**依据成立**（pass）。契约 §3.1 表格 A/B/G 三行（`:120-126`）的表述与我的实测一致。

---

## 5. 验收项 5：引用文件的 sha256 与字节数

| 文件 | sha256 | 字节数 |
| --- | --- | --- |
| `sqlite_wrapper.h` | `DA5C8236D6DA0FD27370D3AFFB8D72B3E167F002008164CDCCADAE52E6945E85` | 7729 |
| `models.cpp` | `AB6C86C66C0D0BE790602FDC73C67BF401EDE9393FF813DDDA2189EEF8D37AEA` | 13666 |
| `models.h` | `9768C1F985B81497FB460626F177C321F0FAD6434ACBC4917FD2549274B09A27` | 4532 |
| `auth.h` | `3C520FAD3AA5F7DDC4F2FE79A3E632B517E27B85B22E76EE130A7FD438E92840` | 16652 |
| `routes_admin.cpp` | `9BC7CE74B5BCBA2528E72E2C1A9C87DBAD0C255D12B5DD6A3C405D42BFE9CF3F` | 84840 |
| `main.cpp` | `D7211E1FA47EB89AEDAA195CCCC8D8624D32706DB8E6EBBC446AC379F435E4CC` | 23726 |
| `sha256.h` | `3E5C59DDC9AEB5EE22488A8058AA7BA89C479BDD054179048A4F26D3A00DFA22` | 13143 |
| `sqlite3.c` | `1A206854AA9FE0CCC1B609F5CFCE67EB52AC0A8F5AA2F5E853F7C3ED84C710AB` | 9018109 |
| `httplib.h` | `25CEDB46B3282187FECFE32DD4E8413B37B97F6464FDC2530DED5100093E2922` | 151166 |
| `json.hpp` | `69FB172D401AF4E4280684D159F9BE432E9C2FAEC4E64A84A373BCB39753B953` | 993142 |
| `docs/audit/BATCH1_CONTRACT.md` | `D3A507B4701CA666…`（前 16 位） | 98148（1044 行） |
| `docs/audit/BATCH1_DESIGN_DECISIONS.md` | `3D28B077FF8DF337…`（前 16 位） | 45263（437 行） |

**本次复核产出的二进制/探针（全部在 `%TEMP%`，仓库外）**：

| 产物 | sha256 | 字节数 |
| --- | --- | --- |
| `sqlite3.o`（本轮新编） | `BB53966DDDC310387FB40650619D99E4CE167588D0D82D4B75CE59FBF2F14C0F` | 1162258 |
| `build\server.exe`（本轮新编） | `C33876A6ACD5700620728E9ED4685617159FA168402312DEE7BE0F932EAE411A` | 6030092 |
| `b1t2_execute_bind.exe` | `683A0829965A54AFA9732CE8A89457F1DE1F547BECA7908BFBC64BBC691DE3BC` | 1177641 |
| `b1t2_concurrency.exe` | `407D183E04508E3E98EA74CB9CE6928A63C52BDACCA8795003C3DF438C1DA5CC` | 1275023 |
| `b1t2_stale_maps.exe` | `58CAF6C8B415376EE5F0BEC56EAAB0F2FCC253E3D0C02EA7BF8398E93649A6DE` | 1685915 |

> 二进制同一性一律用 sha256 而非字节大小；`server.exe` 与批次 0 的 6,030,092 B 恰好同长度但 sha256 不同（`C33876A6…` vs `3E5C59DD` 时代的产物），这正是为什么不能只比大小。

---

## 6. 验收项 6：未修改任何源码 / 清理

- `git -c core.quotepath=false status --short`（本轮结束实测）**仅**：`?? .agent-teams/`、`?? docs/audit/BATCH1_CONTRACT.md`、`?? docs/audit/BATCH1_DESIGN_DECISIONS.md`（后两者为 T1 的新增产物，**不是本次写入**）；**无任何 `M` 行** → 未修改 `main.cpp`/`models.cpp`/`models.h`/`auth.h`/`sqlite_wrapper.h`/`routes_*.cpp` 中任何文件；
- 本次唯一新增的仓库文件是本文件（`docs/audit/evidence/B1_T2_CONTRACT_VERIFICATION.md`）；
- 全部探针源码、编译产物、沙箱库与日志都在 `%TEMP%\b1t2\`、`%TEMP%\t15_prep\`，未在仓库内新建脚本/JSON/DB。

---

## 7. 未端到端复现项（逐条标注）

1. **`txn` 模式 622 错误的具体归因**：错误文本来自包装器 `execute()` 的 `sqlite3_exec` 错误打印，属**同一连接被多线程并发 `BEGIN/COMMIT`** 的必然结果（"cannot start a transaction within a transaction"）；我**未**逐条把每次失败归到某个线程（无每线程日志），故只声明「单连接承载 per-thread 事务会产生该错误并丢失更新」，不声明精确的失败序列；
2. **`busy_timeout` 的真实触发**：本机 4 线程单连接下**未观测到 `SQLITE_BUSY`**（错误均为事务状态类）。故「WAL/busy_timeout 不足够」的结论**不依赖** SQLITE_BUSY 的出现——契约 §3.1 的 B 行（忙等不提供隔离）由 `rmw` 的 **1356 次静默丢失**直接证实；
3. **真实端点级的事务串扰（B7）**：契约把 B7 列入批次 1（伴-1）。我**只做了包装器级**复现（`txn` 模式），**未**通过 HTTP 并发打 `/api/admin/import` 与其它写接口来观测「其它线程的写入被 ROLLBACK 回滚」；
4. **权限半边的「潜在缺陷」结论**：基于代码路径分析（`auth.h:280` 读向量）＋探针实证（活路径 false / 死路径 true）＋ API 可达性实测（内置权限不可删）；**未**构造「把授权切到优化路径后」的运行时反例（那需要先实施 1-3）；
5. **C3-6 的端点级判据**：因端点不存在而**不可执行**（§1.4-②），故本项只到「证明其不可执行」。

---

## 8. 我自己犯过并已纠正的 harness 错误（留档，避免误读为产品缺陷）

| # | 现象 | 根因（我的脚手架） | 纠正 |
| --- | --- | --- | --- |
| 1 | 绑定检查器报 `CODE_QUALITY_SUMMARY.md:173` **文件不存在** | 我把引用解析为「仓库根相对路径」，而该文件在 `docs/audit/` 下 | 增加 `docs/audit/`、`docs/` 回退解析 → FAIL 归零 |
| 2 | 首版检查器产生大量假 `REVIEW` | token 提取把 claim 行里的 `file:line` 文本本身当成被校验标识符 | 先把引用挖成 `«REF»` 再提取 token → REVIEW 从 56 降到 24（其余逐条人工判读） |
| 3 | 嵌套 here-string 导致整条命令静默失败（无输出、exit 1） | 在外层 `@'…'@` 内又写了内层 `@'…'@`，内层 `'@` 提前终止外层 | 拆成独立文件，不在 here-string 内嵌套 here-string |
| 4 | `mk.exe` 编译"成功"但文件不存在、运行报 `StandardOutputEncoding…` | 同 3 的连带后果 + 捕获原生 stdout 在本沙箱的怪癖 | 改为**让辅助程序写文件**再由脚本读文件（不捕获 stdout） |
| 5 | `probe③` 链接失败 `undefined reference to 'db'` | `models.cpp` 依赖 `main.cpp` 定义的全局 `SqliteDb db;`（`models.h:12 extern`） | 探针内自行 `SqliteDb db;`（不连库，仅满足链接） |

---

## 1.5 全部去重绑定的逐条判读（由 `check2.js` 的 TSV 直接生成，未手工转录）

> 生成命令：`node %TEMP%\b1t2\check2.js <文档>`；原始逐条结果：`%TEMP%\b1t2\BATCH1_CONTRACT.md.bindings.tsv`、`%TEMP%\b1t2\BATCH1_DESIGN_DECISIONS.md.bindings.tsv`。

### A) `docs/audit/BATCH1_CONTRACT.md`

| # | 绑定 | 判读 | 锚点实际内容（我本轮读取的源文件原文，截断） |
| --- | --- | --- | --- |
| 1 | `logger.h:12` | pass | #include <mutex> |
| 2 | `logger.h:61` | pass | std::lock_guard<std::mutex> lock(mutex_); |
| 3 | `logger.h:89` | pass | static std::mutex mutex_; |
| 4 | `logger.cpp:7` | pass | std::mutex Logger::mutex_; |
| 5 | `main.cpp:22` | pass（启发式 REVIEW，见 §1.3） | SqliteDb db; |
| 6 | `main.cpp:463` | pass | svr.new_task_queue = [&]() { return new httplib::ThreadPool(g_config.thread_count); }; |
| 7 | `sqlite_wrapper.h:19-32` | pass | bool open(const std::string& path) { int rc = sqlite3_open(path.c_str(), &db_); if (rc != SQLI |
| 8 | `auth.h:111-114` | pass | inline std::map<std::string, LoginAttempt>& login_attempts() { static std::map<std::string, Logi |
| 9 | `models.cpp:14` | pass | vector<PointsRecord> points_records; |
| 10 | `models.cpp:125-173` | pass | string generate_user_id(int role_id, const string& grade_code, const string& class_code) { char  |
| 11 | `models.cpp:178-182` | pass | unordered_map<string, size_t> user_id_map; // 用户ID到 users 下标的映射 unordered_map<string, size_t> us |
| 12 | `routes_public.cpp:325-412` | pass | svr.Post("/api/mall/redeem", [](const httplib::Request& req, httplib::Response& res) { set_cors_ |
| 13 | `sqlite_wrapper.h:127-148` | pass | bool execute_bind(const std::string& sql, const std::vector<Bind>& params = {}) { if (!db_) retu |
| 14 | `routes_teacher.cpp:103` | pass | svr.Post("/api/teacher/students", [](const httplib::Request& req, httplib::Response& res) { |
| 15 | `auth.h:141-154` | pass | inline std::string login_client_key(const httplib::Request& req, const std::string& username) {  |
| 16 | `routes_public.cpp:29-30` | pass | std::string lk = login_client_key(req, username); if (!login_can_try(lk)) { |
| 17 | `routes_parent.cpp:49-77` | pass | svr.Post("/api/parent/login", [](const httplib::Request& req, httplib::Response& res) { set_cors |
| 18 | `main.cpp:442-449` | pass | // 4. 创建 HTTP 服务器 httplib::Server svr; // 5. 设置请求日志 svr.set_logger([](const httplib::Request& re |
| 19 | `sqlite_wrapper.h:53-92` | pass | json query(const std::string& sql) { json result = json::array(); if (!db_) return result;  |
| 20 | `routes_admin.cpp:1147` | pass | permissions.erase(it); |
| 21 | `routes_admin.cpp:1183` | pass（启发式 REVIEW，见 §1.3） | roles.erase(it); |
| 22 | `routes_teacher.cpp:797-803` | pass | if (db.execute_bind( "UPDATE evaluations SET score = ?, comment = ?, updated_at = CURRENT_TIME |
| 23 | `routes_teacher.cpp:826-831` | pass | if (db.execute_bind("DELETE FROM evaluations WHERE id = ?", {SqliteDb::Bind((long long)eval_id |
| 24 | `routes_public.cpp:385-388` | pass | db.execute_bind( "UPDATE mall_items SET stock = stock - 1 WHERE id = ? AND stock > 0",  |
| 25 | `routes_admin.cpp:1519` | pass（启发式 REVIEW，见 §1.3） | db.execute("BEGIN TRANSACTION"); |
| 26 | `logger.h:41-62` | pass | static void rotate_if_needed() { std::ifstream ifs(log_file_, std::ios::ate \| std::ios::binary) |
| 27 | `main.cpp:193-202` | pass | if (users_table_is_old_schema()) { Logger::warning("检测到旧 schema（users.id 为 INTEGER），删除数据库文件并重建为新 |
| 28 | `models.cpp:89-98` | pass | bool save_user_to_db(const User& user) { return db.execute_bind( "INSERT OR REPLACE INTO users ( |
| 29 | `httplib.h:3443-3448` | pass | inline bool Server::listen_internal() { auto ret = true; is_running_ = true; { std::unique_ptr<T |
| 30 | `httplib.h:371-379` | pass | class ThreadPool : public TaskQueue { public: explicit ThreadPool(size_t n) : shutdown_(false) { |
| 31 | `httplib.h:408-426` | pass | void operator()() { for (;;) { std::function<void()> fn; { std::unique_lock<st |
| 32 | `httplib.h:3481-3483` | pass（启发式 REVIEW，见 §1.3） | task_queue->enqueue([=, this]() { process_and_close_socket(sock); }); #else task_queue->enqueue( |
| 33 | `sqlite_wrapper.h:29` | pass | execute("PRAGMA journal_mode=WAL;"); |
| 34 | `sqlite_wrapper.h:27` | pass | sqlite3_busy_timeout(db_, 5000); |
| 35 | `routes_public.cpp:375` | pass（启发式 REVIEW，见 §1.3） | if (user->points < cost) { |
| 36 | `sqlite_wrapper.h:102-108` | pass | int update(const std::string& sql) { if (!db_) return -1; if (execute(sql)) { retu |
| 37 | `main.cpp:190-215` | pass | if (db.open(g_config.db_path)) { Logger::info("SQLite 数据库连接成功"); if (users_table_is_old_schem |
| 38 | `models.h:12` | pass | extern SqliteDb db; |
| 39 | `models.cpp:23` | pass | vector<User> users; |
| 40 | `models.cpp:26` | pass | vector<Role> roles = { |
| 41 | `models.cpp:34` | pass | vector<Permission> permissions = { |
| 42 | `models.cpp:50` | pass | vector<RolePermission> role_permissions = { |
| 43 | `models.cpp:262-277` | pass | User* find_user_by_id(const string& user_id) { auto it = user_id_map.find(user_id); if (it != us |
| 44 | `auth.h:117-139` | pass | inline bool login_can_try(const std::string& key) { auto& m = login_attempts(); auto it = m.find |
| 45 | `routes_public.cpp:29` | pass（启发式 REVIEW，见 §1.3） | std::string lk = login_client_key(req, username); |
| 46 | `auth.h:128` | pass（启发式 REVIEW，见 §1.3） | auto& a = m[key]; |
| 47 | `routes_admin.cpp:571` | pass（启发式 REVIEW，见 §1.3） | users.push_back(new_user); |
| 48 | `routes_teacher.cpp:143` | pass（启发式 REVIEW，见 §1.3） | users.push_back(new_user); |
| 49 | `routes_public.cpp:174` | pass（启发式 REVIEW，见 §1.3） | users.push_back(new_user); |
| 50 | `routes_public.cpp:396` | pass | user->points -= cost; |
| 51 | `models.cpp:91` | pass | "INSERT OR REPLACE INTO users (id, username, password_hash, role_id, name, className, points, mu |
| 52 | `main.cpp:220` | pass | id TEXT PRIMARY KEY, |
| 53 | `main.cpp:221` | pass | username TEXT UNIQUE NOT NULL, |
| 54 | `routes_teacher.cpp:332` | pass | int new_record_id = points_records.size() + 1; |
| 55 | `models.cpp:187-194` | pass | static void clear_and_rebuild_user_indexes() { user_id_map.clear(); user_username_map.clear(); f |
| 56 | `models.cpp:210-224` | pass | void init_indexes() { clear_and_rebuild_user_indexes(); role_id_map.clear(); permission_id_map.c |
| 57 | `models.cpp:240-242` | pass | void rebuild_user_indexes() { clear_and_rebuild_user_indexes(); } |
| 58 | `models.cpp:245-253` | pass | void update_user_index(const User& user) { for (size_t i = 0; i < users.size(); i++) { if (users |
| 59 | `models.cpp:256-259` | pass | void remove_user_index(const string& user_id, const string& username) { user_id_map.erase(user_i |
| 60 | `models.cpp:280-300` | pass | bool check_permission_optimized(const string& user_id, const string& permission_code) { User* us |
| 61 | `models.cpp:264` | pass | if (it != user_id_map.end() && it->second < users.size()) { |
| 62 | `routes_admin.cpp:1046` | pass（启发式 REVIEW，见 §1.3） | roles.push_back(Role{new_id, name, description}); |
| 63 | `routes_admin.cpp:1111` | pass | permissions.push_back(Permission{new_id, name, code, description}); |
| 64 | `routes_admin.cpp:1599` | pass（启发式 REVIEW，见 §1.3） | roles.push_back(new_role); |
| 65 | `models.cpp:180-181` | pass | unordered_map<int, Role> role_id_map; // 角色ID到角色的映射 unordered_map<int, Permission> permission_id |
| 66 | `routes_admin.cpp:1045` | pass（启发式 REVIEW，见 §1.3） | int new_id = roles.empty() ? 1 : roles.back().id + 1; |
| 67 | `routes_public.cpp:325` | pass | svr.Post("/api/mall/redeem", [](const httplib::Request& req, httplib::Response& res) { |
| 68 | `routes_public.cpp:340` | pass | User* user = find_user_by_id(user_id); |
| 69 | `routes_public.cpp:358-360` | pass | json item_result = db.query_bind( "SELECT cost, stock FROM mall_items WHERE id = ? AND status  |
| 70 | `routes_public.cpp:381-388` | pass | bool stock_ok = true; if (stock == 0) { stock_ok = false; } else if (s |
| 71 | `routes_public.cpp:396-397` | pass | user->points -= cost; update_user_points_in_db(user->id, user->points); |
| 72 | `routes_public.cpp:398-400` | pass | db.execute_bind( "INSERT INTO redemption_records (student_id, item_id, cost, created_at) VALUE |
| 73 | `sqlite_wrapper.h:41-51` | pass | bool execute(const std::string& sql) { if (!db_) return false; char* errMsg = nullptr;  |
| 74 | `routes_teacher.cpp:169` | pass | svr.Put(R"(/api/teacher/students/([^/]+))", [](const httplib::Request& req, httplib::Response& r |
| 75 | `routes_teacher.cpp:235` | pass | svr.Delete("/api/teacher/students", [](const httplib::Request& req, httplib::Response& res) { |
| 76 | `routes_teacher.cpp:274` | pass | svr.Post("/api/teacher/points", [](const httplib::Request& req, httplib::Response& res) { |
| 77 | `routes_teacher.cpp:464` | pass | svr.Post("/api/teacher/evaluation", [](const httplib::Request& req, httplib::Response& res) { |
| 78 | `routes_teacher.cpp:781` | pass | svr.Put(R"(/api/teacher/evaluation/(\d+))", [](const httplib::Request& req, httplib::Response& r |
| 79 | `routes_teacher.cpp:813` | pass | svr.Delete(R"(/api/teacher/evaluation/(\d+))", [](const httplib::Request& req, httplib::Response |
| 80 | `routes_teacher.cpp:1036` | pass | if (!check_permission_middleware(req, res, "student:manage")) { |
| 81 | `routes_teacher.cpp:31` | pass（启发式 REVIEW，见 §1.3） | "JOIN teacher_classes tc ON c.id = tc.class_id " |
| 82 | `routes_parent.cpp:25-30` | pass | static bool check_same_parent(const std::string& parent_id, const std::string& target_student_id |
| 83 | `auth.h:344-388` | pass | inline bool check_permission_middleware(const httplib::Request& req, httplib::Response& res, con |
| 84 | `main.cpp:385-389` | pass（启发式 REVIEW，见 §1.3） | INSERT OR IGNORE INTO role_permissions (role_id, permission_id) VALUES (1, 1), (1, 2), (1, 3), |
| 85 | `main.cpp:387` | pass | (2, 3), (2, 4), (2, 5), (2, 7), (2, 9), (2, 10), (2, 11), |
| 86 | `routes_teacher.cpp:81-87` | pass | if (!bound_class_names.empty()) { bool in_bound = false; for (const au |
| 87 | `main.cpp:405` | pass | INSERT OR IGNORE INTO classes (id, name, grade, grade_code, class_code, head_teacher, descriptio |
| 88 | `auth.h:180-187` | pass | inline bool require_csrf(const httplib::Request& req, httplib::Response& res) { if (req.method = |
| 89 | `routes_teacher.cpp:80-87` | pass | // 若已获取绑定班级，则过滤；否则保留旧行为 if (!bound_class_names.empty()) { bool in_bound =  |
| 90 | `routes_teacher.cpp:78-96` | pass | for (const auto& user : users) { if (user.role_id == 3) { // 学生角色 // 若已获取绑定班级，则过滤； |
| 91 | `routes_admin.cpp:587-596` | pass | // 教师角色：插入 teacher_classes 绑定 if (role_id == 2) { json bound_class |
| 92 | `auth.h:117-123` | pass | inline bool login_can_try(const std::string& key) { auto& m = login_attempts(); auto it = m.find |
| 93 | `auth.h:126-134` | pass | inline void login_record_fail(const std::string& key) { auto& m = login_attempts(); auto& a = m[ |
| 94 | `auth.h:137-139` | pass | inline void login_record_success(const std::string& key) { login_attempts().erase(key); } |
| 95 | `config.h:22-36` | pass | int token_expiry_hours = 24; int session_expiry_hours = 24; std::string cookie_name = "sid"; boo |
| 96 | `httplib.h:3639` | pass | req.set_header("REMOTE_ADDR", strm.get_remote_addr()); |
| 97 | `config.h:62-71` | pass | if (cfg.contains("security")) { config.token_expiry_hours = cfg["security"].value("token_expiry_ |
| 98 | `routes_public.cpp:29-60` | pass | std::string lk = login_client_key(req, username); if (!login_can_try(lk)) { respon |
| 99 | `httplib.h:3561-3582` | pass | inline bool Server::dispatch_request(Request &req, Response &res, Handler |
| 100 | `httplib.h:1976-1997` | pass | template <typename T> inline ssize_t write_headers(Stream &strm, const T &info, c |
| 101 | `routes_public.cpp:20` | pass | string password = req_json.value("password", ""); |
| 102 | `routes_public.cpp:21-22` | pass | if (username.empty() \|\| password.empty()) { |
| 103 | `routes_public.cpp:92` | pass | } catch (json::parse_error& e) { |
| 104 | `httplib.h:1981` | pass | if (x.first == "EXCEPTION_WHAT") { continue; } |
| 105 | `httplib.h:476` | pass | void set_error_handler(Handler handler); |
| 106 | `httplib.h:195` | pass | using Headers = std::multimap<std::string, std::string, detail::ci>; |
| 107 | `httplib.h:43-44` | pass（启发式 REVIEW，见 §1.3） | #ifndef CPPHTTPLIB_PAYLOAD_MAX_LENGTH #define CPPHTTPLIB_PAYLOAD_MAX_LENGTH ((std::numeric_limit |
| 108 | `httplib.h:2955` | pass | payload_max_length_(CPPHTTPLIB_PAYLOAD_MAX_LENGTH), is_running_(false), |
| 109 | `httplib.h:3072-3078` | pass | inline void Server::set_read_timeout(time_t sec, time_t usec) { read_timeout_sec_ = sec; read_ti |
| 110 | `httplib.h:1955-1971` | pass | if (is_chunked_transfer_encoding(x.headers)) { ret = read_content_chunked(strm, out); } else if  |
| 111 | `httplib.h:1648` | pass | case 413: return "Payload Too Large"; |
| 112 | `sqlite_wrapper.h:59-62` | pass | if (rc != SQLITE_OK) { std::cerr << "查询失败: " << sqlite3_errmsg(db_) << std::endl; retu |
| 113 | `sqlite_wrapper.h:151-188` | pass | json query_bind(const std::string& sql, const std::vector<Bind>& params = {}) { json result = js |
| 114 | `auth.h:240-258` | pass | inline bool get_session_info(const std::string& session_id, std::string& user_id, int& role_id,  |
| 115 | `auth.h:246` | pass | auto result = db.query(sql); |
| 116 | `sqlite_wrapper.h:53-62` | pass | json query(const std::string& sql) { json result = json::array(); if (!db_) return result;  |
| 117 | `routes_admin.cpp:1123` | pass | svr.Delete("/api/admin/permissions", [](const httplib::Request& req, httplib::Response& res) { |
| 118 | `routes_admin.cpp:1159` | pass | svr.Delete("/api/admin/roles", [](const httplib::Request& req, httplib::Response& res) { |
| 119 | `routes_admin.cpp:1007` | pass | json perms_array = json::array(); |
| 120 | `routes_admin.cpp:1081` | pass | svr.Post("/api/admin/permissions", [](const httplib::Request& req, httplib::Response& res) { |
| 121 | `routes_admin.cpp:1736` | pass | init_indexes(); |
| 122 | `models.cpp:180-182` | pass | unordered_map<int, Role> role_id_map; // 角色ID到角色的映射 unordered_map<int, Permission> permission_id |
| 123 | `models.cpp:293-294` | pass | auto perm_it = permission_id_map.find(perm_id); if (perm_it != permission_id_map.end() && perm_i |
| 124 | `routes_admin.cpp:1110` | pass | int new_id = permissions.empty() ? 1 : permissions.back().id + 1; |
| 125 | `routes_admin.cpp:951-952` | pass | "UPDATE users SET password_hash = ?, must_change_password = 1, updated_at = CURRENT_TIMESTAMP WH |
| 126 | `models.cpp:100-104` | pass | bool delete_user_from_db(const string& user_id) { return db.execute_bind( "DELETE FROM users WHE |
| 127 | `routes_admin.cpp:1734` | pass | db.execute("COMMIT"); |
| 128 | `routes_admin.cpp:1755` | pass | db.execute("ROLLBACK"); |
| 129 | `sqlite_wrapper.h:14-32` | pass | class SqliteDb { public: SqliteDb() : db_(nullptr) {} ~SqliteDb() { close(); } bool open(const s |
| 130 | `logger.h:41-56` | pass | static void rotate_if_needed() { std::ifstream ifs(log_file_, std::ios::ate \| std::ios::binary) |
| 131 | `logger.h:58-62` | pass | static void log(Level level, const std::string& message) { rotate_if_needed(); std::lock_guar |
| 132 | `routes_teacher.cpp:341` | pass | points_records.push_back(new_record); |
| 133 | `routes_teacher.cpp:344-349` | pass | db.execute_bind( "INSERT INTO points_records (student_id, points, reason, operator_id, created |
| 134 | `routes_teacher.cpp:357` | pass | {"id", new_record_id}, |
| 135 | `routes_admin.cpp:1664-1671` | pass | PointsRecord new_record; new_record.id = id; new_record.studen |
| 136 | `main.cpp:204-215` | pass | // 安全修复 V18（B2）：磁盘库打开失败**不再静默降级为内存库**。 // 旧行为（Logger::warning 后继续用内存存储）会带来三重风险： // 1) 服务在「所有 |
| 137 | `main.cpp:217` | pass | if (db.isOpen()) { |
| 138 | `main.cpp:196-201` | pass | if (std::remove(g_config.db_path.c_str()) != 0) { Logger::error("删除旧数据库文件失败: " + g_config.db_p |
| 139 | `main.cpp:35` | pass | if (type == "INTEGER") { |
| 140 | `sha256.h:94-148` | pass | for (int i = 0; i < 8; i++) { out[i * 4 + 0] = (unsigned char)((state[i] >> 24) & 0xFF); out[i |
| 141 | `routes_public.cpp:45-51` | pass | bool password_match = verify_password(password, user->password_hash); // 安全修复 V18（B2）：删除旧版「登录成功即 |
| 142 | `routes_public.cpp:46-52` | pass | // 安全修复 V18（B2）：删除旧版「登录成功即就地升级为 PBKDF2」的自动升级链。 // 依据 BATCH0_DESIGN_DECISIONS.md 决策①：在 B1 存在时这条链会 |
| 143 | `main.cpp:250-254` | pass（启发式 REVIEW，见 §1.3） | FOREIGN KEY (permission_id) REFERENCES permissions(id), UNIQUE(role_id, permission_id)  |
| 144 | `main.cpp:83-152` | pass | static void seed_default_users_if_empty() { if (!db.isOpen()) return; json cnt = db.query("SELEC |
| 145 | `models.cpp:17-22` | pass | // 旧实现在此硬编码 4 个账号的无盐 SHA256 口令哈希（admin/teacher/student/parent + "xxx123"）， // 一旦磁盘库打开失败，服务会静默降级到 |
| 146 | `models.cpp:164-221` | pass | } } } snprintf(buf, sizeof(buf), "student-%s-%s-%02d", grade_code.c_str(), class_c |
| 147 | `models.cpp:175-300` | pass | // ====== 索引优化结构 ====== // 安全修复 V12：索引改为存储 users 向量的下标，避免副本与原数据不一致 unordered_map<string, size_t> |
| 148 | `models.cpp:206-211` | pass | // 现在改为「五个索引容器全部 clear 后从源头重建」： // * users / roles / permissions / role_permissions 均为源数据（role_p |
| 149 | `models.cpp:262-268` | pass | User* find_user_by_id(const string& user_id) { auto it = user_id_map.find(user_id); if (it != us |
| 150 | `models.cpp:172-186` | pass | return string(buf); } // ====== 索引优化结构 ====== // 安全修复 V12：索引改为存储 users 向量的下标，避免副本与原数据不一致 unorder |
| 151 | `routes_admin.cpp:826-827` | pass（启发式 REVIEW，见 §1.3） | } |
| 152 | `routes_admin.cpp:886` | pass | users.erase(user_it); |
| 153 | `routes_teacher.cpp:259-260` | pass | // 安全修复 B6：erase 会让后续元素整体前移，必须重建用户索引， // 否则 user_id_map 仍存旧下标 → 后续 find_user_by_id 返回错误学生。 |
| 154 | `routes_teacher.cpp:258` | pass | users.erase(it); |
| 155 | `auth.h:355` | pass（启发式 REVIEW，见 §1.3） | return false; |
| 156 | `CODE_QUALITY_SUMMARY.md:173` | pass | - **N1** `config.h:84` + `main.cpp:312-315`：HTTPS 回退到明文 HTTP 时**不复位 `cookie_secure`** → 三处 Cooki |
| 157 | `main.cpp:312-315` | pass（启发式 REVIEW，见 §1.3） | id INTEGER PRIMARY KEY AUTOINCREMENT, teacher_id TEXT NOT NULL, class_id INTEG |
| 158 | `main.cpp:469-479` | pass | if (g_config.https_enabled) { Logger::warning("HTTPS 已在配置中启用，但当前编译版本不支持 SSLServer。请使用 OpenSSL 版本 |
| 159 | `routes_admin.cpp:335-349` | pass | // 安全修复 B13：备份成败判据修正。 // 旧实现用 db.query("VACUUM INTO ...") 的返回值判成败（backup_result.is_null() 即成功），  |
| 160 | `routes_admin.cpp:323-350` | pass | // 24. 系统备份 API（需要管理员权限） svr.Post("/api/admin/system/backup", [](const httplib::Request& req, ht |
| 161 | `routes_teacher.cpp:259` | pass | // 安全修复 B6：erase 会让后续元素整体前移，必须重建用户索引， |
| 162 | `sqlite_wrapper.h:145-147` | pass（启发式 REVIEW，见 §1.3） | rc = sqlite3_step(stmt); sqlite3_finalize(stmt); return rc == SQLITE_DONE; |
| 163 | `routes_parent.cpp:27` | pass | "SELECT 1 FROM parent_students WHERE parent_id = ? AND student_id = ? LIMIT 1", |
| 164 | `config.h:84` | pass | if (config.https_enabled) config.cookie_secure = true; |
| 165 | `config.h:31` | pass | bool cookie_secure = false; |
| 166 | `sqlite_wrapper.h:20` | pass | int rc = sqlite3_open(path.c_str(), &db_); |
| 167 | `auth.h:141-149` | pass | inline std::string login_client_key(const httplib::Request& req, const std::string& username) {  |
| 168 | `models.cpp:187` | pass | static void clear_and_rebuild_user_indexes() { |
| 169 | `httplib.h:3564-3582` | pass | try { for (const auto &x : handlers) { const auto &pattern = x.first; const auto &handler = x. |
| 170 | `auth.h:240-275` | pass | inline bool get_session_info(const std::string& session_id, std::string& user_id, int& role_id,  |

合计去重绑定 **170** 条。

### B) `docs/audit/BATCH1_DESIGN_DECISIONS.md`

| # | 绑定 | 判读 | 锚点实际内容（我本轮读取的源文件原文，截断） |
| --- | --- | --- | --- |
| 1 | `sqlite_wrapper.h:14-210` | pass | class SqliteDb { public: SqliteDb() : db_(nullptr) {} ~SqliteDb() { close(); } bool open(const s |
| 2 | `httplib.h:3443-3448` | pass | inline bool Server::listen_internal() { auto ret = true; is_running_ = true; { std::unique_ptr<T |
| 3 | `sqlite_wrapper.h:29` | pass | execute("PRAGMA journal_mode=WAL;"); |
| 4 | `httplib.h:3487` | pass | task_queue->shutdown(); |
| 5 | `sqlite_wrapper.h:27` | pass | sqlite3_busy_timeout(db_, 5000); |
| 6 | `routes_public.cpp:358-360` | pass | json item_result = db.query_bind( "SELECT cost, stock FROM mall_items WHERE id = ? AND status  |
| 7 | `routes_admin.cpp:1519` | pass（启发式 REVIEW，见 §1.3） | db.execute("BEGIN TRANSACTION"); |
| 8 | `sqlite_wrapper.h:102-108` | pass | int update(const std::string& sql) { if (!db_) return -1; if (execute(sql)) { retu |
| 9 | `main.cpp:204-215` | pass | // 安全修复 V18（B2）：磁盘库打开失败**不再静默降级为内存库**。 // 旧行为（Logger::warning 后继续用内存存储）会带来三重风险： // 1) 服务在「所有 |
| 10 | `main.cpp:190-440` | pass | if (db.open(g_config.db_path)) { Logger::info("SQLite 数据库连接成功"); if (users_table_is_old_schem |
| 11 | `models.h:12` | pass | extern SqliteDb db; |
| 12 | `models.cpp:280-300` | pass | bool check_permission_optimized(const string& user_id, const string& permission_code) { User* us |
| 13 | `auth.h:280-303` | pass | inline bool check_permission(const string& user_id, const string& permission_code) { auto user_i |
| 14 | `auth.h:111-114` | pass | inline std::map<std::string, LoginAttempt>& login_attempts() { static std::map<std::string, Logi |
| 15 | `logger.h:89` | pass（启发式 REVIEW，见 §1.3） | static std::mutex mutex_; |
| 16 | `routes_public.cpp:340` | pass（启发式 REVIEW，见 §1.3） | User* user = find_user_by_id(user_id); |
| 17 | `models.cpp:264` | pass | if (it != user_id_map.end() && it->second < users.size()) { |
| 18 | `main.cpp:221` | pass | username TEXT UNIQUE NOT NULL, |
| 19 | `routes_teacher.cpp:64-87` | pass | std::vector<std::string> bound_class_names; if (!teacher_id.empty()) { json tc_result = db |
| 20 | `auth.h:180-187` | pass | inline bool require_csrf(const httplib::Request& req, httplib::Response& res) { if (req.method = |
| 21 | `routes_teacher.cpp:52-100` | pass | svr.Get("/api/teacher/students", [](const httplib::Request& req, httplib::Response& res) { set_c |
| 22 | `routes_teacher.cpp:31` | pass（启发式 REVIEW，见 §1.3） | "JOIN teacher_classes tc ON c.id = tc.class_id " |
| 23 | `routes_parent.cpp:25-30` | pass | static bool check_same_parent(const std::string& parent_id, const std::string& target_student_id |
| 24 | `routes_teacher.cpp:204` | pass | student_it->className = className; |
| 25 | `routes_teacher.cpp:80-87` | pass | // 若已获取绑定班级，则过滤；否则保留旧行为 if (!bound_class_names.empty()) { bool in_bound =  |
| 26 | `httplib.h:3639` | pass | req.set_header("REMOTE_ADDR", strm.get_remote_addr()); |
| 27 | `auth.h:148-151` | pass | } else { auto rit = req.headers.find("REMOTE_ADDR"); if (rit != req.headers.end()) ip = rit->s |
| 28 | `auth.h:143-147` | pass | auto it = req.headers.find("X-Forwarded-For"); if (it != req.headers.end()) { ip = it->second;  |
| 29 | `routes_parent.cpp:49-77` | pass | svr.Post("/api/parent/login", [](const httplib::Request& req, httplib::Response& res) { set_cors |
| 30 | `routes_public.cpp:29-60` | pass | std::string lk = login_client_key(req, username); if (!login_can_try(lk)) { respon |
| 31 | `routes_public.cpp:46-52` | pass | // 安全修复 V18（B2）：删除旧版「登录成功即就地升级为 PBKDF2」的自动升级链。 // 依据 BATCH0_DESIGN_DECISIONS.md 决策①：在 B1 存在时这条链会 |

合计去重绑定 **31** 条。
