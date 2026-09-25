# 批次 0 修复 · 审计结项报告（BATCH0_CLOSURE）

> **产出任务**：T28（integration，队长在 `team.json` 中重建的上游链为 `deps = [t5, t6, t27]`）
> **报告作者**：integrator（结项集成）。**本文件是批次 0 唯一的结项结论载体。**
> **落盘时间**：2026-09-19（本机 local 时间；当次命令输出的时间戳见 §6）
> **修订记录**：
> - `rev.4`（2026-09-19）—— 并入 **t33 的完整证据**：§3.7(b″) 改写为「**隔离变量式的对照实验**」（含 `git diff --no-index` 14+/5− / 2 hunk 的差异证明、修复前 200/404 与修复后 403/200/200 的原始对照、**索引 size 等式**、两条死代码 + `update_user_index` 反向提醒、**封装问题的分层对照测量**、两个二进制的 `sha256`）；§6.1 补 **T28 自身产物 sha256 与跨时间复现证据**（13:07:28 第二遍全量重编产出**字节相同**的 `server.exe`）；新增 **§6.5**（t33 当前行号清单的逐条当次核对 + 行号漂移对照）；§7.1.1 补 **M-5（`gcc`/`g++` 分工）/ M-6（二进制必须固定 sha256）/ M-7（隔离变量对照优于"回退试试"）**。
> - `rev.3`（2026-09-19）—— 按队长**两条紧急更正**落实：① `docs/audit` 的打包排除**已完成（T31）**，**R2 两半（T30 文档层 + T31 打包层）均已闭合**，**不得**写成批次 1 建议项；② **findings 编号一律采用 reviewer 的 `T27-R1…R8`**（并附队长旧编号映射，**R3/R4 曾相反**）；③ 收进 reviewer 的 **M-1…M-4 方法论条目**（§7.1.1）；④ `T27-R4` 与 §9⑨ **合并为同一项**。
> - `rev.2`（2026-09-19）—— 追加 §4.3（t27 的 8 条 findings 逐条入表）、§3.7(d)（**R5：权限/角色映射的同类未修路径**，并限定 **B6 范围只含 `users` 索引的三条路径**）与 §5 的产物卫生当次复核。
> - `rev.1` —— T28 提交时的初始版本。
> **仓库根**：`D:\邵敬文\comptation`，HEAD = `47006bd773f3eb55a25c56228b5a02fec432f2af`（`Merge pull request #7 from JingWen983/feat/ci-auto-package`，2026-09-06 12:24:57 +0800），分支 `main`
> **本批改动状态**：**未 commit、未 push**（`git rev-list --left-right --count main...origin/main` → `0 0`，`origin/main` = `47006bd`，与 HEAD 同一提交）。改动全部留在工作区与索引。

## 阅读须知（三条纪律，先读再引用）

1. **本报告所有数字均为 T28 当次实测**，未沿用任何上游（含队长、各成员）上文里的数字。凡引用上游结论处**均标注任务号**，并区分「引用其结论」与「我独立重测」。
2. **凡跨轮次引用的数字一律重新测量**；**凡路径/文件计数一律使用 `git -c core.quotepath=false`（或 `-z`）** —— 本批已因此踩坑 4 次（见 §7 元规则总纲与 §8 实例表）。
3. **未端到端复现的事项逐条列出（§5），不得把静态核对或逻辑复刻写成端到端验证。**

---

## 1. 结论摘要

| 项 | 内容 |
|---|---|
| 批次 0 主项 | **10 条**（B1、F1、B13、N1、F24、F16、B2、F4、B6、N4），不因后段收口任务（t29/t30/t31）增条 |
| 状态 | **10/10 均已修复并闭合**；其中 **B2 的闭合计入 t29（门禁漏接点）与 t32（改名路径）两项后段收口** |
| mock 演示路径 | **3 处**已闭合（t13 两项 + t18 改密端点） |
| 两个 blocker | **B1**（PBKDF2 完整认证绕过）与 **F1**（CI 交付包前后端错配）**均已消除**，并有独立验证（t6 / t26）与评审（t27） |
| 本批新增过程缺陷 | 5 组（§7），其中 **AgentTeams 插件制度缺陷 6 条**（附函数级 file:line），另附「元规则总纲」 |
| 幽灵任务 | **13 条**（§7.6），其中若干**无法作废**，作为幽灵任务永久留在图上 |
| 未端到端复现项 | **5 条**（§5） |
| 批次 1 交接 | **11 项**（§9 的 ①-⑪），逐项具体到可执行 |
| 结项三条断言 | 后端完整编译链接 / `tests/run_tests.ps1` / `frontend npm run typecheck` —— **三条退出码均为 0**（§6） |
| t27 findings | **8 条逐条入表**（§4.3，**采用 reviewer 编号 T27-R1…R8**）：**T27-R1、T27-R2 已 resolved**（R2 = T30 文档层 + T31 打包层，**经 reviewer 运行时/grep 双重复核**），其余 6 条全为 low/medium 且已归属批次 1/2/3，不阻塞结项。⚠️ **正文「R1…R8」一律指 reviewer 编号**（队长的旧编号 R3/R4 与 reviewer 相反，映射见 §4.3） |
| **B6 的范围限定** | **本批只修了 `users` 索引的三条路径**（两条 `users.erase` + 改名）；**权限/角色映射的同类路径未修（T27-R5）** → 见 §3.7(d) 与 §3.9 的 ⚠️ |
| 自我限定 | **T28 不因等待 t33 而延后**；t33（T32 后置独立复核）状态见 §3.7：**verdict = pass，在本报告落盘前已完成**，其方法是**隔离变量式的对照实验**（反向副本与当前源码的差异经 `git diff --no-index` 证明仅限 rename 块），证据见 §3.7(b″)；**T27 覆盖的是含 T32 的树**（自算指纹 `routes_admin.cpp=9BC7CE74`），与 T26（T32 之前）不同 |
| 可复现性证据 | T28 的结项构建**跨时间两次独立全量重编产出字节相同的 `server.exe`**（12:56:54 / 13:07:28，sha256 `F9E79042…F606F48`，3,091,137 B）→ §6.1 |

**一句话结论**：批次 0 的两个 blocker 与其安全硬绑定项（B2 + F13）已消除，其余 8 条低风险高收益项全部落地；本次验证过程另外**抓出两条有行为证据的真实缺陷**（**T27-R1** 门禁漏接点、改名路径索引误删），均已在结项前就地修复并归入 B2；**同时留下一条同一根因类别的未修盲区 `T27-R5`（权限/角色映射删除后不重建，已明确排除在 B6 修复范围之外）**；合计留下 13 条幽灵任务与 6 条插件制度缺陷，全部如实记录于 §7，t27 的 8 条 findings 逐条记录于 §4.3。（编号口径见 §4.3 开头）

---

## 2. 安全类硬绑定：顺序与前置依赖的满足情况（验收项 2）

### 2.1 审计原文结论（原样保留）

`docs/audit/CODE_QUALITY_SUMMARY.md` §三「安全类硬绑定」+ `docs/audit/CODE_QUALITY_REVIEW.md:546-557`：

> **必须先把 B1 修好，再去改默认口令。**「改密码」在 B1 存在时根本救不了场 —— B1 下改密写入的仍是与口令无关的坏哈希，任意口令照样能登录。

`docs/audit/BATCH0_CONTRACT.md:19-41` 将其契约化为顺序门禁 **G1 → G5**，并要求：

> **F13 的严重度是 low，但 low ≠ 可延后** —— 它必须与 B1/B2 同批完成（`CODE_QUALITY_REVIEW.md:557`）。

并附交叉结论：**不得用 F1 的存在降低 B1 的紧急度；修复顺序 B1 先于 F1。**

### 2.2 执行顺序与前置依赖「已满足」的证据

| 门 | 要求 | 实际执行 | 证据 |
|---|---|---|---|
| G1 | 先补 RFC 7914 §11 标准向量测试，且在未修复的 `sha256.h` 上**必然失败** | t2 建立 `tests/pbkdf2_rfc7914_test.cpp` + `tests/run_tests.ps1` | **t3**：独立重跑得 `cases 10 run, 5 ok, 5 failed` / `checks 32 run, 25 ok, 7 failed` / `RESULT: FAIL` / 退出码 1；并独立复刻「内层被写成 `SHA256(k_ipad‖salt‖INT(1))`」的结构与产品实测值**逐字节相等**；机械证据 `sha256.h:94-148` 区间内标识符 `password` 仅出现 **1** 次（形参自身）|
| G2 | 独立复核「缺陷成立 + 该测试能拦住它」 | **t3 verdict = pass** | `docs/audit/evidence/T3_B1_BASELINE.md`（19,203 B）|
| G3 | 按 RFC 8018 重写 PBKDF2，测试转全绿，后端仍可编译 | **t4** 实施，**t6 verdict = pass** | **t6**：`11/11 cases`、`36/36 checks`、退出码 0；自写独立 harness `checks=57 fail=0`；两组 128 hex 走**产品 4 参接口**逐字节相等；RFC 4231 TC1/TC2/TC6/TC7 与 Node `createHmac` 一致；`docs/audit/evidence/T6_B1_FIX_VERIFICATION.md`（13,700 B）|
| G4 | 先有 B1 修复，再改默认口令（否则「改密」无效） | t7 在 **t4 completed 之后**开工（`team.json`: `t7 deps=[t4]`） | **t7** 达成；**t26** 独立续验（见 §3.7）|
| G5 | F13 与 B2 同批处置，不得推迟 | t7（后端）+ t22（前端/CI）+ **t30**（文档层收口） | §3.2 F13 条目 |

**当次复核（T28 自跑）**：`grep` 确认顺序未颠倒 —— `pbkdf2_sha256` 已按 RFC 8018 重写（`sha256.h:155-192`，HMAC key 取 **password**、块计数 `u = hmac_sha256(password, salt + idx)` @`:169`），且 `sha256.h` 全文 **297** 行、SHA256 = `3E5C59DD…DFA22`，与 t6/t26/t27 绑定的修订**完全一致**（三方独立互证）。

---

## 3. 逐条结项（10 主项 + mock 3 处）

> 每条格式：**结论 / 改动文件与关键 file:line / 验证证据出处 / 残余风险与后续批次归属**。
> file:line 均为 **T28 当次打开核对**的工作区行号；`git show HEAD:` 行号另标（用于区分「原有代码」与「本批引入」）。

### 3.1 B1 — PBKDF2 完整认证绕过（blocker）→ **已修复**

- **修复内容**：按 RFC 8018 重写 `pbkdf2_sha256()`（`sha256.h:155-192`，新增 4 参重载支持 `dkLen`），HMAC key 取 password（`:169`/`:172`），消除旧实现的四重缺陷（A：`password` 零引用；B：内层消息体错；C：把 hex 文本当字节用的双重编码；D：`salt>64` 死分支）；`verify_password()` 加固（`:220-256`，严格格式解析 + fail-closed）；`hash_password()` 存储格式维持 `pbkdf2$100000$<32hex>$<64hex>`（`:205-218`）。
- **顺序硬绑定**：**B1 先于 B2** 已满足（§2.2）。
- **自动升级链**：`routes_public.cpp:45-51` 的「旧哈希登录后自动升级」链已按 T1 决策①**删除**（`main.cpp:161-171` 的告警明确写「自动升级链已按决策①删除」）。
- **验证证据**：t3（修复前基线必然失败，`docs/audit/evidence/T3_B1_BASELINE.md`）、t6（verdict=pass，`docs/audit/evidence/T6_B1_FIX_VERIFICATION.md`）、t26（verdict=pass）、t27（verdict=pass，B1「产品接口层面确证不可绕过」）。
- **T28 当次复跑**：`tests/run_tests.ps1` → `cases : 11 run, 11 ok, 0 failed` / `checks: 36 run, 36 ok, 0 failed` / `RESULT: PASS` / 退出码 **0**（§6.2）。断言总数由 T2 的 32 增至 **36**，新增用例 `rfc7914_dklen64_from_production`（4 checks）——**这正是 T2 门槛缺口的修复：完整 128 hex 现在走产品接口**（§7.2）。
- **残余风险 / 归属**：legacy 裸 SHA256 分支的 `==` 非常量时间比较（`sha256.h:255`，**HEAD `:187` 即如此，非 T4 引入**）→ 判定 **low**；最佳修法是随「存量一律失效」**直接删除该分支** → **批次 1**（§9⑤）。`verify_password` 无迭代下限 → informational。

### 3.2 F1 — CI 交付包前后端错配（blocker）→ **已修复**

- **两处根因**：
  1. **前后端错配**：`frontend` job 的产物是 **Mock 构建**（`ci.yml:41-46`，`VITE_USE_MOCK: 'true'`），修复前被 `package` job 复用；现该 job **不再上传 dist**（`ci.yml:54-57` 注释明写），`package` job 以真实配置**现场构建**（`ci.yml:130-139`：`npm ci` + 不设 `VITE_USE_MOCK`）。
  2. **VITE_BASE 泄漏**：`package` job 不设 `VITE_BASE`（`ci.yml:136`），`vite.config.ts:11` `base: env.VITE_BASE || '/'` → 回退站点根；产物守卫 `ci.yml:153-156` 断言 dist 内不得残留 `/campus-master/`。
