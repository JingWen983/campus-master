# T15 独立验证证据：批次 0 其余条目（F4 / mock / B13 / N1 / B6 / N4 / F1 / F16 / F24 / F13 / B2）

| 项 | 值 |
| --- | --- |
| 任务 | t15 —— 独立验证批次 0 其余条目（F4/mock/B13/N1/B6/N4/F1/F16/F24/F13/B2） |
| 承担人 | verifier（独立验证，非实现者） |
| attempt_id | **`cd55eb90-7408-4297-ac22-40edb86155e6`**（任务 **t26**「T26 独立验证批次 0 其余条目（t15 尾部链路重建版）」，deps `[t7,t9,t22]` 全部 completed） |
| 归档说明 | 本文档最初以 **t15** 记录（t15 因 `deps` 含 cancelled 的 t14 而**永久不可 claim**，见 §13）。t15 的尾部链路已由 **t26** 重建，本文档即 t26 的权威证据；§13 保留死锁机制记录 |
| verdict | **pass**（10 条验收项全部通过；2 条为「未端到端复现，仅静态核对」并已逐条标注） |
| 记录时间 | 2026-09-19 12:2x (+08:00) |
| 环境 | g++/gcc `14.2.0 (MinGW-W64 x86_64-msvcrt-posix-seh)`、node `v24.16.0`、npm/`vue-tsc 2.2.12`、pwsh `7.6.5` |

## 0. 判据绑定的修订指纹

| 文件 | SHA256 前缀 |
| --- | --- |
| `sha256.h` | `3E5C59DDC9AEB5EE` |
| `main.cpp` | `D7211E1FA47EB89A` |
| `auth.h` | `3C520FAD3AA5F7DD` |
| `routes_public.cpp` | `FB893F80500AA404` |

被测二进制 `server.exe` 由上述修订**当场编译**（10 步编译 + 链接全部 exit 0，6,030,092 B），输出到 `%TEMP%\t15_run_build`；仓库根**未生成**任何 `.o`/`server.exe`/`.db`/`.log`。
所有验证命令由 verifier 亲自执行，以下均为**我自己终端的原文**。

**判定口径（避免把静态当端到端）**：本文件对每一项显式标注「端到端（真实启动 server.exe / 真实执行 mock）」或「仅静态核对」。

---

## 1. 验收项 [1]：F4 字段名/类型与后端实际返回一致（端到端 + 代码核对）

**后端实际返回形状**（`routes_teacher.cpp:759-771`，逐字段）：

| 字段 | 类型 |
| --- | --- |
| `id` | number |
| `student_id` | **string**（`users.id` 是 TEXT 主键，`main.cpp:80`） |
| `student_name` / `className` / `dimension_name` / `comment` / `evaluator_name` / `time` | string |
| `dimension_id` | number |
| `score` | number |

**前端类型声明与归并**（`frontend/src/pages/teacher/TeacherApp.vue`）：
- `:60-73` 类型注释明确「后端字段名以 `routes_teacher.cpp:758-771` 为准：`student_id` / `dimension_id` 为下划线命名；`users.id` 是 TEXT 主键，故 `student_id` 为字符串」；
- `:328` 调用**复数**端点 `GET /api/teacher/evaluations`；`:333` `student_id: String(item.student_id)`；
- `:653-655` 归并判据：`` const sid = String(studentId) `` / `evaluations.value.find(e => String(e.student_id) === sid && Number(e.dimension_id) === Number(dimensionId))` —— **双侧都归一化为字符串比较**，故修复前 `1 === "student-02-01-01"` 恒 false 的失配已被消除（维度按数值比较）。

**mock 侧实测**（见 §2）返回的正是同一形状：`student_id` 为字符串（`"S001"`）、含 `dimension_name`/`score` —— 断言 `typeof rows[0].student_id === 'string'` 通过。

### 1b. 真后端「评价列回填」端到端（t26 追加证据）

复用播种出的 `teacher-001`（保留其 `teacher_classes`→`高二(1)班` 关联），仅把口令替换为已知值（`must_change_password=0`）：

```
教师-班级关联: teacher-001 -> 高二(1)班
该班学生: student-02-01-01 / 张同学 / 高二(1)班
教师登录 code=200 mcp=False
评价前 GET /api/teacher/evaluations -> HTTP 200 code=200 条数=0
POST /api/teacher/evaluation -> HTTP 200 code=200 msg=评价提交成功
评价后 GET /api/teacher/evaluations -> HTTP 200 code=200 条数=5
判据: 含 student-02-01-01 的德育(1)=90 且 student_id 为字符串 -> True
```

