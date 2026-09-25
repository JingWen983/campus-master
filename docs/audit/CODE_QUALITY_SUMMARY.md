# 校园能量站 — 代码质量评估总览

> **本文档是阅读入口。** 详细论证在四份支撑文档中，本文只做整理与导航。
> 评估方式：**只读审计，未改动任何源码、配置或构建产物。**
> 评估日期：2026-09-12

---

## 一、一段话结论

这个项目的**工程惯例其实已经具备**：接口参数化查询已成主流、CSRF 双重提交 Cookie 设计正确、会话 ID 用 CSPRNG、登录失败锁定、CORS 与 Cookie 属性都处理过、CI 会跑前端类型检查与后端编译冒烟、GitHub Pages/Release 两套发布链路分开。**它不是"写得很乱"的项目，问题也不在整洁度上。**

**真正的短板集中在两层：**

1. **密码学实现层**——一处 PBKDF2 实现错误，直接导致完整认证绕过（B1）。
2. **交付与可观测性链路**——CI 打的"开发版"包里前端是 mock 构建，与同包的 `server.exe` 永远不通信（F1）；后端绝大多数业务错误以 HTTP 200 返回，而前端只在 401/403/429 上做了处理，中间完全静默（F2）。

一句话：**该有的意识都有，但缺"证明它是对的"的手段**——0 个测试文件是关键放大器。B1 这种错误，一个标准向量测试就能拦住。

---

## 二、关键数字

| 项目 | 数值 |
|---|---|
| 审计范围 | 后端自研 18 文件 / 6,084 行；前端 src 34 文件 / 9,513 行；workflow 3 个 → **约 15,600 行自研代码** |
| 发现总数 | **60 条** = 底层报告 54 条（后端 30 + 前端 24）+ 验证阶段新增 6 条 |
| 严重度分布 | **blocker 2 / high 14 / medium 23 / low 21** |
| 复核裁定 | CONFIRMED 35 / PARTIAL 3 / **REFUTED 0** / UNVERIFIABLE 主要项 0 |
| 证据密度 | 主报告含 **164 处互不相同的 `文件:行号` 引用**（本次重新机械核对所得） |
| 测试文件 | **0 个**（`tests/` 被 `.gitignore` 排除） |
| 类型检查 | `npm run typecheck` = **exit 0 / 0 错误** |
| 源码改动 | **0**（`git diff HEAD` 为空） |

排除项：`httplib.h`、`json.hpp`、`sqlite3.c/h`、`vcpkg/` 等第三方 vendored 文件未审计。

---

## 三、批次 0：立刻要做的 10 条（低风险高收益）

预计消除 **2 blocker + 2 high + 5 medium/low**。

| # | 条目 | 位置 | 动作 | 成本 |
|---|---|---|---|---|
| 1 | **B1** 认证绕过 | `sha256.h:94-148` | 重写 PBKDF2，HMAC 的 **key 必须是 password**；**前置：先补 RFC 6070 标准向量测试** | M |
| 2 | **F1** 交付包错配 | `ci.yml:44` → `:117/:123` | 拆开 mock 构建与发布打包链路 | S |
| 3 | **B13** 备份判据写反 | `routes_admin.cpp:335-349` | `VACUUM INTO` 不返回行，改用 `execute()` 而非 `query()` 判成败 | S |
| 4 | **N1** Cookie 永远带 `Secure` | `config.h:84` + `main.cpp:312-315` | HTTPS 回退到明文 HTTP 时复位 `cookie_secure` | S |
| 5 | **F24** 无效请求头 | `api.ts:73-76` | 删掉两行 `Content-Length`（浏览器 forbidden header，永不生效） | S |
| 6 | **F16** 发布不可复现 | `release.yml:51` | `npm install` → `npm ci` | S |
| 7 | **B2** 种子默认口令 | `main.cpp:250-254` | 随机初始口令 + 强制首登改密 | M |
| 8 | **F4** 评价列恒"未评价" | `TeacherApp.vue:61/169/635` | 删假数据，改接 `routes_teacher.cpp:728`（**复数端点**） | M |
| 9 | **B6** erase 后索引错位 | `models.cpp:206-211` | erase 后清空并重建索引 | S |
| 10 | **N4** `.trae/` 误入库 | 仓库 | `git rm --cached` 25 个文件（`.gitignore` 已列但已跟踪） | S |