- **产物守卫（当次核对在位）**：`:147-148`（`campus_mock_db_v1`）、`:150-151`（`[Mock] 未匹配的 API`）、`:154-155`（残留 `/campus-master/`）——与 t26 `[10]` 一致。
- **两处机制性核对（当次自跑）**：`ci.yml:96-100` 的 `upload-artifact` 仅上传 `server.exe`（在 `backend` job，`:59` 开始），**Mock dist 无任何上传点**；`actions/download-artifact` 仅 `:125` 一处，取 `server-exe`（`:127-128`）。
- **验证证据**：t22（落地）、t26（`[10]`：`frontend-dist`=**0**、`name: server-exe`=2、`needs:[frontend, backend]`、`download-artifact`=1；反向对照 clean→EXIT 0、三类污染→EXIT 1）、t27（verdict=pass，「非 mock 交付」）。
- **残余风险 / 归属**：**CI zip 装配链路本机不可端到端复现**（§5.1）；`dev 包产物不含 mock 特征串`仅由 `ci.yml` 产物守卫承担（§5.4）；**t31 属 F1 交付物卫生，且已完成**——同包内「无默认口令」与「仍发布 admin123」的矛盾已消除：`ci.yml:170-178`（`cp -r docs/. release/docs/` → **`rm -rf release/docs/audit`** → 反向守卫 `if [ -d release/docs/audit ] exit 1`）与 `release.yml:114-122` 同构（**当次实测行号**；T31 交卷时记 `ci.yml:172-176` / `release.yml:116-120`）。**F1 证据链未被移动**（审计证据仍留在仓库），reviewer 的 F1 复核即按**含 T31 的现行修订**（`ci.yml=B0909406`、`release.yml=06439A87`）逐 job/step/artifact 核对通过。

### 3.3 B13 — 备份接口判据写反（medium）→ **已修复**

- **机制（用当次实测的 HEAD 原文更正口径）**：旧实现 `routes_admin.cpp`（HEAD 版）：
  ```cpp
  // git show HEAD:routes_admin.cpp
  341: json backup_result = db.query("VACUUM INTO '" + backup_path + "'");
  342: if (backup_result.is_null()) {   // 备份成功
  345: } else {                          // 备份失败
  ```
  而 `db.query()`（`sqlite_wrapper.h:53-54`）**恒返回 `json::array()`**（`json result = json::array();`），空数组的 `is_null()` == **false** → 条件恒假 → **旧判据恒走 else**（即恒判「备份失败」）。**故正确表述是「旧判据恒走 else」，不是「恒判成功」**（见 §8②）。
- **修复后（当次核对）**：`routes_admin.cpp:336-397` —— `:344` 未开库快速失败；`:358-366` 目标已存在→拒绝覆盖（`:363`「目标备份文件已存在，拒绝覆盖」）；**:371 `db.execute("VACUUM INTO …")` 返回码为主判据**；`:374-381` 磁盘兜底（`exists`+`is_regular_file`+`size>0`）；`:386-397` 失败记日志 + 清理半成品 + code 500。新增 `#include <filesystem>`（`:8`）。
- **次要加固**：`get_current_time()` 精度为分钟，同分钟第二次备份会撞名 → 现由「目标已存在即拒绝覆盖」显式暴露（t26 `[6]` 实测同分钟再调得 **500**）。
- **验证证据**：t9（落地）、t26 `[6]`（成功路 200 size=135168 且**磁盘字节一致**；同分钟再调 500）。
- **残余风险 / 归属**：三条子路径**未端到端复现**（§5.3）→ **批次 1**。

### 3.4 N1 — HTTPS 回退后 `cookie_secure` 未复位（medium）→ **已修复**

- **改动位置（行号已漂移）**：`main.cpp:468-479`（**T1 契约写 `:312-315`**；t7 把文件从 487 行扩到 500 行 → 漂移 156 行，见 §8③）。新增 `g_config.cookie_secure = false;`（**`:477`**，紧跟 `:471` `https_enabled=false`）+ `Logger::warning`（`:478`）。
- **唯一开关来源**：`config.h:84`；三处 `Set-Cookie` 均只读它 —— `auth.h:88`、`:95`、`:174`。内部注释（`main.cpp:473-476`）已写清该因果链。
- **验证证据**：t9（落地，含实机日志两行告警）、t26 `[7]`（**用沙箱副本 config、未改仓库文件**；https off/on 两模式各做成功登录 → `Set-Cookie` 均含 `sid` 且**均无 `Secure`**；改回后 `git diff` 为空）。
- **残余风险 / 归属**：无（本机可端到端复现）。批次归属：闭合。

### 3.5 F24 — 无效请求头 `Content-Length`（medium）→ **已修复**

- **改动位置**：`frontend/src/lib/api.ts:82-85`（删除原先为无体 DELETE 手动设置的 `Content-Length`；依据 fetch 规范该头属 Forbidden request header，`Headers.set` 静默忽略）。**当前文件内 `Content-Length` 命中 3 处且全是注释**（`:82`、`:84`、注释文本），与 t26 `[11]` 一致。
- **验证证据**：t22（落地）、t26 `[11]`（3 处全为注释；`npm run typecheck` 退出码 0）、**T28 当次 `npm run typecheck` 退出码 0**（§6.3）。
- **残余风险 / 归属**：真实浏览器行为未端到端复现（本机无浏览器自动化）；但该改动是**删除一行无效代码**，风险方向为「不改变实际请求」。闭合。

### 3.6 F16 — 发布不可复现（medium）→ **已修复**

- **改动位置**：`.github/workflows/release.yml:51-53` —— 发布用 `npm ci`（严格按 `package-lock.json`，不改写 lockfile）。**当次实测**：`release.yml` 内 `npm ci` 命中 **2** 处（`:53` 与另一处前端步骤），`npm install` 命中 **0** 处（t26 `[11]` 亦为 `npm ci=2 / npm install=0`）。
- **验证证据**：t22（落地）、t26 `[11]`。
- **残余风险 / 归属**：`gh release` 发布链路**未端到端复现**（§5.1 同源：Actions runner 本机不可用）。闭合（静态判据成立）。

### 3.7 B2 — 种子默认口令 + 静默内存兜底（high）→ **已修复，但闭合计入两项后段收口（t29 + t32）**

**（a）核心实现（t7）**
- 首启随机强口令：`main.cpp:83-152`（仅当 `users` 表为空；`generate_random_password()` 由 CSPRNG 生成、两两不同、碰撞重试 `:108-116`；`hash_password()` 后**参数化**落库 `:124`；全部 `must_change_password = 1`；明文**只输出到标准输出一次**，刻意不走 `Logger` 以免落盘）。
- 硬编码种子哈希与内存兜底账号表**整体删除**（`models.cpp:16-23` 旧表已移除；4 个旧哈希在后端 `*.cpp/*.h` 命中 0 处）。
- `must_change_password` 单列 + 幂等迁移：建表列 `main.cpp:227`；`migrate_users_must_change_password()` `main.cpp:62-75`（`ALTER TABLE users ADD COLUMN must_change_password INTEGER NOT NULL DEFAULT 0`）。
- **fail-fast**：磁盘库打不开**不再静默降级内存库**，`main.cpp:203-215`（`:210` error 日志、`:214` `return 1`）。
- 双中间件门禁：`auth.h:327` `enforce_password_change()`，由 `check_permission_middleware`（`:346`）与 `check_parent_auth_middleware`（`:395`）调用；403 体为 `{"code":403,"must_change_password":true,…}`（`:337`）。
- 专用改密接口 `POST /api/auth/change-password`（`routes_public.cpp`）。

**（b）门禁漏接点 R1（t29）—— 本次验证抓到的真实缺陷①**
- 缺陷：`GET /api/student/points/records` **修复前未接门禁**（原注释「仅需登录验证，无需特殊权限」，`git diff` 原文可见），未改密会话可达 → **B2 的判据当时并不成立**。
- 修复：`routes_student.cpp:65` 补 `check_permission_middleware(req, res, "mall:manage")`（**当次实测** `git diff --numstat -- routes_student.cpp` → `8 1`）。当前该文件 5 个 handler 的门禁行号 `:15`/`:65`/`:104`/`:162`/`:221` → **5/5 全覆盖**。
- 行为证据：修复前 `HTTP 200`（带积分数据）→ 修复后 `403`，改密后同会话 `200`（t29、t26 `[3]`）。
- **记账口径**：**t29 不是新增缺陷，而是 B2 门禁覆盖面的最后一块**；报告必须写明「**B2 的判据在 t29 之前不成立**」——这是本次验证真正抓到的、有行为证据的缺陷，不得隐去。

**（c）改名路径索引误删（t32）—— 本次验证抓到的真实缺陷②**
- 缺陷（**原有代码，HEAD 即存在**：`git show HEAD:routes_admin.cpp` 的 `:732/:734` 对应当前 `:783/:785`）：
  ```
  783  update_user_index(*user_it);                          // 写入 user_id_map[id]
  785  remove_user_index(user_id, old_username);             // models.cpp:256-259 同时 erase id 键
  ```
  `remove_user_index()`（`models.cpp:256-259`）**同时** `user_id_map.erase(user_id)` 与 `user_username_map.erase(username)` → 刚写入的 id 键被删 → `find_user_by_id(id)` = `nullptr`（`:262-268`）→ `user_must_change_password()`（`models.cpp:83-86`，`u != nullptr && …` 短路）**静默失效** → `auth.h:332` 放行；`check_permission_optimized`（`models.cpp:281-284`）同受影响；**只在重启（`init_indexes()`）后自愈**。
- 修复（T32 已完成，`routes_admin.cpp:795`）：两行替换为一次 **`rebuild_user_indexes();`**，并移除随之无用的 `old_username`（`:772-795` 有完整因果注释）。
- 危害与对照（T32/队长核实，当次已核对代码）：改名后新名登录 `must_change_password=true`，而改密接口返回 **HTTP 404** ⇒ 用户**既进不去也改不了密，账号等于被改坏**；修复后三段对照 **403（含 must_change_password 体）/ 改密 200 / 改密后同会话 200**。
- **契约勘误**：`docs/audit/BATCH0_CONTRACT.md:502` 原写「改名 `routes_admin.cpp:721-735`…**顺序正确**」，`:503-511` 已由队长追加勘误块（原文划删、不改写历史），并把 **B6 的修复范围由「两条 `users.erase` 路径已修；改名路径未修」更新为「三条同源路径现已全部闭合」**（`:506`）。T28 采用勘误后口径。

**（b′）T26/T27 的验证下界 + t33 后置复核（必须与结论同读）**
> **T26 的复核对象为 T32 落地前的树**（其指纹绑定为 T32 之前的 `routes_admin.cpp`）；**T27 覆盖的是含 T32 的树**（reviewer 明确对 T32 改动后的新修订重跑了 B2 端到端，其 `routes_admin.cpp` 绑定为 `9BC7CE74`）—— 两者范围不同，须分别引用。T32 落地后，改名路径另由**后置独立复核任务 t33** 以**隔离变量式对照实验**验证 —— **t33 verdict = pass（attempt 1，已完成）**，其结论作为 B2 的补充证据（见下）。**T28 未因等待 t33 而延后**（t33 在本报告落盘前完成）。

**（b″）t33 的后置独立复核结论（**隔离变量式的对照实验**，非引用 T32）**

> **方法性质必须写清**：t33 **不是**"回退代码试试"，而是构造了一个**只差这一处改动的对照二进制** ——
> ① 把当前 `routes_admin.cpp` 复制为 `%TEMP%\t33\routes_admin_prefix.cpp`，**仅**把 T32 的改名块**反向回退**（正则定位唯一命中）；
> ② 用**当前源码的其它 8 个 TU**（不变）+ 这份副本链接出 `server_prefix.exe`；
> ③ 用 `git diff --no-index` **证明副本与当前源码的差异仅限 rename 块**（**14+/5−，2 个 hunk：`:771-782`、`:794-796`**）。
> → 因此「修复前 200 绕过 / 改密 404」这两个现象**只能归因于该处改动**，排除了"别的差异导致"的可能。**这是本报告采纳其结论而不另做复现的理由。**

- **修复前（反向副本 `server_prefix.exe`）**：`GET /api/student/points/records` → **HTTP 200 / body.code=200**（**强制改密被绕过**）；`POST /api/auth/change-password` → **HTTP 404 / body.code=404 / msg=用户不存在**（**账号被改坏：连改密都做不到**）；`GET /api/student/info` → HTTP 200 / body.code=404（次生现象）。
- **修复后（当前源码二进制 `server_fixed.exe`）**：受门禁端点 → **HTTP 403 + body.code=403 + `must_change_password=true`（文案一致）**；改密 **200**；`/api/student/info` → **200/200 且 `data.id=student-02-01-01`、`data.username=student_e2e`**；同会话再请求 → **200**（不误伤）。
- **索引一致性（自写探针 7/7 PASS）—— 用 size 等式表达，一眼可判**：
  ```
  [fixed]  users=3 / user_id_map=3 / user_username_map=3   ← 等式成立，两个查询均命中
  [prefix] users=3 / user_id_map=2 / user_username_map=3   ← 等式不成立，user_id_map 正好少 1
  ```
  （比「某个 id 查不到」更适合作为**机械判据**。）
