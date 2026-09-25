# 批次 0 结项附录：门禁完备性、两类 403 判别与交付物新鲜度（BATCH0_CLOSURE_APPENDIX_A）

> **为什么单独成文**：本附录的内容来自 **`t27`（reviewer）的补充复核**。`t27` 与 `t28` 均已 **`completed`（终态不可变）**，故其结论**无法追加进 `BATCH0_CLOSURE.md` 或 `t27` 的 `findings`/`commandsRun`**。为不丢失这些证据，由队长出具本附录，并**已对其中每一条做当次独立核实**（见 §3）。
> 本附录属 `BATCH0_CLOSURE.md`（rev.3）的**组成部分**，与 `BATCH0_CLOSURE_RULING.md` 同批出具。
>
> **收录内容**：A.1–A.3 门禁完备性（74 handler）与计数口径警告；**A.3b 两类 403 的结构化判别**；**A.3c 方法学条款 M-8 交付物新鲜度**（⚠️ 与 `BATCH0_CLOSURE.md` 的 M-5 编译分工为不同条目，见 A.3c 的编号消歧）；A.4 R1 的独立 A/B；A.5 与交付物新鲜度问题的关系；**A.6 T27-R9 补记、队长过时措辞的更正、T27-C1 事实更正**。

---

## A.1 强制改密门禁完备性（全量 74 个 handler）

**口径（可复跑）**：逐 handler **区间归因**——区间 = 该 `svr.X(` 注册行到下一个注册行的前一行；**可疑** = 区间内含 `verify_session(` 且**不含** `check_permission_middleware(` / `check_parent_auth_middleware(` / `enforce_password_change(` 三者任一。
**必须同时覆盖两种注册形式**：普通字符串 `svr.X("/api/...")` 与**裸字符串** `svr.X(R"(/api/...)")`。

| 文件 | 普通字符串 | 裸字符串 | handler 合计 | 已接门禁 | 可疑 |
|---|---|---|---|---|---|
| `routes_public.cpp` | 10 | 0 | 10 | 3 | **2（均白名单）**：`/api/auth/me`、`/api/auth/change-password` |
| `routes_admin.cpp` | 24 | **7** | **31** | 31 | 0 |
| `routes_teacher.cpp` | 14 | **5** | **19** | 19 | 0 |
| `routes_student.cpp` | 5 | 0 | 5 | 5 | 0 |
| `routes_parent.cpp` | 3 | **6** | **9** | 7 | 0（未接的 2 个为 `:49` login、`:135` logout，公开/白名单，合法） |
| **合计** | **56** | **18** | **74** | **65** | **2（均白名单）** |

**结论**：除刻意白名单 `/api/auth/me`、`/api/auth/change-password` 外，**不存在「有会话却无门禁」的业务端点**；`routes_student.cpp` 在 T29 修复后为 **5/5**。**R1（唯一真实漏接点）已闭合。**

## A.2 被 T29 扫描漏计的 18 个 handler（逐个确认已接门禁）

T29 曾用「注册数 vs 门禁调用数」对账，得 `admin 24 / teacher 14 / parent 3`（合计 56）。**差集的精确原因**：它未枚举**裸字符串注册**形式 `svr.X(R"(/api/...)"`，而差集与各文件「普通字符串注册数」完全吻合（parent 3、admin 24、teacher 14 正是其表内数字）。