### ⚠️ 安全类硬绑定（本轮唯一顺序强约束）

> **必须先把 B1 修好，再去改默认口令。**
> **"改密码"在 B1 存在时根本救不了场**——B1 下改密写入的仍是与口令无关的坏哈希，任意口令照样能登录。

四步顺序：**修 B1（含补 RFC 6070 向量）→ 修 B2 → F13 随 B2 一并处置 → F13 + B2 与 B1 同批完成，不得推迟。**
`F13` 严重度 low，但 **low ≠ 可延后**。

---

## 四、两个 blocker 详解

### B1 — PBKDF2 完整认证绕过 🔴

**位置：`sha256.h:94-148`**

`pbkdf2_sha256(password, salt, iters)` 的 `password` 形参**在函数体内零引用**：

- HMAC 密钥取自 **salt**（`:101-108`，`k_ipad`/`k_opad` 由 salt 填充）
- HMAC 消息体也**只拼 salt**（`:114-118` `msg.append(salt)`）
- 迭代段（`:129-142`）只用 `u` 与 `k_opad`

**机械验证**（三个验证者各自独立复现）：

```powershell
$lines = Get-Content sha256.h
($lines[94..147] | Select-String 'password').Count   # → 0
```

**因此**：派生值只由 `(salt, iters)` 决定，与口令完全无关。而 `verify_password`（`sha256.h:169-188`）用存储串里的 salt/iters 重算**同一个函数**，再与 `dk` 比较 → **`calc == dk` 恒真** → 对任何 `pbkdf2$` 格式哈希，**任意口令都通过**。

**提权链（`routes_public.cpp:45-51`）**：校验通过且哈希非 `pbkdf2$` 前缀时，就地用 `hash_password(password)` 重写并落库。

**现实可达性（确定判断，非猜测）**：

| 状态 | 依据 |
|---|---|
| 工作区**不存在任何数据库文件** | `Test-Path` False + 全递归扫描为空 + `glob` No files found（三路交叉验证） |
| → **后门此刻未武装** | 种子账号当前是裸 SHA256，走 `sha256.h:187` 正确分支 |
| → **首次成功登录即永久打开** | `routes_public.cpp:47-51` 把该账号改写成坏哈希 |

**影响面：9 处 `hash_password` 调用点**全部产生坏哈希——`routes_public.cpp:48`（升级链自身）、`routes_public.cpp:159`、`routes_admin.cpp:514/626/880/1470/1491`、`routes_teacher.cpp:136/946`。

**最小修复**：标准 PBKDF2 是 `U1 = PRF(P, salt || INT32BE(i))`，即 **HMAC 的 key 必须是 password**。
**前置依赖**：先补 RFC 6070 标准测试向量，否则改完仍无法自证正确。

---

### F1 — CI 交付包前后端错配 🔴

**位置：`ci.yml:44` → `:117/:123`**

1. `ci.yml:44` 构建前端时设 `VITE_USE_MOCK: 'true'`
2. `ci.yml:57` 该 dist 上传为 artifact `frontend-dist`
3. `ci.yml:117` package job 下载 `frontend-dist`，`:123` 下载 `server-exe`，`:135-140` 打成同一个 zip

**后果**：开发版交付包的前端只读写 localStorage，**真实后端永远收不到请求**。

**对照（避免误判为"全都坏了"）**：

- `release.yml` 未设该变量 → **正式发布包不含 mock**（tree-shaking 生效，属优点）
- `pages.yml:45` 设为 true → GitHub Pages 纯前端 Demo，**属有意为之，不是缺陷**

---

### 🔑 B1 × F1 交叉结论（本轮最有价值的一条）

> # 不得用 F1 的存在降低 B1 的紧急度；修复顺序 B1 先于 F1。

**理由**：开发版因前端走 Mock 而**不触发** B1 的升级链——这是**两个缺陷相互掩盖**，不是任何一层被修好了。正式发布包（真实前端）一旦有人用 `admin/admin123` 登录，B1 **立即永久生效**。

---

## 五、high 级发现（14 条）

