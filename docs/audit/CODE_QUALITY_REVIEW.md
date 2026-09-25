# 校园能量站 · 代码质量评估与改进建议书（CODE_QUALITY_REVIEW）

- 报告性质：**纯建议书**。本次评估**未修改任何源码或配置**，全程只读（见 §7.4 只读性自证）。
- 评估对象：`D:\邵敬文\comptation` —— 校园能量站（校园文明能量站）全栈项目
- 执笔：verifier-reviewer（team `code-quality-audit`，任务 t4）
- 依据：本案所有结论均来自 `docs/audit/` 下三份文件 —— 由 backend-auditor 与 frontend-auditor 产出、并经我**逐条独立复核**的
  - `docs/audit/BACKEND_FINDINGS.md`（1805 行，30 条）
  - `docs/audit/FRONTEND_FINDINGS.md`（1267 行，24 条）
  - `docs/audit/VERIFICATION.md`（1033 行，独立复核报告 —— **本文件与底层报告冲突时，以 VERIFICATION.md 为准**，理由见 §7.1）
- 报告日期：见文件 mtime

> **本报告只保留经复核的结论。** 两份底层报告中未被证实或表述失准的部分，已按 `VERIFICATION.md` 更正；不存在被整体推翻而继续留存于正文的条目。

---

## 1. 项目概览与审计范围

### 1.1 技术栈

| 层 | 技术 | 说明 |
|---|---|---|
| 后端 | C++17 + 自研 HTTP 路由 + httplib（vendored）+ SQLite（自带 amalgamation 静态链接）+ nlohmann/json（vendored） | 单一可执行 `server.exe`，静态链接 MinGW 运行时，无外部 DLL 依赖 |
| 鉴权 | Cookie 会话（`sessions` 表）+ 双重提交 CSRF + 角色/权限码 | 自研 SHA256/PBKDF2（`sha256.h`），**此处存在本案最严重缺陷** |
| 前端 | Vue 3 + TypeScript + Vite 5 + Tailwind CSS + ECharts + xlsx + pinyin-pro | 多入口 5 个 HTML（index/admin/teacher/student/parent），无 vue-router |
| 部署 | GitHub Actions（`ci.yml` / `release.yml` / `pages.yml`）；后端同时 serve 前端 dist（同源部署） | Pages 为纯前端 mock 演示站 |

### 1.2 代码量（自研口径）

| 范围 | 文件数 | 行数 |
|---|---|---|
| 后端自研（`*.cpp` / `*.h` / `*.json`） | 18 | **6,084** |
| 前端 `frontend/src`（`.ts` / `.vue` / `.css`，不含二进制字体） | 34 | **9,513** |
| CI workflow（`.github/workflows/*.yml`） | 3 | — |
| **合计自研** | **52** | **≈15,600** |

最大文件：`routes_admin.cpp` 1,892 行、`AdminApp.vue` **2,626 行**、`routes_teacher.cpp` 1,171 行、`TeacherApp.vue` 1,519 行。前端两个巨型 SFC 合占前端行数的 **43%**。

### 1.3 审计了哪些文件

后端逐行：`main.cpp`、`auth.h`、`config.h`、`config.json`、`sqlite_wrapper.h`、`sha256.h`、`models.h/.cpp`、`routes.h`、`routes_public.cpp`、`routes_admin.cpp`、`routes_teacher.cpp`、`routes_student.cpp`、`routes_parent.cpp`、`routes_static.cpp`、`logger.h/.cpp`、`database.h`。

前端逐行：`frontend/src` 全部 34 个文件（含 `lib/`、`composables/`、`components/`、`pages/`、`mock/`、`entries/`）、`package.json`、`tsconfig.json`、`vite.config.ts`、`tailwind.config.js`、`postcss.config.js` 及 5 个 HTML 入口。

基础设施：`.github/workflows/{ci,release,pages}.yml`、`.gitignore`、`README.md`。

### 1.4 明确排除的 vendored 第三方（不作审计）

| 文件 | 体积 | 处理方式 |
|---|---|---|
| `sqlite3.c` | 9.0 MB | 仅定点阅读 `:14046-14052` 以判定 `SQLITE_THREADSAFE` 默认值 |
| `json.hpp` | 993 KB | 仅定点阅读 `:22491-22507` 以判定 `value()` 的抛异常行为 |
| `sqlite3.h` / `sqlite3ext.h` | 641 KB / 39 KB | 仅定点阅读 |
| `httplib.h` | 151 KB | 仅定点阅读（`dispatch_request`、`parse_request_line`、`Headers` 类型、`REMOTE_ADDR` 注入、payload 上限） |
| `vcpkg/` | — | 完全排除 |

排除理由：这些是上游库代码，其缺陷不构成本项目工程能力的评价对象；仅当其行为决定本项目结论真伪时才定点查阅。

### 1.5 实际执行过的命令（均为只读）

```powershell
# —— 复核者（verifier-reviewer）实际执行，非审计员转述 ——

# 后端：认证绕过机械验证
$lines = Get-Content sha256.h; ($lines[93..147] | Select-String 'password').Count
$body  = $lines[94..147]; ($body | Select-String 'password').Count
Select-String -Path sha256.h -Pattern 'k_ipad|k_opad'

# 后端：两套 SQL 风格 / 登录锁定调用点 / 线程模型 / 互斥设施
Select-String -Path auth.h -Pattern 'snprintf|query_bind|execute_bind'
Select-String -Path *.cpp,*.h -Pattern 'login_can_try|login_record_fail|login_record_success|login_client_key'
Select-String -Path <17 个自研文件> -Pattern 'mutex|lock_guard|unique_lock|shared_lock|std::atomic|thread_local'
Select-String -Path main.cpp -Pattern 'ThreadPool';  Get-Content config.json

# 后端：状态码与业务码统计（17 个自研文件）
([regex]::Matches($all,'res\.status\s*=')).Count            # → 16
([regex]::Matches($all,'\{"code",\s*\d+\}')).Count          # → 217

# 后端：CORS 调用点全量枚举、B13 备份判据、B18 回载函数
Select-String -Path <17 个自研文件> -Pattern 'set_cors_headers\('
Select-String -Path models.cpp,main.cpp,routes_admin.cpp -Pattern 'points_records'
Select-String -Path models.h,models.cpp,main.cpp -Pattern 'bool load_|load_points|load_records'

# 后端：数据库实际状态（三路交叉验证）+ 运行时取证
Test-Path campus_system.db                                    # → False
Get-ChildItem -Recurse -Force -File -Include *.db,*.db-wal,*.db-shm,*.sqlite,*.sqlite3
Select-String -Path server.log -Pattern 'SQLite 数据库连接成功|检测到旧 schema|POST /api/auth/login'

# 后端：种子口令独立复算（.NET SHA256，四组）
[System.Security.Cryptography.SHA256]::Create().ComputeHash(...)   # → 四组全 MATCH

# 前端：mock 门控与 CI 配置
Select-String -Path frontend\src\... -Pattern 'VITE_USE_MOCK|from .*mock'
Select-String -Path .github\workflows\*.yml -Pattern 'VITE_USE_MOCK|npm ci|npm install|frontend-dist|server.exe'

# 前端：机械计数（ref/computed/res.code/错误样板/catch 注释）
([regex]::Matches($raw,'\bref[<(]')).Count                  # → 54
([regex]::Matches($all,'res\.code')).Count                  # → 58
([regex]::Matches($all,"toast\.error\('网络错误，请稍后重试'\)")).Count   # → 24
Select-String -Path frontend\src -Pattern 'api 层已提示'      # → 7 处

# 前端：类型检查严格度实测（我亲自重跑）
node .\node_modules\vue-tsc\bin\vue-tsc.js --noEmit --noUnusedLocals --noUnusedParameters
# → 恰 7 个错误 / exit 2（mock/index.ts ×6 + TeacherApp.vue(664,25)）

# 规范查证
web_fetch https://developer.mozilla.org/en-US/docs/Glossary/Forbidden_header_name

# 仓库卫生
git ls-files | Select-String '^\.trae/' | Measure-Object    # → 25
git status --porcelain                                      # → 仅 ?? .agent-teams/ 与 ?? docs/audit/
```

**未执行**（受沙箱与只读纪律限制，已在 §7.3/§7.4 如实标注）：`npm run build`（esbuild `spawn EPERM`）、浏览器端到端测试（Chrome mojo 命名管道被拒）、`npm ci`（会写 `node_modules`）、`npm audit`（registry 不实现 advisories 接口，404）、运行 `server.exe`、发起任何 HTTP 请求。

---

## 2. 总体结论

**这个项目的工程惯例已经具备到"结构上像样"的程度，但它的正确性护栏几乎全部停在"写下来了"而没有"跑起来"。** 值得肯定的是：后端做过一次真实的分层重构（路由按角色拆成 6 个文件、DB 访问收敛到唯一封装 `sqlite_wrapper.h`、参数化查询接口 `execute_bind/query_bind` 已建立并在新代码中占多数使用）、有 12 个权限码 + 4 个角色的 RBAC 骨架、有会话表与 Cookie+CSRF 双重提交、有 5 个索引、开了 WAL 与 `busy_timeout`、前端开了 TS `strict`、`build` 脚本内含 `vue-tsc` 类型门禁、CI 有编译与打包流水线、`.gitignore` 对产物/日志/凭据/数据库逐项声明。这些都不是"随手写的脚本"，说明作者知道正确的做法长什么样。

**但真正的短板集中在三层，且都是系统性的、不是零散的笔误：**

**第一层——关键算法正确性没有验证手段（最致命）：** `pbkdf2_sha256()` 的函数体**从未引用它的 `password` 形参**，密钥材料取自 `salt`。这让"口令校验"退化为常量比较，对任何 `pbkdf2$` 格式的哈希**任意口令都能通过**（B1，blocker）。它能长期存活的原因不是作者疏忽，而是**项目里没有任何一条测试**能证明"两个不同口令必须得到不同结果"——这正是 §6 把"先补测试再改实现"写成硬性前置依赖的原因。同类问题还有 N2（并发建号生成重复 ID 后被 `INSERT OR REPLACE` 静默覆盖）——都是"逻辑上必然出错"的缺陷。

**第二层——并发模型从未被真正设计过：** 8 个 worker 线程共享**一个** SQLite 连接与若干**无锁全局容器**（`users`、两张下标 map、`points_records`、`login_attempts`），全项目除 `Logger` 外零互斥设施。于是出现了同一条根因的多种表现：悬垂的 `User*`、`erase` 后下标错位导致 `find_user_by_id()` **返回另一个用户**、单连接上 `BEGIN/COMMIT` 串扰其他线程并可能回滚已返回 200 的写入、积分绝对值覆盖造成双花超卖（B3/B5/B6/B7/B8/N2）。

**第三层——交付与可观测性链路断开：** CI 打的"开发版"交付包其前端是 **mock 构建**（`VITE_USE_MOCK='true'`），与同一个 zip 里的 `server.exe` 永远不通信（F1，blocker）；后端绝大多数业务错误以 **HTTP 200** 返回（16 : 217），而前端只对 401/403/429 做提示，导致 **HTTP 500 完全静默**（F2）；教师端评价的 5 个分数列**对所有学生恒显示"未评价"**（F4）。三者叠加的结果是：**故障发生时，用户看不到错误，开发者收不到信号。**

**⚠️ 必须明确的一条交叉结论（两个缺陷相互掩盖，请勿误读）：**

> **不得用 F1 的存在降低 B1 的紧急度；修复顺序 B1 先于 F1。**