→ 真后端**确实回填**：POST 后 GET 由 0 条变 5 条，字段与前端声明一致；结合前端 `:653-655` 的 `String(e.student_id) === sid && Number(e.dimension_id) === Number(dimensionId)` 归并，评价列不再恒显示「未评价」。

---

## 2. 验收项 [2]：假数据与死按钮已清除（静态 + 代码核对）

| 检查 | 命令 | 结果 |
| --- | --- | --- |
| 硬编码分数 85/90 | `Select-String TeacherApp.vue -Pattern '\b85\b\|\b90\b'` | **0 命中** |
| 「开发中」残影 | `Select-String TeacherApp.vue -Pattern '开发中'` | **1 命中，且是注释** `:684`（说明「原先只弹『功能开发中...』toast（死按钮）」） |
| 两个死按钮 | `:1090` / `:1093` | `disabled` + `aria-disabled="true"` + `title="编辑评价（后端暂无对应接口，本批次禁用）"` / `title="删除评价（后端无删除接口，本批次禁用）"`；`:684-686` 注释说明改为**纯禁用态**、不再保留任何假交互 |
| 评价数据来源 | `:180-181`、`:323-338` | 「评价数据一律来自 `GET /api/teacher/evaluations`（loadEvaluations），初始为空数组」 |

---

## 3. 验收项 [3]：mock 演示路径（端到端执行）

本机无 tsx/vitest/ts-node，但有仓库自带 `typescript 5.9.3` → 用 `ts.transpileModule` 把 `frontend/src/mock/index.ts` 转译为 ESM（其唯一 import 是 `import type`，被擦除），在 Node 中以 `localStorage` stub + `globalThis.__VITE_USE_MOCK__='true'` 运行**真实 `mockRequest`**（harness：`%TEMP%\t15_mock\run_mock.mjs`，仓库外）。

```
[harness] transpiled mock/index.ts -> mock.mjs (38152 bytes)
[harness] isMockEnabled() = true
[ok]   mock 登录 teacher -> code 200
[raw] GET /api/teacher/evaluations -> {"code":200,"data":[{"id":4,"student_id":"S002",...,"dimension_name":"德育","score":95,...},
       {"id":1,"student_id":"S001",...,"dimension_name":"德育","score":90,...},{"id":2,..."智育",85},{"id":3,..."体育",88}]}
checks=12 fail=0
MOCK RESULT: PASS
mock harness exit=0
```

判据逐条通过：`code 200`；返回**非空**（不是「全部未评价」）；**S001 德育=90 / 智育=85 / 体育=88**；**S002 德育=95**；字段名与后端一致。
写回-读回：`POST /api/teacher/evaluation {studentId:'S003',scores:{1:91,...}}` → `code 200` 且 `data.student_id == 'S003'` → 再次 GET 记录数增加、**读回 S003 德育=91**、`student_id` 为字符串 → F4 在 `VITE_USE_MOCK=true` 的演示站同样成立。

### 2b. mock 的 `POST /api/auth/change-password`：演示站「被门禁 → 改密 → 放行」闭环（t26 追加证据）

同一 harness（`%TEMP%\t26_mock\run_mock.mjs`）追加：

```
[ok]   mock 种子库可解析且含 student 账号
[ok]   以 student 登录 -> 200 且 must_change_password=true
[ok]   置 true 后 /api/auth/me（student 会话）返回 must_change_password=true
[ok]   mock 路由表中存在 POST /api/auth/change-password（非 404）
[ok]   mock 改密：可预测默认口令 admin123 -> 400
[ok]   mock 改密：新旧相同 -> 400
[ok]   mock 改密：合法 -> 200 且 data.must_change_password=false
[ok]   改密后 /api/auth/me 的 must_change_password=false（放行）

checks=14 fail=0
MOCK RESULT: PASS
mock harness exit=0
```

→ **演示站不存在「要求改密却改不了」的死局**：`mock/index.ts:331-351` 的改密端点可真实走通，且镜像真后端的强度判定（可预测默认口令/新旧相同 → 400）。改密入口与门禁（`/api/auth/me` 返回 `must_change_password`）闭环成立。

---

## 4. 验收项 [4]：B13 备份（端到端，成功路 + 失败路）