| id | 位置 | 问题 |
|---|---|---|
| **B3–B4** | `routes_teacher.cpp:340`、`routes_admin.cpp:1602` 等 | 全局容器竞态：8 线程共享无锁容器 |
| **B5** | `routes_public.cpp:325→381` | `find_user_by_id` 返回的 `User*` 在并发 `push_back` 后**悬垂**，写已释放内存 |
| **B6** | `models.cpp:206-211` + `routes_teacher.cpp:259` | `users.erase` 后下标错位 → **返回错误用户** |
| **B7** | `routes_admin.cpp:1453/1665/1686` | 单连接上 `BEGIN/COMMIT/ROLLBACK` 是**连接级全局事务**，会吞掉/回滚其他线程已返回成功的写入 |
| **B8** | `routes_public.cpp:360-385` + `models.cpp:93-97` | 读-改-写**双花/超卖**；`execute_bind` 无法区分 0 行受影响（`sqlite_wrapper.h:145-147`）→ "不存在则 404"分支系统性失效 |
| **B9** | `routes_public.cpp:29/30/39/53/59` → `auth.h:111-114` | **登录锁定数据竞争已可达**：8 线程读写无锁 `static std::map` |
| **B10** | `auth.h:141-154` | 锁定键信任客户端 `X-Forwarded-For`，且 httplib 从不设 `REMOTE_ADDR` → 键退化为 `unknown\|<user>`，**可伪造绕过** |
| **F2** | `api.ts:103-126` | 后端 500 带合法 JSON 体，但前端只分支 401/403/429 → **完全静默**；全项目 **58 处** `res.code` 判据依赖"非 200 必有提示"这一不成立假设；`catch { /* api 层已提示 */ }` **7 处**（含 `StudentApp.vue:240`）注释与事实不符 |
| **F3** | `AdminApp.vue` 等 | 巨型组件：AdminApp **2,626 行**（script 1,237 / template 1,263 / style 124；54 个 `ref`、10 个 `computed`、26 个 API 调用点、12 个 `BaseModal`、7 个 tab） |
| **F4** | `TeacherApp.vue:61/169-170/635` | 评价列**对所有学生恒显示"未评价"**（详见下方六-1） |
| **F5** | `tsconfig.json` | 严格度不足：开 `noUnusedLocals/noUnusedParameters` → **7 错误**；开 `noUncheckedIndexedAccess` → **19 错误**；三项全开 → **26 错误** |
| **N2** | `models.cpp:112-160` + `:79` | 并发建号**生成相同 ID**，`INSERT OR REPLACE` **静默覆盖**已有用户（数据丢失）。由验证阶段新增 |

---

## 六、medium / low 级要点（按主题归类）

### 1. F4 — 教师端评价展示是"永久假数据"（high）

| 环节 | 位置 | 事实 |
|---|---|---|
| 列类型 | `main.cpp:80` | **`id TEXT PRIMARY KEY`** |
| 种子值 | `main.cpp:251-254` | `admin-01`、`student-02-01-01` 等**字符串** |
| 后端返回 | `routes_teacher.cpp:89` | `user.id` → 运行时是**字符串** |
| 前端声明 | `TeacherApp.vue:61` | `Evaluation.studentId: number` |
| 常量 | `TeacherApp.vue:169-170` | `studentId: 1`（**number**） |
| 查找 | `TeacherApp.vue:635` | `e.studentId === studentId` **严格相等** |

`1 === "student-02-01-01"` **恒为 false** → 85/90 是**不可达死数据**，5 个维度对**所有**学生一律走 `v-else` 显示"未评价"。真实评价 POST 会落库，但前端**没有任何读回路径**。

`loadEvaluations()` 的注释自认其性质：
> `// 实际未使用返回值。此处保留 API 调用以维持网络行为。`

**已排查其余模块，未发现同类失配**（4 组扫描：类型声明 / `Number()`·`parseInt()` / `===` 与 id 比较 / id 参与算术或下标，最后一组零命中）。最有力反例是 `ParentApp.vue`：**同一个"选中子项"的活儿却做对了**（`ChildSummary.id: string` `:21`、`currentChildId = ref<string | null>` `:77`）。

> 与 `N2` 的关系：同属"id 是 TEXT 却按整数处理"的根因类别，但**在不同层、相互独立、修一处不能修另一处**。

### 2. 安全与鉴权（medium）