理由：`ci.yml` 那个开发版包的前端走 mock，从不调用后端登录接口，因此 B1 的自动升级链（`routes_public.cpp:47-51`）在该路径上**恰好**不会被触发。但这**不是任何一层被修好了，而是两个缺陷互相掩盖**。`release.yml` 的正式发布包前端是真实的，一旦有人用 `admin`/`admin123` 正常登录一次，`hash_password()` 立即把该账号哈希改写成与口令无关的坏 pbkdf2 —— **此后任意口令都能登录 admin，且永久有效**。`server.log` 中 603 次 `POST /api/auth/login` 证明真实登录确实发生过。**认证绕过是安全事件，mock 串用只是交付质量问题，前者必须先处置。**

**一句话总结：** 这不是一个"写得很差"的项目，而是一个**结构对、惯例对、但缺测试与验证闭环**的项目——它的大部分缺陷都可以被一个中等规模的测试套件在早期拦住。因此本报告把"引入回归测试"排在结构性重构之前（见 §6 批次 2 → 批次 3 的依赖关系）。

---

## 3. 发现清单总表

**共 60 条**：blocker **2** / high **14** / medium **23** / low **21**。（底层报告原有 30 + 24 = 54 条，加上验证阶段新增的 **N1–N6** 共 6 条，合计 60。）

> **一处定级口径说明**：`F13` 在 §3.3 中按**底层报告的 medium** 列示，但**我的独立判定为 low**（其危害完全由 `B2` 承载，见 §7.1 第 9 行）。若按我的判定口径，分布为 **medium 22 / low 22**。两种写法均可，但正文引用时须注明 F13 的性质是"默认凭据随产物分发 + 无强制轮换机制"，**不得写成前端注入或鉴权绕过**。
其中 **6 条为「验证阶段新增」**（N1–N6，两份底层报告均未提及）。

**复核判定列的含义**：`CONFIRMED` = 我独立复现且原文一致；`PARTIAL` = 方向成立但数字/措辞需更正；`未独立复核` = 不在我的抽样范围内，**本报告不为其背书**（仅保留条目与位置供参考）。

### 3.1 blocker（2）

| id | 严重度 | 一句话 | 位置 | 复核判定 | 成本 |
|---|---|---|---|---|---|
| **B1** | **blocker** | `pbkdf2_sha256()` 函数体零引用 `password`，密钥取自 `salt` → 派生值与口令无关 → 任何口令通过任何 `pbkdf2$` 哈希，**完全认证绕过**；且首次成功登录即把该账号永久改写成坏哈希 | `sha256.h:94-148`、`169-188`；升级链 `routes_public.cpp:45-51` | **CONFIRMED**（自验 `count=0`） | M（重写 ~60 行 + 向量测试）；含存量数据迁移则 L |
| **F1** | **blocker** | CI 的 `package` job 把 **mock 构建**的前端与 `server.exe` 打进同一个交付 zip → 该开发包前端只读写 `localStorage`，后端永远收不到请求 | `ci.yml:41-46,105-147`；`api.ts:58-60` | **CONFIRMED** | **S** |

### 3.2 high（14）

| id | 严重度 | 一句话 | 位置 | 复核判定 | 成本 |
|---|---|---|---|---|---|
| **B10** | high | 未捕获异常被 httplib 转成 500 并把**异常原文**写进 `EXCEPTION_WHAT` 响应头回显客户端 | `httplib.h:3574-3579`；触发点 `routes_public.cpp:88,174,392` 等 | **CONFIRMED** | S |
| **B6** | high | `users.erase()` 后仅删自身索引项，后续元素下标全部错位 → `find_user_by_id()` **返回另一个用户**（越权/串号）；`init_indexes()` 不清 map 使错位被固化 | `models.cpp:200-212,172-186`；`routes_teacher.cpp:259-260`；`routes_admin.cpp:826-827` | **CONFIRMED** | S（改清空重建）～M |
| **B8** | high | 兑换"读积分→判断→绝对值写回"非原子，且 `execute_bind` 对 **0 行受影响仍返回 true** → 同一学生双花、库存超卖、"不存在则 404"分支系统性失效 | `routes_public.cpp:360-385`；`sqlite_wrapper.h:145-147`；`models.cpp:93-97` | **CONFIRMED** | S（改原子 SQL）～M |
| **B4** | high | 登录锁定键信任客户端可伪造的 `X-Forwarded-For` → 可无限爆破；不发该头则退化为共享键并可**反向锁死**他人账号；家长端**完全无锁定** | `auth.h:141-154`；`routes_parent.cpp:49-77` | **CONFIRMED** | S～M |
| **B3** | high | `login_attempts()` 的函数级 `static std::map` 被 8 线程无锁读写（**已被登录路由真实调用**，属必发而非理论可能） | `auth.h:107-139`；调用点 `routes_public.cpp:29,30,39,53,59`；`main.cpp:306` + `config.json:5` | **CONFIRMED** | S（加锁）～M（表化） |
| **N2** | high | **【验证阶段新增】** `generate_user_id()` 用内存扫描求 `max_seq+1`，并发建号生成**相同 ID**，而 `save_user_to_db` 用 `INSERT OR REPLACE` → **后建用户静默覆盖先建用户**（数据丢失） | `models.cpp:112-160`、`:79` | **CONFIRMED**（我的新增发现） | S～M |
| **B2** | high | 4 个默认账号以**无盐 SHA256** 硬编码为 `admin123`/`teacher123`/`student123`/`parent123`（我已用 .NET SHA256 **独立复算四组全 MATCH**），且首次登录即升级为 B1 的坏哈希；DB 打开失败时静默退化为内存模式并继续服务 | `main.cpp:250-254`；`models.cpp:16-22`；`main.cpp:73-75` | **CONFIRMED** | S～M |
| **B9** | high | `/api/teacher/*` **写接口完全没有班级作用域**（读接口有）→ 任意教师可改/删任意班级学生、给任意学生加减积分、打分；未绑定班级的教师反而看到**全校**学生 | `routes_teacher.cpp:60-96`（有作用域）vs `193-201,251-263,298-300,487-489`（无） | **CONFIRMED** | M |
| **B5** | high | 全局 `users` 向量 / 两张下标 map / `points_records` 多线程无锁读写：`find_user_by_id()` 返回的 `User*` 在并发 `push_back` 后**悬垂**，跨请求**写已释放内存**；`rehash` 竞态；`points_records` 的 `size()+1` 生成重复 ID | `models.cpp:14,165-169,206-221`；`routes_public.cpp:167,381`；`routes_teacher.cpp:331,340` | **CONFIRMED** | M |
| **B7** | high | 单条共享连接上执行 `BEGIN`/`COMMIT`/`ROLLBACK`：事务是**连接级全局**的，其他线程已被卷入、甚至被回滚；`BEGIN` 返回码未检查；`json::parse_error` 分支不回滚 | `routes_admin.cpp:1453,1665,1679,1686`；`sqlite_wrapper.h:14-32,209` | **CONFIRMED** | M～L |
| **F2** | high | HTTP 500 带 JSON 体但 `api.ts` 只处理 401/403/429 → **完全静默**：无 toast、无异常、无告警；**58** 处 `res.code` 判据建立在"非 200 必有提示"这一不成立假设上；**7** 处 `catch { /* api 层已提示 */ }` 注释与事实不符 | `api.ts:103-126`；后端 `routes_admin.cpp:137-142`；`StudentApp.vue:142,176,185,194,203,212,240` | **PARTIAL**（机制 CONFIRMED；**62→58**、**6→7** 两处计数更正） | S |
| **F4** | **high**（定级与 t2 一致；**机制/描述经我复核更正**） | 教师端评价展示是**永久假数据**：`getStudentEvaluation` 用 `===` 比较 `number 1` 与字符串形式的 `student.id`，恒为 false → **所有学生的 5 个分数列一律渲染"未评价"**，85/90 为不可达死数据；真实评价只写不读 | `TeacherApp.vue:1038-1080`、`:634-637`、`:168-171`、`:313-321`（含原文"保留 API 调用以维持网络行为"） | **CONFIRMED**（机制更正；**定级无需改动** —— t2 原报已为 high，见 §7.1 第 7 行） | M |
| **F5** | high | 类型安全水位：`ApiResponse<T = any>` + `[key: string]: any` 索引签名使响应体全程无约束（58 处判据中 26 处未标泛型）；`noUnusedLocals`/`noUnusedParameters` 关闭掩盖 **26** 个真实错误 | `api.ts:19-24,52,131-134`；`tsconfig.json:14-16`；`routes_teacher.cpp:664` 等 | **CONFIRMED**（我亲自重跑：7 错误 / exit 2） | M |
| **F3** | high | `AdminApp.vue` **2,626 行**单文件：54 个 `ref`、10 个 `computed`、26 个 API 调用点、12 个 `BaseModal`、7 个 tab 挤在一起，7 个业务域共享同一作用域，无法对单一 tab 做隔离测试 | `AdminApp.vue:1-2626`（script 1237 / template 1263 / style 124） | **CONFIRMED**（我的计数与报告一致） | L |

### 3.3 medium（23）