- **两条死代码声明均经 t33 独立检索证实**：`remove_user_index` —— 定义 `models.cpp:256`、声明 `models.h:111`、**仅注释** `models.cpp:228` 与 `routes_admin.cpp:775-776` ⇒ **活调用点 = 0**；`check_permission_optimized` —— 定义 `models.cpp:280`、声明 `models.h:117`、**全仓再无命中** ⇒ **活调用点 = 0**。
  - ⚠️ **反向提醒（务必守住）**：**`update_user_index` 仍有 6 处活调用**（`routes_admin.cpp:572`、`:684`、`routes_public.cpp:175`、`routes_teacher.cpp:144`、`:214`、`:954`）⇒ **不可与前者写成"一并清理"**。（当次实测复核 = 6 处；与契约 `:511` 的警告、§3.9、§8⑤、§9⑧ 一致。）
- **封装问题：按"哪条路径不一致 / 哪条一致"分层书写（t33 的对照测量）**：
  - **次生现象已消失**：`/api/student/info` 由 **200 + body.code=404「用户不存在」** 变为 **200/200 带数据**；
  - **封装不一致仍存在，但不在门禁路径上**（对照实测）：`GET /api/auth/me`（无会话）**200/401**、`POST /api/auth/login`（空）**200/400**、`POST /api/admin/import`（空）**200/400**、`POST /api/admin/users`（空）**200/400**；
  - **基线路径是一致的**：`GET /api/admin/users`、`POST backup` 为 **200/200**，重复 backup 为 **500/500**；
  - **门禁路径本身一致**：`enforce_password_change`（**`auth.h:335` `res.status = 403;`**，当次实测该行原文）⇒ 未改密访问受门禁端点得到 **HTTP 403 + body 403**（**不是** 200/403）。
  - → 该封装问题归 **批次 2（B17/F2）**，**非 T32 引入**；本报告在 §4.3 的 **`T27-R7`** 旁同时标注这组对照测量。
- **二进制来源（固定）**：两个二进制均由当前源码（+隔离副本）当场全量编译，产物在 `%TEMP%\t33`，**未用仓库根任何 `.o`**：
  ```
  server_fixed.exe   sha256=1401516B047483D079DDDA63E3A457CDCA8F8856AE0C45A3C840A9BE8E53240F   6,030,092 B   ← 当前源码（含 T32）
  server_prefix.exe  sha256=FC8085619C0E00883210D049F6C31EFB27A3E5D864A75B922039062B6B2600D6   6,030,116 B   ← 反向副本（仅 rename 块回退）
  ```
  T28 自身断言（§6.1）采用同一编译分工，其产物 `sha256` 见 §6.1。
- **方法学条目（t33 自曝并纠正，也是 T32 作者独立踩过的同一个坑）**：**`sqlite3.c` 是 C 源码，必须用 `gcc` 编译；用 `g++` 会因 C++ 类型规则失败。** 全量重编须依 `ci.yml` 的 **`gcc`（sqlite3.c）/ `g++`（.cpp）** 分工（本报告 §6.1 与 §9 均已写入）。

- **验证证据**：t7 / t26（13 条验收项全过：4 条代设口令路径、改密 7 类非法全 400、fail-fast exit 1 等）/ t27（运行时矩阵）/ t29（R1）/ t32（改名路径）/ **t33（verdict=pass，两路独立复核）**。
- **残余风险 / 归属**：① `execute_bind` 无法区分 0 行受影响（`sqlite_wrapper.h:145-147`，而同文件 `update()` 已返回 `sqlite3_changes()`）→ verifier 已端到端复现「外部删行后改密仍报 200 而库 0 行」→ **批次 1**（§9④）；② legacy 分支绕过强制改密 → **工作区 low / 升级旧库 medium**，并建议 `main.cpp:70` 迁移 `DEFAULT 0→1`（§9②）；③ 改名路径的独立复核（t33）**已完成且 pass**。

**（d）`T27-R5`：权限 / 角色映射的同类未修路径（t27 finding #5，severity=low，resolved=未修）—— B6 修复范围之外的同类路径**

> **这是本批「同一根因类别」的又一个未修实例。** 该根因类别（「容器原地修改后不同步派生索引 / 映射」）在本批至少呈现为 5 个实例：**B6**（`users.erase` 后 `user_id_map` 下标错位）、**改名路径**（`user_id_map[id]` 被 `remove_user_index` 误删）、**`T27-R5`**（删除权限/角色后不重建映射）、**`execute_bind` 0 行不区分**（写入未生效但调用方认为成功）、**`load_users_from_db` 内存超前于库**。

- **缺陷**：`DELETE /api/admin/permissions`（**`routes_admin.cpp:1147` `permissions.erase(it);`**，当次实读原文）与 `DELETE /api/admin/roles`（**`routes_admin.cpp:1183` `roles.erase(it);`**）之后**不重建** `permission_id_map` / `role_id_map` / `role_permission_map` → 被删实体的条目仍在内存中 → `check_permission_optimized()`（`models.cpp:280-300`）在进程存活期间**仍让「已删除的自定义权限」生效**；**只在重启后由 `init_indexes()` 自愈**。
- **与 B6 的关键区别**：这三个 map 以 **id 为键**，不会像 user 索引那样因向量前移而**错位**，故 reviewer 定级 **low** 而非「B6 同类」；但**「删除后仍生效」的语义后果与 B6 同类**。
- **为什么它是盲区**：**T21/T24 只覆盖 `init_indexes()` 幂等**，**B6 只覆盖 `users.erase` 的两条路径**（+ T32 的改名路径）→ 权限/角色映射的删除路径**从未被任何本批任务覆盖**。
- **归属与修法（批次 1）**：在 `permissions.erase` / `roles.erase` 之后同样调用一次整体重建（或把 role/permission 映射纳入稳定键重构），并在同批加一条「**删除后该权限码立即失效**」的回归断言（roadmap 0-9 的措辞「所有 erase 路径」本可覆盖此处）。
- **证据出处**：**t27 findings #5（T27-R5）**；行号由 T28 当次 `read` 核对（`routes_admin.cpp:1147`/`:1183`）。

### 3.8 F4 — 教师端评价列恒「未评价」（high）→ **已修复（真后端 + mock 双路径）**

- **真后端路径（t5）**：删除硬编码假评价数组（原 `TeacherApp.vue:168-171` 的 `studentId:1` 85/90 两条；现为 `:180-181` 注释 + `const evaluations = ref<Evaluation[]>([])`）；`interface Evaluation` 按后端真实 **snake_case 9 字段**重写（`routes_teacher.cpp:758-771` 实际字段：`id/student_id/student_name/className/dimension_id/dimension_name/score/comment/evaluator_name/time`）；`loadEvaluations()` 改请求 **复数端点** `GET /api/teacher/evaluations`（后端路由 `routes_teacher.cpp:729`，t5 时记 `:728`），回填时 `String(item.student_id)` 归一化；匹配判据两侧 `String()` 归一化（修复前 `1 === "student-02-01-01"` 恒 false → 恒「未评价」）；两个「开发中」死按钮改 **disabled** 禁用态（`:1090-1095`），依据后端无 DELETE 端点。
- **mock 演示路径（t13）**：`frontend/src/mock/index.ts` 新增 `GET /api/teacher/evaluations`（**当次行号 `:569-593`**，t13 时记 `:481-506`；返回后端同形 9 字段、未登录 401、按 `created_at` 倒序）；`POST /api/teacher/evaluation` 改为按真实契约解析 `body.studentId` + `body.scores`（**当次行号 `:595-621`**，t13 时记 `:507-534`）；顺带修 `import.meta.env?.DEV`（**当次行号 `:814`**，t13 时记 `:720`）以免非 Vite 运行时被 catch 吞成 500。
- **验证证据**：t5（typecheck exit 0 + Node 等价比较实测）、t13（node 直调 `mockRequest` 14/14 PASS）、t26 `[5]`（真后端 POST 后 GET 由 **0 条 → 5 条**含 `student-02-01-01` 德育(1)=90；mock **14/14 PASS**）、t27（「不再假数据」）。
- **残余风险 / 归属**：`POST /api/teacher/evaluation` 返回的 `id` 语义为维度号，而 GET 的 `id` 为表主键 → 若做评价编辑/删除需注意（t5 已记录）→ 批次 2/3。闭合。

### 3.9 B6 — `users.erase` 后索引错位（high）→ **已修复（三条同源路径全部闭合）**

- **两条 `users.erase` 删除路径（t9）**：
  - `routes_admin.cpp:885-889`（删除用户：`delete_user_from_db` → `users.erase(user_it)` → **`rebuild_user_indexes()`**；T1 记 `:826-827`，t9 记 `:876-880`，见 §8③）
  - `routes_teacher.cpp:258-262`（删除学生：`users.erase(it)` → `delete_user_from_db` → **`rebuild_user_indexes()`**；tou 契约记 `:259-260`）
- **第三条同源路径（改名，t32）**：见 §3.7(c) —— `routes_admin.cpp:795`。
- **索引重建的幂等性（t24）**：`models.cpp:187-195` 新增共用内部实现 **`clear_and_rebuild_user_indexes()`**；`init_indexes()`（`:210-224`）对 `user_id_map`/`user_username_map`（经助手）与 `role_id_map`/`permission_id_map`/`role_permission_map` **全部先 clear 再重建**；`rebuild_user_indexes()`（`:240-242`）复用**同一助手** → 两条路径不会各自漂移（比只加一行 `clear()` 更好）。
- **验证证据**：t9（落地）、t24（幂等）、t26 `[8]`（建 AAA/BBB/CCC → 删 BBB → `/api/user/info` 前后均 CCC；`/api/admin/roles` 删除前后**逐字节相同** 1375→1375，无 7→14→21）、t27、**t24 的第二路独立证据**（不同作者/手法：`role_permission_map` 1/2/3 次调用恒 **23**，旧实现为 23/46/69；`users` 向量未被改动；删中间者后 `uC→C` 且 `uB→nullptr`）。
- **当次源码复核（T28 自跑）**：三条路径确认 —— `routes_admin.cpp:886/:889`、`routes_teacher.cpp:258/:262`、`routes_admin.cpp:795`；且 `remove_user_index` 的**活调用点当次实测 = 0**（仅剩 `models.cpp:256` 定义、`models.h:111` 声明、两处注释），与契约勘误 `:510` 一致 → 印证 §8⑤「不是死代码」→「T32 后成为真死代码」的变化过程。
- **残余风险 / 归属**：B6 本批为**临时修复**（clear + 重建），**稳定键彻底版留批次 1-3**（`BATCH0_DESIGN_DECISIONS.md:368`）；`remove_user_index()` 现可删除或修正其陷阱语义，`update_user_index()` **不可删**（当次实测仍有 **6 个活调用点**：`routes_admin.cpp:572`、`:684`、`routes_public.cpp:175`、`routes_teacher.cpp:144`、`:214`、`:954`）；`check_permission_optimized()` 活调用点 = 0（同为死代码）。
- **两条死代码声明 + 反向提醒（t33 第三方独立确认，T28 当次复核一致）**：
  - `remove_user_index`：定义 `models.cpp:256`、声明 `models.h:111`、**仅注释** `models.cpp:228` 与 `routes_admin.cpp:775-776` ⇒ **活调用点 = 0**；
  - `check_permission_optimized`：定义 `models.cpp:280`、声明 `models.h:117`、**全仓再无命中** ⇒ **活调用点 = 0**；
  - ⚠️ **`update_user_index` 仍有 6 处活调用 ⇒ 不可与前者写成"一并清理"** —— 该警告与 **契约 `docs/audit/BATCH0_CONTRACT.md:511`** 的原文（「切勿写成『`remove_user_index()` 与 `update_user_index()` 一并清理』——那会误删仍在使用的函数」）以及本报告 **§8⑤、§9⑧、§10.3** 完全一致（当次逐处核对）。
- **B6 的机械判据形态（t33 的 size 等式，建议后续沿用）**：`users.size == user_id_map.size == user_username_map.size` —— `[fixed] 3/3/3` 成立、`[prefix] 3/2/3` **user_id_map 正好少 1**（§3.7(b″)）。

> ### ⚠️ B6 的修复范围必须限定（否则会被读成「索引一致性已整体解决」）
> **本批只修了 `users` 索引的三条路径**：① `routes_admin.cpp:885-889`（删用户）② `routes_teacher.cpp:258-262`（删学生）③ `routes_admin.cpp:795`（改名，T32）。
> **权限 / 角色映射的同类路径未修** —— 见 §3.7(d) 的 **`T27-R5`**：`DELETE /api/admin/permissions`（`permissions.erase`，**`routes_admin.cpp:1147`**）与 `DELETE /api/admin/roles`（`roles.erase`，**`routes_admin.cpp:1183`**）之后**不重建** `permission_id_map` / `role_id_map` / `role_permission_map`。
> **读者提示**：不得把本条结论外推为「本批已解决全部 erase 后索引/映射失效问题」。（该盲区 = **`T27-R5`**）

### 3.10 N4 — `.trae/` 25 个文件仍被跟踪（low）→ **已从索引移除 26 条**

- **正确表述（四视角，当次实测，全部用 `-c core.quotepath=false`）**：