- **B11** `auth.h:242-266, 272`：会话 SQL 仍用 `snprintf` 拼接，同文件 `:190-237` 已参数化且注释写着"不应拼接"。**经确认当前不可注入**（`escapeString` 有效），属**模板污染式维护隐患** + `char[1024/256/128]` 静默截断（prepare 失败 → 空数组 → 被当成未登录/登出静默失效）。**范围仅 3 处 `snprintf`、2 处 `escapeString`。**
- **N1** `config.h:84` + `main.cpp:312-315`：HTTPS 回退到明文 HTTP 时**不复位 `cookie_secure`** → 三处 Cookie 都带 `Secure`（`auth.h:88/95/174`）→ 浏览器在 http 下拒不保存 `sid` → **登录静默失效**。触发：`config.json` 的 `https.enabled` 设为 true。
- **F17** `xlsx 0.18.5`：版本匹配 **CVE-2023-30533 / CVE-2024-22363**，无更高修复版 → 建议**替换评估**而非"升级版本"。（本机 `npm audit` 不可用：镜像未实现 advisories 接口）
- **F13** `Login.vue:181-208`：四个 demo 按钮 `fillTestAccount('admin','admin123')` 等。**不是前端注入或鉴权绕过**——它只是**填入**表单。真问题是"产品内置可预测默认凭据 + **无强制改密/首启轮换机制**"。作为独立发现定 **low**，危害完全由 B2 承载，**单修 F13 收益≈0**。

### 3. 数据一致与性能（medium）

- **N3**：**全站无服务端分页**——全仓仅 3 处 `LIMIT`（`routes_admin.cpp:386` 的 `LIMIT 10` + 两处 `LIMIT 1` 存在性检查），而前端有 `Pagination.vue`/`usePagination.ts` → **客户端分页撑全量载荷**，`/api/admin/users`、`/api/admin/export` 随数据量无界增长。
- **N5**：`points_records` **从不回载**——唯一 loader（`models.cpp:58`）只加载 users → 重启后 `/api/admin/export`（`routes_admin.cpp:1388-1399`）导出**空数组**；且 `new_record_id = points_records.size()+1`（`routes_teacher.cpp:331`）每次重启从 1 重号 → **双数据源不一致**。
- **N5'**：`.map` 文件可被静态服务暴露。
- **B12/B14**：未调用 `set_payload_max_length` / `set_read_timeout` / `set_error_handler`（`grep` 零命中）→ 请求体无上限、无读超时。

### 4. 错误处理与响应封装（medium）

- **B17**：**16 处** `res.status=` vs **217 处** `{"code",N}` → 绝大多数业务错误以 **HTTP 200** 返回，而前端据此判断的假设不成立。
- **B12**：`sqlite_wrapper.h:53, 151` — `query()`/`query_bind()` 在 prepare 失败时返回**空 `json::array()`**，调用方无法区分"无数据"与"SQL 失败"。
- **B13** 见批次 0。

### 5. 前端可维护性（medium / low）

- 巨型组件：AdminApp **2,626 行**、TeacherApp **1,519 行**、ParentApp 793 行、StudentApp 731 行。
- 死代码：`usePagination.ts` **整文件零引用**，另有 7 个零引用导出（共 8 处）；`N6` `main.ts:24-26` 的 `globalProperties.$format*` 注册同样零使用。
- 重复实现：分页派生值在 Admin（**两套**）/Teacher 手写 **12 个 `computed`**；错误样板 24 处、`try` 块 68 处。
- TeacherApp：`:161-171` 硬编码假评价数据、`:313-321` 空转 API、`:664-683` 两个"开发中"死按钮。
- **F7** 见上「死代码」。
- **F24** `api.ts:73-76`：`Content-Length` 属浏览器 **forbidden request header**，`Headers.set` 对其**静默忽略、不抛异常、无警告**（依据 MDN，经 `web_fetch` 核实）。→ 该行在浏览器中**永不生效**（无害但无效的死代码）；Node/undici 侧反而保留。
- 依赖体积：echarts 全量 `import * as echarts`（`useChart.ts:10`）约 **1.0 MB** + xlsx 816 KB + pinyin 316 KB 静态引入；字体 4.72 MB + FontAwesome 999 KB。

### 6. 工程化缺口（high 级影响）

- **0 个测试文件**，`package.json` 无 `test` 脚本、无 eslint / prettier。
- CI 门禁**真实存在且有效**（`ci.yml:37-39` 确实跑 `typecheck`），构建用 `build:no-typecheck` **不构成漏洞**（同 job 已跑类型检查）。
- **F16** `release.yml:51` 用 `npm install`（可改写 lockfile）→ **发布不可复现**。
- **N4** `.trae/` 已被 `.gitignore` 列出，但 **25 个文件仍被跟踪**（先提交后加 ignore 的典型后果）。