| id | 严重度 | 一句话 | 位置 | 复核判定 | 成本 |
|---|---|---|---|---|---|
| **N1** | medium | **【验证阶段新增】** `cookie_secure` 在 HTTPS 回退后**从不复位** → 明文 HTTP 下三种 Cookie 全带 `Secure` → 浏览器拒不保存 `sid` → **登录静默失效** | `config.h:84`；`main.cpp:312-315`；`auth.h:88,95,174` | **CONFIRMED**（我的新增发现） | **S** |
| **B13** | medium | 备份接口判据写反：`VACUUM INTO` 无行返回 → `db.query` 返回 `[]` 而非 null → `is_null()` 恒 false → **永远返回 500"备份失败"**，成功日志不可达（文件其实已生成，属假阴性） | `routes_admin.cpp:335-349` | **CONFIRMED** | S |
| **F16** | medium | 正式发布用 `npm install`（可改写 lockfile），与 CI 的 `npm ci` 不一致 → **发布不可复现** | `release.yml:51` vs `ci.yml:35` | **CONFIRMED** | S |
| **F12** | medium | 全局确认框的 Esc/Enter 实际**不生效**（`@keydown` 绑在永不聚焦的 div 上，无 `.focus()`），且缺 `role="dialog"` 与焦点陷阱 | `ConfirmDialog.vue:22-26,32` | **CONFIRMED** | S |
| **F24** | medium | `Content-Length` 属 fetch **禁止请求头**（MDN 规范原文确认），浏览器静默忽略 → 该行**永不生效**；注释声称的"兼容部分反代"在浏览器侧无依据 | `api.ts:73-76` | **CONFIRMED**（规范级） | S |
| **F10** | medium | "系统配置/安全配置/备份配置/立即备份" 4 个按钮只弹 toast，不落库、不调接口（假功能） | `AdminApp.vue:1101-1115` + 模板 `1634,1661,1688,1691` | 未独立复核 | S |
| **B14** | medium | 未调用 `set_payload_max_length`（默认上限 = `SIZE_MAX`）也未设读超时 → 超大 body / 慢速连接可耗尽内存 | `httplib.h:44,2955,3077`；`main.cpp:286-306` | **CONFIRMED** | S |
| **B12** | medium | `query()/query_bind()` 在 prepare 失败时返回**空数组**，与"无数据"不可区分 → 调用方把 DB 错误当业务结果（含把错误当 403） | `sqlite_wrapper.h:53-62,151-159`；`auth.h:246` | **CONFIRMED** | S |
| **B11** | medium | `get_session_info`/`delete_session`/`cleanup_expired_sessions` 仍用 `char[]`+`snprintf` 拼 SQL（同文件上方已是参数化）→ 维护陷阱 + 静默截断隐患；且 `:199-200` 注释宣称参数化而 `:243` 立即拼接，**自相矛盾** | `auth.h:242-246,261-266,270-274` | **CONFIRMED** | S |
| **B19** | medium | 无统一响应封装；`/api/admin/system` 与 `/system/config` 返回**写死的假数据**；**5** 个权限码（`class:manage`、`data:export`、`message:manage`、`parent:manage`、`redemption:manage`）从未被任何中间件使用 | `routes_admin.cpp:60-70,1229-1244` | **CONFIRMED** | S |
| **F9** | medium | 首屏**串行请求瀑布**（6/7/2 次串行往返）；同项目 `StudentApp.vue:274-281` 已有正确 `Promise.all` 写法可直接对照 | `AdminApp.vue:1229-1234`；`TeacherApp.vue:726-732`；`ParentApp.vue:113-114` | **CONFIRMED** | S |
| **F7** | medium | 8 处"设计了但从未接线"的零引用导出（`useTheme`/`ROLE_THEME`/`NavSection`/`resetMockDB`/`showToast`/`formatRelative` 链/`useAuth`）；**+N6**：`main.ts:24-26` 的 `globalProperties.$format*` 注册同样零使用 | `theme.ts:85,90`、`navConfig.ts:18`、`mock/index.ts:690`、`useToast.ts:50`、`auth.ts:157`、`main.ts:24-26` | **CONFIRMED**（并强化） | S |
| **F6** | medium | 现成 `usePagination` composable 是**死代码**（仅出现在自身与注释中），三个页面各手写一遍同样的分页派生值 | `usePagination.ts:1-67`；`AdminApp.vue:314-337`；`TeacherApp.vue:215-222` | **CONFIRMED** | S |
| **F13** | medium →**low**（我的判定） | 登录页 4 个 `fillTestAccount()` 按钮**填入**默认测试口令（**非**注入/鉴权绕过），与 `release.yml:165-167` 的 Release Notes 一并分发；**真正的问题是默认凭据无强制改密/首启轮换机制**。危险完全由 B2 承载，单修 F13 收益≈0 | `Login.vue:181-208`；`release.yml:164-167` | **CONFIRMED**（性质按此重写） | S |
| **N3** | medium | **【验证阶段新增】** 全仓库**没有任何服务端分页**（仅 3 处 `LIMIT`，其中 2 处是 `LIMIT 1` 存在性检查）；列表与导出接口返回**全集**，前端用 `Pagination.vue` 对全量载荷做客户端分页 | 全仓；`routes_admin.cpp:1344-1399`（export 遍历全量） | **CONFIRMED**（我的新增发现） | M |
| **B18** | medium | 内存与 DB 双份数据源不原子；**`points_records` 从不回载**（唯一 loader 只加载 `users`）→ 重启后 `/api/admin/export` 导出**空数组**（静默丢失积分历史），且 `new_record_id = size()+1` 每次重启从 1 重号 | `models.cpp:14,58,93-97`；`routes_admin.cpp:1388-1399`；`routes_teacher.cpp:331,340` | **CONFIRMED**（我把症状落细） | M |
| **B16** | medium | 角色/权限的增删改**只改内存不落库**，而鉴权只读内存、`/api/admin/roles` 却读 DB → 功能无效、重启丢失、两处数据不一致 | `routes_admin.cpp:979-981,1044-1046,936-940`；`auth.h:280-303` | **CONFIRMED** | M |
| **B17** | medium | 业务码全靠手工拼 `{"code":N}`（**217** 处），仅 **16** 处设置 `res.status` → 绝大多数业务错误以 **HTTP 200** 返回，状态码与业务码混用 | 全 `routes_*.cpp` + `auth.h` | **PARTIAL**（**217 无误**；"6 处"→**16 处**） | M |
| **B15** | medium | `/api/admin/import` 把导出文件里 `role_id≠3` 的用户**静默降级为学生**；新用户不更新索引（仅靠导入结束的全量重建兜底） | `routes_admin.cpp:1470-1475,1494-1502,1667` | 未独立复核 | S～M |
| **F15** | medium | **0 个测试文件、0 lint**（无 vitest/eslint/prettier 配置），CI 仅有 `typecheck + build` → 2,626 行核心页面**无任何回归网** | `package.json:6-12`；`ci.yml:37-46` | **CONFIRMED** | L |
| **F11** | medium | 包体：`import * as echarts`（阻断 tree-shaking，约 1.0 MB）+ `xlsx` + `pinyin-pro` 静态引入；字体 4.72 MB + FontAwesome 999 KB 字体/72 KB CSS | `useChart.ts:10`；`AdminApp.vue:14-16` | **CONFIRMED** | M |
| **F8** | medium | `try/catch + console.error + toast.error('网络错误，请稍后重试')` 样板重复 **24** 次（`try {` 全项目 **68** 处），四个页面错误处理策略互不一致 | `AdminApp.vue`(16) + `TeacherApp.vue`(8) | **CONFIRMED**（24 精确） | M |
| **F14** | medium | `csrf_token` 与 `userInfo` 存 localStorage：同源脚本可读（已核查缓解措施：0 处 `v-html`、无第三方 JS、服务端复验、登出清理） | `api.ts:27-39`；`auth.ts:47-67` | 未独立复核 | M |

### 3.4 low（21）

| id | 严重度 | 一句话 | 位置 | 复核判定 | 成本 |
|---|---|---|---|---|---|
| **N4** | low | **【验证阶段新增】** `.trae/` 已被 `.gitignore` 声明却仍有 **25** 个文件被跟踪（gitignore 不追溯已跟踪文件） | `.gitignore` 末尾 `.trae/`；`git ls-files` | **CONFIRMED** | S |
| **N5** | low | **【验证阶段新增】** 静态服务显式支持 `.map`（`routes_static.cpp:42`），`/assets/.*` 无扩展名白名单 → 一旦有人开启 sourcemap，生产会自动**对外提供前端源码** | `routes_static.cpp:42,94` | **CONFIRMED** | S |
| **N6** | low | **【验证阶段新增】** `main.ts:24-26` 注册的 `globalProperties.$formatDateTime/$formatDate/$formatRelative` **零模板使用**（模板全走直接导入）→ 整块注册是死代码 | `main.ts:24-26` | **CONFIRMED** | S |
| **B25** | low | 单参 `set_cors_headers(res)` 在 **response** 上找 `Origin`（`auth.h:30` 另有未使用变量 `it`）→ 永不回显 ACAO。**注意：它不是死代码** —— 单参 **78** 处调用是主流形式（双参仅 8 处）。同源部署下影响 low；一旦配置跨域白名单，影响**近乎全站**而非仅 401/403 | `auth.h:22-33`；调用点 78 处含 `auth.h:329,338,350,358,375,384,395` 与 `routes_static.cpp:73` | **PARTIAL**（"死代码"表述已更正） | S |
| **B29** | low | DB 层错误只写 `std::cerr`，不走 `Logger`（不落轮转文件、无时间戳、GUI 下可能不可见） | `sqlite_wrapper.h:45-48,60,132,157` | **CONFIRMED** | S |
| **B30** | low | 日志轮转在**加锁之前**执行（TOCTOU/并发 rename），且每条日志都 `stat`+`open` | `logger.h:41-62`（lock 在 `:61`） | **CONFIRMED** | S |
| **F21** | low | 下载 CSV 模板未 `URL.revokeObjectURL`（对比 `AdminApp.vue:1130` 已 revoke） | `TeacherApp.vue:578-587` | 未独立复核 | S |
| **F20** | low | Mock 未匹配路由返回 **HTTP 200 + `code:404`**（掩盖错误）并强制 200-500 ms 随机延迟 | `mock/index.ts:661-675` | 未独立复核 | S |
| **F19** | low | 同一模块 `../lib/auth` 重复 import 两条语句 | `Login.vue:13,15` | 未独立复核 | S |
| **F18** | low | `tsconfig.json` 的 `include` 含 `*.html`（对 tsc 无意义）；`@/*` 别名声明了但全项目 0 处使用 | `tsconfig.json:21-25` | 未独立复核 | S |
| **B24** | low | `get_cookie_value()` 用 `find("sid=")` 裸子串匹配，`xsid=` 会被当成 `sid` | `auth.h:67-82` | 未独立复核 | S |
| **B26** | low | 积分记录响应里 `operatorName` 硬编码 `"王老师"` | `routes_teacher.cpp:363` | 未独立复核 | S |
| **B21** | low | `std::random_device` 每次调用重建、每字节一次 `rd()`、`rd() % n` 模偏差（不影响安全性） | `sha256.h:151-159,200-227` | 未独立复核 | S |
| **B28** | low | 声明了外键但从未 `PRAGMA foreign_keys=ON` → 外键约束全部不生效 | `main.cpp:108-211`；`sqlite_wrapper.h:19-32` | 未独立复核 | S |
| **F23** | low | 12 个弹窗各注册常驻 `window` keydown 监听；`body.style.overflow` 并发弹窗下会被错误解锁；`onActivated/onDeactivated` 在无 `<KeepAlive>` 时是死分支 | `BaseModal.vue:55-66`；`useChart.ts:50-57` | 未独立复核 | S |
| **B23** | low | 单连接 + SQLite 串行模式使 8 线程无法并发用库；每请求 3~4 次会话查询（含 1 次 DELETE）；列表接口 N+1 | `sqlite_wrapper.h:27-29`；`auth.h:196-197` | 未独立复核 | M |
| **B20** | low | 旧哈希路径用 `std::string::operator==`（非常量时间）；密码长度校验只在 2 个入口存在；`atoi` 读迭代数无上限 | `sha256.h:176,187`；`routes_public.cpp:132-136` | 未独立复核 | S |
| **B22** | low | 🔶疑似 `localtime()` 在无锁上下文并发调用，依赖 CRT 是否为线程局部缓冲 | `routes_teacher.cpp:554-557,646-647`；`admin.cpp:1277-1285` | 未独立复核 | S |
| **F22** | low | `switchTab` 的 if/else 链在四个页面重复 4 份；`ParentApp.vue:226` 用类型断言绕过联合类型 | 4 处 tab→loader 映射 | 未独立复核 | M |
| **B27** | low | 检测到旧 schema 时直接 `std::remove()` **删库重建**（无备份）；删除失败路径未处理。**该路径已在生产中真实执行过**（`server.log:22809`，2026-06-27） | `main.cpp:60-75`；`models.cpp:63` | **CONFIRMED**（有运行时实证） | S |
| **F17** | low | `xlsx@0.18.5` 命中 CVE-2023-30533（原型污染）/ CVE-2024-22363（ReDoS）。**证据等级：版本匹配 + 公开 CVE 编号**（非本机 `npm audit`，该 registry 不实现 advisories 接口）；npm 上无更高修复版，需评估替换 | `package.json:18` | 未独立复核（证据等级已标注） | M |

---

## 4. 分主题详述

### 4.1 后端并发与数据库（B3/B5/B6/B7/B8/N2/N3/B23/B28）

**位置**：`main.cpp:22,306`；`config.json:5`；`models.cpp:14,112-160,165-169,172-186,200-221`；`sqlite_wrapper.h:14-32,145-147,209`；`auth.h:107-139`；`routes_admin.cpp:1453,1665,1686`；`routes_public.cpp:360-385`

**证据（我复核过的原文）**：

```cpp
// main.cpp:22 与 :306 —— 8 个 worker 线程共享同一个连接
SqliteDb db;                                                    // 全局唯一实例
svr.new_task_queue = [&]() { return new httplib::ThreadPool(g_config.thread_count); };

// config.json:5
"thread_count": 8

// models.cpp:206-212 —— 返回指向向量元素的裸指针
User* find_user_by_id(const string& user_id) {
    auto it = user_id_map.find(user_id);
    if (it != user_id_map.end() && it->second < users.size()) {
        return &users[it->second];                              // ← 并发 push_back 扩容后悬垂
    }
    return nullptr;
}

// models.cpp:172-186 —— 全函数没有任何 clear()，被删用户的残留下标不会被清除
void init_indexes() { for (size_t i = 0; i < users.size(); i++) { user_id_map[users[i].id] = i; ... } }

// sqlite_wrapper.h:145-147 —— 0 行受影响仍返回 true
rc = sqlite3_step(stmt);
sqlite3_finalize(stmt);
return rc == SQLITE_DONE;
```