沙箱 CWD 为 `%TEMP%\t15_run_<port>`（服务器工作目录），故备份文件落在沙箱内，不污染仓库。

```
第 1 次 POST backup（成功路）      HTTP 200  {"code":200,"data":{"path":"campus_system_backup_2026-09-19_12_17.db","size":135168},"msg":"备份成功"}
    data.path=campus_system_backup_2026-09-19_12_17.db size=135168 ; 磁盘 exists=True bytes=135168
第 2 次 POST backup（失败路/已存在） HTTP 500  {"code":500,"data":{"path":"campus_system_backup_2026-09-19_12_17.db"},"msg":"备份失败：目标备份文件已存在，拒绝覆盖"}
    两次 path 相同=True（相同才说明命中「已存在」分支）
```

- **成功路**：业务码 200 + `data.size=135168`，且**磁盘上确实存在**该文件、`Length=135168`（不是只看响应体）；
- **失败路可达**：同一分钟内第二次调用 → **HTTP 500 + `目标备份文件已存在，拒绝覆盖`**，与 `routes_admin.cpp:362-369` 的实现一致；判据不再是 `query()` 返回空即成功（`db.execute("VACUUM INTO …")` 的返回码为主判据 + 磁盘存在且非空兜底）。
- **覆盖边界（如实标注）**：t9 自述的两条子路径 —— ①`db.execute` 返回 false（目录不可写/磁盘满）②备份文件 0 字节 —— 本机**只有逻辑复刻覆盖**，未端到端构造；`DACL 不可写`（Windows ACL 拒绝写入）子路径**本机不可达**。这三条不构成本项判据的通过依据。

---

## 5. 验收项 [5]：N1 Cookie Secure 复位（端到端，两模式对照）

以自造账号（`mkhash.exe` 现算 `hash_password`，不硬编码）在两种 `config.json` 下各做一次**成功登录**，读原始 `Set-Cookie`：

```
[off] https.enabled=False  login HTTP 200  body.code=200
[off] Set-Cookie: sid=48a74b72…; HttpOnly; Path=/; Max-Age=86400; SameSite=Lax || csrf_token=a85654bf…; HttpOnly; Path=/; SameSite=Lax
[off] sid 出现=True  含 Secure=False
[on] https.enabled=True  login HTTP 200  body.code=200
[on] Set-Cookie: sid=29850f1c…; HttpOnly; Path=/; Max-Age=86400; SameSite=Lax || csrf_token=d7c01d22…; HttpOnly; Path=/; SameSite=Lax
[on] sid 出现=True  含 Secure=False
[on] 回退告警行数=2 ; cookie_secure 复位告警=2
```

- `https.enabled=true` 且无证书时：日志出现 **「HTTPS 已在配置中启用，但当前编译版本不支持 SSLServer…回退到 HTTP 模式」**（2 行）+ **「已同步复位 cookie_secure=false…（安全修复 N1）」**（2 行）；
- 两种模式下 `Set-Cookie` **均不含 `Secure`**，且 `sid` 正常下发 → 明文 HTTP 下浏览器不会再丢弃会话 cookie。
- 机制核对：`config.h:84` 在读到 `https.enabled=true` 时会把 `cookie_secure` 置 true（缺陷来源），`main.cpp:473-478` 是**唯一开关**把它复位为 false；`auth.h:88/:95/:174` 三处 `Set-Cookie` 均只读 `g_config.cookie_secure`。

**config.json 还原对照**：我**全程只改沙箱副本**，未触碰仓库 `config.json` —— `git diff --quiet -- config.json` **exit=0**、`git status --short -- config.json` **0 行**；两个沙箱副本的 https 段分别为 `{"enabled":false,…}` 与 `{"enabled":true,…}`，互不影响。
> 与契约 V4-3「临时改 `config.json` 再还原」的差异如实说明：我用沙箱副本替代仓库文件，达到同一验证目的且**不引入仓库变更风险**；仓库文件零改动已被上两条命令证明。

---

## 6. 验收项 [6]：B6 删除中间用户后按 id 查询（端到端）

三个用户按序建号（`AAA`→`BBB`→`CCC`，id 依次 `student---01/02/03`），使 `CCC` 位于被删元素之后：