| 视角 | `.trae/` 计数 | 含义 |
|---|---|---|
| HEAD（最后一次提交 `47006bd`） | **26** | 提交历史里确实跟踪着 26 个 `.trae/` 文件 |
| 当前索引（`git ls-files -- .trae`） | **0** | 已全部从索引移除 |
| 工作区磁盘（`Get-ChildItem -Recurse -File .trae`） | **26** | 文件**未被删除**，只是不再被跟踪 |
| `git status` 的 `D ` | **26** | 已暂存删除（索引态），**尚未 commit** |

- **判定依据**：`git ls-tree -r --name-only HEAD -- .trae/` → **26**；`git diff --cached --numstat -- .trae/` → **26 条全删除**；`git diff --cached --stat` → `26 files changed, 1820 deletions(-)`；`git ls-files .trae/` → **0**；磁盘 → **26**；`git status --porcelain` → 26 行全 `D`。
- **结论表述（定稿）**：**「已从索引移除 26 条、工作区文件全部保留、未提交」** —— **不是**「跟踪 26」（那会被读成"现在仍被跟踪"）。
- **`git diff` 不含这 26 条**（工作区 vs HEAD 只看内容差异；删除在**索引**里）→ 必须看 `git diff --cached` 才可见。**不得**把 N4 写成「已彻底提交完成」。
- **数字更正**：审计文档写「25 个已跟踪文件」、T1 契约记「跟踪 25 / 磁盘 26」，**均为 `core.quotepath` 转义伪差**（见 §8②），**不是「磁盘多一个未跟踪文件」**。
- **残余风险 / 归属**：`D ` 是索引态删除，**需要一次 commit 才落地**（本批不 commit）；`.trae/` 仍在 `.gitignore:91` 中排除。闭合（按「已从索引移除」口径）。

### 3.11 mock 三处（演示站路径）→ **全部闭合**

| # | 项 | 改动 file:line（当次） | 证据 |
|---|---|---|---|
| 1 | `GET /api/teacher/evaluations` 缺失 | `frontend/src/mock/index.ts:569-593` | t13；t26 `[5]`（mock 14/14 PASS） |
| 2 | `POST /api/teacher/evaluation` 字段名错配 → 按 `studentId` + `scores` 解析 | `frontend/src/mock/index.ts:595-621` | t13（写回读回 `student_id==='S001'`，修复前 undefined）；t26 `[5]` |
| 3 | `POST /api/auth/change-password` + `must_change_password` 字段（避免演示站「要求改密却无法改密」死局） | `frontend/src/mock/index.ts:331-350`（改密）、`:291/:301-317`（下发改密标记）、`:814`（`import.meta.env?.DEV`） | t18（落地）；t26 `[5]`（mock 改密闭环：`admin123`→400、新旧相同→400、合法→200 且 `/api/auth/me` 由 true 变 false） |

- **归属**：三者均属 **F4 与 B2/F13 在 mock 演示路径上的等价覆盖**；**不属**批次 0 的 10 条主项之外的新条目，而是同一批的演示站分支。
- **残余风险**：mock 与真后端字段同形性由 t13/t18/t26 断言，未做「同一请求打到两实现并 diff 响应」的自动化对照 → 批次 1 可考虑加该对照。

### 3.12 批次 0 后段收口项（t29 / t30 / t31）—— **不增条，但归入主项**

> **记账口径（三句话）**：
> 1. 审计的批次 0 = **10 条主项**，其闭合状态按各自主项判定，**不因 t29/t30/t31 而增加条数**。
> 2. t29/t30/t31 是「批次 0 期间由验证与评审发现、并在结项前就地修掉的**收口项**」。
> 3. 其中两条是主项判据真实成立所必需的，因此注明归属：**t29 ∈ B2**（门禁覆盖面最后一块，见 §3.7b）、**t30 ∈ F13 的文档层**、**t31 ∈ F1 的交付物卫生**。

| 任务 | 内容 | 归属 | 关键证据 |
|---|---|---|---|
| **t29** | `routes_student.cpp:65` 补强制改密门禁（5/5 覆盖） | **B2** | 修复前 HTTP 200（带积分数据）→ 修复后 403，改密后 200 |
| **t30** | 移除交付文档中已失效的默认口令与已删除的自动升级链描述 | **F13 文档层** | 三处交付文档明文口令 grep = 0（文档层） |
| **t31** | 打包排除 `docs/audit`，消除同一交付 zip 内「无默认口令」与「仍发布 admin123」的相反陈述 | **F1 交付物卫生** | 当次实测：`frontend-dist` 命中 **0**、`name: server-exe` 命中 **2**、`rm -rf release/docs/audit` 命中 **2**、反向守卫 `if [ -d release/docs/audit ]` 命中 **2**；`ci.yml +62/-12`、`release.yml +25/-8` |

### 3.13 本批改动文件总账（当次实测）

`git -c core.quotepath=false diff --numstat`（工作区 vs HEAD，**21 个文件**，+1074/−213）：

```
62    12   .github/workflows/ci.yml
25    8    .github/workflows/release.yml
13    1    .gitignore
21    11   README.md
27    0    auth.h
13    8    docs/sqlite_setup.md
12    6    docs/user_manual.md
19    3    frontend/src/lib/api.ts
54    2    frontend/src/lib/auth.ts
5     1    frontend/src/main.ts
151   16   frontend/src/mock/index.ts
6     3    frontend/src/pages/Login.vue
35    32   frontend/src/pages/teacher/TeacherApp.vue
171   7    main.cpp
69    13   models.cpp
26    0    models.h
81    12   routes_admin.cpp
139   9    routes_public.cpp
8     1    routes_student.cpp
3     2    routes_teacher.cpp
134   66   sha256.h
```

新增未跟踪文件（3 个）：`frontend/src/components/ChangePasswordGate.vue`、`frontend/src/lib/passwordGate.ts`（B2/F13 前端阻断式改密 UI，t22）；`tests/`（t2 的测试基建）。未跟踪目录：`docs/audit/`（审计与结项文档）、`.agent-teams/`（**AgentTeams 编排产物，本批不纳入交付**）。

---

## 4. 本次结项所依赖的验证范围下界（验收项 9）

### 4.1 上游验证与评审的结论

| 任务 | kind | verdict | 证据出处 | 关键内容 |
|---|---|---|---|---|
| **t3** | verification | **pass** | `docs/audit/evidence/T3_B1_BASELINE.md`（19,203 B） | B1 缺陷成立、T2 脚本能拦住、基线必然失败；含变异体实验（**覆盖漏洞**） |
| **t6** | verification | **pass** | `docs/audit/evidence/T6_B1_FIX_VERIFICATION.md`（13,700 B） | B1 修复后不可绕过；两组 128 hex 走产品接口逐字节正确；后端全量编译链接 exit 0；指纹 `sha256.h=3E5C59DD…` |
| **t26** | verification | **pass**（契约 13 条全过） | `docs/audit/evidence/T15_BATCH0_REMAINDER.md`（404 行 / 30,204 B）—— **文件名沿用 T15 前缀，本任务号是 t26**，请勿误读为 t15 产物 | B2 门禁矩阵、4 条代设口令路径、改密闭环、fail-fast、F4 真后端 + mock、B13、N1、B6、N4、F1、F16/F24；判据绑定修订 `sha256.h=3E5C59DD / main.cpp=D7211E1F / auth.h=3C520FAD / routes_public.cpp=FB893F80` |
| **t27** | review | **pass** | t27 output | B1/B2/F1 三条主线自建工具复现为「不可绕过 / 不再假数据 / 非 mock 交付」；8 项逐条因果链；未闭合项**全为 low 级**且已归属批次 1/2；评审期指纹 16 项自算 |
| **t24** | implementation | **completed** | t24 output | `init_indexes()` 幂等（`role_permission_map` 不再累积）+ `clear_and_rebuild_user_indexes()` 共用助手 → **B6/`init_indexes()` 已覆盖，不再标「未闭环」** |
| **t29 / t30 / t31 / t32** | implementation | **completed** | 各自 output | 后段收口（§3.12）与改名路径修复（§3.7c） |
| **t33** | verification | **pass**（attempt 1，已完成） | t33 output（本次落盘**前**完成） | T32 改名路径的**后置独立复核**：反向构造 T32 前二进制复现绕过与账号被改坏；修复后三段对照；索引一致性 7/7；两条死代码声明属实；封装不一致归批次 2 |

### 4.2 必须与结论同读的边界

1. **T26 / t3 / t6 的复核对象是 T32 落地前的树**（指纹绑定为 T32 前修订）—— T27 **不是**：reviewer 明确「**对 T32 改动后的新修订重跑了端到端**」（其绑定的 `routes_admin.cpp` 指纹为 **`9BC7CE74`**，即含 T32 的树），故 **T27 的评审覆盖含 T32 的树**，与本批其他上游复核的范围不同，须分别引用。
2. 随后由 **t33** 做**第三路独立复核**，**t33 verdict = pass（已完成）**，故 §3.7(c) 的改名路径结论 = **「代码已落地（T32 completed）+ 我当次源码核对 + t33 两路独立复核（含反向构造修复前二进制）」**。
3. **T28 不因等待 t33 而延后**（队长的明确口径）：未闭环面与复核状态由本报告内容承载。
4. **T28 自身的断言**全部当次执行（§6），不引用上述任何任务的数字。
5. `t24` 状态为 **completed**，故按 **已覆盖** 记账；本报告不再把 B6/`init_indexes()` 标为未闭环（但**范围限定**见 §3.9 的 ⚠️ 提示与 §3.7(d)）。

### 4.3 t27 的 findings 逐条（8 条，rev.2 补入；rev.3 按 reviewer 编号定稿）

> **判据绑定**：reviewer 的指纹为**自算**（构建前后各打印一次、无漂移），且**对 T32 改动后的新修订重跑了 B2 端到端** → **T27 覆盖的是含 T32 的树**。以下 8 条为 t27 的原始 findings，`resolved` 状态照抄。
>
> ⚠️ **编号口径（务必先读，否则极易误引）**：**本报告一律使用 reviewer 自报的编号 `T27-R1…R8`**。队长在先前通报中曾用**另一套 R 编号**，其中 **R3/R4 与 reviewer 相反**；下表最后一列给出映射。为免读者对不上，本报告正文中凡出现「R1…R8」均指 reviewer 编号。

| # | finding id（**采用 reviewer 编号**） | severity | resolved | 主题 | 队长的旧编号映射 | 归属 / 状态 |
|---|---|---|---|---|---|---|
| 1 | T27-R1 | **medium** | **True** | 学生积分端点门禁漏接（`routes_student.cpp:65`） | R1 | **已闭合：T29 修**，reviewer 运行时复核（同会话 200→403、改密后 200）→ §3.7(b) |
| 2 | T27-R2 | low | **True** | 交付包文档失效口令 + 已删除升级链描述（`README.md:615-626`、`docs/sqlite_setup.md:132-140`、`docs/user_manual.md:16-20`） | R2 | **已闭合：文档层 T30 + 打包层 T31**，经 reviewer **运行时 / grep 双重复核**（`ci.yml=B0909406`、`release.yml=06439A87` 的现行修订；`ci.yml:170-178` + `release.yml:114-122`）→ §3.12。**不是批次 1 建议项**（否则与代码矛盾） |
| 3 | T27-R3 | low | — | **legacy 裸 SHA256 分支非常量时间比较**（`sha256.h:255`；原语级可复现 Δ=+1.61ns，但**产品级被 ~850ns 的 SHA-256 淹没、无法稳定复现**；右操作数不可由请求控制 ⇒ 攻击模型不成立） | **R4** ⚠️ 反 | **批次 1**：随「存量一律失效」直接删除该分支（§9⑤）。**本批不修的第二个原因**：`sha256.h` 指纹 `3E5C59DD` 已被 T6 证据绑定，改动需重跑整条 B1 验证链 |
| 4 | T27-R4 | low | — | **F13 产物级证据缺失 / CI 守卫不覆盖口令字面量**：四个演示按钮包在**运行时** `v-if="isMockEnabled()"`（`Login.vue:181`）内，字面量是否被 rollup/esbuild 摇掉**无字节级证明**；且 `ci.yml` 中 `admin123\|teacher123\|student123\|parent123` 命中 = **0**（**当次实测**）⇒ 守卫**不覆盖**固定口令字面量 | **R3** ⚠️ 反 | **无法在本机验证**（`npx vite build` 被沙箱拦下：exit 1 / esbuild `spawn EPERM`，与 T26 同一限制）→ **判据改由 CI 每次 push 强制证明**，**归批次 1**；与 §9⑨（CI 层收敛）是**同一件事，已合并为一项**（不写成两项）。影响有限：这些口令已全部失效（种子为随机口令），泄漏物是惰性字面量而非可用凭据 |
| 5 | T27-R5 | low | — | **删除自定义权限 / 角色后不重建索引 → 「已删权限仍生效」**（`routes_admin.cpp:1147` / `:1183`） | — | **本批未修**，见 §3.7(d)（单列强调）；修法归 **批次 1** |
| 6 | T27-R6 | **medium** | — | 家长登录**完全未接**登录锁定（`routes_parent.cpp:49-132`）+ `login_client_key` 无条件信任 `X-Forwarded-For`（`auth.h:141-154`）+ `login_attempts` 无上限无过期（`auth.h:111-114`）+ `verify_password` 实测 **147.9ms/次**（放大比 ~1.5×10⁵） | R5 | **批次 1（B4）**；本批不修（属既定范围且与 B1/B2/F1 无耦合） |
| 7 | T27-R7 | low | — | 登录失败 **transport 恒 200**、语义在 `body.code`（实测错误/空/不存在用户名三种均 200） | — | **批次 2（B17/F2）** → 与 §8⑥ 的「HTTP 状态与 body code 不一致」同一族 |
| 8 | T27-R8 | low | — | mock 路由表 **62 条**（GET 30 / POST 19 / **PUT 7** / **DELETE 6**）缺 `register`、`/api/user/info`、`/api/behavior/history`、`/api/admin/system/backup` 等端点 | — | **批次 1**（队长口径；reviewer 记「批次 3（F6/F7/N6 附近）」，两者取**更早的批次 1** 作为排期上界）。**并更正旧说法**：契约/下游材料里「mock 缺 PUT/DELETE」在当前修订**已不成立**（PUT 7、DELETE 6） |