我独立复现的关键前提：全 17 个自研文件 grep `mutex|lock_guard|unique_lock|shared_lock|std::atomic|thread_local` **只命中 Logger**（`logger.h:61,89`、`logger.cpp:7`）→ **除日志外零互斥设施**。

**问题**：8 线程共享一个 SQLite 连接与若干无锁全局容器，产生五种彼此独立但同源的表现：①`User*` 悬垂后**写已释放内存**（`routes_public.cpp:381` 的 `user->points -= cost`）；②`erase` 后下标错位使 `find_user_by_id()` **返回另一个用户**（越权/串号），且 `init_indexes()` 不清 map 导致错位被固化；③单连接上 `BEGIN/COMMIT/ROLLBACK` 是**连接级全局事务**，导入失败回滚会连带丢弃其他线程**已返回 200** 的写入；④兑换"读-判断-绝对值写回"非原子 + `execute_bind` 无法识别 0 行受影响 → 双花/超卖；⑤**N2**：`generate_user_id` 并发生成相同 ID，`INSERT OR REPLACE` 静默覆盖已有用户。

**影响**：登录/注册/`/api/auth/me`/兑换/全部教师与管理员用户类接口/权限校验；最坏为堆破坏与静默数据丢失，常态为积分与库存错账、删除用户后操作落到错误对象。

**改法（分工与次序见 §6 批次 1）**：
1. **连接策略先定，再谈锁粒度**：推荐改为**每线程一条连接**（`thread_local SqliteDb`）或小型连接池 —— 这能让 `busy_timeout(5000)` + WAL 真正发挥作用，并使事务天然按线程隔离（一并解决 ③）。若坚持单连接，则必须补一把全局递归写锁并把 `BEGIN/COMMIT` 与锁同生命周期。
2. `users`/索引 map/`points_records` 统一用一把 `std::shared_mutex`：读 `shared_lock`、增删改 `unique_lock`；**禁止 `find_user_by_id` 返回裸指针**，改为 `std::optional<User>` 值拷贝或回调式 `with_user(id, [](User&){...})`，从类型上消除悬垂。
3. 索引不要再用"向量下标"：改存稳定键或 `std::deque<User>`/`vector<unique_ptr<User>>`；或让所有 `erase` 路径调用一个**先 `clear()` 再重建**的 `rebuild_indexes()`。
4. 兑换改为**单条原子 SQL**：`UPDATE users SET points = points - ? WHERE id = ? AND points >= ?`，以**受影响行数 == 1** 为继续条件；补 `CHECK (points >= 0)`、`CHECK (stock >= -1)` 让超卖在 SQLite 层被拒。为此需给 `execute_bind` 增加返回受影响行数的变体。
5. **N2**：`users.id` 改由 DB 生成或改用 `INSERT`（非 `OR REPLACE`）让唯一约束暴露冲突；序号生成放进同一把写锁内。
6. **N3**：给列表与导出接口加分页（`LIMIT/OFFSET` 或游标），前端改为请求分页而非对全量载荷做客户端分页。

**成本**：连接策略 M～L；锁与索引 M；原子 SQL M；分页 M。

### 4.2 后端安全与鉴权（B1/B2/B4/B9/B10/B27）

**位置**：`sha256.h:94-148,162-188`；`routes_public.cpp:45-51`；`auth.h:141-154`；`routes_teacher.cpp:193-201,298-300,487-489`；`httplib.h:3574-3579`；`main.cpp:60-75,250-254`

**证据（我的机械验证输出）**：

```powershell
PS> $lines = Get-Content sha256.h; ($lines[93..147] | Select-String 'password').Count
1                                    # 唯一命中是第 94 行的形参声明本身
PS> $body = $lines[94..147]; ($body | Select-String 'password').Count
0                                    # 函数体 95-148 行零引用
PS> Select-String -Path sha256.h -Pattern 'k_ipad|k_opad'
103: std::memcpy(k_ipad, s.data(), s.size() < 32 ? s.size() : 32);   # ← 密钥材料取自 salt
106: std::memcpy(k_ipad, salt.data(), salt.size());
109: for (int i = 0; i < BLOCK; i++) { k_ipad[i] ^= 0x36; k_opad[i] ^= 0x5c; }
```

**问题**：
1. **B1（完全认证绕过）**：`pbkdf2_sha256(password, salt, iters)` 的结果**与 password 无关**，只由 `(salt, iters)` 决定；而 `verify_password`（`:180`）用存储串自带的 salt/iters 重算同一个 F 再与 `dk` 比较 → `calc == dk` **恒真** → 对任何 `pbkdf2$` 哈希，**任意口令（含空串）都通过**。这不是弱哈希，是认证绕过。
2. **B1 的提权链**：`routes_public.cpp:47-51` 在"旧哈希登录成功"后就地用 `hash_password()` 重写并落库。种子账号当前是裸 SHA256（走 `sha256.h:187` 的正确分支），**因此 admin 只要正常登录一次，其哈希就被改写成坏 pbkdf2，此后任意口令可登录 admin 且永久有效**。我已枚举 `hash_password` 全部 **9 处**调用点，**每一处都产生坏哈希**。
3. **B1 的现实可达性（明确判断）**：工作区**不存在任何数据库文件**（`Test-Path` False + 全递归为空 + `glob` No files found，三路交叉验证）→ **后门此刻尚未武装**；但 `server.log` 记录 **603 次** `POST /api/auth/login`，说明真实登录发生过。**任一首次成功登录或任一建号/改密/注册即永久打开。**
4. **B2**：4 个默认账号的明文我已用 .NET SHA256 **独立复算，四组全部 MATCH**（`admin123`/`teacher123`/`student123`/`parent123` ↔ `main.cpp:251-254`）。另当 DB 打开失败时服务**静默退化为内存模式**继续提供硬编码账号。
5. **B4**：锁定键 `ip|username` 的 `ip` 来自客户端可伪造的 `X-Forwarded-For` → 每次换一个值即可无限爆破；不发该头则键退化为 `unknown|username`，可反向锁死任意账号 30 分钟；家长端 `/api/parent/login` 完全没有锁定。
6. **B9（水平越权 IDOR）**：鉴权确实在**服务端**做（`check_permission_middleware`），但粒度只到**角色级**；`student:manage` 授予全体教师（`main.cpp:246`），而所有 `/api/teacher/*` **写接口**都没有对象级（班级）过滤 → 任意教师可改/删/加分/打分任意班级学生。**注意区分：不是"校验只在前端做"，而是"服务端只做角色校验、缺 ownership 校验"。**
7. **B10**：handler 只捕 `json::parse_error`，而 `req_json.value("username","")` 在类型不符时抛 `type_error` → 冒泡到 `dispatch_request` → `res.set_header("EXCEPTION_WHAT", ex.what())`（`httplib.h:3576`）→ **异常原文进响应头**。可无认证触发（`POST /api/auth/login`，body `{"username":123,...}`）。
8. **B27**：检测到旧 schema 时直接 `std::remove()` 删库重建、无备份。**该路径已在生产中真实执行**（`server.log:22809`，2026-06-27 14:05:06）。

**改法**：B1 按 RFC 8018 重写（`U1 = HMAC-SHA256(key=password, msg=salt||INT32BE(1))`，即 **HMAC 的 key 必须是 password**），**且必须先补 RFC 6070 标准向量测试再改实现**（见 §6 依赖）；存量 `pbkdf2$` 记录一律视为失效并强制重置。B2 改为首启生成随机强口令并一次性输出 + 首登强制改密，去掉内存兜底账号表（磁盘库失败直接 `exit(1)`）。B4 默认不信任 XFF（仅在显式配置可信代理时采信**最后一跳**），并把账号维度与 IP 维度分离计数；家长端接入同一套锁定。B9 抽出 `teacher_owns_student(teacher_id, student_id)` 授权原语并在 5~6 个写接口入口调用，未绑定班级的教师返回空列表或 403 而非全校。B10 在路由注册处加一层统一 wrapper（`try{...}catch(...){500+日志}`），并加 CI 冒烟断言响应头不含 `EXCEPTION_WHAT`。

**成本**：B1 M（+存量迁移 L）；B2 S～M；B4 S～M；B9 M；B10 S；B27 S。

### 4.3 后端错误处理与结构（B10/B11/B12/B13/B17/B19/B29）

**位置**：`sqlite_wrapper.h:53-62,145-147`；`auth.h:242-274`；`routes_admin.cpp:60-70,335-349,1229-1244`；全 `routes_*.cpp`

**证据**：

```cpp
// sqlite_wrapper.h:59-62 —— prepare 失败伪装成"空结果"
if (rc != SQLITE_OK) {
    std::cerr << "查询失败: " << sqlite3_errmsg(db_) << std::endl;
    return result;                 // result 是 json::array()，与"无数据"不可区分
}

// routes_admin.cpp:341-347 —— 判据写反
json backup_result = db.query("VACUUM INTO '" + backup_path + "'");
if (backup_result.is_null()) {          // VACUUM INTO 无行返回 → db.query 返回 [] 而非 null → 恒 false
    response = {{"code", 200}, {"msg", "备份成功"}, ...};
    Logger::info("数据库备份成功: " + backup_path);      // ← 不可达
} else {
    response = {{"code", 500}, {"msg", "备份失败"}};      // ← 恒走这里
}
```

**我复核的统计（17 个自研文件）**：`res.status =` **16** 处 vs `{"code",N}` **217** 处。

**问题**：错误语义没有单一出口。①绝大多数业务错误以 **HTTP 200** 返回，仅 16 处设置真实状态码，状态码与业务码混用；②DB 层错误与"无数据"不可区分，调用方会把 DB 故障当业务结果（甚至当 403 权限不足）；③备份接口**判据写反**，实际生成文件却永远报"备份失败"（假阴性会让运维/自动化误判备份全线失败，成功日志永不可达）；④DB 错误只走 `std::cerr`，不落 Logger 文件；⑤`/api/admin/system` 与 `/system/config` 返回写死假数据。⑥`auth.h` 内两种 SQL 风格并存，且 `:199-200` 注释明确宣称参数化而 `:243` 立即改用 `snprintf` 拼接 —— **注释与代码自相矛盾**，这是最典型的维护陷阱。

**改法**：统一响应封装（一个 `ok()/fail()` 或 `send_json(res, http_status, business_code, msg, data)`），`res.status` 与业务 `code` 一次设定；`SqliteDb` 的 `query/query_bind` 增加"是否出错"的独立通道（返回值 + `last_error()`），禁止用空数组表达失败；备份判据改为按受影响语句/文件存在性判断，或检查 `VACUUM INTO` 的 `sqlite3_step` 返回码；`sqlite_wrapper` 的错误改走 `Logger::error`；`auth.h` 三处会话 SQL 迁到 `execute_bind/query_bind`；假数据接口要么接真实数据要么删除。假功能与死权限码一并清理。

**成本**：统一响应 M（涉及 217 处调用点，建议按模块分批）；其余各 S。

### 4.4 前端可维护性与巨型组件（F3/F4/F6/F7/F8/F9/F10/N6）

**位置**：`AdminApp.vue:1-2626`；`TeacherApp.vue:1-1519`；`usePagination.ts:1-67`；`main.ts:24-26`

**证据（我的独立计数）**：