| 文件 | 漏计数 | 端点 |
|---|---|---|
| `routes_parent.cpp` | 6 | `:181 GET /api/parent/student/([^/]+)/info`、`:233 …/points`、`:278 …/evaluation`、`:329 …/redemptions`、`:377 …/messages`、`:424 POST …/messages` |
| `routes_admin.cpp` | 7 | `:192 PUT /api/admin/mall/(\d+)`、`:233 DELETE …/mall/(\d+)`、`:721 PUT /api/admin/users/([^/]+)`、`:1195 PUT …/roles/(\d+)`、`:1236 PUT …/permissions/(\d+)`、`:1862 PUT …/classes/(\d+)`、`:1911 DELETE …/classes/(\d+)` |
| `routes_teacher.cpp` | 5 | `:169 PUT /api/teacher/students/([^/]+)`、`:781 PUT …/evaluation/(\d+)`、`:813 DELETE …/evaluation/(\d+)`、`:1032 POST …/parent-messages/(\d+)/reply`、`:1097 PUT …/parent-messages/(\d+)/read` |
| `routes_public.cpp` / `routes_student.cpp` | 0 | 无裸字符串注册（故两表条目一致：10 / 5） |

**这 18 个恰是带正则路径的编辑/删除端点——即风险最高的子集**（含 `PUT /api/admin/users/([^/]+)`，正是 T32 修复的改名路径）。**逐个确认全部 GATED ⇒ A.1 的结论在全量 74 个 handler 上成立。**

## A.3 计数口径警告（方法论）——本批第 3 个计数陷阱的实例

本轮共出现**三次**「计数陷阱」：
1. **grep 符号名不符产生"零命中"假证据**：`login_attempt|lockout|…` vs 真实符号 `login_can_try` / `login_record_fail` / `login_record_success`（均不含 `login_attempt` 子串）→ 曾误得「登录锁定零调用点」。
2. **路由注册的裸字符串形式被漏计**：T29 的 `56` vs 实测 `74`，**漏掉的 18 个正是正则路径的编辑/删除端点**（见 A.2）。
3. **文本命中数 ≠ 覆盖数**：白名单 handler 本就无门禁（合法）；且同一 handler 内可多次出现 `verify_session(`/中间件符号 → **文本命中与覆盖数无单调关系**。
   - 更正一处早前表述：按**正确的** handler 数（`routes_parent.cpp` = 9）计，其门禁文本命中 7 是**少于**而非多于 handler 数——早前"命中多于 handler 数"的说法源于 3 这个漏计后的错误基数。

**结论**：**门禁完备性检查必须按 handler 区间归因，并同时覆盖两种注册形式；任何"零命中/全覆盖"结论都要给出定义点+调用点或区间级证据。**

## A.3b ⭐ 两类 403 的**结构化判别**（reviewer 补测；队长已核实源码依据）

**背景**：队长曾提醒「不要"看到 403 就判通过"——必须区分**强制改密门**与**权限门**」。reviewer 承认这是其 B2 证据链的薄弱处（前两轮只打印了 `body.code` 而未打印响应体），**并已补测**。结论：**两类 403 可结构化区分，不依赖人工阅读文案。**

| 场景（会话 `mcp=1`） | transport | body 原文 | 归类 |
|---|---|---|---|
| student `GET /api/student/points/records` | 403 | `{"code":403,"must_change_password":true,"msg":"首次登录必须修改初始口令后才能使用其他功能"}` | **强制改密门** |
| teacher `GET /api/teacher/students` | 403 | 同上（逐字一致） | **强制改密门** |
| admin `GET /api/admin/users` | 403 | 同上（逐字一致） | **强制改密门** |
| parent `GET /api/parent/children` | 403 | 同上（逐字一致） | **强制改密门** |
| 反向对照：student 改密后（`mcp=0`）访问 `/api/admin/users` | 403 | `{"code":403,"msg":"权限不足"}` | **权限门** |
| 同会话 `GET /api/student/points/records` | 200 | `{"code":200,"data":[]}` | 放行 |

**判据**：**强制改密门的 body 带 `must_change_password:true`；权限门的 body 没有该字段。**

**队长的源码级核实（当次实测）**：
```
auth.h:335-338  强制改密门  → res.status = 403; R"({"code":403,"must_change_password":true,"msg":"首次登录必须修改初始口令后才能使用其他功能"})"
auth.h:373/381  权限门      → res.status = 403; {{"code",403},{"msg","权限不足"}}     ← 无 must_change_password 字段
```
⇒ 判别依据**在源码层成立**，且与 `backend-engineer` 的第 2 条第三方证据**独立吻合**（两条独立路径得出同一判别）。