**小结**：8 条中 **2 条已 resolved（T27-R1 / T27-R2）**，其余 6 条**全部为 low / medium 且均已明确归属批次 1/2/3**，**不阻塞批次 0 结项**；其中 **T27-R5 是本批唯一「同一根因类别、本批未覆盖」的功能性盲区**（已在 §3.7(d) 单列）。**编号提醒**：`T27-R3` = legacy 非常量时间比较（**队长旧编号 R4**）；`T27-R4` = F13 产物级证据缺失（**队长旧编号 R3**）。

---

## 5. 未端到端复现事项（逐条，验收项 3）

> **纪律**：以下事项**不得**写成端到端验证；本报告对应条目均已标注为「静态核对」或「逻辑复刻」。

| # | 事项 | 状态 | 说明 |
|---|---|---|---|
| 1 | **CI zip 装配链路**（`.github/workflows/ci.yml` 的 `frontend`→`backend`→`package` 全链路、artifact 上传下载、zip 组装） | **未端到端复现** | Actions runner 本机不可用。仅有**静态核对**（job/step/artifact 数据流 + `needs:` + 上传/下载点计数，T28 §3.2 当次复测）与 t26 `[10]` 的**等价复刻**反向对照（clean→EXIT 0、三类污染→EXIT 1）。 |
| 2 | **本地 Vite 构建**（`npm run build` / `build:no-typecheck`） | **未端到端复现** | `esbuild` 子进程被拒 → **`spawn EPERM`**；队长已独立复现，与 `docs/audit/VERIFICATION.md` 第 10 节局限一致。因此「dev 包产物内不含 mock 特征串」**没有本机字节级证据**。 |
| 3 | **B13 的三条子路径** | **未端到端复现** | ① `db.execute=false`（VACUUM INTO 返回失败但未落盘）② 备份文件 **0 字节** ③ **DACL 不可写** —— 三者在沙箱内均为**逻辑复刻**；可端到端复现的只有「成功路 200 + 磁盘字节一致」与「同分钟再调 500」（t26 `[6]`）。 |
| 4 | **「dev 包产物不含 mock 特征串」** | **仅由 `ci.yml` 产物守卫承担** | 判据本身（`grep -rIl "campus_mock_db_v1" dist`）**可本机双向复现**（用 `%TEMP%` 合成 dist 做正/反两向对照），但**真实 runner 未验证**。 |
| 5 | **仓库根既存 `.o` / `server.exe` 的版本不可由 git 判定** | **不可判定 / 影响断言有效性** | 当次实测四项均被 ignore 覆盖（`-c core.quotepath=false git check-ignore -v`）：`main.o ← .gitignore:2:*.o`、`server.exe ← .gitignore:5:*.exe`、`sqlite3.o ← .gitignore:2:*.o`、`routes_admin.o ← .gitignore:2:*.o`（exit=0）；且 `git status --porcelain -- '*.o' 'server.exe'` **无任何命中**（rev.2 复核，见下）。**故任何复用了既存产物的断言都不构成本批结论**；本报告全部编译/链接断言由**当前工作区源码当场全量编译**得到（§6.1），产物一律输出到 `%TEMP%`，未覆盖仓库根既存产物。 |

**（第 5 项的 rev.2 产物卫生复核）**：该批产物（mtime **12:49–12:52**）由 reviewer 主动报告「**不在其构建路径内**（其 `-o` 全部指向 `%TEMP%`），应为并发队友产物，已被 `.gitignore` 覆盖，**未删除以免干扰他人验证**」。T28 当次复核：
- `git -c core.quotepath=false check-ignore -v` 四项**全部命中**（见上，exit=0）；
- `git -c core.quotepath=false status --porcelain -- '*.o' 'server.exe'` → **无命中**（即不出现在 `git status`）；
- **处置：不删除**（可能仍是某位成员的验证依赖），仅记录状态。

**补充（同类风险，已发生）**：t29 中 backend-engineer 自曝曾**误用陈旧 `routes_student.o` 重链过一次 `server.exe`**；t4 的 `sha256.h` 在 verifier 观测期间被写过 3 次。故「二进制来源」必须显式声明（§6.1）。

---

## 6. 结项断言（三条命令，全部 T28 当次执行，退出码均为 0）

### 6.1 断言一：完整后端编译链接（**不复用仓库根既存 `.o`**）

**命令形态**：`%TEMP%\b0_fullbuild.ps1`（T28 自写；1 次 `gcc -c sqlite3.c` + 9 次 `g++ -c <cpp> -std=c++17 -O2 -I.` + 1 次 `g++ <10 个 .o> -o %TEMP%\b0_build\server.exe -lws2_32`，与 `ci.yml:67-82` 的编译序列同口径，产物全部写入 `%TEMP%`）。

**原始输出（尾部）**：
```
=== [1/11] gcc -c sqlite3.c ===
<<< exit code = 0
=== [2/11] g++ -c main.cpp ===
<<< exit code = 0
=== [3/11] g++ -c models.cpp ===
<<< exit code = 0
=== [4/11] g++ -c logger.cpp ===
<<< exit code = 0
=== [5/11] g++ -c routes_static.cpp ===
<<< exit code = 0
=== [6/11] g++ -c routes_public.cpp ===
<<< exit code = 0
=== [7/11] g++ -c routes_admin.cpp ===
<<< exit code = 0
=== [8/11] g++ -c routes_teacher.cpp ===
<<< exit code = 0
=== [9/11] g++ -c routes_student.cpp ===
<<< exit code = 0
=== [10/11] g++ -c routes_parent.cpp ===
<<< exit code = 0
=== [11/11] link server.exe ===
<<< exit code = 0

=== artifacts ===
server.exe       3091137

=== repo-root write check (must be empty) ===
(no .o/server.exe tracked-or-modified entries; root artifacts are ignore-covered)

=== BUILD SUMMARY ===
failures   = 0
server.exe = 3091137
RESULT: ALL COMPILE+LINK EXIT 0
```
（作业退出码 **0**；构建起止：local 2026-09-19 12:56:54 起，约 4.6 分钟。`server.exe` = 3,091,137 B，**小于仓库根既存的 6,030,092 B**，原因是本断言按**验收项要求的编译序列**链接、未采用 `ci.yml:86` 的 `-static -static-libgcc -static-libstdc++ -lwinpthread` 静态开关 —— 大小差异**不影响**「编译+链接退出码 0」这一断言，但**记录在此以免读者误认同一产物**。）

**二进制来源声明**：本结项断言的 `server.exe` 由**当前工作区源码**于 **2026-09-19 12:56:54（local）**当场全量编译链接，**未复用仓库根既存 `.o`**；仓库根既存 `server.exe`/`*.o`（时间戳 2026/9/19 12:49–12:52）**未被本次断言读取或覆盖**。

**产物 `sha256`（rev.4 补充固定；同时给出**跨时间复现**证据）**：T28 在 **13:07:28** 又独立跑了**同一脚本的第二遍**（`b0_build2`，全新 `.o`）：
```
13:07:28 全量重编（b0_build2，与 12:56:54 的 b0_build 无共享产物）
server.exe = 3,091,137 B   sha256 = F9E79042655E655846686895B61FAB7EE1C480AA711507FEC2F573F18F606F48
两遍逐文件字节数完全一致（routes_admin.o 均为 1,438,311 B 等）
```
→ 即：**同一源码在不同时刻的两次独立全量重编产出字节相同的 `server.exe`**（这两遍都慢于 t33 的 sha256 锚点，故不与 t33 的 6,030,092 B / `1401516B…` 冲突 —— t33 采用了 `ci.yml:86` 的 `-static*` 静态开关，而本断言按验收项的编译序列链接，**已在上一段说明大小差异**）。

**源代码状态锚点（当次计算的事件点指纹）**：
```
sha256.h            3E5C59DDC9AEB5EE22488A8058AA7BA89C479BDD054179048A4F26D3A00DFA22
main.cpp            D7211E1FA47EB89AEDAA195CCCC8D8624D32706DB8E6EBBC446AC379F435E4CC
auth.h              3C520FAD3AA5F7DDC4F2FE79A3E632B517E27B85B22E76EE130A7FD438E92840
routes_public.cpp   FB893F80500AA404A37DCD5068E43D0234CF09AF8422BB2302B0CCF9C166D846
```
四个锚点与 **t26 判据绑定修订逐字一致**（t27 亦独立算出同一值）→ 本次断言的对象 = 被验证/被评审的那棵树。

### 6.2 断言二：C++ 单元测试

```
>>> pwsh -NoProfile -File tests/run_tests.ps1
--- [ OK ] verify_password_legacy_sha256  (2 checks)

============================================================
cases : 11 run, 11 ok, 0 failed
checks: 36 run, 36 ok, 0 failed
RESULT: PASS
<<< test exit code = 0
WRAPPER_EXIT=0
```
（包装脚本退出码 **0**。）

### 6.3 断言三：前端类型检查

```
>>> cd frontend; npm run typecheck
> campus-energy-station-frontend@1.0.0 typecheck
> vue-tsc --noEmit

EXIT=0
[stderr] npm warn Unknown env config "npm-globalconfig". This will stop working in the next
         major version of npm. See `npm help npmrc` for supported config options.
```
（退出码 **0**；stderr 仅为 npm 环境变量提示，与类型检查无关，无任何 TS 错误。）

### 6.4 交付记账：git 状态（原始输出）

`git -c core.quotepath=false status --porcelain` —— **52 行**（`-c core.quotepath=false` 下中文文件名不再被转义）：

```
 M .github/workflows/ci.yml
 M .github/workflows/release.yml
 M .gitignore
D  .trae/documents/admin_fix_plan.md
D  .trae/documents/cookie-auth-refactor.md
D  .trae/documents/localize-fonts-plan.md
D  .trae/documents/remove-memory-storage-mode.md
D  .trae/documents/优化教师班级编辑与编号自动匹配.md
D  .trae/specs/add-parent-portal/checklist.md
D  .trae/specs/add-parent-portal/spec.md
D  .trae/specs/add-parent-portal/tasks.md
D  .trae/specs/batch-import-students/checklist.md
D  .trae/specs/batch-import-students/spec.md
D  .trae/specs/batch-import-students/tasks.md
D  .trae/specs/fix-admin-and-login/checklist.md
D  .trae/specs/fix-admin-and-login/spec.md
D  .trae/specs/fix-admin-and-login/tasks.md
D  .trae/specs/fix-teacher-click-parent-nav/checklist.md
D  .trae/specs/fix-teacher-click-parent-nav/spec.md
D  .trae/specs/fix-teacher-click-parent-nav/tasks.md
D  .trae/specs/refactor-uid-and-teacher-classes/checklist.md
D  .trae/specs/refactor-uid-and-teacher-classes/spec.md
D  .trae/specs/refactor-uid-and-teacher-classes/tasks.md
D  .trae/specs/unify-mobile-nav-and-admin-parent/checklist.md
D  .trae/specs/unify-mobile-nav-and-admin-parent/spec.md
D  .trae/specs/unify-mobile-nav-and-admin-parent/tasks.md
D  .trae/specs/write-project-docs/checklist.md
D  .trae/specs/write-project-docs/spec.md
D  .trae/specs/write-project-docs/tasks.md
 M README.md
 M auth.h
 M docs/sqlite_setup.md
 M docs/user_manual.md
 M frontend/src/lib/api.ts
 M frontend/src/lib/auth.ts
 M frontend/src/main.ts
 M frontend/src/mock/index.ts
 M frontend/src/pages/Login.vue
 M frontend/src/pages/teacher/TeacherApp.vue
 M main.cpp
 M models.cpp
 M models.h
 M routes_admin.cpp
 M routes_public.cpp
 M routes_student.cpp
 M routes_teacher.cpp
 M sha256.h
?? .agent-teams/
?? docs/audit/
?? frontend/src/components/ChangePasswordGate.vue
?? frontend/src/lib/passwordGate.ts
?? tests/
```

`git -c core.quotepath=false diff --stat`（尾部）：`21 files changed, 1074 insertions(+), 213 deletions(-)`（完整逐文件 numstat 见 §3.13）。
`git -c core.quotepath=false diff --cached --stat`（尾部）：`26 files changed, 1820 deletions(-)`（即 N4 的索引态删除）。