```
AdminApp.vue：总行数 2626 / \bref[<(] = 54 / computed( = 10 / watch( = 2
              api.(get|post|put|delete) = 26 / try { = 27 / console.error( = 25
TeacherApp.vue：总行数 1519
  :168  const evaluations = ref<Evaluation[]>([ {studentId:1,dimensionId:1,score:85,...}, {studentId:1,dimensionId:2,score:90,...} ])
  :635  const ev = evaluations.value.find(e => e.studentId === studentId && e.dimensionId === dimensionId)
  :313  async function loadEvaluations() {
  :315    // 实际未使用返回值。此处保留 API 调用以维持网络行为。
```

**问题**：
1. **F3**：7 个业务域（用户/班级/角色/权限/系统配置/商城/概览）挤在一个 2,626 行 SFC 内，互不相关的状态共享作用域；`pageSize = 10` 与 `mallPageSize = 10` 各自硬编码；12 个弹窗各配一套"打开赋值/关闭重置"。**无法对单一 tab 做隔离测试**。
2. **F4（t2 原报已定为 high；我复核纠正的是其机制，不是定级）**：我把机制查得更准 —— `Student.id` 在运行时是**字符串**（后端返回 `user.id`，而 `users.id` 是 TEXT 主键，形如 `"student-02-01-01"`），而硬编码常量的 `studentId` 是 **number `1`**，`getStudentEvaluation` 用 **`===`** 比较 → `1 === "student-02-01-01"` **恒 false** → **所有学生的 5 个分数列一律渲染 `v-else` 的"未评价"**；85/90 是**不可达死数据**；提交成功后 `loadEvaluations()` 也**从不回填**（`:313-321` 丢弃返回值）。真实评价已落库但**前端没有任何读回路径**。`:315` 那句"保留 API 调用以维持网络行为"说明这是**明知无效仍有意保留**，性质是"半途而废的功能"而非"演示数据混入"。**注意：t2 的原描述"硬编码假评价数据…（显示假分数）"方向没错但不够准 —— 实际没有任何分数被显示出来。**
3. **F6/F7/N6**：`usePagination` 与 8 个导出（含 `globalProperties.$format*` 整块注册）从未接线，同时三个页面各手写一遍分页派生值。
4. **F8**：错误样板重复 **24** 次、`try {` **68** 处，四页策略互不一致。
5. **F9**：首屏串行瀑布 6/7/2 次，而 `StudentApp.vue:274-281` 已有正确 `Promise.all` 写法。
6. **F10**：4 个配置按钮只弹 toast。

**改法**：
1. **F4 先做（它同时是正确性问题）**：删除 `:168-171` 的硬编码常量，让 `loadEvaluations()` 真正请求 **`GET /api/teacher/evaluations`**（`routes_teacher.cpp:728` 的**复数**端点 —— **注意：t2 原建议改法指向的 `?studentId=` 端点并不存在，这是必须修正的一处**），按学生×维度回填 `evaluations`；`editEvaluation`/`deleteEvaluation` 在拿到真实评价 ID 前改为 `disabled` + `title="暂未开放"`，不要保留"确认后无动作"的路径。
2. **F3 拆分为 4~6 个 PR**，目标 `AdminApp.vue` ≤ 350 行：7 个 `tabs/*.vue` + 各业务弹窗组件（添加/编辑复用同一组件用 `mode` prop 区分）+ 3 个 composable（`useAdminUsers`/`useAdminMall`/`useAdminBatchImport`），`AdminApp.vue` 只留 `checkAuth` + `switchTab` 分发表。TeacherApp 同法拆到 ≤400 行。
3. **F6/F7/N6**：删除死代码，或把分页派生统一收敛到 `usePagination`；模板改用 `$format*` 或删掉 `globalProperties` 注册（二选一，不要两套并存）。
4. **F8/F9/F10**：抽 `requestAction(api.get(...), { onOk, okMsg })` 包装器让"失败必有提示"成为封装保证；首屏改 `Promise.all`；假按钮接真实实现或删除。

**成本**：F4 M；F3/TeacherApp 拆分 L；其余各 S～M。

### 4.5 前端类型安全与构建（F5/F24/B14/F11/F17）

**位置**：`api.ts:19-24,52,73-76,103-126`；`tsconfig.json:14-16`；`useChart.ts:10`；`AdminApp.vue:14-16`；`httplib.h:44`

**证据**：

```ts
// api.ts:19-24
export interface ApiResponse<T = any> {
  code: number; msg?: string; data?: T
  [key: string]: any          // ← 任意字段访问都合法，拼写错误不再报错
}
// api.ts:73-76
} else if (method === 'DELETE') {
  // 无请求体的 DELETE 需 Content-Length:0，兼容部分反代
  headers['Content-Length'] = '0';      // ← fetch 禁止请求头，浏览器静默忽略
}
// tsconfig.json:14-16
"strict": true, "noUnusedLocals": false, "noUnusedParameters": false
```

**我亲自重跑的严格度实测**：

```powershell
PS> node .\node_modules\vue-tsc\bin\vue-tsc.js --noEmit --noUnusedLocals --noUnusedParameters
src/mock/index.ts(298,97): error TS6133: 'db' is declared but its value is never read.
src/mock/index.ts(298,101): error TS6133: 'body' is declared but its value is never read.
src/mock/index.ts(411,82 / 411,86 / 439,94 / 439,98): error TS6133: ...
src/pages/teacher/TeacherApp.vue(664,25): error TS6133: 'student' is declared but its value is never read.
[exit code: 2]
```

**恰 7 个错误 / exit 2**；三项全开据另一份报告为 26 个（mock 12 / student 6 / admin 5 / teacher 2 / parent 1）。

**问题**：①`ApiResponse<T = any>` + 索引签名让 `res.data.xxx` 的拼写错误与字段缺失在编译期完全不可见，58 处 `res.code` 判据中 26 处未标泛型；②CI 门禁因关掉 `noUnusedLocals/noUnusedParameters` 而看不到 26 个真实错误（其中 `AdminApp.vue:648` 的 `wb.SheetNames[0]` 正是空工作簿下运行期 `TypeError` 的经典来源）；③`Content-Length` 是 fetch 禁止请求头（MDN 原文：*"cannot be set or modified programmatically in a request"*，清单中明确列出 `Content-Length`）→ 该行在浏览器**永不生效**，注释所称"兼容部分反代"无依据（**注意：Node/undici 会保留该头，故在非浏览器运行时会改变行为；但它是无害的死代码，不是"导致 DELETE 失败"**）；④包体：`import * as echarts` 阻断 tree-shaking（约 1.0 MB）+ xlsx + pinyin 静态引入 + 字体 4.72 MB；⑤`xlsx@0.18.5` 命中 CVE-2023-30533 / CVE-2024-22363（**证据等级：版本匹配 + 公开 CVE 编号**，非本机 `npm audit`）；⑥后端 `CPPHTTPLIB_PAYLOAD_MAX_LENGTH = SIZE_MAX` 且从未调用任何 setter。

**改法**：`ApiResponse<T = unknown>` 并删掉索引签名，默认泛型收紧到 `unknown`；新增 `src/types/api.ts` 为 34 个端点写响应 DTO；`tsconfig.json:15` 改 `noUnusedLocals: true`（先修 7 处，约 10 分钟）；`noUncheckedIndexedAccess` 留待下批（19 处）；删除 `api.ts:74-75` 两行；echarts 改按需引入（`echarts/core` + 显式 `use()`），xlsx/pinyin 改动态 `import()`；主 `server` 上设置 `set_payload_max_length` 与 `set_read_timeout`；xlsx 评估替换（无更高修复版）。

**成本**：类型收紧 M；死代码清理 S；按需引入 M；后端限额 S。

### 4.6 测试与工程化缺口（F15/B19/B17/F2/F16/F1）

**位置**：`package.json:6-12`；`ci.yml:35-46,105-147`；`release.yml:51`；`main.cpp:246`

**证据**：`frontend/` 下**不存在** eslint/prettier/vitest/jest 任何配置文件；`package.json` 的 scripts 只有 `dev/build/build:no-typecheck/typecheck/preview`（**无 test、无 lint**）；CI 的前端把关仅 `npm ci` + `npm run typecheck` + build。全仓库**无任何测试文件**。

**问题**：这是**最根本的短板**，且是其他缺陷能长期存活的直接原因 —— B1 那种"两个不同口令必须得到不同结果"的错误，只要有 1 个 PBKDF2 向量测试就会当场暴露；F4 的假数据、B13 的判据写反、N2 的重复 ID 同理。同时 `ci.yml` 的 `package` job 误用 mock 构建（F1 blocker）、`release.yml:51` 用 `npm install` 破坏发布可复现性（F16）。

**改法**：引入 **Vitest**（前端）+ 一个最小的 C++ 单元测试目标（可用单头测试框架或 `ctest`），优先覆盖：PBKDF2 的 RFC 6070 向量、`verify_password` 的"不同口令不同结果"、`is_path_safe`、`execute_bind` 的受影响行数语义、mock 模块的产物断言；引入 ESLint + Prettier；CI 增加 `test` 与 `lint` 步骤；把 `tsconfig.json:15` 打开；修 `ci.yml` 的 mock 串用与 `release.yml` 的 `npm ci`。**关键依赖：测试必须先于巨型组件重构（见 §6 批次 2→3）。**

**成本**：M（框架引入）～L（覆盖核心路径）。

### 4.7 仓库卫生（N4/N5/B27/F18/F19/F20）

**位置**：`.gitignore` 末尾；`routes_static.cpp:42,94`；`main.cpp:60-75`；`tsconfig.json:21-25`；`Login.vue:13,15`

**证据**：

```powershell
PS> git ls-files | Select-String '^\.trae/' | Measure-Object
Count : 25
PS> git status --porcelain
?? .agent-teams/
?? docs/audit/
```

**问题**：
- **N4**：`.gitignore` 末尾确有 `# Trae 工具目录（spec 文档可选择性保留，这里排除）` + `.trae/` 规则，但**该规则是后加的、未执行 `git rm --cached`** → 25 个文件仍被跟踪。这是典型的"gitignore 不追溯已跟踪文件"，会让 `git status` 看起来干净而实际仍在入库。
- **N5**：`content_type_for` 显式支持 `.map`，`/assets/.*` 路由无扩展名白名单 → 一旦有人为排障开启 `build.sourcemap`，生产服务会**自动**把完整前端源码对外提供。当前 `vite.config.ts` 未开 sourcemap，故**目前不构成泄露**，属"配置一改就出事"的潜在项。
- **B27**：删库重建路径已实际执行过（`server.log:22809`）。
- **F18/F19/F20**：`tsconfig` 的 `include` 含对 tsc 无意义的 `*.html`、`@/*` 别名零使用、`Login.vue` 重复 import、mock 未匹配路由返回 HTTP 200 + `code:404`。

**改法**：`git rm -r --cached .trae/` 后提交（**建议性操作，本次审计未执行**）；从 `content_type_for` 删除 `.map` 分支或给 `/assets/` 路由加扩展名白名单；清理 `tsconfig` 与重复 import；mock 未匹配路由改为返回真实 404 而非 200。顺带清理死代码（F6/F7/N6）与瘦身依赖（F11）。

**成本**：各 S。
**已核对为干净**：`.gitignore` 对 `.idea/`、`.vscode/`、`.zcode/`、`*.log`、`*.db`、`node_modules/`、`dist/` 的声明均未被违反（`git ls-files` 无命中）。

---

### 4.8 定向排查：「TEXT 主键迁移未传导到前端类型层」是否系统性成立

**排查动机**：`users.id` 已是 **TEXT 主键**（`main.cpp:80` `id TEXT PRIMARY KEY`，种子 `main.cpp:251-254` 为 `admin-01` / `teacher-001` / `student-02-01-01` / `parent-001`），且 `server.log:22809` 记录过显式迁移（"检测到旧 schema（users.id 为 INTEGER）→ 删库重建为 TEXT 主键"）。F4 撞上的 `TeacherApp.vue:61` 把用户标识当 `number` 用。问题是：这是**一处孤立的类型笔误**，还是**一次 schema 迁移未传导到前端类型层**的系统性缺陷？