**由此确认**：reviewer 的 B2 结论**不是**"只看到 403 就判通过"——四角色 403 的响应体均已核对为强制改密门文案。

## A.3c 方法学条款 **M-8：交付物新鲜度**（reviewer 建议，队长采纳；**编号见下方说明**）

> ⚠️ **编号消歧（队长于 2026-09-19 补）**：本附录的 **M-8（交付物新鲜度）** 与 `BATCH0_CLOSURE.md` §7.1.1 的 **M-5（编译分工：`sqlite3.c` 用 `gcc`、`.cpp` 用 `g++`）** 是**两条不同的方法学条目**。本附录原误用 `M-5` 编号，与 CLOSURE 冲突（同一 ID 指向两条不同条款），已由队长改为 **M-8**。CLOSURE 的方法学编号为 M-1…M-7，其 M-5/M-6/M-7 分别对应「编译分工」「二进制断言须固定 sha256」「隔离变量对照优于回退试试」；本附录另增 **M-8**，二者合阅。

> **M-8（交付物新鲜度）**：仓库根 `*.o` / `server.exe` 命中 `.gitignore`，`git status` / `git diff` 均**不可见** ⇒ **无法从 git 侧判断某个 `.o` 对应哪个版本的 `.cpp`**。本批已发生两次：`t29` 误用陈旧 `routes_student.o` 重链、`t4` 的 `sha256.h` 在观测期被写过三次。
> **规程**：任何 E2E 复测必须**由当前源码当场全量编译**（`sqlite3.c` 也重编，注意用 `gcc`），并在**构建前后各核一次关键文件指纹**；报告须**显式记录二进制来源与编译命令**。
> **推论**：若结项报告要主张"从干净检出可复现"，必须以**一次全新的 clean build** 为证据，**不得引用任何既存中间产物**。

**reviewer 的自证**：其每轮 E2E（B1 探针、B2 四角色矩阵、B13 端到端、T32 改名、R1 的 A/B）均满足该规程——9 个 `.cpp` 取自仓库当次源码 + 当场从 `sqlite3.c` 编出的 `sqlite3.o`，输出到 `%TEMP%`，**从未链接仓库根 `*.o`/`server.exe`**；构建前后逐项指纹一致（`sha256.h=3E5C59DD`、`main.cpp=D7211E1F`、`auth.h=3C520FAD`、`routes_public.cpp=FB893F80`、`routes_student.cpp=65672273`、`models.cpp=AB6C86C6`），产物 `server.exe` SHA256 = `63BF6E66…`、size = 6,030,604。
**reviewer 主动披露的一处瑕疵**：其脚本里那行"编译命令回显"因格式串 bug 把 `-I` 打成了 `%TEMP%` 路径，**实际执行的是 `-I <repo>`**——它主动更正，以免结项报告引用一条被错误回显污染的命令行。**该自纠值得记录：它宁可暴露自己的脚本瑕疵，也不让一个不可复现的命令行进入交付记录。**

## A.4 R1 的独立 A/B（reviewer 亲自测得，加强 T29 的结论）

方法：从**当前源码**派生一个「仅移除 `routes_student.cpp:61-67` 门禁块」的前置副本（副本在 `%TEMP%`，**仓库文件零改动**，事后复核 `routes_student.cpp` 指纹仍为 `65672273`），与现行源码同参数编译（**两二进制同为 6,030,604 B**，唯一差异即那 7 行），各自用全新 DB/端口跑端到端：

```
[prefix 无门禁] login=200 mcp=True | 未改密请求该端点 body.code=200 (transport=200) | change-password=200 | 改密后 body.code=200
[fixed 现行版]  login=200 mcp=True | 未改密请求该端点 body.code=403 (transport=403) | change-password=200 | 改密后 body.code=200
```