---

## 七、仓库卫生

| 项 | 状态 |
|---|---|
| `.gitignore` 覆盖编译产物、日志、数据库、凭据文件 | ✅ 设计正确 |
| 编译产物（`*.o`/`*.exe`/`*.log`）是否入库 | ✅ **已正确排除**（工作区有 `main.o` 等，但 `git ls-files` 零命中） |
| `.trae/` | ❌ 25 个文件被跟踪（N4） |
| 前端字体子集 | ⚠️ 107 个 woff2 + `fonts.css`（113 KB）纳入版本库，属生成产物 |

---

## 八、值得保留的优点（14 项，均有行号依据）

1. **静态路径穿越已正确防护** —— `httplib.h:3116` 服务端 percent-decode，`routes_static.cpp:14-23` 拒绝任何解码后含 `..` 的路径 → `%2e%2e` 不可绕过
2. **日志不含敏感信息** —— 所有 `Logger::*` 无 password/token/session_id/csrf/body；`log_request`（`auth.h:54-57`）只记 method+path
3. **`/api/admin/export` 不泄露口令哈希** —— `routes_admin.cpp:1347-1354` 显式只取 6 个非敏感字段
4. **全仓库无 `SELECT *`**
5. **参数化查询已成主流** —— `query_bind`/`execute_bind` 广泛使用，遗留拼接仅 3 处
6. **CSRF 双重提交 Cookie 设计正确** —— token 同时经 JSON body 返回（`routes_public.cpp:84`、`routes_parent.cpp:120`），HttpOnly 不构成问题
7. **`Set-Cookie` 多值正确** —— `httplib.h:195` 是 `std::multimap`，`set_header` 用 `emplace`，两个 Cookie 都保留
8. **会话 ID 用 CSPRNG** —— `sha256.h:151-158` `std::random_device`，经二进制取证确认为 CSPRNG
9. **口令哈希有 per-user 随机盐 + 100000 迭代** —— `sha256.h:162-166`（问题是算法实现，不是没加盐）
10. **登录失败锁定机制存在** —— `auth.h:117-139`（问题是并发安全与 IP 信任，不是没做）
11. **正式发布包不含 mock** —— tree-shaking 生效
12. **`npm run typecheck` = exit 0 / 0 错误**
13. **CI 门禁真实有效** + 后端编译冒烟与 release 同源
14. **前端工程结构合理** —— 多入口配置、统一 `apiRequest` 封装、mock 与真实后端同构、composables 抽取

---

## 九、路线图（四批，含硬依赖）

| 批次 | 内容 | 条目 |
|---|---|---|
| **批次 0** | 高风险低成本，可立即做 | B1, F1, B13, N1, F24, F16, B2, F4, B6, N4（见第三节） |
| **批次 1** | 并发修复 | B3, B4, B5, B7, B8, B9, B10, **N2**（N2 与 B3/B5 **同批**，不得孤立） |
| **批次 2** | 错误语义与工程化 | B11, B12, B17, F2, F5, F17, N3, N5 |
| **批次 3** | 结构性重构 | F3（巨型组件拆分）, F7, N6, 依赖瘦身 |

### 三条硬依赖（不得颠倒）

1. **B1 修复必须先补 RFC 6070 标准测试向量，再改实现** —— 否则改完仍无法自证正确。
2. **批次 2-1（引入测试与 lint）必须先于批次 3 全部重构** —— 没有回归测试就拆 2,626 行的组件，等于蒙眼手术。
3. **批次 1-1（连接策略决策）是 1-2 / 1-3 / 1-4 / 2-6 的前置** —— 先定"多连接 vs 单连接加锁"，再动各处并发代码。

---

## 十、本次审计的局限（务必一并阅读）