**排查方法（4 组独立扫描，覆盖 `frontend/src` 全部 34 个文件）**：

| 扫描 | 模式 | 目的 |
|---|---|---|
| 1 | `^\s*(id\|studentId\|userId\|teacherId\|parentId\|itemId\|classId\|roleId\|role_id\|dimensionId\|student_id\|item_id\|class_id)\s*:\s*(number\|string\|\s*number\|…)` | 枚举全部标识符类型声明 |
| 2 | `parseInt(\|Number(\|parseFloat(` | 找出对标识符的数值强制转换 |
| 3 | `\.(id\|studentId\|userId\|…)\s*===\s*\|===\s*.*\.(id\|studentId\|userId)` | 找出严格相等比较 |
| 4 | `\.id\s*[+\-*/]\|\[\s*…\.(id\|studentId\|userId)\s*\]` | 找出把标识符当算术量或下标用 |

**结论：不成立系统性问题 —— 全站仅 1 处真实失配。**

| 处 | 位置 | 类型对照 | 性质 |
|---|---|---|---|
| **唯一失配** | `TeacherApp.vue:61` `studentId: number`（配 `:169-170` 常量 `studentId: 1`、`:635` `===` 比较） | 声明 `number` ← 实参 `Student.id: number \| string`（`:29`），后端 `routes_teacher.cpp:89` 返回 TEXT `"student-02-01-01"` | 即 F4；已更正机制 |

**已排查其余模块，未发现同类失配**（以下均给出类型对照，证明确非"可能存在"）：

| 模块 | 检查内容 | 为何**不是**失配 |
|---|---|---|
| `ParentApp.vue` | 子切换 `currentChildId === child.id`（`:282,292,296,323,340`）、`selectChild(id: string)`（`:104`）、`c.id === currentChildId.value`（`:124`） | **两侧都是 string**：`ChildSummary.id: string`（`:21`）、`ChildInfo.id: string`（`:29`）、`currentChildId = ref<string \| null>(null)`（`:77`）。**家长端把 TEXT 迁移做对了** —— 这是证明团队掌握正确写法的反例 |
| `TeacherApp.vue` 其他 | `replyingTo === message.id`（`:448,1196`） | 两侧都是联合类型：`replyingTo = ref<number \| string \| null>`（`:187`）、`ParentMessage.id: number \| string`（`:74`）。联合 vs 联合，`===` 安全 |
| `TeacherApp.vue` 其他 | `Evaluation.id: number`（`:60`）、`EvalDimension.id: number`（`:53`）、`dimensionId: number`（`:62`） | **正确**：`evaluations.id` 是 `INTEGER PRIMARY KEY AUTOINCREMENT`（`main.cpp:125`），评价维度是前端固有的 1–5 常量，均非用户标识 |
| `AdminApp.vue` | 用户 id 声明（`:34,59,70,79,140`）、`user.id` 在 URL/`v-for :key` 中的使用（`:514,540,1419,1420`） | 声明为 `number \| string`（容错联合），用法为**字符串插值与 `:key`**，无数值比较 |
| `AdminApp.vue` | 全部 `Number(...)` 与 `parseInt(...)`（`:309,327,343,459,476-481,509-513,529,925,961,1026-1028,1058-1060,1424`） | **全部作用在 `role_id` / `cost` / `stock` / `status` / `class_code` 上**，这些在后端均为 INTEGER（`main.cpp:83` 等），转换**正确** |
| `StudentApp.vue` | `id: 'p' + r.id`（`:155`）、`'r' + r.id`（`:166`）、`item_id: item.id`（`:232`） | 前两者是**合成的带前缀字符串键**（用于列表 `:key`），非标识比较；后者是透传 |
| `mock/index.ts` | `u.id === params[0]`（`:287,428`）与 `Number(params[0])`（`:311,333,355,377,535,544`） | **内部自洽**：用户按**字符串** id 比较（与 TEXT 主键一致），roles/permissions/classes/products/messages 按**整数** id 转换（与各自 INTEGER 主键一致）。且 mock 不进入正式发布产物 |
| `useToast.ts` | `id: number`（`:12`）、`t.id === id`（`:36`） | 本地自增计数器，与后端标识无关 |
| 全局 | `id` 参与算术或作数组下标（扫描 4） | **零命中**。唯一的 `counts[Number(u.role_id)]`（`AdminApp.vue:343`）以 INTEGER 的 `role_id` 为下标，**正确** |

**与 `N2` 的关系（同源不同层，须分清）**：`F4` 与 `N2` 确实共享**同一根因类别** —— "`id` 是 TEXT，却在某处按整数处理"。但两者是**相互独立**的缺陷，修一处不能修另一处：

| | `F4` | `N2` |
|---|---|---|
| 所在层 | **前端类型层**（`Evaluation.studentId: number`） | **后端 ID 生成层**（`models.cpp:112-160` 用 `max_seq+1`） |
| 表现 | 比较恒 false → UI 恒显示"未评价" | 并发生成相同 id → `INSERT OR REPLACE` 覆盖用户 |
| 影响 | 用户可见的功能性错误信息 | 静默数据丢失 |
| 修复 | 改类型为 `string` 并接通列表接口 | 改由 DB 生成 id 或改 `INSERT` 暴露冲突 |

**这条负面结论的价值**：它明确否定了"全站 id 处理都烂"的推断 —— 除 `TeacherApp.vue:61` 外，**其余 33 个文件、以及 `AdminApp`/`ParentApp`/`StudentApp`/`mock` 四套 id 处理均经类型对照确认为正确**。其中 `ParentApp.vue` 用 `id: string` 做同样"选中子项"的工作而完全正确，是最有说服力的反例。最终报告据此**只立 F4 一条**，不额外制造"系统性类型迁移缺陷"的大条目。

---

## 5. 值得保留的优点（均有行号依据）

以下各项我**主动排查并独立验证为真正做对了**，改进时不应误伤：

| # | 优点 | 依据 |
|---|---|---|
| 1 | **静态文件路径穿越已被正确防护** | `httplib.h:3116`（`Server::parse_request_line` 内）执行 `req.path = detail::decode_url(m[3], false);` —— **服务端会先做百分号解码**；随后 `routes_static.cpp:14-23` 的 `is_path_safe` 拒绝任何解码后含 `..` 的路径，且是唯一入口（`:50`），无旁路。`%2e%2e` 变体解码后必然含 `..` 而被拒。 |
| 2 | **日志不记录敏感信息** | 对全部 `Logger::(info\|warning\|error)` 做敏感词过滤（`password\|token\|session_id\|csrf\|req.body\|secret`）→ **零命中**。`log_request`（`auth.h:54-57`）只记 `method + " " + path`，而 httplib 把查询串分离进 `req.params`（`httplib.h:3120`），故 URL 里的 token 也不会入日志。 |
| 3 | **导出接口不泄露口令哈希** | `routes_admin.cpp:1347-1354` 只取 `id/username/role_id/name/className/points` 六个非敏感字段，注释自称的"安全修复 V7"**确实落实**。 |
| 4 | **无 `SELECT *`** | 全后端 grep `SELECT \*` 零命中，查询均显式列名。 |
| 5 | **`Set-Cookie` 正确支持多值** | `httplib.h:195` `Headers = std::multimap<...>`，`Response::set_header` 用 `headers.emplace`（`:2794-2803`）→ 登录时 `set_session_cookie`（`routes_public.cpp:69`）与 `issue_csrf_token`（`:71`）的两个 Cookie **都会保留**（不存在"第二个覆盖第一个"的问题）。 |
| 6 | **CSRF 的 `HttpOnly` 是正确设计** | `issue_csrf_token`（`auth.h:171-177`）除写 Cookie 外**还 return 该 token**，路由把它放进 JSON body（`routes_public.cpp:84`、`routes_parent.cpp:120`），前端从 body 取——因此 `HttpOnly` 既不妨碍双重提交，又防止 XSS 直接读取。校验用常量时间比较（`auth.h:165-167`）。 |
| 7 | **Cookie 属性组合合理** | `sid`（`auth.h:86-87`）与 `csrf_token`（`:173`）均为 `HttpOnly; Path=/; SameSite=Lax`，能挡住跨站携带。 |
| 8 | **参数化查询已成为新代码的默认做法** | `execute_bind`/`query_bind`（`sqlite_wrapper.h:127-188`）在 `routes_admin.cpp`(26)、`routes_teacher.cpp`(20)、`routes_parent.cpp`(9)、`auth.h`(5) 等大量使用；`escapeString` 全项目仅剩 2 个调用点。 |
| 9 | **正式发布构建不含 mock** | 我确认 `release.yml:53-55` 的 `npm run build` **未设** `VITE_USE_MOCK`；t2 的同版本 Rollup 对照实验进一步显示非 `'true'` 时 mock 整模块被剔除（**强间接证据，未经端到端构建复现，环境受限**，详见 `VERIFICATION.md` §9.1 的三层拆解）。`pages.yml:45` 设 `'true'` 属**有意的**纯前端演示站设计。 |
| 10 | **数据库连接做了并发防护** | `sqlite_wrapper.h:27` 设 `busy_timeout(5000)`、`:29` 开 `PRAGMA journal_mode=WAL`，并在注释里说明理由 —— 说明作者意识到并发存在，只是防护层级不足。 |
| 11 | **CBAC 骨架完整** | `main.cpp:230-248` 定义 12 个权限码 + 4 角色的映射，`auth.h:280-317` 实现权限/角色判定，且**服务端确实执行**（`check_permission_middleware`，`auth.h:323-364`）。缺口只是**对象级 ownership**（B9）。 |
| 12 | **前端开了类型门禁** | `tsconfig.json:14` `"strict": true`；`package.json` 的 `build` 脚本为 `vue-tsc --noEmit && vite build` —— 类型检查**确实在 CI 门禁里**（`ci.yml:39`）。 |
| 13 | **`.gitignore` 覆盖面好** | 对编译产物、IDE 目录、日志、数据库、凭据文件（`cookies*.txt`/`*.pem`/`*.key`/`.env`）、`node_modules/`、`dist/` 逐项声明，且除 N4 外均未被违反。 |
| 14 | **前端已存在正确写法的范本** | `StudentApp.vue:274-281` 的 `Promise.all` 并发加载、`AdminApp.vue:1130` 的 `URL.revokeObjectURL` —— 说明团队知道正确做法，其余处属遗漏而非不懂。 |

---

## 6. 分批改进路线图

排序原则：**严重度 × 修复成本**，并显式标注前置依赖。批次 0 为"立即做、低风险高收益"。

> **贯穿全局的一句结论（请与 §2 同读）：**
> **不得用 F1 的存在降低 B1 的紧急度；修复顺序 B1 先于 F1。**

### 批次 0 —— 立即（低风险高收益，当日～2 日）