**记账结论**：
- **无 commit、无 push**：HEAD 仍为 `47006bd…`；`git rev-list --left-right --count main...origin/main` → **`0 0`**，`origin/main` = `47006bd`（同一提交）。
- **无运行期数据库/日志产物入库**：`*.db`/`*.log` 在 `.gitignore:84-88`，**跟踪数 = 0**；`git status --porcelain --ignored` 下无任何 `.db`/`.log`（仅 `!! server.log` 与 `!! vcpkg/`，均被 ignore 且未被跟踪，且**未被本批触碰**）。
- **编译产物如实区分**：`tests/pbkdf2_test.exe`（150,058 B）是脚本本次运行的编译产物，`git check-ignore -v` 命中 `.gitignore:40 tests/*`（exit=0）→ **被忽略、未入库**，**不计为「运行期产物入库」**；可跟踪的是 `tests/pbkdf2_rfc7914_test.cpp`（27,752 B）与 `tests/run_tests.ps1`（4,056 B）两份**源代码**（`.gitignore:41-42` 显式放行）。
- **编排产物**：`.agent-teams/` 单列为 **AgentTeams 编排产物，本批不纳入交付**（是否加入 ignore 留批次 1 评估；**本批不动 `.gitignore` 的放行规则**，它已被 t2 用于测试源码放行）。

### 6.5 t33 交出的「当前行号清单」当次核对（防「他人改动被回退」）

> 用途：t33 逐条列出 t7/t9/t24/T32 的关键落点在 **T32 插入后的当前位置**；T28 当次 `read` 复核，确认**没有任何上游改动被回退**，且行号与契约里的旧值**存在漂移**（故一律以当次实测为准）。

| 归属 | 声明位置 | **T28 当次 read 实测原文** | 结论 |
|---|---|---|---|
| t9 | `routes_admin.cpp:8` | `#include <filesystem>` | ✅ 在位 |
| t9 | `routes_admin.cpp:337-393`（判据 `:371`） | `:337` 为 B13 旧实现注释起；**`:371` `bool exec_ok = db.execute("VACUUM INTO '" + backup_path + "'");`**；`:393` 为 reason 三元 | ✅ 在位 |
| t9 | B6 调用点 `routes_admin.cpp:889` | `:889 rebuild_user_indexes();` | ✅ 在位 |
| t9 | `routes_teacher.cpp:262` | `:262 rebuild_user_indexes();` | ✅ 在位 |
| t24 | `routes_admin.cpp:1736` | `:1736 init_indexes();`（导入后全量重建） | ✅ 在位 |
| t24 | `main.cpp:460` | `:460 init_indexes();` | ✅ 在位 |
| t24 | `models.cpp:211` / `:241` | `:211 clear_and_rebuild_user_indexes();` / `:241 same`（`init_indexes()` / `rebuild_user_indexes()` 共用助手） | ✅ 在位 |
| t7 | `routes_admin.cpp:951` | `:951 "UPDATE users SET password_hash = ?, must_change_password = 1, …"` | ✅ 在位 |
| t7 | `routes_admin.cpp:1539` | `:1539 existing->must_change_password = true;`（导入 overwrite 分支置 true） | ✅ 在位 |
| t7 | `routes_public.cpp:525` | `:525 "UPDATE users SET password_hash = ?, must_change_password = 0, …"`（改密置 0） | ✅ 在位 |
| T32 | `routes_admin.cpp:772-795`（`:795`） | `:772` T32 因果注释起；**`:795 rebuild_user_indexes();`** | ✅ 在位 |
| — | `auth.h:335`（门禁设 HTTP 状态） | `:335 res.status = 403;` | ✅ 在位（对应 §3.7(b″)） |

**行号漂移对照**（同一改动点的不同历史口径）：`routes_admin.cpp` 删用户分支 826（契约）→ 876-880（t9）→ **889（当次）**；改名块 721-735（契约）→ 732/734（HEAD）→ 783/785（T32 前）→ **772-795（当次）**；`routes_teacher.cpp` 删学生分支 259（契约）→ 258-262（t9）→ **262（当次）**。

---

## 7. 过程缺陷清单（验收项 6，不得淡化）

### 7.1 元规则总纲（本批最值得留下的结论）

> **总纲：任何判据 / 规则 / 契约条款，若不能由一条命令、一个退出码或原始字节机械判定，就必然在本批这种多轮协作里失效。**

本批有**三件看似不同的事**，全部是同一根因：

| 表面现象 | 实质 | 代价 |
|---|---|---|
| **计数陷阱**（4 次，§7.5） | 数字没有被当场重新测量 | 报告与决策一度建立在错值上 |
| **T2 的门禁缺口**（§7.2） | 判据存在但**没有判别力**（不可构造失败样本） | 「只修 A/B/C」的实现能让脚本 10/10 全绿 |
| **队长那条 verify 命令**（§7.4⑥） | 契约文本**不可执行**且**字面不稳定** | t31 首次提交被门禁拒；命令在本机无法直接执行 |

**三条可执行规则**（配合总纲）：
1. 任何写进报告的数字都必须附**当次原始命令输出**；跨轮次引用的数字一律**重新测量**。
2. 涉及**字符内容 / 编码**的判断，必须读**原始字节**或带编码的原始输出，不得以控制台渲染结果为事实依据。
3. **任何路径 / 文件计数，必须使用 `git -c core.quotepath=false …` 或 `-z`**；`core.quotepath=true`（本机默认）会把非 ASCII 路径转义为 `"..."` 八进制形式并使前缀过滤**静默漏计**（本批出现 **4 次，其中 2 次是队长本人**，逐条见 §7.5）。

**两条推论**：
- **「判据是否可执行」可用反向构造检验：能构造出一个失败样本，判据才算存在**（T2 变异体实验即此方法）。
- **verify 命令的形式不是风格问题，而是正确性问题**：`pwsh -NoProfile -File <脚本>` 同时满足「可执行」与「字面稳定」，而「一行里塞格式化字符串」两个条件都不满足 —— 这解释了它为何同时触发 §7.4⑥ 那一族的两个缺陷。

#### 7.1.1 七条「假阴性 / 假证据 / 可复现性」方法论条目（M-1…M-7）

> 总纲推论：**「零命中」「缺端点」「未调用」这类否定结论比肯定结论更容易造假** —— 因为一个**错误的模式**就会产生一个**看起来像证据的零命中**。

- **M-1 —— 错误的 grep 模式产生「零命中」，而零命中看起来像证据**：reviewer 曾用 `login_attempt|lockout|…` 检索，得到「未见锁定实现」的假阴性；真实符号是 `login_can_try` / `login_record_fail` / `login_record_success`（定义 `auth.h:117/126/137/141`、调用 `routes_public.cpp:29/30/39/54/60`）。
  → **教训（可执行）**：凡「某函数零调用 / 某能力不存在」这类**否定结论**，必须**用多个候选符号交叉验证**，并给出**定义点与调用点的双向证据**（不能只给一侧）。
  → **本报告的自检**：本报告也给出了三处「活调用点 = 0」的否定结论（`remove_user_index`、`check_permission_optimized`，以及 `remove_user_index` 曾为活的更正）。这三处由 **T28 独立检索 + t33 独立检索**两路验证，且同时列出**定义点**（`models.cpp:256`/`:280`）与**候选调用符号的全部命中**（仅声明/注释），符合 M-1 要求。
- **M-2 —— 正则转义造成的假阴性**：mock 路由写作 `/^\/api\/teacher\/evaluations$/`，**按字面检索**会得到「缺端点」的假阴性；把 `\/` 归一化为 `/` 后 **44 个端点齐备**（本报告 §3.11 的 mock 三处即在核对时踩到同一坑，最终改用 `-match 'evaluation'` 复核）。
  → **教训**：检索源码中的**正则字面量**时，必须至少用「归一化后的片段」与「宽模式」两种方式交叉。
- **M-3 —— 把「传输状态」当业务语义**：登录失败不设 HTTP 状态、真实语义在 `body.code`（= **T27-R7**）。这不是误读源码，而是**误读运行证据**。
  → **教训**：运行期证据必须同时记录 **HTTP 状态 + body.code**，两者不一致时**以谁为准必须显式声明**。
- **M-4 —— 行号漂移**：53/59→54/60、`tests/**`→`tests/`、`main.cpp` HTTPS 回退块 312-315→468-477。
  → **教训（可执行）**：凡引用 `file:line`，**必须同时给当次指纹**（本报告 §6.1 给出七个锚点指纹，且 §3 各条的 file:line 均为当次 read 核对）。**同族补充**：本批还实测到 `routes_admin.cpp` 的**行号整体漂移**（T32 插入 11 行注释后，t33 交出的行号清单 —— `:889`/`:951`/`:1539`/`:1736`/`:772-795` —— 才是**当前位置**，与契约里的旧行号相差数十行）；T28 已按 t33 的清单逐条 `read` 复核并**以当次实测为准**（§6.5）。

**另三条由 t27/t33/t32 的实测教训沉淀（M-5…M-7，rev.4 补入）**：
- **M-5 —— 编译分工**：**`sqlite3.c` 是 C 源码，必须用 `gcc`；`.cpp` 才用 `g++`。** 用 `g++` 编 `.c` 会因 C++ 类型规则失败 —— **T32 作者与 t33 复核者各自独立踩到并纠正**（两人都自曝）。全量重编一律依 `ci.yml` 的 `gcc`/`g++` 分工（本报告 §6.1、§9 已写入）。
- **M-6 —— 二进制来源必须固定 `sha256`**：凡"某二进制表现如此"的断言，必须给出该二进制的 `sha256` 与字节数，否则**无法排除"你说的和我测的不是同一个文件"**（t33 已固定 `server_fixed.exe` / `server_prefix.exe` 的 sha256；本报告 §6.1 固定 T28 自身产物的 sha256）。
- **M-7 —— 隔离变量的对照实验优于"回退试试"**：修复前/后对照必须证明**两次构建的差异仅限目标改动**（t33 用 `git diff --no-index` 证明副本差异 = 14+/5−、2 个 hunk 且都在 rename 块）—— 否则现象无法归因（§3.7(b″)）。

### 7.2 T1 契约的向量事故（① ②）

- **① 向量为伪造 / 截断值**：`docs/audit/BATCH0_CONTRACT.md` 初版 `:123-126` 的两条 RFC 7914 §11 向量常量分别只有 **96 / 104 个 hex 字符**（应为 **128**），且从第 81 / 97 字符起与 RFC 正文分歧。由队长用 RFC 正文 + Node 双重复算更正（契约 §3.1 更正记录 + §8 勘误 1b）。**该错误若未被发现，T4 会被迫去迁就一个错误期望值**（T2 报告原文）。
- **② T1 声称「已逐字核对原文」与事实不符**：契约自检清单（§6 事实核对记录）声明逐字核对，但向量长度即已不满足 128 —— **声明与事实不符**。
- **性质**：属**「判据本身错误」**族（见 §7.3⑤）—— 一个"已核对"的肯定结论**从未被任何可执行判据验证过**。

### 7.3 本批「判据本身错误 / 不可执行」的实例汇总

| # | 实例 | 表现 | 谁发现 |
|---|---|---|---|
| ① | T1 契约为「**发现类**」 | RFC 向量 96/104 vs 应为 128（§7.2①） | 队长用「长度必须 128」自查 |
| ② | T1 声明不实 | 「已逐字核对原文」与事实不符（§7.2②） | 出题者/下游复盘 |
| ③ | T2 门槛缺口 | 见 §7.4①，**让 10/10 全绿成为可能** | verifier 变异体实验 |
| ④ | 契约「**结论类**」错误 | `BATCH0_CONTRACT.md:502` 原写改名路径「**顺序正确**」，`:503-511` 勘误为**错误结论**（§3.7c） | 队长实测 + backend-engineer 探针/E2E |
| ⑤ | 已撤回的声明 | t9/mechanical-fixes-engineer 曾称 `remove_user_index()` 为「死代码、零调用点」；实测 `routes_admin.cpp:785` 是**活调用点且正是缺陷所在**（t32 后它**才**成为真死代码）；mechanical-fixes-engineer 已主动撤回其批次 1 建议 | mechanical-fixes-engineer 自撤 + t33 检索 |
| ⑥ | 验证过程抓到的**两条真实缺陷** | **R1**：`GET /api/student/points/records` 门禁漏接（修复前 200 绕过 → t29 修复后 403）；**改名路径**：索引误删致强制改密被绕过 + 账号被改坏（t32 修复） | t26/t29/t32/t33 |
| ⑦ | **同一根因类别的未修盲区** | **`T27-R5`**：删除自定义权限/角色后不重建 `permission_id_map`/`role_id_map`/`role_permission_map` → 「已删权限仍生效」（`routes_admin.cpp:1147`/`:1183`）；与 B6、改名路径、`execute_bind` 0 行不区分、`load_users_from_db` 内存超前于库**同源** | t27 finding #5（见 §3.7(d)、§4.3） |

> **该根因类别的完整清单（「容器原地修改后不同步派生索引/映射」）**：① B6（`users.erase` 后下标错位）② 改名路径（`user_id_map[id]` 被误删）③ **`T27-R5`（权限/角色映射未重建，本批未修）** ④ `execute_bind` 无法区分 0 行受影响 ⑤ `load_users_from_db` 内存超前于库。

### 7.4 AgentTeams 插件的制度缺陷（**6 条**，附函数级 file:line）