```
建号 t15b6a id=student---01 code=200（mcp 默认 true，按设计）
建号 t15b6b id=student---02 code=200（mcp 默认 true，按设计）
建号 t15b6c id=student---03 code=200（mcp 默认 true，按设计）
C 先自行改密（使 mcp=0）                HTTP 200  {"code":200,"data":{"must_change_password":false},"msg":"密码修改成功"}
删除前 C 的 /api/user/info             HTTP 200  {"code":200,"data":{"className":"Class 1","name":"CCC","points":0},"msg":"success"}
删除中间用户 B                          HTTP 200  {"code":200,"msg":"用户删除成功"}
删除后 C 的 /api/user/info             HTTP 200  {"code":200,"data":{"className":"Class 1","name":"CCC","points":0},"msg":"success"}
    判据: 删除前 name=CCC ; 删除后 name=CCC ; 两次均 CCC -> True
    users 表中 t15b6b 行数=n=0
```

`/api/user/info`（`routes_public.cpp:248`）正是 `find_user_by_id()` 的消费者：若 `user_id_map` 未随 `erase` 重建，删除后它会返回**另一个用户**的数据。实测两次均为 `CCC` → **不错位**。

**是否误用 `init_indexes()`（`push_back` 污染 `role_permission_map`）**：
- 删除路径调用的是 **`rebuild_user_indexes()`**（`routes_admin.cpp:880`），而 `rebuild_user_indexes()`（`models.cpp:240-242`）只调 `clear_and_rebuild_user_indexes()`，**不触碰** `role_permission_map`；
- `init_indexes()`（`models.cpp:210-222`）在 `push_back` 前先 `role_permission_map.clear()` → 现已幂等（`models.h:105-109` 记为 T21 修复）；
- **运行时污染核查**：删除前后各取一次 `/api/admin/roles`，**响应体逐字节相同**（长度 1375 → 1375，`相同=True`），未见 7→14→21 的累积。
- 按队长指令，**「删除后调用 `init_indexes()`」这一路径由 t24 单独覆盖，不在本任务内**。

---

## 7. 验收项 [7]：N4 `.trae/` 取消跟踪（静态，git 侧）

```
git ls-files .trae/ 命中 = 0
磁盘 .trae 文件数 = 26
git diff --cached 中已暂存删除 = 26
HEAD = 47006bd773f3eb55a25c56228b5a02fec432f2af
git status --short 中 .trae 行数 = 26
```

- 我**自己的计数**：**跟踪 0 / 磁盘 26 / 暂存删除 26 / HEAD `47006bd`**；
- **差异记录**：我的「磁盘 26」与 t9 的「改动前跟踪 26 / 磁盘 26」一致，而与 T1 契约记的「跟踪 25 / 磁盘 26」及审计文档的「25」**不同** → **以 26 = 26 为准**（T1/审计的 25 应视为计数偏差）；
- 未执行 `git commit` / `git push`。

---

## 8. 验收项 [8]：F1 / F16 / F24 / F13 可本机判定部分

### 8.1 可端到端/可机械判定的部分（通过）

| 项 | 命令 | 结果 |
| --- | --- | --- |
| F16 `npm ci` | `Select-String release.yml` | `npm install`=**0** / `npm ci`=**2**（ci.yml=4、pages.yml=1，全仓无 `npm install`） |
| F24 `Content-Length` | `Select-String frontend/src/lib/api.ts -Pattern 'Content-Length'` | **3 命中，全部是注释**（`:82-85` 解释「不再为无体 DELETE 手动设置 Content-Length」）；**无任何可执行代码设置该头** |
| F13 构建产物不暴露固定口令（源码侧） | `Login.vue:181` + `mock/index.ts:789` + `vite.config.ts:14` | 四个「演示账号一键填充」按钮整体包在 `v-if="isMockEnabled()"` 内；`isMockEnabled()` 为**严格 `=== 'true'`**；`vite.config.ts:14` 用 `define` 把 `import.meta.env.VITE_USE_MOCK` 替换为 `JSON.stringify(env.VITE_USE_MOCK \|\| 'false')` → 未设该变量时产物里该表达式恒为 `'false'` |
| F13 文档文案 | `.github/workflows/release.yml:167-170` | 已改为「控制台输出…随机初始口令」「登录后**强制要求先修改口令**」「**不再提供固定默认口令**」，无任何 `admin123/teacher123/…` 文案（grep 0 命中） |
| 前端类型检查 | `npm run typecheck`（`vue-tsc --noEmit`） | **exit 0** |