| 序 | 条目 | 动作 | 预期收益 | 前置依赖 |
|---|---|---|---|---|
| 0-1 | **B1** | 按 RFC 8018 重写 `pbkdf2_sha256`，使 HMAC 的 key = **password**；存量 `pbkdf2$` 记录一律置为失效并强制重置；`verify_password` 对空哈希恒返回 false | 消除**完整认证绕过**（本项目最严重缺陷） | ⚠️ **必须先补 RFC 6070 标准测试向量再改实现** —— 否则改完仍无法自证正确，同类错误会再次静默回归。**这是本路线图最重要的依赖关系。** |
| 0-2 | **F1** | `ci.yml` 的 `package` job 改用非 mock 构建（或在 package job 内重新 `npm run build`）；mock 构建只上传给 Pages。并给 `vite.config.ts:14` 的默认值加断言 `=== 'true' ? 'true' : 'false'` | 交付包恢复"开箱即用"，消除"后端日志空无一物"的极难排查故障 | 无 |
| 0-3 | **B13** | 修正备份接口判据（检查 `VACUUM INTO` 的 `sqlite3_step` 返回码或文件存在性） | 备份不再恒报"失败"，成功日志可达 | 无 |
| 0-4 | **N1** | 在 `main.cpp:314` 之后补 `g_config.cookie_secure = false;` | 消除"配置 HTTPS 就登录失效"的静默故障 | 无 |
| 0-5 | **F24** | 删除 `api.ts:73-76` 的 `Content-Length` 无效两行 | 去掉无害但误导的死代码 | 无 |
| 0-6 | **F16** | `release.yml:51` 改 `npm ci` | 发布可复现 | 无 |
| 0-7 | **B2** | 首启生成随机强口令并一次性输出 + 首登强制改密；磁盘库打开失败改 `exit(1)` 而非内存兜底 | 消除可预测默认凭据；消除静默降级 | 建议与 0-1 同批（B1 会让"改密"失效） |
| 0-8 | **F4** | 删除 `TeacherApp.vue:168-171` 假数据；`loadEvaluations()` 改请求 **`GET /api/teacher/evaluations`（`routes_teacher.cpp:728` 复数端点）** 并真正回填；两个死按钮改禁用态 | 评价分数列从"恒显示未评价"恢复正确；消除假操作 | 无（**注意 t2 原建议指向的端点不存在**） |
| 0-9 | **B6** | 所有 `erase` 路径改调"先 `clear()` 再重建"的 `rebuild_indexes()` | 消除"删用户后操作落到错误对象" | 无（后续由批次 1 的索引重构彻底替代） |
| 0-10 | **N4** | `git rm -r --cached .trae/` 后提交 | 仓库不再跟踪 25 个 IDE 目录文件 | 无（**建议性操作，本次审计未执行**） |

**批次 0 预计收益：消除 2 条 blocker、2 条 high、5 条 medium/low，且全部为低风险改动。**

> ⚠️ **批次 0 必须遵守的处置顺序与理由（务必写进运维/开发的操作规程）：**
>
> **必须先把 B1 修好，再去改默认口令；"改密码"在 B1 存在时根本救不了场。**
>
> 理由：B1 使 `pbkdf2_sha256` 的派生值与口令无关，`verify_password` 对**任何** `pbkdf2$` 哈希都恒返回 true。因此运维最自然的第一反应 ——"赶紧把 admin 的默认口令改掉" —— **完全无效**：改密后写入的仍然是与口令无关的坏哈希，任意口令照样能登录。同理，`F13`（登录页 `fillTestAccount()` 预填默认口令）与 `B2`（硬编码弱口令种子）**都不是独立可解的问题，而是 B1 的放大器**：
>
> 1. **先修 B1**（含补 RFC 6070 向量测试 → 再改实现 → 存量 `pbkdf2$` 记录全部置为失效）；
> 2. **再修 B2**（首启生成随机强口令 + 强制首登改密）—— 只有此时"改密"才真正生效；
> 3. **F13 随 B2 一并处置**（删掉登录页预填按钮与 Release Notes 中的明文口令），**单修 F13 收益≈0**，若在 B1/B2 之前单独"清理文案"只会造成"已经安全了"的错觉；
> 4. **F13 + B2 与 B1 同批（批次 0）完成**，不得把其中任何一项推迟到后续批次 —— 这是本路线图唯一的"安全类硬绑定"。
>
> 另注：`F13` 本身作为独立发现的严重度为 **low**（其风险完全由 B2 承载），但**批次归属仍须与 B1/B2 绑定**，因为"严重度低"与"可延后处理"在这条链上不等价。

### 批次 1 —— 并发与数据正确性（结构性，约 1～2 周）

| 序 | 条目 | 动作 | 预期收益 | 前置依赖 |
|---|---|---|---|---|
| 1-1 | **连接策略（B7/B23 的根）** | 先决策：**每线程一条连接**（`thread_local SqliteDb`）或小型连接池；或退而求其次加全局递归写锁 + `BEGIN/COMMIT` 同生命周期 RAII 封装 | 让 WAL/`busy_timeout` 真正生效；事务按线程隔离 | 无（**本批次的第一个决策点，锁粒度方案取决于此**） |
| 1-2 | **B3 + B5 + N2** | **同批处理**（同一根因）：`login_attempts` 封装为带锁结构或迁移到 SQLite 表；`users`/索引 map/`points_records` 统一 `shared_mutex`；`find_user_by_id` 改返回值语义；**N2** 的 ID 生成改由 DB 生成或改 `INSERT` 暴露冲突 | 消除悬垂指针写入、rehash 竞态、计数丢失、**并发建号静默覆盖用户** | 依赖 1-1 的连接决策 |
| 1-3 | **B6（彻底版）** | 索引改存稳定键（或 `deque`/`vector<unique_ptr>`），取代 0-9 的临时修复 | 从类型上杜绝下标错位 | 依赖 1-2 的锁方案 |
| 1-4 | **B8** | 兑换改单条原子 SQL（`WHERE points >= ?` / `stock > 0`）并检查受影响行数；`execute_bind` 增加行数返回；补 `CHECK` 约束 | 消除双花与超卖 | 依赖 1-1 |
| 1-5 | **B9** | 抽出 `teacher_owns_student()` 原语并在 5~6 个写接口调用；未绑定班级返回空/403 | 消除教师端水平越权 | 无 |
| 1-6 | **B4** | 默认不信任 XFF；账号维度与 IP 维度分离计数；家长端接入锁定；给 map 加上限与过期清理 | 消除爆破绕过与反向锁死 | 与 1-2 同批（共用锁定结构） |
| 1-7 | **B10 + B14 + B12** | 路由层统一异常 wrapper（响应头不含异常原文）；设置 payload 上限与读超时；`SqliteDb` 增加独立错误通道 | 消除异常原文泄露、DoS 面、错误与空结果混淆 | 无 |

### 批次 2 —— 错误语义、类型与工程化（结构性，约 2～3 周）

| 序 | 条目 | 动作 | 预期收益 | 前置依赖 |
|---|---|---|---|---|
| 2-1 | **测试与 lint 引入（F15）** | 引入 Vitest + 最小 C++ 测试目标；优先覆盖 PBKDF2 向量、`verify_password` 反例、`is_path_safe`、`execute_bind` 行数语义、mock 产物断言；引入 ESLint + Prettier；CI 增 `test`/`lint` | **建立回归网** —— 这是所有后续重构的安全前提 | 无，但**必须是批次 3 的前置** |
| 2-2 | **B17 + F2** | 统一响应封装，`res.status` 与业务码一次设定（16 : 217 收敛）；`api.ts` 增 5xx 兜底；修正 7 处误导性 `catch` 注释 | 服务器异常从"静默无反应"变为"可见错误" | 2-1（需要测试守住契约） |
| 2-3 | **F5** | `ApiResponse<T = unknown>` + 删索引签名；为 34 个端点写 DTO；`noUnusedLocals: true`（先修 7 处）；下批 `noUncheckedIndexedAccess`（19 处） | 编译期捕获字段拼写与 `undefined` | 2-1 |
| 2-4 | **F8 + F9** | 抽 `requestAction` 包装器（消 24 处样板）；首屏改 `Promise.all`（6/7/2 次串行） | 一致性 + 首屏提速 | 2-2 |
| 2-5 | **B11 + B16 + B19 + B15** | 会话 SQL 全参数化；角色/权限增改落库；假数据接口接真实数据或删除；清理 5 个死权限码；导入不再静默降级角色 | 消除注释与代码矛盾、功能无效、权限码虚设 | 2-1 |
| 2-6 | **N3 + B18** | 列表/导出接口加分页；`points_records` 增加回载或改为 DB 直读；记录 ID 由 DB 生成 | 消除无界载荷与"重启后导出为空、ID 重号" | 依赖 1-1（分页需在连接策略确定后做） |

### 批次 3 —— 结构性重构与瘦身（长期，约 3～4 周）

| 序 | 条目 | 动作 | 预期收益 | 前置依赖 |
|---|---|---|---|---|
| 3-1 | **F3（AdminApp 拆分）** | 7 个 `tabs/*.vue` + 弹窗组件 + 3 个 composable；`AdminApp.vue` 从 2,626 行降到 ≤350 行（建议 4~6 个 PR） | 可读、可测、可并行开发 | ⚠️ **依赖 2-1：必须先有回归测试再重构巨型组件**，否则重构本身会引入新缺陷 |
| 3-2 | **TeacherApp 拆分** | 同法降到 ≤400 行 | 同上 | 依赖 3-1 的模式与 2-1 |
| 3-3 | **F6 + F7 + N6** | 分页派生统一収敛到 `usePagination`；删除 8 处零引用导出与 `globalProperties.$format*` 注册 | 消除误导后续维护者的死代码 | 依赖 3-1/3-2 的重构结果 |
| 3-4 | **F11 + F17** | echarts 改按需引入、xlsx/pinyin 改动态 `import()`、字体子集化；xlsx 评估替换（无更高修复版） | 包体显著下降、首屏提速 | 依赖 2-1（需体积回归断言） |
| 3-5 | **N5 + B30 + B29 + 其余 low** | 移除 `.map` 服务或加白名单；日志轮转移入锁内；DB 错误改走 Logger；清理 `tsconfig` 冗余、重复 import 等 | 收尾清理，降低长期维护成本 | 无 |

**依赖链总结（三条硬依赖，务必遵守）**：
1. **`B1` 的修复以"先补 RFC 6070 向量测试"为前提** —— 否则无法自证实现正确；
2. **批次 2-1（测试与 lint）必须早于批次 3 的所有重构** —— 否则巨型组件拆分与依赖调整失去安全网；
3. **批次 1-1（连接策略决策）决定 1-2/1-3/1-4/2-6 的具体做法** —— 先定连接模型，再谈锁粒度与分页实现。

---

## 7. 附录

### 7.1 底层报告勘误表（为何本报告以 `VERIFICATION.md` 为准）

底层两份报告质量整体很高（关键引文与源码**逐字一致**，我未发现任何**断章取义**），但存在下列需更正的表述与计数。凡与底层报告冲突处，本报告一律采用终值：