⇒ 「修复前确可绕过（200）」「修复后 403」「`mcp=0` 仍 200 不误伤」三段由 reviewer **独立复现**，与 T29 的证据一致。**R1 记为已闭合**（t27 的 findings 中 `T27-R1` 已为 `resolved:true`，本次 A/B 是对该结论的加强，不改变 verdict）。

## A.5 与「交付物新鲜度」问题的关系（reviewer 的说明）

reviewer 声明：其全部 E2E（含本次 A/B）**都是当场从当前源码全量编译到 `%TEMP%`**，并在构建前后打印指纹比对；**从未复用仓库根的 `server.exe` / `.o`**（那批 mtime 12:49–12:52 的产物不在其构建路径内）。另 `t4` 的 `sha256.h` 在其整个评审期指纹恒为 `3E5C59DD`。**故其 R1 / 结项证据与 T29 自曝的「陈旧 `.o` 重链」无关联。**

---

## 3. 队长对本附录的当次独立核实（不依赖 reviewer 自述）

| 本附录的声明 | 队长的独立核实（当次实测） |
|---|---|
| handler 合计 **74**，其中裸字符串注册 **18** | 逐文件计数：`plain+raw` = public 10+0 / admin 24+**7** / teacher 14+**5** / student 5+0 / parent 3+**6** = **74** ✅ 与正文逐项吻合 |
| **18 个裸串注册端点全部已接门禁** | 队长自写**区间归因扫描**（区间 = 注册行到下一注册行前一行）：`handler 总数 = 74`、`裸字符串注册数 = 18`、`其中已接门禁 = 18`、**`裸串端点无门禁者 = 0`** ✅ 成立 |
| T29 漏计的原因是未枚举裸串形式 | 差集与各文件 plain 计数完全吻合（parent 3 / admin 24 / teacher 14）✅ 成立 |
| R1 的三段 A/B（200 / 403 / 不误伤） | 队长未重跑 E2E（t29 与 t27 两路已各有一份），但**静态侧已确认**：`routes_student.cpp` 五个 handler 的门禁行号为 `:15`/`:65`/`:104`/`:162`/`:221` → **5/5** ✅ |

**核实结论**：本附录的全部事实性声明成立。**A.1–A.3 的方法论警告应作为后续任何「门禁覆盖完备性」检查的强制口径。**

---

## A.6 补记：`T27-R9`（t27 的悬空引用）与 `requestedFix` 的事实更正

### A.6.1 reviewer 主动补记的 `T27-R9`（其原 `output` 引用了不存在的 `R9`）
reviewer 在其 t27 `output` 中写「`routes_admin.cpp=9BC7CE74`（评审期由 T32 更新，**见 R9**）」，但其 findings 列表只到 `R8` —— **`R9` 是悬空引用，属其自身记录缺陷**。它主动补齐全文供本附录留档：

> **T27-R9（low → 已闭合）| file=`routes_admin.cpp` | line=HEAD:732-734（当前 `:795`）| problem=管理员改名路径先 `update_user_index` 再 `remove_user_index`，后者连刚写入的 id 键一并 erase → `find_user_by_id` 返回 nullptr → 强制改密门禁对该用户失效（B2 绕过面），并对合法用户返回「用户不存在」（含其自身改密接口 404）；缺陷只在重启后由 `init_indexes()` 自愈。**属原有代码，非本批引入。** | requiredFix=改名后整体重建用户索引（已由 **T32** 实现为 `routes_admin.cpp:795 rebuild_user_indexes()`）；复核证据＝reviewer 端到端（改名 → 新名登录 200/mcp=True → 门禁端点 403 → 旧名 401 → 改密 200 → 业务 200）与 **T33** 独立复核。| resolved=true。**NOTE**：t27 落盘时 T32 尚未完成，故未列入原 findings 列表，此为事后补记；**t27 verdict 不变**（该路径在当前修订已闭合，且其 pass 不依赖对修复前行为的追认）。