### 8.2 F1：package job 依赖链（静态核对，通过）

```
ci.yml:
  :98  name: server-exe          （上传侧）
  :110 needs: [frontend, backend]
  :125 actions/download-artifact@v4   （全文件仅 1 处）
  :127 name: server-exe          （下载侧，与 :98 一致）
  :130-132 npm ci
  :137-139 npm run build:no-typecheck（本 job 内现场构建，未设 VITE_USE_MOCK / VITE_BASE）
  :141-158 产物守卫
计数：frontend-dist = 0（两处 artifact 步骤均已删除）；download-artifact = 1
```

**产物守卫的 file:line 与机制**（与队长给的清单一致）：
- `:145` `test -f dist/index.html`；
- `:147-148` `campus_mock_db_v1` → `exit 1`；
- `:150-151` `\[Mock\] 未匹配的 API` → `exit 1`；
- `:154-155` 残留 `/campus-master/` → `exit 1`。

**守卫字面量的保真性核查（我逐字符比对）**：`:150` 的文件原文是 `if grep -rIl "\[Mock\] 未匹配的 API" dist; then`（码点含 `92,91` 与 `92,93`）→ **反斜杠转义的方括号在 grep BRE 中等价于字面 `[` `]`**，故实际匹配串为 `[Mock] 未匹配的 API`；而 mock 侧原文是 `mock/index.ts:807` 的 `` console.warn(`[Mock] 未匹配的 API: ${method} ${url}`) `` → 守卫串是它**逐字的子串**。`campus_mock_db_v1` 亦与 `mock/index.ts:203` 的 `STORAGE_KEY` 逐字一致 → **守卫不会因字面量不匹配而永不触发**。

**反向对照（我自己做，**等价复刻**）**：本机 `grep`/`bash` **均不可用**（`Get-Command grep` 无结果；Git 安装目录下无 `grep.exe`/`bash.exe`），故**无法原样执行**该 bash 守卫块；我按 `ci.yml:144-158` 的语义做了等价复刻（`grep -rIl` → 命中即非 0；`grep -rIo … | head -n 1 | grep -q .` → 出现即非 0），在 `%TEMP%` 造合成 dist：

| 合成 dist | 判定 |
| --- | --- |
| `clean`（无任何特征串） | **EXIT 0** |
| 含 `campus_mock_db_v1` | **EXIT 1**（命中 mock 特征串） |
| 含 `[Mock] 未匹配的 API` | **EXIT 1**（命中 mock 提示串） |
| 含 `/campus-master/` | **EXIT 1**（命中 Pages 子路径） |
| 缺 `dist/index.html` | **EXIT 1** |

### 8.3 未端到端复现项（如实标注）

1. **CI 的 zip 装配链路**（`actions/upload-artifact` → `package` job → zip）：本机无法运行 GitHub Actions，**仅静态核对**（依赖链 + 守卫块 + 计数），未产出真实 zip。
2. **本地 Vite 构建 / 产物字节级校验**：`npm run build:no-typecheck -- --outDir <TEMP>` → **exit 1，`Error: spawn EPERM`**（`esbuild/lib/main.js:1975 ensureServiceIsRunning`，即沙箱禁止 esbuild 起子进程），**未生成 dist**；`--outDir` 指向 `%TEMP%`，仓库内未产生 dist。因此「产物里不含 mock 特征串 / 不含固定口令字面量」**未做字节级证明**，机制由 §8.2 的 CI 守卫 + §8.1 的源码门控承担。此结论与审计文档第 10 节记录的已知沙箱局限一致。
3. 上面 §4 的 B13 两条子路径（`db.execute` false、0 字节备份文件）与 DACL 子路径，见 §4 末尾。

---

## 9. 验收项 [9]：B2/F13 强制改密不可绕过（端到端）

### 9.1 两条中间件都被接入（代码 + 运行时双证）

- `auth.h:346`：`check_permission_middleware()` 首行即 `if (!enforce_password_change(req, res)) return false;`
- `auth.h:395`：`check_parent_auth_middleware()` 首行即 `if (!enforce_password_change(req, res)) return "";`
（`enforce_password_change` 见 `auth.h:327-340`，`must_change_password=true` 时 `403 + {"code":403,"must_change_password":true,…}`）

**运行时实测（mcp=1 会话）**：