| # | 来源 | 原表述 | 核实结果 | 处理 |
|---|---|---|---|---|
| 1 | t1 `B25` | 单参 `set_cors_headers(res)` 是**"死代码"** | **不是死代码**：单参 **78** 处调用（`auth.h` 7、`routes_public` 6、`routes_admin` 27、`routes_teacher` 19、`routes_student` 9、`routes_parent` 9、`routes_static` 1），双参仅 **8** 处 | 改写为"缺陷重载占主流"；**严重度维持 low**（同源部署下零影响），但注明**一旦配置跨域白名单则影响近乎全站**，非仅 401/403 |
| 2 | t1 `B17` | "仅 **6** 处设置 `res.status`" | 实为 **16** 处（`auth.h` 8 + `routes_*.cpp` 8） | 采用 **16**；"**217** 处业务码"**精确无误**，保留 |
| 3 | t1 摘要 | 自述 "medium 10 / low 10" | 按其自身标题为 **medium 9 / low 11** | 采用 **9 / 11**（内部统计不一致已修正） |
| 4 | t1 `B1` | 影响面列 8 处 `hash_password` 调用点 | 实为 **9** 处（漏 `routes_public.cpp:48` 的**升级链自身**） | 采用 **9** |
| 5 | t2 `F2` | "**62** 处 `res.code`" | 实为 **58** 处（TeacherApp 17→**15**、StudentApp 8→**6**） | 采用 **58** |
| 6 | t2 `F2` | "StudentApp **6** 处 `catch { /* api 层已提示 */ }`" | 实为 **7** 处（漏列 `StudentApp.vue:240`） | 采用 **7** |
| 7 | t2 `F4` | **定级**：t2 原报**已为 `high`**（`FRONTEND_FINDINGS.md:327` 标题【high】、`:164` 表行 `\| F4 \| high \|`）。此前"medium→high 提级"的说法**系误传，本报告予以撤回** | 我复核确认的**不是定级**，而是**机制/描述**：t2 写"硬编码假评价数据…（显示假分数）"，实际 `1 === "student-02-01-01"` 恒 false → **所有学生 5 个维度一律显示"未评价"，85/90 是不可达死数据（没有任何分数被显示）** | **定级保持 high 不变**；仅**改写标题与描述**，不宣称任何提级 |
| 8 | t2 `F4` 建议改法 | 指向 `?studentId=` 端点 | 该端点不存在；真正返回评价数据的是 **`routes_teacher.cpp:728` 的 `GET /api/teacher/evaluations`（复数）** | 改法已更正 |
| 9 | t2 `F13` | "登录页内置真实默认口令"（易读成泄露/绕过） | 实为 4 个 `fillTestAccount()` **填充**按钮，**非注入/鉴权绕过**；且四组口令我已经 .NET SHA256 **独立复算与 `main.cpp:251-254` 全 MATCH** | 性质改写为"默认凭据随产物分发 + 无强制轮换机制"；**与 B2 同源，单修 F13 收益≈0**；严重度 my判 **low** |
| 10 | t2 `F7` | 8 处零引用导出 | 成立，**且** `main.ts:24-26` 的 `globalProperties.$format*` 注册同样零使用 | 与新增 **N6** 合并 |
| 11 | t2 §6.1 | mock 入产物结论"未端到端验证" | 拆为 L1 门控机制（我已确认）/ L2 tree-shaking（强间接证据）/ L3 F1 后果（我已确认） | 终判 **CONFIRMED（机制）+ 附注"未经端到端构建复现"**；**不得回退成 UNVERIFIABLE** |
| 12 | t2 `F17` | "命中 CVE" | 成立，但证据等级为**版本匹配 + 公开 CVE 编号**，非本机 `npm audit` | 保留等级标注；`xlsx` 无更高修复版，建议替换评估 |
| 13 | t1 `B25` 附近 | （队长提出待确认）`POST /api/teacher/evaluation` 与 `GET /api/teacher/evaluations` 是否遮蔽 | **不存在遮蔽**：httplib 按方法分离 handler 容器（`httplib.h:547-556`）+ 按方法分派（`:3544-3554`）+ `regex_match` 整串匹配（`:3569`） | **不立发现**；仅记录单复数命名隐患 |
| 14 | **t2 严重度分布** | 有说法称 t2 应改为 **blocker 1 / high 5 / medium 11 / low 7**（"含 F4 提级"） | **不成立。** 我按标题逐条重数 `FRONTEND_FINDINGS.md`：`blocker = 1`、`high = 4`、`medium = 12`、`low = 7`；其自报统计行（`:186`）**完全正确**。F4 从头就是 `high`，**不存在 medium→high 的移动** | **采用 t2 自报的 1 / 4 / 12 / 7**；撤回"提级"说法。本报告的全局分布（2 / 14 / 23 / 21 = 60）不受影响，因为 F4 在总表中本就计入 high |

**行号漂移记录（共 10 处，均不影响结论）**：`sha256.h` 校验段 `179-184`→实为 `180-184`；`models.cpp:16-22`/`164-169` 含注释行；`routes_public.cpp:367-373`→实为 `366-373`；`api.ts:73-76` 语句实为 `74-75`；余为区间含注释行。**所有被引用的 `文件:行号` 均已重新打开核对。**

### 7.2 验证阶段新增发现（N1–N6）

均标注为「**验证阶段新增**」，两份底层报告均未提及：

| id | 严重度 | 一句话 | 位置 | 我的证据 |
|---|---|---|---|---|
| **N2** | **high** | 并发建号生成**相同 ID**，`INSERT OR REPLACE` **静默覆盖**先建用户（数据丢失） | `models.cpp:112-160`、`:79` | `generate_user_id` 用内存扫描求 `max_seq+1`，无锁无唯一约束兜底；`save_user_to_db` 用 `INSERT OR REPLACE`。**已并入批次 1-2 与 B3/B5 同批修复** |
| **N1** | medium | `cookie_secure` 在 HTTPS 回退后**从不复位** → 明文 HTTP 下 Cookie 带 `Secure` → 浏览器拒存 `sid` → **登录静默失效** | `config.h:84`；`main.cpp:312-315`；`auth.h:88,95,174` | `cookie_secure` 全项目仅赋值一次、无复位点 |
| **N3** | medium | 全仓库**无服务端分页**（仅 3 处 `LIMIT`，2 处是 `LIMIT 1`）→ 列表/导出返回全集，前端客户端分页 | 全仓；`routes_admin.cpp:1344-1399` | grep `LIMIT\|OFFSET` 命中仅 `routes_admin.cpp:386`、`routes_teacher.cpp:564`、`routes_parent.cpp:27` |
| **N4** | low | `.trae/` 已被 `.gitignore` 声明却仍跟踪 **25** 个文件 | `.gitignore` 末尾 | `git ls-files \| Select-String '^\.trae/' \| Measure-Object` → **25** |
| **N5** | low | 静态服务显式支持 `.map`，`/assets/.*` 无扩展名白名单 → 开启 sourcemap 即对外泄露源码 | `routes_static.cpp:42,94` | `if (ext == ".map") return "application/json";` |
| **N6** | low | `main.ts:24-26` 的 `globalProperties.$format*` 注册**零模板使用** | `main.ts:24-26` | 模板全走 `import { formatDateTime } from '../../lib/format'` 直接导入（11 处调用点） |

### 7.3 t2 §6 七条存疑事项逐条判定

审计员自认的空白最有复核价值。七条裁定如下（详证见 `VERIFICATION.md` §3.9）：

| # | t2 的存疑事项 | 判定 | 说明 |
|---|---|---|---|
| 1 | `dist` 未能生成，mock 剔除结论来自 Rollup 对照实验 | **CONFIRMED（机制）** + 附注 | 三层证据拆解见 §7.1 第 11 行；并确认 `esbuild.exe` 存在、`dist` 不存在 → 限制来自沙箱 `spawn`，非操作失误 |
| 2 | `Content-Length` 缺浏览器端到端实测 | **CONFIRMED（规范级）** | 我已 fetch MDN《Forbidden request header》原文，清单明确含 `Content-Length`，措辞为 "cannot be set or modified programmatically" |
| 3 | "后端是否真的依赖该头" | **已解决：不依赖** | **逻辑闭合**：既然浏览器始终丢弃该头而 DELETE 功能正常（同源部署），后端不可能依赖它；DELETE 的 CSRF 走 `X-CSRF-Token`（`auth.h:162-164`、`api.ts:79-82`）。**故删除 `api.ts:74-75` 安全** |
| 4 | 默认口令是否与后端种子一致（"未复算哈希"） | **已解决：四组全部 MATCH** | 我用 .NET SHA256 独立复算 `admin123`/`teacher123`/`student123`/`parent123` ↔ `main.cpp:251-254`。**不再需要"以后端结论为准"的转述** |
| 5 | `/api/teacher/evaluation/dimensions` 是否还有其他消费者 | **已解决：前端仅一处** | 前端唯一命中 `TeacherApp.vue:317`（丢弃返回值）；并且发现 F4 建议改法端点有误（见 §7.1 第 8 行） |
| 6 | `npm audit` 环境限制 | **无法复核，维持原样** | 我未运行 `npm audit`；F17 一律标注"版本匹配 + 公开 CVE 编号" |
| 7 | `node_modules` 由 `--ignore-scripts` 安装是否影响构建 | **已解决：与缺文件无关** | `Test-Path @esbuild/win32-x64/esbuild.exe` → **True**；`vue-tsc` 可执行且我成功跑出 7 个错误 |

### 7.4 本次审计的局限与未验证事项（**仅供留意，未复现**）

> **标注口径：本节全部条目均为「仅供留意，未复现」——即**存疑、未经复现**的一类，不可作为已确认结论使用。** 与之相对，§3 总表中标注 `CONFIRMED` / `PARTIAL` 的条目为**"已复核确认，可据此动手"**；标注"未独立复核"的条目**本报告不背书**。两类结论的区分贯穿全文，请勿混用。

**必须与结论同读：以下各项本次**未能验证**，读者不应把相关结论当作已验证事实。

| 项 | 原因（如实标注，不猜） |
|---|---|
| **真实 `vite build` 端到端未执行** | 沙箱阻断（esbuild `spawn EPERM`；已确认 `esbuild.exe` 存在，属环境限制而非缺件）。mock tree-shaking 结论为"同版本 Rollup 进程内对照实验 + 与空壳对照字节数完全一致"的**强间接证据** |
| **浏览器端到端测试未执行** | Chrome 因沙箱 mojo 命名管道被拒（`0x5`）。`Content-Length` 结论为**规范级**确认；`F2` 的 500 静默链路为"前端代码 + 后端代码"双向静态推导，未在浏览器中实测 |
| **未运行 `server.exe`，未发起任何 HTTP 请求** | 任务纪律禁止。B10 的触发路径（`{"username":123}` → 500 + `EXCEPTION_WHAT` 头）为**框架层逐字确认 + 契约层静态推导**，未经端到端复现 |
| **未运行 `npm ci` / `npm audit`** | `npm ci` 会写 `node_modules`（与只读纪律冲突）；`npm audit` 因 registry 不实现 advisories 接口而 404。**故本轮无运行时依赖漏洞扫描结果**，F17 来自公开 CVE 库 |
| **B1 的库内实际状态无法查证** | 工作区**不存在任何数据库文件**（三路交叉验证）。"后门此刻未武装"是**已验证的否定事实**；但"历史上曾有哪些行被改写为坏哈希"**无物证**（`server.log` 的 603 次登录仅表明真实登录发生过） |
| **t1 引用的第三方/工具链行未逐行复核** | `sqlite3.c:14046-14052` 的 `SQLITE_THREADSAFE` 默认值、`C:\mingw64\...\c++config.h` 的 `_GLIBCXX_USE_DEV_RANDOM` 等，我**未独立验证**。t1 对并发的收敛结论（串行模式 → 否定 API 级内存损坏）**方向上我认可**，但其前提引用我未复核 |
| **未独立复核的条目（本报告不背书）** | 后端 `B15`、`B20`–`B24`、`B26`、`B28`；前端 `F10`、`F14`、`F17`–`F23`。这些条目**不在我的抽样范围内**（低于 60% medium 门槛或属 low），表中已逐条标注"未独立复核"。其中 `B22` 原报告本身就标注为"🔶疑似" |
| **两份底层报告的沙箱失败命令未复跑** | 仅确认最终状态：`frontend/dist` 不存在、`git status` 无 `package-lock.json` 变更 |

**只读性自证**：

```powershell
PS> git status --porcelain
?? .agent-teams/
?? docs/audit/
```

本次评估**未修改任何源码或配置**：未改动任何 `.cpp`/`.h`/`.vue`/`.ts`/配置文件，未执行 `git commit`/`git push`，未运行 `server.exe`，未发起 HTTP 请求，未运行前端构建，`frontend/package-lock.json` 无变更、`frontend/dist` 未生成。写入 `docs/audit/` 之外的文件数为 **0**。

本报告与 `VERIFICATION.md` 均为**建议书**，其中出现的 `git rm -r --cached .trae/`、`ci.yml` 修改、`config.h:84` 补复位等字样**全部是修复建议，均未被执行**。