1. **`npm run build` 未能在本环境执行**（esbuild `spawn EPERM`，沙箱对 piped stdio 的限制）。tree-shaking 结论由**同版本 Rollup 4.62.3 进程内对照实验**支撑（`VITE_USE_MOCK=false` → 4405 B / 3 模块，mock 特征串全 MISS，与空壳对照**字节数相同**；`=true` → 34273 B / 4 模块全 HIT），属**强间接证据**，非端到端构建复现。
2. **真实浏览器未实测**（Chrome 被沙箱 mojo 命名管道拒绝，错误码 0x5）。`Content-Length` 一行按 MDN 规范判定为静默失效，未经浏览器端到端验证。
3. **`npm audit` 未跑通**（镜像 npmmirror 未实现 advisories 接口，404）。`xlsx` 的 CVE 结论基于**公开编号 + 版本匹配**。
4. **历史上被删库重建的数据库无法取证**（`server.log:22809` 记录过 `检测到旧 schema…删除数据库文件并重建`）→ "B1 升级链是否曾在生产中被触发过"标为 **UNVERIFIABLE**。
5. **B1 为静态推导**（推导链完整且三方独立复现），未运行 `server.exe`、未发 HTTP 请求。

---

## 十一、底层报告勘误（14 行，冲突时以 `VERIFICATION.md` 为准）

审计过程中修正了底层报告的多处数字与表述，**均未流入本文档与主报告**：

| # | 条目 | 原表述 | 核实结果 |
|---|---|---|---|
| 1 | `B17` | "仅 6 处设 `res.status`" | **16 处**（"217 处业务码"无误） |
| 2 | `B25` | 单参 CORS 重载是"死代码" | **78 处调用**（双参仅 8 处），是**主流调用形式**；同源下 low，配跨域白名单则影响**近乎全站** |
| 3 | `t1` 摘要 | medium 10 / low 10 | **medium 9 / low 11** |
| 4 | `F2` | "62 处 `res.code`" | **58 处** |
| 5 | `F2` | `catch` 注释 6 处 | **7 处**（漏 `StudentApp.vue:240`） |
| 6 | `F13` | 独立的中危信息泄露 | 与 `B2` 种子口令**同一组**，根因是 `B2`，**单修 F13 收益≈0** |
| 7 | `F4` | "medium → high 提级" | **F4 从头就是 high**（`FRONTEND_FINDINGS.md:164/327`），**不存在提级**；t2 自报的 1/4/12/7 **完全正确** |
| 8 | `F4` | "硬编码假数据（显示假分数）" | 真相是 `1 === "student-02-01-01"` 恒 false → **没有任何分数会被显示** |
| 9 | `F4` 改法 | 指向 `/api/teacher/evaluation/dimensions` | 应为 **`routes_teacher.cpp:728`** 的 `GET /api/teacher/evaluations`（**复数**） |
| 10 | `F7` + `N6` | 分列两条 | 合并处理 |
| 11 | mock 证据 | UNVERIFIABLE | **CONFIRMED（机制）** + 附注"未经端到端构建复现" |
| 12 | `F17` | 未标证据等级 | 版本匹配 + 公开 CVE 编号，**非本机 `npm audit`** |
| 13 | `B1` 影响面 | 8 处调用点 | **9 处**（漏 `routes_public.cpp:48` 升级链自身） |
| 14 | 行号 | — | 记录 **10 处**行号/数字不符，**未发现断章取义** |

**被剔除的 5 条伪问题**（复核者证伪，不进报告）：
①单参 CORS"死代码" ②`Set-Cookie` 会互相覆盖 ③`HttpOnly` 破坏 CSRF 双重提交 ④备份接口路径注入 ⑤口令哈希无 per-user salt。

---

## 十二、支撑文档导航

| 文档 | 体量 | 内容 |
|---|---|---|
| **`CODE_QUALITY_SUMMARY.md`** | 本文 | 阅读入口，整理与导航 |
| `CODE_QUALITY_REVIEW.md` | 678 行 | **主交付物**：完整改进建议书（七节 + 勘误表 + 路线图） |
| `VERIFICATION.md` | 1033 行 | 独立复核：逐条裁定 + 14 行勘误表 + 存疑事项判定 |
| `BACKEND_FINDINGS.md` | 1805 行 | 后端原始审计（30 条，含源码原文证据） |
| `FRONTEND_FINDINGS.md` | 1267 行 | 前端原始审计（24 条，含量化指标附表） |

---

## 十三、底线声明

**本次评估未改动任何源码、配置或构建产物。**

- `git diff HEAD` 为空 → 无任何已跟踪文件被修改
- 新增内容仅 `docs/audit/` 下 5 个 Markdown 文件
- 审计期间未运行 `server.exe`、未发起 HTTP 请求、未 `commit` / `push`
- 报告中所有修复动作（`git rm --cached`、`ci.yml` 修改等）**均为建议，尚未执行**