```
学生(mcp=1) GET /api/admin/system            HTTP 403  {"code":403,"must_change_password":true,"msg":"首次登录必须修改初始口令后才能使用其他功能"}
学生(mcp=1) GET /api/teacher/students        HTTP 403  同体
学生(mcp=1) GET /api/student/info            HTTP 403  同体
学生(mcp=1) GET /api/user/info               HTTP 403  同体
家长(mcp=1) GET /api/parent/children（parent 中间件） HTTP 403  同体
白名单 GET /api/auth/me                       HTTP 200  {"code":200,"data":{...,"must_change_password":true,...}}
白名单 POST /api/auth/logout（带 CSRF）          HTTP 200  {"code":200,"msg":"退出成功"}
对照 admin(mcp=0) GET /api/admin/system      HTTP 200  {"code":200,"data":{...,"version":"1.0.0"},...}
对照 teacher(mcp=0) GET /api/teacher/students HTTP 200  正常数据
匿名 POST /api/auth/login（错误口令）              HTTP 200  {"code":401,...}   ← 业务码 401、HTTP 200（F2 既有行为，非本项判据）
匿名 GET /api/mall/items                     HTTP 200  正常数据
匿名 GET /api/rank/class                     HTTP 200  正常数据
```

→ **四个角色 + 两条中间件**均被 403 拦住；三个白名单端点放行；匿名公开端点未被误拦（按队长指令**不因「它们没被 403」判失败**）。

### 9.2 4 条代设口令路径（端到端）

| 路径 | 原始结果 |
| --- | --- |
| ① `POST /api/admin/users`（admin 代设口令） | **200** `{"data":{"id":"teacher-002"}}` → 新号 login `must_change_password=**true**` → `GET /api/user/info` **403** |
| ②a `reset-password {auto_generate:true}` | **200** `data.new_password`（12 位随机）→ 目标 login `mcp=**true**` → `/api/user/info` **403**；**[直查库] mcp=1** |
| ②b `reset-password {auto_generate:false,new_password:…}` | **200** → 显式新口令 login `mcp=**true**`；**[直查库] mcp=1** |
| ③ `POST /api/teacher/students`（教师代生成 `initial_password`） | **200** 带 `initial_password` → 学生 login `mcp=**true**` → `/api/student/info` **403** → `change-password`(旧=initial_password) **200** → `/api/student/info` **200**（闭环）；**[直查库] mcp=0** |
| ④ `POST /api/auth/register`（反向判据） | **200** → login `mcp=**false**` → `GET /api/user/info` **200**（**未被误拦**） |

直查库整表（干跑后 9 行）印证设计约定：首启种子 `admin/teacher/student/parent` **mcp 全 = 1**；两个操作员 `mcp=0`；被代设的账号 `mcp=1`；自行改密者与自助注册者 `mcp=0`。

### 9.3 改密闭环（端到端）

```
非法: 错误原口令        HTTP 400  {"code":400,"msg":"原密码错误"}
非法: 新口令<8位        HTTP 400  {"code":400,"msg":"新密码至少 8 位，且必须同时包含字母和数字"}
非法: 不含数字          HTTP 400  同体
非法: 不含字母          HTTP 400  同体
非法: 新旧相同          HTTP 400  {"code":400,"msg":"新密码不能与原密码相同"}
非法: 可预测默认口令 admin123 HTTP 400 {"code":400,"msg":"新密码过于常见/可预测，请更换"}
非法: new==username (t15cuser9) HTTP 400 {"code":400,"msg":"新密码不能与用户名相同"}
合法改密               HTTP 200  {"code":200,"data":{"must_change_password":false},"msg":"密码修改成功"}
改密后 GET /api/student/info（应 200） HTTP 200  正常数据
[直查库] t15cpw mcp=0
新口令重登 code=200 mcp=False
旧口令重登 code=401（应 401）
```

→ **5 类非法输入 400**（实测 7 类，超集）、合法 200、**直查库确认 `must_change_password=0`**、改后业务接口 200、新口令重登 200、旧口令 401。

---

## 10. fail-fast：`database.path` 指向目录 → 退出码 1（端到端）

```
直接调用 server.exe 的 $LASTEXITCODE = 1
--- combined 输出尾部 ---
  [INFO] 配置加载成功: 端口=18099, 数据库=…\t15_run_ff3\db_is_a_directory
  无法打开数据库: unable to open database file
  [ERROR] SQLite 数据库连接失败，拒绝以内存模式启动（安全修复 V18）：…\db_is_a_directory
  [FATAL] 无法打开数据库: …\db_is_a_directory
          已按安全修复 V18 拒绝内存兜底启动（退出码 1），请检查路径/权限/磁盘后重启。
```