> 插件位置（本机）：`C:\Users\Administrator\.dsh\profiles\web\node_modules\@nanmicoder\dsh-agent-teams\lib\`。
> 以下每条均由 **integrator 自己打开源码取到**（非引用队长转述）。

1. **`pathMatchesScope()` 不支持 glob** —— `quality-gates.js:93-113`：只支持「精确路径相等」或「以 `/` 结尾的目录前缀」。因此 `tests/**`、`frontend/**`、`.github/workflows/**` **永不匹配**任何文件 → 任务完成时 `changedPaths` 被判 `undeclared` 而**必然卡死**（受害：t2 `tests/**`、t8 `frontend/**`+`.github/workflows/**`）。
2. **`inScope` / `dependencies` / `deliverables` 创建后均不可修改**：`update_task` 与 `reassign_task` 都不提供修改入口 → 发现 inScope 写错后**唯一出路是重建任务**。
3. **`unsatisfiedDependencies` 未使用插件自身的终态集合** —— `state.js:106-109` 只认 `=== 'completed'`，而 `types.js:11` 的 `TERMINAL_TASK_STATUSES` = `completed/failed/cancelled` → **cancelled / failed 依赖恒判「未完成」**，形成不可达任务（t10←t8、t15←t14、t16←t15、t17←t16…）。
4. **`TASK_TRANSITIONS` 禁止 `pending→in_progress`** —— `state.js:114-119` → 出现**「可指派但不可认领、不可接管、不可作废、不可开工」的不可解状态**（t11/t12/t16/t17 即此）。附带：`reassign_task(assignee="captain")` 走**同一个** `unsatisfiedDependencies` 检查（`tools.js:1239-1242`）→ **连作废都做不到**。
5. **目录前缀必须带尾斜杠，而任务模板默认写成无尾斜杠**（t14/t19/t20/t22 的 inScope 修正史）。**可用修法只有重建任务**；`reassign_task(assignee=<member>)` 只能恢复归属、**不能**恢复可 claim 性（队长曾误判一次，务必写明）。
6. **提交校验只用 `inScope`，且报错不提示真正被校验的字段** ——
   - `quality-gates.js:371`：`const commands = update.commandsRun ?? task.commandsRun;`（**`??` 回退**：`commandsRun` 也可来自任务自身已有值）；
   - `:383-384`：`!verifyCovered(task.verify, commands)` → 报错原文 **`implementation completion requires a passed commandsRun entry for every verify command`**。准确语义：**每一条 `verify` 都必须在「生效的 `commandsRun`」（本次传入 **或** 任务已有值）中有对应 passed 条目**；这也解释了「某些任务 submit 时不传 commandsRun 也能过」（任务上已带值）。
   - `:380`：`acceptanceCovered(task.acceptance, acceptanceResults)` 属**同一族**（文本契约 + 顺序）。
   - `:386-395`：`classifyChangedPath` **先查 `outOfScope` 再查 `inScope`**（`:135/137`）→ outOfScope 不得与 inScope 有重叠目录前缀；`deliverables` **从不参与**校验；报错只说 `is undeclared`，**不提示真正被校验的字段**（把队长与执行者各误导一轮）。
   - **因此**：任务的 `verify` / `acceptance` 文本必须**同时满足「可执行」与「字面稳定」**。队长的 verify 原文含转义引号与花括号（`pwsh -NoProfile -Command "Select-String … | ForEach-Object { \"{0}:{1}  {2}\" -f … }"`）→ 在本机不可直接执行（PowerShell 报 `ScriptBlock should only be specified as a value of the Command parameter`），且质量门禁按**字面**比对 `commandsRun[].command` 与 `verify`，**执行者若顺手改写命令文本就会被拒（t31 第一次提交即因此被拒）**。**建议 verify 一律写成无转义引号、无花括号、无内层字符串格式化的形式**（优先 `pwsh -NoProfile -File xxx.ps1`，或 `Select-String -Path a,b -Pattern 'x' -SimpleMatch`）。
   - **归因**：该 verify 命令的设计缺陷**归队长本人**（t31 的 verify 由队长起草），**不写成执行者的问题**。

**由此派生的执行层受害实例（`changedPaths` 与实际改动集不一致，被迫"少报"）**：t2（只填 `.gitignore`，实改 3 文件）、t7（只填 4 个 inScope 文件，实改 5 个含 `auth.h`）、t9（只填 3 个，实改 5 个含 `models.h`/`routes_teacher.cpp`）。三者的披露文本一致：「门禁不接受未声明路径 → 只能少报，并在报告中显式声明第 N 个文件」。

### 7.5 数字陷阱 4 实例（含责任归属，不淡化）

| # | 实例 | 表现 | 谁发现 | 责任 |
|---|---|---|---|---|
| 1 | T1 契约 RFC 7914 §11 向量长度 | 手抄劣化为 **96 / 104** 字符（应为 128） | 队长用「长度必须 128」自查 | 队长 |
| 2 | `.trae/` 的 **25 vs 26** | `core.quotepath` 转义 + 前缀过滤静默漏 1 | mechanical-fixes-engineer 实测 26 → 队长去核 | 上游文档（审计/T1 契约）与复述者 |
| 3 | `models.h` 的 **22 vs 26** | 队长引用 **t9 时期过期读数**未当场重测 | **integrator 反查**（三条命令交叉验证 + 行数旁证 119−93=26） | 队长 |
| 4 | **本轮两例**：① T31 的 `+47/−11` / `+10/−5`（实为 **62/12、25/8**，引的是 t22 那轮读数）；② 队长自己的健康快照把 `.trae` 暂存 `D` 数打成 **25**（实为 26） | 引用过期读数 / 未用 `quotepath=false` | ① frontend-ci-engineer 重测；② 队长自查 | 队长（2 次） |

**T28 当次复现的机制（自跑，非采信）**：
```
$ git -c core.quotepath=false diff --numstat -- ci.yml release.yml
62	12	.github/workflows/ci.yml
25	8	.github/workflows/release.yml
$ (git status --porcelain | Select-String '^D  \.trae/').Count                     → 25   ← 错
$ (git -c core.quotepath=false status --porcelain | Select-String '^D  \.trae/').Count → 26 ← 对
原始行：D  ".trae/documents/\344\274\230\345\214\226…优化教师班级编辑与编号自动匹配.md"
```
→ `D  ` 之后紧跟双引号，故 `^D  \.trae/` 静默漏 1；`quotepath=false` 下该行为 `D  .trae/documents/优化教师…`，前缀即可匹配。

### 7.6 幽灵任务链清单（13 条）

> 定义：因插件制度缺陷（§7.4）而**永久不可达或重复**、且部分**无法作废**的任务。终态为 `cancelled` 的 9 条、`cancelled` 但**历史不可变**的 1 条、以及被作废后仍留在图上的记录。

| 任务 | 原职责 | 状态 | 由谁取代 | 是否曾被 claim | 能否作废 |
|---|---|---|---|---|---|
| **t8** | F1/F16/F24/F13 | cancelled | t14 → t19 → t20 → **t22** | **曾被 claim（attempt 2）** | 已 cancelled |
| **t10** | 批次 0 其余条目验证 | cancelled | **t26** | 否（从未 claim） | 已 cancelled |
| **t11** | 安全与正确性评审 | pending（幽灵） | **t27** | 否 | **不能**（依赖链 t10←t8 锁死） |
| **t12** | 结项 | pending（幽灵） | **t28** | 否 | **不能**（依赖 t11 锁死） |
| **t14** | F1/F16/F24/F13 | cancelled | t19/t20 → **t22** | **曾被 claim（attempt 5）** | 已 cancelled |
| **t15** | 批次 0 其余条目验证 | cancelled | **t26** | 否 | 已 cancelled（后被 t25 误判复活） |
| **t16** | 评审 | pending（幽灵） | **t27** | 否 | **不能**（依赖 t15 锁死） |
| **t17** | 结项 | cancelled | **t28** | 否（我从未 claim） | 已 cancelled |
| **t19** | F1/F16/F24/F13 | cancelled | t20 → **t22** | **曾被 claim（attempt 2）** | 已 cancelled |
| **t20** | F1/F16/F24/F13 | cancelled | **t22** | **曾被 claim（attempt 2）** | 已 cancelled |
| **t21** | `init_indexes()` 非幂等 | cancelled | **t24** | **曾被 claim（attempt 1）** | 已 cancelled |
| **t23** | `init_indexes()` 非幂等 | cancelled | **t24** | 否 | 已 cancelled |
| **t25** | 批次 0 其余条目验证 | cancelled | **t26** | **曾被 claim（attempt 1）** | 已 cancelled |

**另附**：`t13/t18/t29/t30/t31/t32` **不是**幽灵任务（已完成且产出被采纳）。

### 7.7 团队协作中的协作技巧/教训（补充）

- **「声明过的判据 ≠ 可执行的判据」**：T2 写下了 RFC 向量断言，但断言只覆盖**第一块**，完整 128 hex 走测试自带参考实现 → 门禁对「无多块能力」的实现**零判别力**。
- **「立规则 ≠ 不再犯」**：队长在**立完规则的同一条消息里**又踩了 quotepath 陷阱 → 规则必须落成**命令形态**才有约束力。
- **独立角色反查上游数字的必要性**：本批三次数值纠正中，两次由非作者角色（integrator / frontend-ci-engineer）反查发现。

---

## 8. 数字 / 机制更正（验收项 7，逐条给实测依据）

| # | 更正 | 原说法 | 当次实测依据 |
|---|---|---|---|
| ① | **`.trae/`** | T1 记「跟踪 25 / 磁盘 26」、审计写「25 个已跟踪文件」 | **HEAD 树 26 / 索引 0 / 磁盘 26 / 暂存 `D` 26**（§3.10 四视角 + 三条判定命令）；25 = `core.quotepath` 转义伪差（§7.5④），**不是磁盘多一个未跟踪文件** |
| ② | **B13 的机制表述** | 「恒判成功」 | 应为「**旧判据恒走 else**」：`db.query()` 恒返回 `json::array()`（`sqlite_wrapper.h:53-54`），空数组 `is_null()==false` → `git show HEAD:routes_admin.cpp:342` 恒假 → 走 `:345-346` 的失败分支（§3.3） |
| ③ | **三处行号漂移** | `main.cpp` HTTPS 回退块 312-315；`routes_admin.cpp` 删用户分支 826；`routes_teacher.cpp` 删学生分支 259 | 实测：`main.cpp` **468-477**（t7 扩文件 487→500 行）；`routes_admin.cpp` 删除分支 **885-889**（t9 时 876-880）；`routes_teacher.cpp` 删除分支 **258-262**；另 `routes_admin.cpp` 改名块 721-735 → **772-795** |
| ④ | **`models.h` 行尾** | 曾被记为 `+22/-0` | 行尾曾为 LF、已由 backend-engineer 字节级转回 CRLF；**当次实测 CR=119 / LF=119**（全文 CRLF）；**`git diff --numstat -- models.h` = `26 0`**（非 22，见 §7.5③）；归属三段式：**mechanical-fixes-engineer 发现不一致 → 队长裁决不改（当时判断 diff 干净、改只会制造噪音）→ backend-engineer 主动字节级修正** |
| ⑤ | **`remove_user_index()` 死代码声明** | t9 期曾称「死代码、零调用点」 | 当时 **假**（`routes_admin.cpp:785` 是活调用点且正是缺陷所在）；**t32 后当次实测为真**（活调用点 = 0）→ 批次 1 可删。**切勿写成「`remove_user_index()` 与 `update_user_index()` 一并清理」**（后者仍有 **6 个活调用点**） |
| ⑥ | **`/api/student/info` 的 `HTTP 200 + body {"code":404}`** | 曾与「响应封装问题」混为一谈 | 是改名缺陷的**次生现象**（按 ID 查用户失败），修复后应恢复正常（t33 实测 `200/200` 带数据）；而「**HTTP 状态与 body code 不一致**」属 **B17/F2 一类 → 批次 2**（t27 finding #7 = **T27-R7** 独立实测：登录失败三种情况的 transport **恒 200**、真实语义在 `body.code`；另有 `GET /api/auth/me` 无会话 200/401、`POST /api/auth/login` 空 200/400、`POST /api/admin/import` 空 200/400、`POST /api/admin/users` 空 200/400）。**两者必须分开写**。**注意**：`enforce_password_change`（`auth.h:335`）**确实设了 `res.status=403`** ⇒ **门禁路径本身 HTTP 与 body 一致**，不属于该不一致面 |
| ⑦ | **`T31` 落盘量** | `+47/−11`、`+10/−5` | 当次实测 `ci.yml +62/−12`、`release.yml +25/−8`（§7.5④） |

---

## 9. 批次 1 交接清单（验收项 8，具体到可执行）

> 排序性：**① 连接策略决策（1-1）必须先于 1-2 / 1-3 / 1-4 / 2-6**。

① **连接策略决策（1-1）是前置**：先定「**每线程一条连接**（`thread_local SqliteDb`）或小型连接池」vs「单连接 + 全局递归写锁」，再动锁粒度与并发代码。出处：`docs/audit/CODE_QUALITY_REVIEW.md:563-566,580,595`、`CODE_QUALITY_SUMMARY.md:252`、`BATCH0_DESIGN_DECISIONS.md:347-349,368`。

② **`main.cpp:70` 的迁移 `DEFAULT 0` 与 T1 决策① 的 fail-safe 方向相反 → 建议改 `DEFAULT 1`**：一行改动，**对全新安装零影响**（新装库由代码显式赋值 `must_change_password=1`），但使**存量账号全部首登强制改密**。当次核对：`main.cpp:70` = `ALTER TABLE users ADD COLUMN must_change_password INTEGER NOT NULL DEFAULT 0`。

③ **存量重置清单必须同时覆盖「裸 SHA256 行」与「`pbkdf2$` 行」**：否则删掉 legacy 分支后，裸 SHA256 行会成为「既不能登录也未被重置」的死账号。`main.cpp:168-171` 的启动告警已给出可直接执行的 SQL：`UPDATE users SET must_change_password=1 WHERE password_hash NOT LIKE 'pbkdf2%';`（当次核对该行在位）。

④ **`execute_bind` 无法区分 0 行受影响**：`sqlite_wrapper.h:145-147` 只判 `SQLITE_DONE`，而同文件 `update()` 已返回 `sqlite3_changes()` → 建议统一检查 `sqlite3_changes()`。verifier 已端到端复现「**外部删行后改密仍报 200 而库 0 行**」（残余判定 **low**）。

⑤ **legacy 裸 SHA256 分支**（`sha256.h:255` 的 `sha256(password) == hash`，非常量时间比较）：最佳修法是随「存量一律失效」**直接删除该分支**；若保留则须改为常量时间比较（仓库既有约定见 `auth.h:166` `csrf_check`）。注意与 ③ 联动。

⑥ **`POST /api/auth/register` 的 `data` 退化为数组**：`routes_public.cpp:179` 的 nlohmann 初始化列表语义所致（前端 **0 处**调用 → 潜伏缺陷）；**mock 缺 register 端点需一并修**。

⑦ **`init_indexes()` 幂等与「删除后调用」路径由 t24 覆盖**（**已完成**）：`models.cpp:187-195` 的 `clear_and_rebuild_user_indexes()` 为共用助手；批次 1 若改动索引结构，必须保持两条路径共用该助手。

⑧ **B6 目前只是临时修复**：本批取「erase 后 clear + 重建」；**稳定键彻底版留批次 1-3**（`BATCH0_DESIGN_DECISIONS.md:368`）。同批可清理 `remove_user_index()`（现为死代码，活调用点 = 0）与 `check_permission_optimized()`（活调用点 = 0），**但 `update_user_index()` 不可删**（**6 个活调用点**：`routes_admin.cpp:572`、`:684`、`routes_public.cpp:175`、`routes_teacher.cpp:144`、`:214`、`:954`）。⚠️ **措辞纪律**：**不得**写成「`remove_user_index()` 与 `update_user_index()` 一并清理」—— 那会误删仍在使用的函数（契约 `BATCH0_CONTRACT.md:511` 原文警告，当次核对一致）。
   - **追加（批次 1 应补）**：把 **B6 的机械判据**固化为「`users.size == user_id_map.size == user_username_map.size`」断言（t33 的 size 等式形态，§3.9 末），并覆盖 `permissions.erase` / `roles.erase`（`T27-R5`，§3.7(d)）。
⑪ **（追加）全量重编的编译分工（跨版本教训）**：**`sqlite3.c` 用 `gcc`、其余 9 个 `.cpp` 用 `g++`**（`-std=c++17 -O2 -I.`），依 `ci.yml:67-82`。用 `g++` 编 `.c` 会因 C++ 类型规则失败 —— **T32 作者与 t33 复核者各自独立踩到并纠正**（§7.1.1 的 M-5）；任何后续批次的全量重编脚本都应按此分工，并在报告中固定产物的 `sha256`（M-6）。

⑨ **（追加，与 t27 finding `T27-R4` 合并为同一项）F13 的 CI 层收敛 —— 把本机无法证明的事搬到 CI 每次 push 强制证明**：
   1. 在 `ci.yml` 的 package job「Verify frontend dist 为真实后端模式」步骤（`ci.yml:141` 起）的守卫块（**`:146-156`**，与 `:145`（`test -f dist/index.html`）、`:147`（`campus_mock_db_v1`）、`:150`（`[Mock] 未匹配的 API`）、`:154`（残留 `/campus-master/`）**四条既有判据同一步骤内**）追加一条：
      `grep -rIn -e admin123 -e teacher123 -e student123 -e parent123 dist` → 命中即 `echo ERROR + exit 1`。
   2. **动机（= T27-R4）**：F13「固定口令不进 JS 产物」**在本沙箱无字节级证明** —— 四个演示按钮只在**运行时** `v-if="isMockEnabled()"`（`Login.vue:181`）内，字面量是否被 rollup/esbuild 摇掉**无法验证**（`vite build` 因 esbuild 子进程被拒而 `spawn EPERM`，captain/verifier/reviewer 三方一致复现）；且**当次实测** `ci.yml`/`release.yml` 中 `admin123|teacher123|student123|parent123` 命中 = **0** ⇒ **现有守卫不覆盖固定口令字面量**。→ 该判据**改由 CI 守卫承担**。
   3. 与 **t30** 的文档层收口构成**双重覆盖**：三处交付文档明文口令 grep = 0（文档层）+ 产物内四个历史口令串 grep = 0（产物层），覆盖同一事实的两个独立面。
   4. **附注**：本条属「机制证据」而非「本机复现」，报告中仍须标注**未端到端复现**；唯一可本机复现的部分是 **grep 谓词本身**（可用 `%TEMP%` 合成 dist 做正/反两向对照）。
   5. **措辞纪律**：这条是**批次 1 的增强项**，**不得**写成「批次 0 未闭合」—— F13 的**文档层**已由 **t30** 闭合，**产物层从未有过本机证据（非回归）**。
   6. **编号提醒**：本条 = reviewer 的 **`T27-R4`**（队长旧编号曾称「R3」）—— **不要与 `T27-R3`（legacy 非常量时间比较，队长旧编号「R4」）混用**。

⑩ **（追加）任务模板中 verify 命令的写法（制度缺陷清单的一部分）**：
   - 现状问题：t31 的 verify 原文含转义引号与花括号，在本机不可直接执行；且质量门禁按**字面**比对 `commandsRun[].command` 与 `verify`（§7.4⑥ 的报错原文），执行者若顺手改写命令文本就会被拒（t31 第一次提交即因此被拒）。
   - 建议：verify 一律写成无转义引号、无花括号、无内层字符串格式化的形式：优先用脚本文件（`pwsh -NoProfile -File xxx.ps1`），或用最简选择器（`Select-String -Path a,b -Pattern 'x' -SimpleMatch`）。
   - 同类已知：`pathMatchesScope` 的**尾斜杠**语义（t14/t19/t20/t22）与 **glob 不匹配**（t2）。
   - **归因**：该缺陷属**队长的 verify 设计缺陷**（t31 命令由队长起草），**不写成执行者的问题**。

---

## 10. 附录

### 10.1 本报告直接引用的证据文件

| 文件 | 大小 | 归属任务 |
|---|---|---|
| `docs/audit/evidence/T3_B1_BASELINE.md` | 19,203 B | t3（含变异体实验） |
| `docs/audit/evidence/T6_B1_FIX_VERIFICATION.md` | 13,700 B | t6 |
| `docs/audit/evidence/T15_BATCH0_REMAINDER.md` | 30,204 B（404 行） | **t26**（文件名沿用 T15 前缀，勿误读） |
| `docs/audit/BATCH0_CONTRACT.md` | 731 行（含 `:503-511` 勘误块） | t1 + 队长勘误 |
| `docs/audit/BATCH0_DESIGN_DECISIONS.md` | 379 行 | t1 |
| `docs/audit/BACKEND_FINDINGS.md` / `FRONTEND_FINDINGS.md` / `CODE_QUALITY_REVIEW.md` / `CODE_QUALITY_SUMMARY.md` / `VERIFICATION.md` | — | 审计基线（未改动） |

### 10.2 T28 自身的纪律自证

- **只改一个文件**：`docs/audit/BATCH0_CLOSURE.md`（inScope 精确命中）。**未修改任何业务源码**（`*.cpp/*.h`、`frontend/**`、`.github/**`）。
- **未 commit / 未 push**（§6.4）。
- **编译/测试产物全部输出到 `%TEMP%`**，仓库根既有产物未被覆盖（§6.1）；T28 临时脚本仅 1 个（`%TEMP%\b0_fullbuild.ps1`），**不在仓库内**。
- 本报告所有断言（§6）与数字（§3.13、§6.4、§7.5）均为**当次执行**所得。
- **rev.2 的修订方式（过程透明化）**：T28 以 `rev.1` 提交（acceptanceResults/commandsRun 落定）后，队长发来**补充输入**（t27 的 8 条 findings 逐条入表、R5 单列强调、产物卫生核对重做），T28 据此在同一文件上追加 §4.3、§3.7(d)、§5 第 5 项复核与若干范围限定。**该修订未重跑 §6 的三条断言**（业务源码未变，七个锚点指纹当次复核不变：`sha256.h=3E5C59DD / main.cpp=D7211E1F / auth.h=3C520FAD / routes_public.cpp=FB893F80 / routes_admin.cpp=9BC7CE74 / models.cpp=AB6C86C6 / models.h=9768C1F9`）；**如需以 rev.2 为结项正式版本，建议由独立复核任务确认 §4.3/§3.7(d) 与 t27 原文一致**。
- **rev.3 的修订方式（同法，未重跑断言）**：按队长**两条紧急更正**（① `docs/audit` 打包排除**已完成**，R2 两半闭合，不得写成批次 1 建议项；② **编号口径改为 reviewer 的 `T27-R1…R8`**，并附队长旧编号映射）与追加要求（M-1…M-4 方法论条目入 §7.1.1、`T27-R4` 与 §9⑨ 合并为一项），在同一文件上落实。**修订前后七个锚点指纹不变**，`git status --porcelain -- '*.cpp' '*.h'` 无新增；三条断言（§6）沿用 rev.1 的当次执行结果，**未重跑**（业务源码未变）。**本报告正文中的「R1…R8」一律指 reviewer 编号。**
- **rev.4 的修订方式（同法，未重跑三条断言 —— 但**新跑了一遍全量重编做可复现性**）**：按队长要求并入 **t33 的完整证据**（隔离变量式对照实验、size 等式、死代码反向提醒、封装分层测量、两个二进制的 sha256）、新增 **§6.5** 行号核对与 **M-5…M-7** 三条方法论。**业务源码零改动**（七个锚点指纹不变、全部业务源码 mtime ≤ 12:44:50）；§6.1 的三条断言结果为 rev.1 的当次执行（未重跑），但 rev.4 期间**额外做了一次全量重编（13:07:28）**，用于给出本断言产物的 `sha256` 并证明**跨时间字节一致**。`main...origin/main` = `0 0`（无 push）。**正文中的「R1…R8」一律指 reviewer 编号。**

### 10.3 待办（本报告明确移交）

1. **t33（T32 后的改名路径独立复核）**：**verdict = pass，已完成**（两路方法：反向构造 T32 前二进制复现绕过 + 修复后三段对照；索引一致性 7/7；两条死代码声明属实）。**无遗留**；其「封装不一致归批次 2」的发现与 **`T27-R7`** 同族，已登记于 §8⑥（t33 原文未使用 R 编号）。
2. **N4 的 `D ` 需要一次 commit 才落地**（本批不 commit）。
3. `docs/audit/` 与 `.agent-teams/` 的入库策略由批次 1 决定（本批**不动** `.gitignore` 的放行规则）。**注**：交付包内排除 `docs/audit` **已经做完**（**T31**，`ci.yml:170-178` / `release.yml:114-122` + 反向守卫），属 **R2 已闭合**的一半，**不是**待办项。
4. **`T27-R5`（权限/角色映射删除后不重建）**：本批**明确未修**，归批次 1（§3.7(d)）；建议批次 1 加一条「删除后该权限码立即失效」的回归断言。**注意与第 3 条的 `docs/audit` 打包排除区分**：后者是已完成项，`T27-R5` 是未修项。
5. **仓库根被 ignore 覆盖的编译产物**（`server.exe` / `*.o`，mtime 12:49–12:52）：**保留、不删除**（可能是某位成员的验证依赖），仅记录状态（§5 第 5 项）。
6. **队长决定不为本轮补充内容另建任务**（`t27` 已 terminal 不可变；这些内容全部归属 T28 的报告内容——findings 表 + 批次 1/2 清单 + 方法论节），且**不为「T33 后 `routes_admin.cpp` 若再变」预先派 reviewer 复跑**（避免与 t33 重复劳动）。本报告遵循该决定：**无新增任务、无新增幽灵条目**。
7. **全量重编脚本务必固定跨版本教训**（§9⑪ / §7.1.1 的 M-5、M-6）：`sqlite3.c` → `gcc`、`.cpp` → `g++`；并在任何"二进制表现"断言中固定产物的 `sha256` 与字节数。
8. **`docs/audit/BATCH0_CLOSURE.md` 当前为 `rev.4`**（T28 的结构化提交对应 `rev.1`）。如需以 rev.2/3/4 的任一版为结项正式版本，建议由独立复核任务确认「§4.3/§3.7(d)/§6.5 与 t27/t33 原文一致」。