**队长注**：`R9` 的悬空引用不是无害的笔误——**它会让读者去找一条不存在的 finding**，进而怀疑 findings 列表漏项。这与本批其它记录缺陷（t33 未含契约 verify、t26 无法回补 changedPaths）同属「记录层」问题，一并记录。

### A.6.2 ⭐ reviewer 拒绝照抄队长的一处**过时措辞**（并给出正确版本）
队长曾要求 reviewer 把 B2 的限制写成「**存在一条已定位的绕过面，本条 verdict 不对该路径作出结论**」。**该措辞在 T32 完成后已过时**——reviewer 拒绝照抄，并给出了按修复后真实状态的版本：

> **B2 判据的适用范围**：B2 的运行时门禁经 reviewer 实测，在**未改名账号**上不可绕过（4 角色 `mcp=1` 会话对被门禁端点返回 `{"code":403,"must_change_password":true,…}`，四角色响应体逐字一致，与权限门的 `{"code":403,"msg":"权限不足"}` 可结构化区分；两条中间件 `auth.h:346`/`:395` 均首行接入；4 条代设口令路径的 mcp 赋值已核对）。**另有一条历史绕过面**：管理员改名路径（HEAD:732-734，`update_user_index` 后紧跟 `remove_user_index`，后者连 id 键一并 erase → `find_user_by_id` 返回 nullptr → 门禁判定失效）属**原有代码、非本批引入**，波及 6 个 `find_user_by_id` 调用点；**已由 T32 修复**（`:795` 改为整体重建），并由 reviewer **端到端复现为修复后不可绕过**，独立复核由 **T33** 承担（verdict=pass）。**本条 verdict 的 B2 结论不对「修复前的改名路径绕过」作追认**，仅确认当前修订下该路径已闭合。

**队长注（自我更正）**：reviewer 的拒绝是正确的。队长的模板若被采用，会让结项报告写成「仍存在一条绕过面」，**与当前代码状态矛盾**——这正是本批反复出现的「文本与代码不同步」：**限制性措辞必须随修复状态更新，否则它会从"谨慎"退化为"错误陈述"。** 本附录据此采用 reviewer 的版本。

### A.6.3 `T27-C1` 事实更正：`remove_user_index()` 的"死代码"声明**在其声明时不成立**
> **T27-C1（事实更正，非新缺陷）**：t9 曾声明「`remove_user_index()` 全仓零调用、已成死代码」。**在其声明时该说法不成立**——当时仍有活调用点：`routes_admin.cpp` 改名分支（`git show HEAD` 为 HEAD:734；本批工作树中为 `:785`），而它**正是**改名绕过缺陷的成因。**截至当前修订（T32 之后）该函数才真正无活调用者**：reviewer 的 grep 为——定义 `models.cpp:256`、声明 `models.h:111`，其余命中只有注释（`models.cpp:228`、`routes_admin.cpp:775-776`）与 HEAD 基线，**无任何实际调用**。
> **正确表述**：「**T32 之前是活调用点且用错；T32 之后成为死代码**」，**不得写成「一直是死代码」**。
> **批次 1 清理建议**：删除该函数，或改为**只摘用户名键、保留 id 键**——其「同时 erase id 键」的行为在任何未来复用中仍会致错。

**队长注**：`T27-C1` 与 `T33`、`T24` 的独立核对三方吻合（`remove_user_index` 活调用点 = 0；`update_user_index` 仍有 6 处**不可删**）。

---

## 4. 自证

- 本文件为队长新增；未修改任何业务源码、未 commit、未 push（`HEAD 47006bd`、`ahead/behind = 0 0`）。
- 引用本附录时请同时引用 `BATCH0_CLOSURE.md`（rev.2）与 `BATCH0_CLOSURE_RULING.md`，以确认版本与授权状态。