源码对应 `main.cpp:214 return 1`（DB 打开失败）与 `main.cpp:438 return 1`（建表失败）。
> 方法论留档：我用 `Start-Process -PassThru` 读 `.ExitCode` 两次都得到空值、用 `cmd /c … %errorlevel%` 得到假 0（`%errorlevel%` 在整行解析期展开）。**正确做法是直接调用 `& .\server.exe` 读 `$LASTEXITCODE`**，结果为 **1**。

---

## 11. 队长追加的三条残余残项：我的独立判定

### 11.1 `execute_bind()` 无法区分 0 行受影响 → **low**（同意归批次 1）
**机制探针（我自写，仓库外）**：

```
[probe] 命中 1 行的 UPDATE : execute_bind=true  ; update() 返回 rows=1
[probe] 命中 0 行的 UPDATE : execute_bind=true  ; update() 返回 rows=0
[probe] 命中 0 行的 DELETE : execute_bind=true
[probe] 判据：execute_bind 对 0 行返回 true 且无法区分 -> 缺陷成立（同一语句 update() 能报 0，execute_bind 报成功）
```

源码：`sqlite_wrapper.h:145-147` 只判 `rc == SQLITE_DONE`；同文件的 `update()`（`:102-108`）**已经**返回 `sqlite3_changes()` → 能力存在但参数化路径未使用。

**端到端可达性实测（我构造的路径）**：建号 → 首次改密（正常落库）→ **外部直接删除该行** → 再次改密：

```
库内 t15b8 行数（删除前）=n=1
库内 t15b8 行数（外部删除后）=n=0
库行已不存在时再次改密   HTTP 200 / code=200（"密码修改成功"）
库内 t15b8 行数（改密后）=n=0
判据: HTTP=200 / code=200 且库中 0 行 -> 「更新未命中却报成功」端到端复现成立
```

**我的判定**：**low**。理由：`routes_public.cpp:527-535` 对 `!saved` 已有回滚分支，但 0 行 UPDATE 的 `saved` 仍为 true，故该分支拦不住；不过触发前提是「内存向量与库不同步」（需外部改库/部分恢复；API 删除用户会同时 `erase` 内存并重建索引），本批无这样的 API 路径。**同意归批次 1 的 B8**，修法即在调用点使用已有的 `update()` 返回码或 `sqlite3_changes()`。

### 11.2 legacy 裸 SHA256 行可绕过强制改密 → **工作区 low / 升级旧库 medium**（同意归批次 1）
- 工作区**无任何 `.db` 文件**（`Get-ChildItem -Recurse -Include *.db,*.sqlite*,… -Exclude vcpkg/node_modules` → 0 个），故本机不存在存量 legacy 库；
- 升级旧库时，迁移语句 `main.cpp:70` 为 `ALTER TABLE users ADD COLUMN must_change_password INTEGER NOT NULL DEFAULT **0**` → 存量行**不会**被强制改密，而 legacy 分支仍可正常登录（弱口令已可离线还原，B2 原像已实测）；
- **便宜的改进建议**：把默认值改为 `DEFAULT 1`，与 T1 决策①「存量一律失效 + 强制重置」的 fail-safe 方向一致（新装库该列恒由代码显式赋值，不受影响）。归批次 1 的 legacy 清理项一并评估。

### 11.3 `verify_password` 无迭代次数下限 → **informational（确认）**
`sha256.h:237` 只判 `iters <= 0` 与上限 `1e7`，接受 `pbkdf2$1$…`（我的计时实验正是利用这一点）。哈希串仅服务端存储：注册/建号/改密/重置均由服务端 `hash_password()` 生成，我全仓核对后**未发现任何外部可写入哈希串的入口** → 维持 informational；若未来出现导入外部哈希串的通道，应补下限（如 `iters >= 100000`）。

---

## 12. 合规、清理与临时件

- **未修改任何业务源码**（`git status` 中的 `M` 文件全部来自 T2/T4/T7/T9/T13/T14/T18/T22 等实现任务；verifier 本任务唯一写入的仓库文件是本文件）；
- **仓库内不新建任何脚本/JSON**；所有 harness、沙箱配置、构建产物、临时库均在 `%TEMP%`：`t15_prep\`（lib/dbq/mkhash/probe_b8/smoke*/runtime_*）、`t15_run_build\`（编译产物）、`t15_run_<port>\`、`t15_n1_*\`、`t15_mock\`、`t15_g1\`、`t15_b8_probe.db`；
- **构建产物零污染**：`server.exe`/`.o` 全部输出到 `%TEMP%\t15_run_build`（未覆盖仓库既有的 `server.exe`/`sqlite3.o`）；`npm build` 的 `--outDir` 指向 `%TEMP%\t15_dist` 且因 EPERM 未生成；
- **测后清理**：删除全部 `t15_run_*`/`t15_n1_*` 沙箱目录（含临时 `.db`/`.log`/备份文件）、`t15_g1` 合成 dist、`t15_mock`、`t15_dist`、`t15_b8_probe.db` 与全部临时 `.js`；确认无残留 `server.exe` 进程、测试端口无监听；仓库 `config.json` 零改动（§5）；
- 未执行 `git commit` / `git push`。

---

## 13. 任务状态：t15 目前**无法 claim**（团队状态缺陷，与验证结论无关）

```
agent_teams_claim_task(t15) → Error: task t15 is blocked by unfinished dependencies: t14 — complete them first
而 team.json: t15.dependencies = [t7, t14, t9] ; t7=completed, t9=completed, t14=cancelled
```

**函数级根因**（读插件源码，非推测）：`@nanmicoder/dsh-agent-teams/lib/state.js:106-109`
```js
export function unsatisfiedDependencies(tasks, dependencies) {
    const byId = new Map(tasks.map((task) => [task.id, task]));
    return dependencies.filter((id) => byId.get(id)?.status !== 'completed');   // 只认 completed
}
```
它**没有**使用插件自己定义的终态集合 `TERMINAL_TASK_STATUSES`（`lib/types.js:11` = `['completed','failed','cancelled']`），因此 `cancelled`/`failed` 依赖恒被判为「未完成」。同一函数亦被 `reassign_task(assignee="captain")` 使用（`lib/tools.js:1239-1242`），故 captain takeover 同样被拦；而 `TASK_TRANSITIONS`（`lib/state.js:114-119`）中 `cancelled` 无出边，无法改回 completed。
**可行修法**：`reassign_task(t15, assignee="verifier")`（目标为成员的分支见 `lib/tools.js:1244-1249`，**不调用** `unsatisfiedDependencies`），或新建等价任务并把 deps 定为 `[t7, t9]`。

本文件即 T15 的完整证据；一旦任务变为可 claim/可 update 状态，verifier 将立即按契约条目顺序落盘 `acceptanceResults` + `commandsRun`（非空）并给出 verdict。

---

## 14. 我自己犯过并已纠正的 harness 错误（留档，避免误读为产品缺陷）

| # | 现象 | 根因（我的脚手架） | 纠正后结论 |
| --- | --- | --- | --- |
| 1 | 探针账号登录/`me` 返回 **500** | 中文 `name` 经 PowerShell argv 编码后成为**非法 UTF-8** 入库，`response.dump()` 抛 `type_error.316`（`routes_public.cpp` 只 catch `json::parse_error`） | 改用 ASCII name → 200/200/403；非产品缺陷 |
| 2 | B13「失败路」先返回 200、再返回 500 | 我预置文件名用的分钟与服务器取时**差一分钟** | 改为「连续两次调用」→ 200 成功 + 500 已存在（同 path）；非产品缺陷 |
| 3 | B6 探针读不到 `name`（两次都 403） | API 建号的 `mcp` 默认 **true**（设计如此），探针未先改密 | 让探针先用 `change-password` 归一化为 `mcp=0` → 两次均读到 `CCC`；非产品缺陷 |
| 4 | fail-fast 退出码读到空 / 假 0 | `Start-Process -PassThru` 的 `.ExitCode` 未取到；`cmd /c … %errorlevel%` 在解析期展开 | 直接 `& .\server.exe` 读 `$LASTEXITCODE` → **1**；非产品缺陷 |
| 5 | N1 首次「Set-Cookie 为空」 | 用了首启**随机**口令登录（必失败→无 cookie），且 `Write-Output` 被函数赋值吞掉 | 改用自造账号 + `Write-Host` → 两模式均观测到 `sid` 且无 `Secure`；非产品缺陷 |
