# 校园能量站 · Vue3 + TypeScript 前端代码质量审计报告（FRONTEND_FINDINGS）

- 审计对象：`D:\邵敬文\comptation\frontend\`（Vue 3.5 + TypeScript 5 + Vite 5 多入口工程）
- 审计基线：`git rev-parse HEAD` = `47006bd773f3eb55a25c56228b5a02fec432f2af`（分支 main，工作区干净：`git status --porcelain` 仅有未跟踪的 `.agent-teams/`、`docs/audit/`）
- 审计立场：**只读**。除本报告外未创建/修改任何受跟踪文件；`package-lock.json` 未被改动（`git status` 无变更），`frontend/dist` 未产出（构建被沙箱阻断，见 1.3）
- 行数口径：`Get-Content | .Count` 与 `read` 工具的 `of N` 一致（例如 `TeacherApp.vue` 1519 行、`AdminApp.vue` 2626 行）

---

## 1. 审计范围与方法

### 1.1 逐行阅读的自研文件（`read` 工具，行号以下文引用为准）

| 文件 | 行数 | 读法 |
| --- | --- | --- |
| `src/lib/api.ts` | 135 | 全文 |
| `src/lib/auth.ts` | 159 | 全文 |
| `src/lib/format.ts` | 54 | 全文 |
| `src/lib/navConfig.ts` | 62 | 全文 |
| `src/lib/theme.ts` | 95 | 全文 |
| `src/mock/index.ts` | 693 | 1-130、160-274、650-693 全文精读 + 全量正则扫描 |
| `src/main.ts` / `src/env.d.ts` / `src/style.css` | 39 / 17 / 120 | 全文 |
| `src/entries/*.ts`（5 个） | 各 4 | 全文 |
| `src/components/`（10 个） | 123+50+139+119+36+117+42+145+66+96 | 全部全文 |
| `src/composables/`（4 个） | 65+62+67+60 | 全部全文 |
| `src/pages/Login.vue` | 291 | 全文 |
| `src/pages/admin/AdminApp.vue` | 2626 | script 段 1-1237 通读（分 4 段），template 段 1245-2499 结构地标全量扫描 + 定点精读（1604-1763、1868-1900 等） |
| `src/pages/teacher/TeacherApp.vue` | 1519 | script 段 1-734 通读，template 段 736-1519 结构地标全量扫描 |
| `src/pages/parent/ParentApp.vue` | 842 | script 段 1-257 通读，template 段 259-842 结构地标全量扫描 |
| `src/pages/student/StudentApp.vue` | 788 | script 段 1-283 通读，template 段 285-788 结构地标全量扫描 |
| 工程配置 | — | `vite.config.ts`(40)、`tsconfig.json`(27)、`package.json`(29)、5 个入口 HTML、`tailwind.config.js`、`postcss.config.js`、`.github/workflows/{ci,pages,release}.yml` |

**按任务约定排除**：`src/assets/fonts/*.woff2`（107 个）与 `src/assets/fonts.css`（113,012 B）只做体积评估，不逐行审计。

**跨边界取证（只读，用于判定前端分支）**：`routes_admin.cpp:130-147`、`routes_static.cpp:58-69`，以及 `.github/workflows/release.yml:164-167`（用于确认演示口令即真实默认口令）。

### 1.2 为判定“运行时/构建期行为”而查阅的第三方与规范资料

- `node_modules/vite` 默认 treeshake 行为（`grep moduleSideEffects`）
- MDN：[Forbidden request header](https://developer.mozilla.org/en-US/docs/Glossary/Forbidden_request_header)、[Headers.set()](https://developer.mozilla.org/en-US/docs/Web/API/Headers/set)（判定 `Content-Length`，见 F24）
- SheetJS `xlsx` 已知漏洞（CVE-2023-30533 / CVE-2024-22363，见 F17）：[Snyk](https://security.snyk.io/package/npm/%252540keep-lts%25252Fxlsx/0.18.6)、[Sonatype CVE-2023-30533](https://guide.sonatype.com/vulnerability/CVE-2023-30533)、[weareu/xlsx 修复分支](https://github.com/weareu/xlsx)

### 1.3 执行过的关键命令与真实输出（原样粘贴）

**① `npm ci`（默认缓存）→ 失败：npm 全局缓存在工作区外，被沙箱拒绝**

```
npm error code EPERM
npm error syscall open
npm error path C:\Users\Administrator\.astrbot_launcher\components\nodejs\.npm_cache\_cacache\tmp\***
npm error errno -4048
```

**② `npm ci --cache <TEMP>` → 失败：npm 生命周期脚本 spawn 子进程被沙箱拒绝（piped stdio）**

```
npm error code EPERM
npm error syscall spawn
npm error errno -4048
```

**③ `npm ci --cache <TEMP> --ignore-scripts` → 成功**

```
added 138 packages in 14s
### exit=0
True / True / True     # node_modules/vue、vue-tsc、@esbuild/win32-x64 均就位
```

**④ `npm run typecheck`（= `vue-tsc --noEmit`，与 ci.yml:37-39 同款）→ 通过**

```
> campus-energy-station-frontend@1.0.0 typecheck
> vue-tsc --noEmit

### exit=0     # 无任何错误输出
```

**⑤ `npm run build:no-typecheck`（= `vite build`）→ 失败（沙箱边界，非代码问题）**

```
failed to load config from D:\邵敬文\comptation\frontend\vite.config.ts
error during build:
Error: spawn EPERM
    at ChildProcess.spawn (node:internal/child_process:441:11)
    at ensureServiceIsRunning (...\node_modules\esbuild\lib\main.js:1975:29)
```

隔离探针确认这是沙箱对“子进程 + pipe”的限制，与 vite/esbuild 无关：

```
### probe1: node spawnSync with piped stdio
error: EPERM | stdout: undefined | status: null
### probe2: node spawn with stdio inherit
99
error: undefined | status: 0
### probe4: esbuild version via node API
Error: spawn EPERM ... at ensureServiceIsRunning (node_modules\esbuild\lib\main.js:1975:29)
```

真实浏览器实测同样被沙箱阻断（Chrome 需要 mojo 命名管道）：

```
ERROR:net\base\network_change_notifier_win.cc:195] WSALookupServiceBegin failed with: 10108
FATAL:mojo\public\cpp\platform\platform_channel.cc:108] Check failed: . : 拒绝访问。 (0x5)
```

→ 因此 **`dist/` 产物无法产出**，第 5.3 节改用“依赖/资源源码体积口径”并明确标注为估算；mock 剔除结论改用**同版本 Rollup 4.62.3 的对照实验**（见 ⑤′）。

**⑤′ mock tree-shaking 对照实验（只读，纯进程内 Rollup，无子进程）**

探针脚本（临时目录，未入库）：模拟 `vite.config.ts:12-15` 的 define 替换 + esbuild 常量折叠，捆绑 `src/lib/api.ts` 子图后检索 mock 特征串。三次对照结果：

```
### VITE_USE_MOCK=false mock=真实 折叠OFF defineOFF
    bundle bytes=37556  模块数=4      HIT  mock 种子数据(三年级一班) / mock csrf 常量(mock-csrf-token)
### VITE_USE_MOCK=false mock=真实 折叠OFF defineON
    bundle bytes=4405   模块数=3      MISS mock 种子数据 / MISS mock-csrf-token / MISS newpass123
### VITE_USE_MOCK=false mock=真实 折叠ON  defineON
    bundle bytes=4405   模块数=3      MISS 全部 mock 特征串
### VITE_USE_MOCK=false mock=空壳对照 折叠ON defineON
    bundle bytes=4405   模块数=3      MISS 全部 mock 特征串
### VITE_USE_MOCK=true  mock=真实 折叠ON  defineON
    bundle bytes=34273  模块数=4      HIT  mock 种子数据 / mock-csrf-token / newpass123
```

**结论：`VITE_USE_MOCK !== 'true'` 时 `src/mock/index.ts`（693 行、含 `mock-csrf-token`、`newpass123`、种子班级/学生数据）被完整 tree-shaking 剔除**（4405 B 与“把 mock 换成空壳”的对照组字节数完全相同，模块数 3 vs 4）。`import.meta.env.VITE_USE_MOCK` 是唯一开关，**没有中间态**。

**⑥ 类型安全水位实验（仅命令行覆盖，不改 `tsconfig.json`；`--noEmit` 不写盘）**

```
### 基线：npm run typecheck                        -> exit=0，0 个错误
### + --noUnusedLocals --noUnusedParameters        -> 7 个错误 / exit=2
src/mock/index.ts(298,97): error TS6133: 'db' is declared but its value is never read.
src/mock/index.ts(298,101): error TS6133: 'body' is declared but its value is never read.
src/mock/index.ts(411,82): ... src/mock/index.ts(439,94): ...
src/pages/teacher/TeacherApp.vue(664,25): error TS6133: 'student' is declared but its value is never read.
### + --noUncheckedIndexedAccess                     -> 19 个错误
按文件：mock/index.ts 6 / StudentApp.vue 6 / AdminApp.vue 5 / ParentApp.vue 1 / TeacherApp.vue 1
错误码：TS2532×12、TS18048×4、TS2322×1、TS2345×1、TS2538×1
示例：AdminApp.vue(648,31): error TS2538: Type 'undefined' cannot be used as an index type.
      AdminApp.vue(350,17): error TS18048: 'd.value' is possibly 'undefined'.
### 三项全开                                      -> 26 个错误（mock 12 / student 6 / admin 5 / teacher 2 / parent 1）
```

**⑦ 依赖与资源体积测量**（`Get-ChildItem -Recurse | Measure-Object Length -Sum`，见 5.3）与 `npm audit` 失败取证：

```
npm warn audit 404 Not Found - POST https://registry.npmmirror.com/-/npm/v1/security/advisories/bulk
npm error [NOT_IMPLEMENTED] /-/npm/v1/security/* not implemented yet
```

**⑧ 死代码/引用面扫描**：`Select-String` 全量符号检索（`usePagination`、`useTheme`、`ROLE_THEME`、`NavSection`、`resetMockDB`、`showToast`、`formatRelative`、`useAuth`、`v-html`、`localStorage` 等），结果见各条发现。

---

## 2. 结论摘要（按严重度排序）

| id | 严重度 | 位置 | 问题一句话 | 修复成本 |
| --- | --- | --- | --- | --- |
| F1 | blocker | `.github/workflows/ci.yml:41-46,114-146` | CI 打包出的“开发版”前端是 Mock 构建，与包内 `server.exe` 永远不通信（发布包功能失效） | S |
| F2 | high | `src/lib/api.ts:103-126` + 4 个页面 62 处 `res.code` 判据 | HTTP 500 带 JSON 体 → 无 toast、无异常、无日志告警，用户零反馈 | S |
| F3 | high | `src/pages/admin/AdminApp.vue:1-2626` | 2626 行单文件（script 1237 + template 1263），54 个 ref / 10 个 computed / 26 个 API 调用点 / 12 个弹窗 / 7 个 tab | L |
| F4 | high | `src/pages/teacher/TeacherApp.vue:161-171,313-321,634-637,664-683` | 1519 行单文件 + 硬编码假评价数据被评价弹窗消费（显示假分数）+ 空转 API + 2 个“开发中”死按钮 | L |
| F5 | high | `src/lib/api.ts:19-24,131-134`；`tsconfig.json:14-16` | 类型安全水位：`ApiResponse<T = any>` + 索引签名 `any` 让响应体全程无约束；关掉的两项编译检查下潜伏 26 个真实错误 | M |
| F6 | medium | `src/composables/usePagination.ts:1-67`（整文件零引用）；`AdminApp.vue:314-337`、`TeacherApp.vue:215-222` | 现成分页 composable 是死代码，三个页面各自手写一遍同样的 6 个派生值 | S |
| F7 | medium | 8 处零引用导出（`useTheme`/`ROLE_THEME`/`NavSection`/`resetMockDB`/`showToast`/`formatRelative` 链/`useAuth`） | “设计了没接线”的死代码，误导后续维护者 | S |
| F8 | medium | `AdminApp.vue`(16 处) + `TeacherApp.vue`(8 处) | `try/catch + console.error + toast.error('网络错误，请稍后重试')` 样板重复 24 次；且四个页面错误处理策略互不一致 | M |
| F9 | medium | `AdminApp.vue:1229-1234`、`TeacherApp.vue:726-732`、`ParentApp.vue:113-114` | 串行请求瀑布（6/7/2 次串行往返）；`StudentApp.vue:274-281` 已有正确的 `Promise.all` 写法可直接对照 | S |
| F10 | medium | `AdminApp.vue:1101-1115` + 模板 `1634,1661,1688,1691` | “系统配置/安全配置/备份配置/立即备份”4 个按钮只弹 toast，不落库、不调接口（假功能） | S |
| F11 | medium | `src/composables/useChart.ts:10`；`AdminApp.vue:15,16`；`main.ts:2-6` | 包体：echarts 全量（`import * as echarts` 阻断 tree-shaking）+ xlsx + pinyin-pro 静态引入；字体 4.72 MB + FontAwesome 999 KB 字体/72 KB CSS | M |
| F12 | medium | `src/components/ConfirmDialog.vue:22-26,32` | 全局确认框的 Esc/Enter 快捷键实际不生效（只绑在未聚焦的 div 上），且无 `role="dialog"`/焦点陷阱 | S |
| F13 | medium | `src/pages/Login.vue:181-208`；`.github/workflows/release.yml:164-167` | 登录页内置**真实默认口令**明文（admin/admin123 等），随所有构建产物分发 | S |
| F14 | medium | `src/lib/api.ts:27-39`、`src/lib/auth.ts:47-67` | csrf_token 与 userInfo 存 localStorage：同源脚本可读；已核查缓解措施（0 处 v-html、无第三方 JS、服务端复验、登出清理） | M |
| F15 | medium | `package.json:6-12`、`.github/workflows/ci.yml:37-46` | 0 个测试文件、无 vitest/eslint/prettier；CI 仅有 typecheck+build，2 626 行核心页面无任何回归网 | L |
| F16 | medium | `.github/workflows/release.yml:51` | 正式发布用 `npm install`（可改写 lockfile、解析出与锁文件不同的版本），与 ci.yml:35 的 `npm ci` 不一致 → 发布不可复现 | S |
| F17 | low | `package.json:18`（xlsx 0.18.5） | 命中 CVE-2023-30533（原型污染）/ CVE-2024-22363（ReDoS），npm 上无修复版；且 `npm audit` 在本环境不可用 | M |
| F18 | low | `tsconfig.json:21-25` | `include` 里的 `*.html` 对 tsc 无意义；`@/*` 别名声明了但全项目 0 处使用 | S |
| F19 | low | `src/pages/Login.vue:13,15` | 同一模块 `../lib/auth` 重复 import 两条语句 | S |
| F20 | low | `src/mock/index.ts:661-675` | 未匹配路由返回 **HTTP 200 + code 404**（掩盖错误）且强制 200-500 ms 随机延迟 | S |
| F21 | low | `src/pages/teacher/TeacherApp.vue:578-587` | 下载 CSV 模板未 `URL.revokeObjectURL`（对比 `AdminApp.vue:1130` 已 revoke） | S |
| F22 | low | 4 处 tab→loader 映射；`ParentApp.vue:226` | `switchTab` 的 if/else 链在四个页面重复 4 份；`ParentApp` 用类型断言绕过联合类型 | M |
| F23 | low | `src/components/BaseModal.vue:55-66`；`src/composables/useChart.ts:50-57` | 12 个弹窗实例各注册一个常驻 window keydown 监听；`body.style.overflow` 在并发弹窗下会被错误解锁；`onActivated/onDeactivated` 在无 `<KeepAlive>` 时是死分支 | S |
| F24 | medium | `src/lib/api.ts:73-76` | `Content-Length` 是 fetch 规范禁止的请求头 → 浏览器静默丢弃，注释声称的“兼容部分反代”从未生效（Node/undici 行为相反，实测保留） | S |

统计：**blocker 1 / high 4 / medium 12 / low 7，共 24 条**。

---

## 3. 逐条发现详情

### F1 —— CI 打出的“开发版”包内前端是 Mock 构建，与包内 `server.exe` 永久失联【blocker】

**位置**：`.github/workflows/ci.yml:41-46`、`.github/workflows/ci.yml:105-146`、`.github/workflows/pages.yml:41-47`、`vite.config.ts:12-15`

**证据**

```yaml
# ci.yml:41-46
      - name: Build (Mock 模式，与 pages.yml 同参)
        working-directory: frontend
        env:
          VITE_USE_MOCK: 'true'
          VITE_BASE: '/campus-master/'
        run: npm run build:no-typecheck
```
```yaml
# ci.yml:114-118 / 126-130（package job：把上面那份 dist 与 server.exe 打在一起）
      - name: Download frontend dist
        uses: actions/download-artifact@v4
        with:
          name: frontend-dist
          path: release/frontend/dist
...
          cp config.json README.md release/
```

**问题**：`VITE_USE_MOCK` 一旦为字符串 `'true'`，`src/lib/api.ts:58-60` 会把**所有** `/api/*` 请求交给 `mockRequest()`，前端只读写 `localStorage['campus_mock_db_v1']`。而 package job 同时塞进 `server.exe` + `config.json`（一个需要真实前端调用的 C++ 服务）。即：**这份“开发版”产物里，后端永远收不到任何请求**。5.5 节的实测进一步证明该开关没有中间态——`'true'` 时 34 个 mock 处理器与全部种子数据进产物，非 `'true'` 时整模块被剔除。

**影响面**：`ci.yml` 的 `package` job（push 到 main 时自动产出的 `campus-energy-station-dev.zip`）。**不影响** `release.yml`（第 53-55 行 `npm run build` 未设 `VITE_USE_MOCK`，走真实后端模式，mock 已被 tree-shaking 剔除），也不影响 GitHub Pages 演示站（`pages.yml` 的 mock 是刻意设计）。

**建议改法**：把 “Mock 演示构建” 与 “可发布构建” 拆成两个步骤/两个产物名：mock 那份只上传到 Pages；package job 改为下载一份 `VITE_BASE=/`、不设 `VITE_USE_MOCK` 的 dist（或直接在 package job 里重新 `npm run build`）。同时给 `vite.config.ts:14` 的默认值加断言（`env.VITE_USE_MOCK === 'true' ? 'true' : 'false'`）以防拼写错误。

**预期收益**：交付包恢复“开箱即用”；消除一类极难排查的“后端日志空无一物”故障。**成本 S。**

---

### F2 —— HTTP 500 完全静默：`api.ts` 只处理 401/403/429，调用方只判 `code === 200`【high】

**位置**：`src/lib/api.ts:94-127`；后端取证 `routes_admin.cpp:137-142`、`routes_static.cpp:60-66`；调用点 `AdminApp.vue:374-381`、`TeacherApp.vue:280-289`、`StudentApp.vue:135-143`

**证据**

```ts
// api.ts:102-127
  if (response.status === 401) { ... return json }        // 清本地态 + 跳登录
  if (response.status === 403) { toast.error(msg); return json }
  if (response.status === 429) { toast.warning(...); return json }
  // 业务码非 200 但 HTTP 200：交由调用方处理，不打断
  return json        // ← 500 / 502 / 404 全部落到这里
```

后端 500 **带 JSON 体**（因此 `response.json()` 成功、不会抛错、也不会有任何 toast）：

```cpp
// routes_admin.cpp:137-142（同形于 routes_student.cpp:205、routes_teacher.cpp:1025）
        } catch (const std::exception& e) {
            Logger::error("商城 API 错误: " + std::string(e.what()));
            json response = {{"code", 500}, {"msg", "服务器内部错误"}};
            res.status = 500;
            res.set_content(response.dump(), "application/json");
        }
```

调用点只判业务码，`catch` 永不触发：

```ts
// AdminApp.vue:374-381
async function loadUsers() {
  try {
    const res = await api.get<User[]>('/api/admin/users')
    if (res.code === 200) users.value = res.data || []   // 500 时：静默保持旧值/空列表
  } catch (e) { console.error('Load users error:', e) }
}
```
```ts
// StudentApp.vue:142 / 176 / 185 / 194 / 203 / 212 —— 注释与事实不符
  } catch { /* api 层已提示 */ }
```

**问题**：全项目 62 处 `res.code === 200/!== 200` 判据（`AdminApp` 26、`TeacherApp` 17、`StudentApp` 8、`ParentApp` 7、`auth.ts` 3、`Login.vue` 1）都建立在“非 200 一定有提示”的假设上，而这个假设只对 401/403/429 + 网络异常成立。真实后果分三种：

1. HTTP 500 + JSON 体（最常见：后端 catch 分支）→ **完全静默**，用户看到列表不刷新、按钮无反应，连 `console.error` 都没有；
2. HTTP 404（`routes_static.cpp:63` 返回 `text/plain`）→ `api.ts:96` 的 `response.json()` 抛错 → toast「服务器响应格式错误」（文案与真实原因无关）；
3. mock 模式未匹配路由 → `mock/index.ts:672-675` 返回 HTTP 200 + `code:404` → **完全静默**。

**影响面**：四个角色页的全部读写操作（登录、用户/班级/角色/权限/商城 CRUD、积分、评价、导入导出）。

**建议改法**：
1. `api.ts` 增加 5xx 兜底分支：`if (!response.ok && response.status >= 500) { toast.error(json?.msg || '服务器异常，请稍后重试'); return json }`（或统一抛 `ApiError`，由调用方 `catch` 一处处理）；
2. 把 62 处 `if (res.code === 200) {...} else { toast.error(res.msg || 'xx失败') }` 收敛为 `requestAction(api.get(...), { onOk, okMsg })` 包装器，让“失败必有提示”成为封装保证而不是每个调用点的自觉；
3. 修正 `StudentApp.vue` 里 6 处 `catch { /* api 层已提示 */ }` 的注释。

**预期收益**：服务器异常从“静默无反应”变成“可见的明确错误”，减少 24 处样板（配合 F8）。**成本 S。**

---

### F3 —— `AdminApp.vue` 2626 行单文件：54 个 ref / 10 个 computed / 26 个 API 调用点 / 12 个弹窗 / 7 个 tab【high】

**位置**：`src/pages/admin/AdminApp.vue:1-2626`（script 1-1237，template 1239-2501，style 2503-2626）

**证据（机械计数，`Select-String -AllMatches` 逐项复核）**

| 指标 | 数量 | 关键行 |
| --- | --- | --- |
| 总行数 | 2626 | script 1237 / template 1263 / style 124 |
| `ref(` | 54 | 状态声明集中在 150-301 |
| `computed(` | 10 | 304-367 |
| `watch(` | 2 | 370-371（均为“搜索词变化→回到第 1 页”） |
| `api.get/post/put/delete` 调用点 | 26 | 376…1178 |
| `BaseModal` 弹窗 | 12 | 1874,1940,2071,2133,2167,2196,2221,2250,2326,2374,2422,2462 |
| `v-show` tab 区块 | 7 | 1252,1367,1460,1514,1557,1604,1766 |
| `<table>` | 8 | 1407,1473,1570,1780,1839（tab 内）+ 1975,2021,2039（批量导入弹窗内） |
| `toast.success/error/warning/info()` 调用 | 75 | 贯穿全文件 |
| `try {` | 27 | — |
| `console.error(` | 25 | — |
| `confirmDialog` | 8 | 533,805,890,996,1076,1167（+2 处 import/注释） |

**问题**：7 个业务域（用户/班级/角色/权限/系统配置/商城/概览）挤在一个 SFC 里，互不相关的状态与函数共享同一作用域：`pageSize = 10` 与 `mallPageSize = 10`（176/264）各自硬编码；用户与商品两套分页派生值各写 4 个 computed（314-337）；12 个弹窗的 `showXxxDialog` 各配一套“打开赋值 / 关店重置”（如 553-606 的 `resetPassword`/`closeResetPasswordDialog` 共 12 行只为 4 个状态）。任何改动都要在 2600 行里重新建立上下文，且**无法对单一 tab 做隔离测试**（配合 F15）。

**建议拆分方案（按收益排序，可作为独立 PR 分批落地）**

1. `src/pages/admin/tabs/AdminDashboard.vue`（1252-1363）——4 张 `StatCard` + 饼图。
2. `src/pages/admin/tabs/AdminUsers.vue`（1367-1456）+ `components/admin/UserFormModal.vue`（1874-1938）、`UserEditModal.vue`（2071-2131）、`ImportUsersModal.vue`（1940-2069，含 xlsx/pinyin 逻辑 616-745）。
3. `src/pages/admin/tabs/AdminClasses.vue`（1460-1510）+ `ClassFormModal.vue`（2326-2420，添加/编辑复用同一组件，用 `mode` prop 区分）。
4. `src/pages/admin/tabs/AdminRoles.vue`（1514-1553）+ 角色弹窗（2133-2165、2196-2219）。
5. `src/pages/admin/tabs/AdminPermissions.vue`（1557-1600）+ 权限弹窗（2167-2194、2221-2248）。
6. `src/pages/admin/tabs/AdminMall.vue`（1766-1868）+ 商品弹窗（2422-2498）。
7. `src/pages/admin/tabs/AdminSystem.vue`（1604-1762，含导入导出 1100-1202）。
8. 抽出 3 个 composable：`useAdminUsers()`（状态 173-195 + 304-327 + 数据加载）、`useAdminMall()`、`useAdminBatchImport()`（616-745）。
9. `AdminApp.vue` 只保留 `checkAuth` + `switchTab` 分发表（1204-1236）+ `<component :is>` 或 `v-show` 分发，预计降到 **250-350 行**。

**预期收益**：单文件从 2626 行降到 ~300 行；每个 tab 组件 150-300 行可独立阅读/测试；`UserFormModal` 复用后可删掉 3 处重复的表单重置代码；配合 F6 再删掉两套手写分页。**成本 L（建议拆 4-6 个 PR）。**

---

### F4 —— `TeacherApp.vue` 1519 行：硬编码假评价数据被 UI 消费 + 空转请求 + 2 个“开发中”死按钮【high】

**位置**：`src/pages/teacher/TeacherApp.vue:161-171`、`313-321`、`634-637`、`664-683`、`1078-1092`

**证据 1：硬编码 demo 数据被评价弹窗读取**

```ts
// TeacherApp.vue:161-171
const evaluationDimensions: EvalDimension[] = [
  { id: 1, name: '德育', ... }, ... 5 项
]
const evaluations = ref<Evaluation[]>([
  { id: 1, studentId: 1, dimensionId: 1, score: 85, comment: '表现良好' },
  { id: 2, studentId: 1, dimensionId: 2, score: 90, comment: '学习认真' },
])   // ← 生产代码里的假数据

// TeacherApp.vue:634-637 —— 评价弹窗靠它回填“已有分数”
function getStudentEvaluation(studentId: number | string, dimensionId: number): number | null {
  const ev = evaluations.value.find(e => e.studentId === studentId && e.dimensionId === dimensionId)
  return ev ? ev.score : null
}
```
而 `evaluations` **永远不会被接口填充**：唯一可能写入它的 `loadEvaluations()` 根本不赋值。

**证据 2：空转 API**

```ts
// TeacherApp.vue:313-321
async function loadEvaluations() {
  // 原始代码调用 /api/teacher/evaluation/dimensions 但赋值到不存在的 this.dimensions，
  // 实际未使用返回值。此处保留 API 调用以维持网络行为。
  try {
    await api.get('/api/teacher/evaluation/dimensions')   // 返回值被丢弃
  } catch (e) { console.error('Load evaluations error:', e) }
}
```
它在 `onMounted`（729）与每次切到评价 tab（705）和提交评价后（651）都被调用 → **纯浪费往返**，并把 `/api/teacher/evaluation/dimensions` 变成事实上的死端点。

**证据 3：未实现但可点的按钮**

```ts
// TeacherApp.vue:664-683
function editEvaluation(student: Student) {          // 664: 'student' 声明未使用
  toast.info('编辑评价功能开发中...')
}
async function deleteEvaluation(student: Student) {
  const ok = await confirmDialog({ message: `确定要删除学生 ${student.name} 的评价吗？`, ... })
  if (!ok) return
  try {
    // 这里需要根据实际的评价ID来调用删除API
    // 暂时使用模拟数据
    toast.info('删除评价功能开发中...')               // 用户确认“删除”后什么也没发生
  } catch (e) { ... }
}
```
模板 `TeacherApp.vue:1078-1092` 有两个真实可点的按钮绑定它们。

**问题**：三处叠加后，教师端的“学生评价”页呈现的是**假数据 + 假操作**：维度分值可能回填出 85/90 的陈旧分数，删除操作确认后静默失败。这类“看起来能用其实是演的”功能比明显的 TODO 更危险（演示/验收时难以察觉）。

**影响面**：教师端「学生评价」tab（`TEACHER_NAV[3]`）。

**建议改法**：
1. 删除 `TeacherApp.vue:168-171` 的硬编码 `evaluations` 初始值，改为 `ref<Evaluation[]>([])`；
2. 若需要回填，让 `loadEvaluations()` 真正请求 `/api/teacher/evaluation?studentId=` 并按学生维度填充；否则删掉 `getStudentEvaluation`，`evaluateStudent`（627-632）已把 `scores` 清空（629）——两者逻辑本来就矛盾；
3. `editEvaluation`/`deleteEvaluation`：在拿到真实评价 ID 之前，直接 `disabled` + `title="暂未开放"`，不要提供会“成功确认但无动作”的路径；
4. 同 F3 的拆分方式，把评价 tab 拆成 `tabs/TeacherEvaluation.vue`，`TeacherApp.vue`（1519 行）目标 ≤ 400 行。

**成本 L。**

---

### F5 —— 真实类型安全水位：`ApiResponse<T = any>` + 索引签名 `any`，且两项编译检查关闭掩盖 26 个错误【high】

**位置**：`src/lib/api.ts:19-24,131-134`；`tsconfig.json:14-16`；`src/mock/index.ts:213,661-665`

**证据**

```ts
// api.ts:19-24
export interface ApiResponse<T = any> {
  code: number
  msg?: string
  data?: T
  [key: string]: any          // ← 任意字段访问都合法，拼写错误不再报错
}
// api.ts:131-134 —— 便捷方法默认 T = any
export const api = {
  get: <T = any>(url: string) => apiRequest<T>('GET', url),
  post: <T = any>(url: string, data?: any) => apiRequest<T>('POST', url, data),
  ...
}
```
```json
// tsconfig.json:14-16
    "strict": true,
    "noUnusedLocals": false,
    "noUnusedParameters": false,
```

**实测水位（1.3 节 ⑥ 的真实输出）**

| 配置 | 错误数 | 关键样例 |
| --- | --- | --- |
| 基线 `npm run typecheck`（CI 门禁同款） | **0** | — |
| `+noUnusedLocals +noUnusedParameters` | **7** | `TeacherApp.vue(664,25): 'student' is declared but its value is never read`；mock/index.ts 6 处未使用形参 |
| `+noUncheckedIndexedAccess` | **19** | `AdminApp.vue(648,31): Type 'undefined' cannot be used as an index type`（`wb.Sheets[wb.SheetNames[0]]`）；`AdminApp.vue(350,17): 'd.value' is possibly 'undefined'` |
| 三项全开 | **26** | mock 12 / student 6 / admin 5 / teacher 2 / parent 1 |

**问题**：
1. **响应体全程无类型约束**。69 个调用点里只有 **34 处**写了显式泛型，**26 处**未标注（`AdminApp` 16、`TeacherApp` 9、`ParentApp` 1），直接落到 `ApiResponse<any>`，例如 `api.get('/api/teacher/evaluation/dimensions')`（TeacherApp:317）、`api.get('/api/admin/export')`（AdminApp:1120）、`api.post('/api/admin/users', payload)`（AdminApp:482）；写操作请求体又有 `payload: Record<string, any>`（AdminApp:473、506）进一步逃逸。写错 `res.data.xxx` 字段名不会有任何提示。
2. 显式 `any` 数量看起来少（实测 `api.ts` **11 处** = `ApiResponse<T = any>`、`[key: string]: any`、`apiRequest<T = any>`、`data?: any`、`api.get/post/put/delete` 的 6 个默认泛型与形参；`mock/index.ts` **3 处** = `Handler` 的 `body: any`、`mockRequest<T = any>`、`data?: any`）**不代表安全**——真正的问题在默认泛型与索引签名，它们让 `any` 变成默认值而非例外。
3. **CI 门禁看不到上面 26 个错误**：`tsconfig.json:15-16` 关掉两项后，`vue-tsc` 只报“类型不匹配”，不报“死变量/可能是 undefined”。`AdminApp.vue:648` 的 `wb.SheetNames[0]` 在空工作簿场景下正是运行期 `TypeError` 的经典来源。

**影响面**：全项目（34 个端点的请求/响应契约、xlsx 解析、图表数据）。

**建议改法**（按投入产出排序）：
1. `ApiResponse<T = unknown>` 并删掉 `[key: string]: any` 索引签名（保留 `msg?: string`）；顺手把默认泛型从 `any` 收紧到 `unknown`，强迫每个调用点写清 `api.get<User[]>`（现成 30 处已是正确写法，可直接照抄）。
2. 新增 `src/types/api.ts`，为 34 个端点各写一份响应 DTO（`AdminUsersRes = { code: 200; data: User[] }`），至少覆盖写操作。
3. `tsconfig.json:15` 改 `noUnusedLocals: true`（先按实验清单修 7 处，成本约 10 分钟——`TeacherApp.vue:664` 改为 `function editEvaluation()` 即可）。
4. `noUncheckedIndexedAccess` 可放到下一批（19 处）。
5. `mock/index.ts:213` 的 `type Handler = (db, body: any, params) => ApiResponse` 改为 `body: unknown` + 各 handler 内窄化。

**预期收益**：编译期即可捕获字段拼写错误与潜在 `undefined`；把 26 个已知隐患变成显式待办。**成本 M。**

---

### F6 —— 现成 `usePagination` 是死代码，三个页面各写一遍分页派生逻辑【medium】

**位置**：`src/composables/usePagination.ts:1-67`（零引用）；`src/pages/admin/AdminApp.vue:314-337`；`src/pages/teacher/TeacherApp.vue:215-222`

**证据**

```
### 全项目符号检索（usePagination）
  Pagination.vue:3   * 分页控件 —— 配合 usePagination composable 使用     ← 仅注释
  Pagination.vue:6   *   const { state, totalPages, changePage } = usePagination()   ← 仅注释
  usePagination.ts:5 *   ...                                              ← 自身文档
  usePagination.ts:16 export function usePagination(options: PaginationOptions = {}) {   ← 定义
（0 处 import）
```
```ts
// AdminApp.vue:314-337 —— 手写第 1 套（用户）
const pagedUsers = computed(() => {
  const start = (currentPage.value - 1) * pageSize
  return filteredUsersList.value.slice(start, start + pageSize)
})
const userTotalPages = computed(() => Math.ceil(filteredUsersList.value.length / pageSize) || 1)
const userStartIndex = computed(() => filteredUsersList.value.length === 0 ? 0 : (currentPage.value - 1) * pageSize + 1)
const userEndIndex = computed(() => Math.min(currentPage.value * pageSize, filteredUsersList.value.length))
// 329-337 —— 手写第 2 套（商城），与上面 4 个 computed 逐行同构，只是换了变量名
```
```ts
// TeacherApp.vue:215-222 —— 手写第 3 套（学生），与上面同构
const filteredStudents = computed(() => { const start = (currentPage.value - 1) * pageSize; ... })
const totalPages = computed(() => Math.ceil(filteredStudentsList.value.length / pageSize) || 1)
const startIndex = computed(() => ...)
const endIndex = computed(() => ...)
```

**问题**：`usePagination.ts` 已实现完全相同的 6 个派生值（`totalPages/hasNext/hasPrev/startIndex/endIndex/setTotal`）且带**越界回退**（43 行：删除最后一页最后一条后自动回退页码）——手写版**没有**这个回退，是实际的行为差异（删光末页后会出现空列表页）。三个页面共复制 12 个 computed。

**建议改法**：`Pagination.vue` 保持纯展示组件（现在已是），三个页面改为 `const { state, totalPages, startIndex, endIndex, changePage, setTotal } = usePagination({ pageSize: 10 })`，列表过滤后调用 `setTotal(filtered.length)`；给 `usePagination` 补一个单元测试（这也是 F15 的起步样例：67 行纯逻辑，测试成本极低）。顺带修掉 `AdminApp.vue:370-371` 两个 watch 直接改 `state.currentPage = 1` 的重复。

**预期收益**：删 12 个 computed ≈ 60 行；获得越界回退行为；`usePagination.ts` 从死代码变成有测试覆盖的公共设施。**成本 S。**

---

### F7 —— 8 处“设计了但从未接线”的导出（死代码）【medium】

**位置与证据（全项目符号检索，排除字体）**

| 符号 | 定义位置 | 引用数 | 说明 |
| --- | --- | --- | --- |
| `usePagination()` | `composables/usePagination.ts:16` | 0 | 见 F6 |
| `useTheme()` | `lib/theme.ts:85` | 0 | 四个页面都直接写 `THEMES.rose/teal/emerald/indigo` |
| `ROLE_THEME` | `lib/theme.ts:90-95` | 0 | 注释写着“用于 AppLayout 自动选色”，但 AppLayout 只接 `theme` prop |
| `NavSection` | `lib/navConfig.ts:18-22` | 0 | 侧边栏无“分区标题”结构 |
| `resetMockDB()` | `mock/index.ts:690-693` | 0 | 注释“开发调试用”，无任何调用入口 |
| `showToast()` | `composables/useToast.ts:50-52` | 0 | 注释“替换 alert() 的最小改动写法”，实际全项目统一用 `toast.*` |
| `formatRelative()` | `lib/format.ts:40-54` | 0（仅 `main.ts:26` 注册） | 连带 `env.d.ts:15` 的 `$formatRelative` 类型声明、`main.ts:13,17,26` 三处注册代码都是死链 |
| `useAuth()` | `lib/auth.ts:157-159` | 0 | 四个页面都直接从 `lib/auth` 具名导入 |

**问题**：`useTheme`/`ROLE_THEME`/`NavSection` 会让读者以为存在“主题/导航自动配置”机制；`formatRelative` 那条链尤其误导——`main.ts:26` 把它注册成全局属性，`env.d.ts:15` 为它写类型，模板里却 0 处使用（全项目 `$formatDateTime`/`$formatDate`/`$formatRelative` 的模板使用数也是 0，页面统一 `import { formatDateTime }` 后局部调用）。

**建议改法**：一次性删除 8 处导出（以及 `main.ts:13,17,26`、`env.d.ts:13-15` 的对应注册与声明，共约 20 行）；`resetMockDB` 若确实要用，就接一个开发态隐藏按钮或 `window.__resetMock`。删除后请把 `noUnusedLocals` 打开（F5），防止再长回来。

**成本 S。**

---

### F8 —— 错误处理样板 24 处重复 + 四个页面策略互不一致【medium】

**位置**：`AdminApp.vue`（16 处）、`TeacherApp.vue`（8 处）、`ParentApp.vue`、`StudentApp.vue`

**证据**

```
### toast.error('网络错误，请稍后重试') 出现次数
src\pages\admin\AdminApp.vue             16
src\pages\teacher\TeacherApp.vue          8
### console.error( 出现次数
src\pages\admin\AdminApp.vue             25
src\pages\teacher\TeacherApp.vue         18
src\pages\parent\ParentApp.vue            7
src\mock\index.ts                         1
### try { 出现次数（全项目 68）
admin 27 / teacher 18 / student 8 / parent 7 / api 2 / auth 2 / mock 2 / login 2
```
```ts
// AdminApp.vue:494-497（同形样板 ×16）
  } catch (e) {
    console.error('Add user error:', e)
    toast.error('网络错误，请稍后重试')
  }
```
```ts
// 而 ParentApp.vue:131-134 是另一种写法：catch 里 console.error + 语义化 toast
  } catch (e) {
    console.error('加载子女信息失败:', e)
    toast.error('网络错误，无法获取子女信息')
  }
```
```ts
// StudentApp.vue:142 又是第三种：完全静默，依赖“api 层已提示”
  } catch { /* api 层已提示 */ }
```

**问题**：同一件事（一次请求失败）有四种处理风格；每种风格在 4 个页面各复制一遍（全项目 68 个 `try {`）。改动 toast 文案或接入错误上报要改 68 个块。

**建议改法**：在 `lib/api.ts` 增加 `export async function callApi<T>(p: Promise<ApiResponse<T>>, opts?: { okMsg?: string; errMsg?: string }): Promise<T | null>`，内部统一 `try/catch + code 判定 + toast`（同时解决 F2）；页面侧降级为 `const users = await callApi(api.get<User[]>('/api/admin/users'))`。分页/tab 的 `console.error` 只保留在 `api` 层一处。

**预期收益**：删掉 24 处 catch 样板（约 100 行），并让 62 处业务码判据的“失败必有提示”由封装保证。**成本 M。**

---

### F9 —— 首屏串行请求瀑布：6/7/2 次串行往返；同项目已有正确写法【medium】

**位置**：`AdminApp.vue:1225-1236`、`TeacherApp.vue:722-733`、`ParentApp.vue:113-114`；反例 `StudentApp.vue:274-281`

**证据**

```ts
// AdminApp.vue:1225-1236 —— 6 个 await 顺序执行
onMounted(async () => {
  const user = await checkAuth(1)
  if (!user) return
  userInfo.value = { ...user }
  await loadUsers(); await loadRoles(); await loadPermissions()
  await loadDashboard(); await loadProducts(); await loadExchangeRecords()
  nextTick(() => dashboardChartRef.value?.resize())
})
```
```ts
// TeacherApp.vue:726-732 —— 7 个 await 顺序执行
  await loadMyClasses(); await loadStudents(); await loadPointsRecords(); await loadEvaluations()
  await loadDashboard(); await loadRecentActivities(); await loadParentMessages()
```
```ts
// StudentApp.vue:274-281 —— 同一项目里的正确写法（6 个并发）
  await Promise.all([fetchUserInfo(), fetchHistory(), fetchMallItems(), fetchLeaderboard(), fetchEvaluation(), fetchRedemptions()])
```
（`TeacherApp.vue:336-339`、`StudentApp.vue:147-150`、`StudentApp.vue:236` 也已经是 `Promise.all`。）

**问题**：假设单次 RTT 100 ms，管理员首屏 = `checkAuth` + 6 × 100 ms ≈ 700 ms 纯等待（本机部署可忽略，校园局域网/远程部署会成倍放大）；教师端 7 次。除 `checkAuth` 必须最先执行（决定是否跳转）外，其余请求互不依赖，完全可以并发。`ParentApp.vue:113-114` 的 `await loadChildInfo(); await loadActiveTabData()` 同理。

**建议改法**：`await Promise.all([loadUsers(), loadRoles(), ...])`（保留 `checkAuth` 在最前）。若后端并发能力有限（见后端审计报告的连接/事务问题），至少把“当前 tab 必需”的请求优先。

**预期收益**：首屏等待从 O(n) 降到 O(1)。**成本 S。**

---

### F10 —— “系统配置”tab 的 4 个按钮是纯 toast 假实现【medium】

**位置**：`src/pages/admin/AdminApp.vue:1100-1115`（函数）与模板 `1634`、`1661`、`1688`、`1691`

**证据**

```ts
// AdminApp.vue:1100-1115
function saveSystemConfig() { toast.success('系统配置保存成功！') }
function saveSecurityConfig() { toast.success('安全配置保存成功！') }
function saveBackupConfig() { toast.success('备份配置保存成功！') }
function backupNow() { toast.info('备份已开始，请稍后查看备份结果') }
```
```html
<!-- 模板 1622-1630：这三个输入框绑定的 systemConfig 只活在前端 ref 里 -->
<input v-model="systemConfig.systemName" ...>
<select v-model="backupConfig.frequency" ...>
...
<button type="button" @click="saveSystemConfig" ...>保存配置</button>   <!-- 1634 -->
<button type="button" @click="backupNow" ...> 立即备份 </button>        <!-- 1688 -->
```

**问题**：`systemConfig`/`securityConfig`/`backupConfig`（239-252）三个 ref 既没有 `loadXxx` 读取、也没有保存接口（该文件 26 个调用点里没有 `/api/admin/config`），刷新即丢失；「立即备份」提示“已开始”但后端无对应调用。管理员会以为配置生效了。

**建议改法**：要么接上后端（`GET/PUT /api/admin/config`、`POST /api/admin/backup`），要么把这个 tab 标注为“暂未开放”并禁用按钮（`disabled` + `title`），与 F4 的第 3 条同一处理原则。

**成本 S（禁用路线）/ M（接线路线）。**

---

### F11 —— 包体：echarts 全量 + xlsx + pinyin-pro 静态引入，字体/FontAwesome 资源约 5.8 MB【medium】

**位置**：`src/composables/useChart.ts:10`；`src/pages/admin/AdminApp.vue:15,16`；`src/main.ts:2-6`；`index.html` 等 5 个入口

**证据（命令实测，见 1.3 ⑦）**

```
### echarts
echarts.min.js        1009.9 KB      # 全量
echarts.esm.js        2967.4 KB
### xlsx
xlsx.js                815.8 KB      # 仅 AdminApp 的批量导入用
### pinyin-pro
index.js               315.9 KB      # 仅 AdminApp 生成账号用
### 字体（src/assets/fonts）
Noto Sans SC   101 个子集  4410.5 KB
Fraunces         6 个子集   314.0 KB      → 合计 107 文件 4724.4 KB
fonts.css                  113.0 KB      # 107 条 @font-face，全局 import
### FontAwesome (@fortawesome/fontawesome-free)
all.min.css                 72.2 KB
webfonts（4×woff2 + 4×ttf）999.0 KB      # all.min.css 同时引用 .ttf 与 .woff2
### 实际用到的图标类名（去重） = 81
```
```ts
// useChart.ts:10 —— 全量命名空间导入，阻断 echarts 的 tree-shaking
import * as echarts from 'echarts'
// AdminApp.vue:15-16 —— 只在批量导入（631-649）与账号生成（670-677）时用，却是静态 import
import * as XLSX from 'xlsx'
import { pinyin } from 'pinyin-pro'
```

**问题**：
1. `echarts.min.js` 1.0 MB（gzip 约 340 KB）；项目只用了 pie/bar/line/radar 四种图（`AdminApp.vue:340-367`、`TeacherApp.vue:225-273`、`StudentApp.vue:101-132`）。ECharts 官方给出 `echarts/core` + `use([...])` 的按需方案（`node_modules/echarts/core.js` 存在），而 `import * as echarts from 'echarts'` 把全部图表类型都留在包里。`TeacherApp.vue:12`、`AdminApp.vue:14`、`StudentApp.vue:10` 的 `import type { EChartsOption }` 是类型导入，不产生运行时体积——真正的元凶是 `useChart.ts:10`，而它被 `BaseChart.vue:24` 引入，即**所有含图表的页面**。
2. 管理员页额外背 815 KB（xlsx）+ 316 KB（pinyin-pro），只在“批量导入/重置密码”用；`import()` 动态引入即可从首屏拆出。
3. 资源面：4.72 MB 字体 + 999 KB FontAwesome 字体全部进产物；`all.min.css` 同时 `url()` 了 `.ttf` 与 `.woff2` 两种格式（Vite 会原样发两份）。字体确实用了 `unicode-range`（107 条齐全），浏览器按需拉取，所以**运行时开销可控，但部署产物（zip/Pages 站点）会多出约 5.8 MB**，CI artifact 与 Pages 部署带宽都按这个量级计费。
4. 81 个图标换来 72 KB CSS + 999 KB 字体（含 4 个 `.ttf` 冗余格式）。已逐一核对：81 个类名在 FA6 免费版 `all.min.css` 中均存在（无“幽灵图标”）。

**建议改法**：
1. `useChart.ts` 改为按需注册：
   ```ts
   import * as echarts from 'echarts/core'
   import { PieChart, BarChart, LineChart, RadarChart } from 'echarts/charts'
   import { TooltipComponent, LegendComponent, GridComponent, RadarComponent } from 'echarts/components'
   import { CanvasRenderer } from 'echarts/renderers'
   echarts.use([PieChart, BarChart, LineChart, RadarChart, TooltipComponent, LegendComponent, GridComponent, RadarComponent, CanvasRenderer])
   ```
2. `AdminApp.vue` 把 `import * as XLSX` 与 `pinyin` 改为 `const XLSX = await import('xlsx')` / `const { pinyin } = await import('pinyin-pro')`（两处使用点都是异步函数内，改动很小）。
3. 字体：只保留实际用到的字重区间；`Noto Sans SC` 的 101 个子集若目标是校园内网，可考虑按需裁剪或在部署文档里说明首次中文渲染会按需拉取 ~4.4 MB。FontAwesome 可换成按需 SVG（或删掉 `.ttf` 的 `@font-face` 分支）。
4. 在 CI 里加体积门槛：`vite build` 后用脚本断言 `dist/**/*.js` 总量与单个 chunk 上限（这样即使没有测试也能防住回归）。

**预期收益**：首屏 JS 预计减少 1-2 MB（echarts 按需 + xlsx/pinyin 拆出），产物整体缩小约 1 MB（去掉 ttf）。**成本 M。**

---

### F12 —— 全局确认框的 Esc/Enter 实际不生效，且缺 dialog 语义【medium】

**位置**：`src/components/ConfirmDialog.vue:22-26,32`

**证据**

```html
<!-- ConfirmDialog.vue:32 —— keydown 只绑在这个 div 上 -->
<div v-if="state.visible" class="modal-backdrop" @click.self="answer(false)" @keydown="onKey" tabindex="0">
```
```ts
// ConfirmDialog.vue:22-26
function onKey(e: KeyboardEvent) {
  if (!state.visible) return
  if (e.key === 'Escape') answer(false)
  if (e.key === 'Enter') answer(true)
}
```
对照 `BaseModal.vue:62-66` 的正确写法（全局监听）：

```ts
onMounted(() => window.addEventListener('keydown', onKey))
onBeforeUnmount(() => { window.removeEventListener('keydown', onKey); document.body.style.overflow = '' })
```

**问题**：`ConfirmDialog.vue` 全文件没有 `window.addEventListener`（全项目检索 `window.addEventListener` 只有 `BaseModal.vue:62`、`useChart.ts:47,51`）。弹框出现时没有任何代码把焦点移进去（虽写了 `tabindex="0"`），`keydown` 事件的 target 是 `document.body`，不会冒泡到该 div → **Esc/Enter 基本不会触发**（除非用户恰好 Tab 进这个 div，而此时焦点又落在按钮上）。另外缺少 `role="dialog"`/`aria-modal="true"`/`aria-labelledby`，也没有焦点陷阱——`BaseModal.vue:73-112` 同样缺（只有 `ToastContainer.vue:27` 写了 `aria-live="polite"`、`ToastContainer.vue:34` 写了 `role="alert"`）。

**影响面**：全项目 9 处 `confirmDialog` 真实调用点（`AdminApp` 6 处 533/805/890/996/1076/1167、`TeacherApp` 2 处 524/669、`StudentApp` 1 处 225）的键盘可用性；屏幕阅读器用户无法识别弹框（`ConfirmDialog.vue` 与 `BaseModal.vue` 均无 `role="dialog"`/`aria-modal`）。

**建议改法**：把 `onKey` 挂到 `window`（`watch(state.visible)` 内 add/remove），弹框打开时聚焦确认按钮，关闭后把焦点还给触发元素；补 `role="dialog" aria-modal="true" :aria-labelledby`；`BaseModal.vue` 同样处理。可顺带修 F23 的 body overflow 竞态（用一个全局 overlay 计数替代直接赋值）。

**成本 S。**

---

### F13 —— 登录页明文内置**真实默认口令**，随所有构建产物分发【medium】

**位置**：`src/pages/Login.vue:181-208`；交叉证据 `.github/workflows/release.yml:164-167`、`.github/workflows/pages.yml:73-77`

**证据**

```html
<!-- Login.vue:179-195 -->
<p class="text-center text-xs ...">演示账号 · 一键填充</p>
  <button type="button" @click="fillTestAccount('admin', 'admin123')" ...>管理员</button>
  <button type="button" @click="fillTestAccount('teacher', 'teacher123')" ...>教师</button>
  <button type="button" @click="fillTestAccount('student', 'student123')" ...>学生</button>
  <button type="button" @click="fillTestAccount('parent', 'parent123')" ...>家长</button>
```
```yaml
# release.yml:164-167（正式 Release notes）
          ## 默认账号
          - 管理员：`admin` / `admin123`
          - 教师：`teacher` / `teacher123`
          - 学生：`student` / `student123`
```

**问题**：这不是“假数据”，而是产品**真实默认凭据**（正式 Release 文档确认）。通过 `index.html`（登录入口）进入的任何人都能一键拿到管理员口令——如果部署后没有人改密码（校园项目常见），就是完全开放的管理后台。口令以明文字面量进了所有 bundle（含生产构建，与 mock 无关，因此**不受 tree-shaking 保护**）。

**影响面**：所有入口的登录页（`index.html` → `src/entries/login.ts` → `Login.vue`）。

**建议改法**（三选一，按代价递增）：
1. 用 `import.meta.env.VITE_SHOW_DEMO_ACCOUNTS === 'true'` 包住整块（181-208 与 `fillTestAccount`），仅在 Pages 演示构建里开；真实构建直接不出现在 DOM 与 JS 里；
2. 保留按钮但不带密码（只填用户名），或从后端 demo 接口取；
3. 首次登录强制改密（后端 + 前端流程），并在部署文档里删除默认口令段落。

**成本 S。**

---

### F14 —— csrf_token 与 userInfo 存 localStorage 的风险评估（含缓解事实）【medium】

**位置**：`src/lib/api.ts:27-39`、`src/lib/auth.ts:47-67`、`src/lib/api.ts:105-110`

**证据**

```ts
// api.ts:27-39
function getCsrfToken(): string { return localStorage.getItem('csrf_token') || '' }
export function setCsrfToken(token: string) { if (token) localStorage.setItem('csrf_token', token) }
export function clearCsrfToken() { localStorage.removeItem('csrf_token') }
```
```ts
// auth.ts:59-67 / 125-151
function saveUserInfo(info: UserInfo) { Object.assign(userInfo, info); localStorage.setItem('userInfo', JSON.stringify(info)) }
...
export async function checkAuth(roleId: number) {
  const local = loadFromStorage()                 // 127：仅用于“无本地态直接跳登录”的快速短路
  if (!local) { location.href = ... }
  const res = await apiRequest<UserInfo>('GET', '/api/auth/me')   // 134：真正的鉴权来源
  if (res.code !== 200 || !res.data) { clearUserInfo(); clearCsrfToken(); location.href = ... }
  saveUserInfo(res.data)                          // 143：以服务端数据覆盖本地（防篡改）
```

**问题**：本地存储的 `csrf_token` 是双提交模式的前端副本，任何同源脚本（XSS/供应链）都能读取并携带它发起写请求，这削弱了 CSRF 防护的“攻击者读不到 token”前提。`userInfo` 存 localStorage 则带来“本地态可被伪造”的假象（虽然 `checkAuth` 会以 `/api/auth/me` 的结果覆盖）。

**已核实的缓解事实（因此定级 medium 而非 high）**：
- 全项目 `v-html`/`innerHTML`/`eval`/`new Function` 使用数 **0**（`grep` 全量确认，含 `KeepAlive`=0 的同批扫描），Vue 默认转义生效；
- 5 个 HTML 入口无任何第三方 `<script>`（内联 script 只有 `type="module"` 本地入口），FontAwesome/字体均已本地化；
- `checkAuth` 不信任 localStorage 的角色信息，只用服务端返回值判定与跳转（`auth.ts:134-151`）；
- 登出与 401 都会清理（`auth.ts:113-114`、`api.ts:105-106`）。

**建议改法**：`csrf_token` 改用 `sessionStorage`（或干脆只依赖 HttpOnly cookie + `X-CSRF-Token` 由后端在 `/api/auth/me` 下发内存态），至少不要跨标签页长期驻留；`userInfo` 只保留“非敏感的显示用字段”，并统一加 `sessionStorage`。若 C++ 端能支持，最稳妥是双提交 + `SameSite=Strict`（属后端范围，见后端报告）。

**成本 M。**

---

### F15 —— 0 测试 / 0 lint：2626 行核心页面无任何回归网【medium】

**位置**：`package.json:6-12`、`.github/workflows/ci.yml:37-46`

**证据**

```
### scripts
"dev": "vite", "build": "vue-tsc --noEmit && vite build",
"build:no-typecheck": "vite build", "typecheck": "vue-tsc --noEmit", "preview": "vite preview"
### 仓库内不存在任何测试/lint 配置（全仓搜索，排除 node_modules/vcpkg）
.eslintrc* / eslint.config.* / .prettierrc* / prettier.config.* / vitest.config.* / jest.config.*
*.spec.ts / *.spec.js / *.test.ts / *.test.js   →  0 个命中
```
```yaml
# ci.yml:37-46：CI 的全部前端把关就是 typecheck + build
      - name: Typecheck (vue-tsc)
        run: npm run typecheck
      - name: Build (Mock 模式，与 pages.yml 同参)
        run: npm run build:no-typecheck
```

**问题**：`dependencies` 里没有 vitest/eslint/prettier，也没有 `test`/`lint` 脚本；CI 无 lint、无单测。而 `AdminApp.vue` 2626 行里承载 26 个 API 调用与 12 个表单弹窗，`TeacherApp.vue` 1519 行里已存在“硬编码假数据被 UI 消费”这类只靠肉眼能发现的缺陷（F4）。同时 `ci.yml:46` 用 `build:no-typecheck` —— 由于第 37 行已单独跑过 typecheck，这条**不构成漏洞**，但意味着“只要有一步顺序被改动，构建就不再拦类型错误”。

**建议改法**（最小起步）：
1. 加 `vitest` + `@vue/test-utils`，先测 **3 个纯逻辑单元**：`lib/api.ts` 的 `apiRequest` 四分支（401/403/429/500，可用 `vi.stubGlobal('fetch')`）、`lib/auth.ts` 的 `checkAuth` 跳转矩阵、`composables/usePagination.ts`（F6）。约 150 行测试就能覆盖最易回归的路径。
2. 加 `eslint` + `@typescript-eslint` + `eslint-plugin-vue` 与 `prettier`，CI 增加 `npm run lint` 步骤；这条能立刻把 F7/F19 这类问题变成红灯。
3. `tsconfig.json:15-16` 打开（配合 F5），并在 CI 里保持 `npm run typecheck` 与 build 两步都在。

**预期收益**：把“只能人工点”的关键页面纳入可回归范围；lint 一步即可自动拦住本轮 24 条里约 1/3 的机械问题。**成本 L（但可分批，第一批 S）。**

---

### F16 —— 正式发布用 `npm install`（可改写 lockfile），与 CI 的 `npm ci` 不一致【medium】

**位置**：`.github/workflows/release.yml:49-55`（对照 `.github/workflows/ci.yml:33-35`）

**证据**

```yaml
# release.yml:49-55
      - name: Install frontend dependencies
        working-directory: frontend
        run: npm install          # ← 会按 package.json 的 ^ 范围解析，必要时改写 package-lock.json
      - name: Build frontend
        working-directory: frontend
        run: npm run build
```
```yaml
# ci.yml:33-35
      - name: Install dependencies
        working-directory: frontend
        run: npm ci
```

**问题**：`package.json` 里全是 `^` 范围（`vue: ^3.4.27`、`vite: ^5.2.11`、`vite: ^5.2.11`…），本机 `npm ci` 实际装到 `vue 3.5.40 / vite 5.4.21 / typescript 5.9.3 / vue-tsc 2.2.12`。`npm install` 在 tag 构建时会重新解析范围，可能出现“CI 通过、Release 装到别的版本”的情况，且会把 `package-lock.json` 改动带入工作区。发布可复现性被破坏。

**建议改法**：`release.yml:51` 改为 `npm ci`（并在需要时显式 `npm ci --no-audit --no-fund`）。若要允许升级依赖，应通过单独的 Dependabot/手动 PR 修改 lockfile，而不是让发布流水线自己解析。

**成本 S。**

---

### F17 —— `xlsx@0.18.5` 命中已知漏洞，且本环境无法跑 `npm audit`【low】

**位置**：`package.json:18`；实测 `node -e "require('xlsx/package.json').version"` → `0.18.5`

**证据**

```
xlsx 0.18.5 | homepage: https://sheetjs.com/
### npm audit（生产依赖）
npm warn audit 404 Not Found - POST https://registry.npmmirror.com/-/npm/v1/security/advisories/bulk
npm error [NOT_IMPLEMENTED] /-/npm/v1/security/* not implemented yet
### exit=1
```

**问题**：`xlsx@0.18.5` 是 npm registry 上 SheetJS 的最后一版，`CVE-2023-30533`（原型污染）与 `CVE-2024-22363`（ReDoS）在其后版本才修复，而修复版只发布在 SheetJS 自有 CDN（社区有 0.18.5 的修复分支）。当前用法是解析**管理员手动上传的 xlsx**（`AdminApp.vue:638-667`），攻击面局限在管理员本人，但属于“已知不可修”的依赖债。同时因为 registry 指向 `registry.npmmirror.com`（未实现 advisories 接口），CI 里也无法用 `npm audit` 自动发现。

**建议改法**：
1. 优先换成 `exceljs`（活跃维护、npm 上可升级）或锁定到 SheetJS 官方 CDN 的修复版并记录校验和；
2. 若保留：在 `AdminApp.vue:647` 的 `XLSX.read` 前加文件大小/行数上限（现无任何上限），并在 README 里记录该风险；
3. CI 增加依赖检查（`npm audit --registry=https://registry.npmjs.org` 或 `osv-scanner`/Dependabot），摆脱对镜像安全接口的依赖。

**成本 M。**

---

### F18 —— `tsconfig.json` 的 `include` 含 `*.html`、`paths` 别名零使用【low】

**位置**：`tsconfig.json:20-25`

**证据**

```json
    "baseUrl": ".",
    "paths": { "@/*": ["src/*"] }
  },
  "include": ["src/**/*.ts", "src/**/*.d.ts", "src/**/*.vue", "*.html"],
```

**问题**：`*.html` 对 `vue-tsc --noEmit` 没有实质作用（HTML 不是 TS 编译单元，只有被 `src/**/*.ts` 引用时才参与）；`@/*` 别名在 `vite.config.ts:19-27` 的 `rollupOptions.input` 之外**没有任何一处使用**——全项目 import 全是相对路径（如 `../../components/AppLayout.vue`，四个页面各 2-3 层 `../../`）。别名声明了却不用，会让新加入者以为存在统一路径规范。

**建议改法**：删掉 `include` 里的 `"*.html"`；要么在 `vite.config.ts` 补 `resolve.alias` 并统一迁移到 `@/components/...`（更耐重构，但改动面大），要么删掉 `paths` 只留相对路径。二选一，不要维持现状。

**成本 S。**

---

### F19 —— `Login.vue` 同一模块重复 import【low】

**位置**：`src/pages/Login.vue:12-15`

**证据**

```ts
import { ref, onMounted } from 'vue'
import { login } from '../lib/auth'
import { apiRequest } from '../lib/api'
import { getRoleHome } from '../lib/auth'      // ← 与第 13 行重复
```

**问题**：两条 `from '../lib/auth'` 语句（`src` 内唯一的重复 import；全项目其余 import 均单行）。功能无影响，但属于 lint 可自动拦下的机械问题（配合 F15）。

**建议改法**：合并为 `import { login, getRoleHome } from '../lib/auth'`。**成本 S。**

---

### F20 —— Mock 未匹配路由返回 HTTP 200 + `code:404`，并强制 200-500 ms 随机延迟【low】

**位置**：`src/mock/index.ts:661-687`

**证据**

```ts
// index.ts:666-675
  // 模拟网络延迟，让 Demo 更真实
  await new Promise(resolve => setTimeout(resolve, 200 + Math.random() * 300))

  const db = loadDB()
  const matched = matchRoute(method.toUpperCase(), url.split('?')[0])
  if (!matched) {
    console.warn(`[Mock] 未匹配的 API: ${method} ${url}`)
    return { code: 404, msg: `[Mock] 接口未实现: ${method} ${url}` }
  }
```

**问题**：真实后端对未知路径返回 HTTP 404（`routes_static.cpp:63`），mock 却返回 HTTP 200 + 业务码 404。于是同一个前端代码在两种模式下走**不同**分支：真实模式下 `api.ts:96` 的 `response.json()` 抛错 → toast；mock 模式下静默（F2 情形 3）。这会让 demo 掩盖掉真实的 404 处理缺陷。另外每次请求都强制 200-500 ms 延迟（对一个纯 localStorage 读取而言），demo 交互无谓变慢。

**建议改法**：`mockRequest` 返回体保留 `{code, msg}`，但让调用方按 `code` 判断就够了——更好的做法是让 mock 与真实后端**语义一致**（未匹配时抛错或返回一个统一的错误对象，由 `api.ts` 同一分支处理）；延迟降到 50-100 ms，或仅在 `import.meta.env.DEV` 下启用。

**成本 S。**

---

### F21 —— 下载 CSV 模板未释放 ObjectURL【low】

**位置**：`src/pages/teacher/TeacherApp.vue:578-587`

**证据**

```ts
// TeacherApp.vue:578-587
function downloadCsvTemplate() {
  const csvContent = '学号,姓名,班级,初始密码\n2024001,测试学生,高二(1)班,123456\n'
  const blob = new Blob(['\ufeff' + csvContent], { type: 'text/csv;charset=utf-8;' })
  const link = document.createElement('a')
  link.href = URL.createObjectURL(blob)        // ← 无 revokeObjectURL
  link.download = '学生导入模板.csv'
  document.body.appendChild(link); link.click(); document.body.removeChild(link)
}
```
对照 `AdminApp.vue:1117-1131` 的正确写法（`URL.revokeObjectURL(url)` 在 1130 行）。

**问题**：每次点击泄漏一个 blob URL（页面生命周期内不释放）。量级很小，但这是同一项目里已有的正确模式没被复用。

**建议改法**：抽取 `lib/download.ts` 的 `downloadBlob(blob, filename)`（内部 `revokeObjectURL`），`TeacherApp.vue:578-587`、`AdminApp.vue:1122-1130` 与 `TeacherApp.vue:625-636`（xlsx `writeFile`）统一调用。**成本 S。**

---

### F22 —— 四份重复的 tab→loader 映射；`ParentApp` 用类型断言绕过联合类型【low】

**位置**：`AdminApp.vue:1204-1222`、`TeacherApp.vue:690-719`、`StudentApp.vue:243-267`、`ParentApp.vue:220-241`、`ParentApp.vue:226`

**证据**

```ts
// 四处的共同形状（AdminApp:1205-1221）
function switchTab(tab: string) {
  if (tab === '__logout__') { logout(); return }
  currentTab.value = tab
  if (tab === 'dashboard') { loadDashboard(); nextTick(() => dashboardChartRef.value?.resize()) }
  else if (tab === 'users') { loadUsers() }
  else if (tab === 'classes') { loadClasses() }
  else if (tab === 'mall') { loadProducts(); loadExchangeRecords() }
}
```
```ts
// ParentApp.vue:221-241 —— 同一模式，但拆成两个函数 + 断言
function switchTab(tab: string) {
  if (tab === '__logout__') { logout(); return }
  currentTab.value = tab as typeof currentTab.value     // ← 226：用断言绕过 'points'|'evaluation'|'redemptions'|'messages' 联合类型
  loadActiveTabData()
}
function loadActiveTabData() { if (!currentChildId.value) return; if (currentTab.value === 'points') ... }
```

**问题**：tab 名（`navConfig.ts` 里已有一份 `NavItem.key`）与加载函数的关系在四个页面各写一遍，共 4 份 `if/else` 链；新增一个 tab 要改 3 处（`navConfig`、`switchTab`链、模板 `v-show`）。`ParentApp.vue:226` 的断言让非法 tab 名（例如 `'admin'`）静默通过类型检查。

**建议改法**：统一用映射表 + 类型守卫：
```ts
const loaders: Record<AdminTab, () => void> = { dashboard: ..., users: ..., classes: ..., mall: ... }
function switchTab(tab: string) {
  if (tab === '__logout__') return void logout()
  if (!(tab in loaders)) return
  currentTab.value = tab as AdminTab
  loaders[tab]()   // + 每个 loader 内部自己 nextTick(resize)
}
```
`ParentApp` 的联合类型可用 `const PARENT_TABS = ['points','evaluation','redemptions','messages'] as const` 导出复用。

**成本 M。**

---

### F23 —— 弹窗的全局监听器与 body overflow 竞态；`useChart` 的死分支【low】

**位置**：`src/components/BaseModal.vue:42-66`；`src/composables/useChart.ts:46-62`

**证据**

```ts
// BaseModal.vue:55-66
watch(() => props.show, (v) => { document.body.style.overflow = v ? 'hidden' : '' })
onMounted(() => window.addEventListener('keydown', onKey))     // ← 每个实例一个常驻监听器
onBeforeUnmount(() => { window.removeEventListener('keydown', onKey); document.body.style.overflow = '' })
```
```ts
// useChart.ts:46-57
onMounted(() => { window.addEventListener('resize', onWindowResize) })
onActivated(() => { window.addEventListener('resize', onWindowResize); resize() })
onDeactivated(() => { window.removeEventListener('resize', onWindowResize) })
```

**问题**：
1. `AdminApp.vue` 挂 12 个 `BaseModal`、`TeacherApp.vue` 挂 5 个 → **17 个常驻 `window` keydown 监听器**，每次按键触发 17 次回调（回调里只读 `props.show`）。功能上无害，但属于可省的开销与内存持有。
2. `document.body.style.overflow` 用“直接赋值”而非计数：若有 2 个弹窗同时打开（例如批量导入结果 + 编辑弹窗，或后续新增），其中一个关闭时会把 `''` 写回，导致背景滚动在另一个弹窗仍打开时被解锁；`onBeforeUnmount`（65）也会无条件解锁。
3. `useChart.ts:50-57` 的 `onActivated/onDeactivated` 只在 `<KeepAlive>` 内才有意义，而全项目 0 处 `KeepAlive`（检索确认）→ 是永不执行的分支，同时 `onActivated` 里重复 `addEventListener` 的写法在 KeepAlive 场景下还会重复注册。

**建议改法**：把弹窗的 keydown/overflow 收敛到 `main.ts` 已挂载的全局 overlay 层（一个 `overlayCount` + 一个 `keydown`），`BaseModal.vue` 只上报开关；`useChart.ts` 删除 `onActivated/onDeactivated`（或保留但补上 KeepAlive 场景的对称性注释）。

**成本 S。**

---

### F24 —— `Content-Length: 0` 是 fetch 禁止的请求头：浏览器静默丢弃，注释声称的兼容性从未生效【medium】

**位置**：`src/lib/api.ts:70-76`

**证据（源码）**

```ts
// api.ts:70-76
  if (data !== undefined && data !== null) {
    headers['Content-Type'] = 'application/json'
    options.body = JSON.stringify(data)
  } else if (method === 'DELETE') {
    // 无请求体的 DELETE 需 Content-Length:0，兼容部分反代
    headers['Content-Length'] = '0'
  }
```

**证据（规范/文档，双重确认）**

- MDN《Forbidden request header》：该类头“**cannot be set or modified programmatically in a request**”，并明确把 `Content-Length` 列在禁止清单中（"the user agent retains full control over them"，示例中 `fetch` 设置 `Date` 头**不报错也不生效**）。见 [MDN Forbidden request header](https://developer.mozilla.org/en-US/docs/Glossary/Forbidden_request_header)。
- MDN《Headers.set()》：“**For security reasons, some headers can only be controlled by the user agent. These headers include the forbidden request headers**” —— 即通过 `Headers`/`RequestInit` 设置该类头是被忽略的路径，而非抛 `TypeError` 的路径。见 [MDN Headers.set()](https://developer.mozilla.org/en-US/docs/Web/API/Headers/set)。
- 因此实际后果是**静默忽略**：不会抛 `TypeError`、不会有控制台警告（`Headers` 的 request guard 对 forbidden name 是直接 return）。也就是说这行代码**在浏览器里永远不产生该请求头**，注释里的“兼容部分反代”既无依据也无效果。

**证据（本机实测，注意环境差异）**

```
### node fetch 侧（undici 24.16.0）行为
node -e "const r=new Request('http://127.0.0.1:1/x',{method:'DELETE',headers:{'Content-Length':'0','X-CSRF-Token':'t'}});
         console.log('content-length ->', JSON.stringify(r.headers.get('content-length')));"
content-length -> "0"          # Node/undici 保留（与浏览器相反）
x-csrf-token -> "t"
```

**未能完成的部分（如实记录）**：本机 Chrome 无法在沙箱内启动（`FATAL:mojo\public\cpp\platform\platform_channel.cc:108 Check failed: . : 拒绝访问。(0x5)`，见 1.3 ⑤），因此**没有拿到“真实浏览器 + 服务端回显”的端到端证据**；上述“静默忽略、无警告”的结论来自 MDN/规范，以及 Node/undici 与规范不一致这一旁证。建议在真实浏览器里用 DevTools Network 面板确认一次（操作步骤见第 6 节）。

**受影响的调用点（4 处无体 DELETE，走这条分支）**：`AdminApp.vue:812`（`/api/admin/roles?id=`）、`AdminApp.vue:897`、`AdminApp.vue:1003`、`AdminApp.vue:1083`。带体的 DELETE（`AdminApp.vue:540`、`TeacherApp.vue:531`）走 `data !== undefined` 分支，本来就不受影响。这 4 个请求的**安全关键头是 `X-CSRF-Token`（api.ts:79-82）**，它不在禁止清单里，实测在 Node 侧也正常保留。

**建议改法**：删掉 `api.ts:73-76` 整段（浏览器会自动按 body 计算 `Content-Length`，无体 DELETE 本来就不需要该头），并在注释里去掉无法复现的“兼容部分反代”说法；若确实遇到某个反代拒收无 `Content-Length` 的 DELETE，应记录具体的反代/版本/复现步骤，改用后端或反代侧配置解决，而不是在前端写一个无效头。

**成本 S。**

---

## 4. 做得好的地方（均有源码依据）

1. **统一请求封装与 CSRF 注入**：所有 34 个端点都经 `api.get/post/put/delete`（`api.ts:130-135`）→ `apiRequest`（`api.ts:52-127`），写方法统一注入 `X-CSRF-Token`（79-82）+ `credentials: 'include'`（64），没有一处裸 `fetch`（全项目检索 `api.` 之外无 `fetch(` 调用）。
2. **多入口配置干净**：`vite.config.ts:19-27` 显式声明 5 个 HTML 入口，`base` 走 `VITE_BASE`（11 行）兼容 GitHub Pages 子路径；`index.html`/`admin.html`/… 每个都是 13 行的最小壳，只引一个 entry（`src/entries/*.ts` 各 4 行 + `main.ts:20-39` 的 `mountApp` 工厂），新增入口的成本很低。
3. **UI 组件抽取到位**：`AppLayout`/`Sidebar`/`BaseModal`/`Pagination`/`StatCard`/`EmptyState`/`RoleBadge`/`BaseChart`/`ToastContainer`/`ConfirmDialog` 十个组件**全部被真实引用**（`AppLayout→Sidebar` 组合见 `AppLayout.vue:14,54-68`；`EmptyState` 被四个页面共用 14 次；`BaseModal` 复用 17 次），并且 `Sidebar` 用 `NavItem[]` + `#extra-section` 插槽（`Sidebar.vue:27-40,77`）覆盖了 4 个角色的差异，没有为每个角色复制侧边栏。
4. **配置驱动的导航与主题**：`lib/navConfig.ts:25-62` 集中定义 4 套 `NavItem`，`lib/theme.ts:32-83` 集中 5 套主题 class（`THEMES.rose/teal/emerald/indigo/amber`），页面里没有散落的色值硬编码（虽然 `ROLE_THEME` 未使用，见 F7）。
5. **`alert`/`confirm` 已彻底替换**：`toast`（`useToast.ts:41-47`）与 `confirmDialog`（`useConfirm.ts:40-50`）是模块级 Promise 化设计，`main.ts:31-36` 用一个独立 app 挂到 `body`，跨页面共享；全项目检索 **0 处 `window.alert(` / `window.confirm(`**。
6. **认证流程比多数项目严谨**：`auth.ts:125-154` 的 `checkAuth` 不信任 localStorage，先用 `/api/auth/me` 验证会话、再用服务端数据覆盖本地（143）、做角色校验并按角色跳转；`loadFromStorage` 有 `try/catch`（52-56）容忍脏数据。
7. **Mock 与真实后端同构**：`mock/index.ts:213-231` 用 `{method, pattern, paramNames, handler}` 路由表 + 正则参数（与后端 34 个端点一一对应），返回同一个 `ApiResponse` 形状，`api.ts:58-60` 单点开关；并且**默认不启用**（`vite.config.ts:14` 缺省 `'false'`），生产真实构建里整模块被 tree-shaking 剔除（见 5.5 实测）。
8. **XSS 面极小**：全项目 `v-html`/`innerHTML` = 0；`style.css:85` 的内联 SVG 噪点纹理用 data-URI 且无用户输入；用户内容全部走 `{{ }}` 插值。
9. **表格/空态/加载态的处理一致**：`EmptyState` 的三种 variant（`EmptyState.vue:6-16`）在 14 处使用，`colspan` 可按表格列数传入（31 行），比内联 `v-if="list.length===0"` 更统一。
10. **toast 可访问性**：`ToastContainer.vue:27,34` 带 `aria-live="polite"` 与 `role="alert"`（对比弹窗缺失 dialog 语义，见 F12）。

---

## 5. 量化指标附表

### 5.1 每个文件的规模与结构（`Get-Content` 行数 / `read` 工具 `of N` 双向核对）

| 文件 | 总行数 | script 段 | template 段 | style 段 |
| --- | --- | --- | --- | --- |
| `pages/admin/AdminApp.vue` | **2626** | 1237 | 1263 | 124 |
| `pages/teacher/TeacherApp.vue` | **1519** | 734 | 691 | 92 |
| `pages/parent/ParentApp.vue` | 842 | 257 | 409 | 174 |
| `pages/student/StudentApp.vue` | 788 | 283 | 313 | 190 |
| `mock/index.ts` | 693 | — | — | — |
| `pages/Login.vue` | 291 | 59 | 158 | 72 |
| `lib/auth.ts` | 159 | — | — | — |
| `components/Sidebar.vue` | 145 | 53 | 75 | 15 |
| `components/BaseModal.vue` | 139 | 71 | 40 | 26 |
| `lib/api.ts` | 135 | — | — | — |
| `components/AppLayout.vue` | 123 | 43 | 79 | 0 |
| `style.css` | 120 | — | — | 120 |
| `components/ConfirmDialog.vue` | 119 | 27 | 23 | 67 |
| `components/Pagination.vue` | 117 | 47 | 37 | 31 |
| `components/ToastContainer.vue` | 96 | 23 | 20 | 51 |
| `lib/theme.ts` | 95 | — | — | — |
| `composables/usePagination.ts` | 67 | — | — | — |
| `components/StatCard.vue` | 66 | 33 | 20 | 11 |
| `composables/useChart.ts` | 65 | — | — | — |
| `lib/navConfig.ts` / `composables/useConfirm.ts` | 62 / 62 | — | — | — |
| `composables/useToast.ts` | 60 | — | — | — |
| `lib/format.ts` | 54 | — | — | — |
| `components/BaseChart.vue` | 50 | 46 | 3 | 0 |
| `components/RoleBadge.vue` | 42 | 26 | 3 | 11 |
| `main.ts` / `components/EmptyState.vue` | 39 / 36 | — / 17 | — / 18 | — / 0 |
| `env.d.ts` | 17 | — | — | — |
| `entries/{admin,login,parent,student,teacher}.ts` | 各 4 | — | — | — |
| **合计（32 个 TS/Vue 文件，不含 fonts）** | **8527 行 / 350,150 B** | | | |
| `assets/fonts.css`（生成物，不计入逐行审计） | 113,012 B | | | |

### 5.2 API 调用点与端点

| 文件 | `api.get/post/put/delete` 调用点 | 说明 |
| --- | --- | --- |
| `pages/admin/AdminApp.vue` | **26** | 7 个 GET 加载 + 19 个写操作 |
| `pages/teacher/TeacherApp.vue` | **18** | 含 `TeacherApp.vue:317` 的空转 GET（F4） |
| `pages/student/StudentApp.vue` | 8 | 6 个首屏并发 + 兑换 + 复用 |
| `pages/parent/ParentApp.vue` | 7 | 子女/信息/积分/评价/兑换/留言 |
| `lib/auth.ts` | 3+1 | login/parent-login/logout + `checkAuth` |
| `lib/api.ts` | 5 | 便捷方法自身实现（F5） |
| `pages/Login.vue` | 1 | `/api/auth/me` 自动跳转 |
| **合计** | **69** | 覆盖 **34 个去重端点** |
| `res.code === 200 / !== 200` 判据 | **62 处** | 分布：admin 26 / teacher 17 / student 8 / parent 7 / auth 3 / login 1 |
| `try {` 块 | **68 处** | admin 27 / teacher 18 / student 8 / parent 7 / api 2 / auth 2 / mock 2 / login 2 |
| `console.error(` | **51 处** | admin 25 / teacher 18 / parent 7 / mock 1 |

### 5.3 依赖与静态资源体积（源码口径；`dist` 因沙箱无法产出，故非构建实测）

| 资源 | 体积 | 使用面 |
| --- | --- | --- |
| `echarts`（全量，`useChart.ts:10` 命名空间导入） | 源 `echarts.min.js` **1009.9 KB** | 三个角色页的图表 |
| `xlsx`（`AdminApp.vue:15` 静态引入） | `xlsx.js` **815.8 KB** | 仅管理员批量导入/模板下载 |
| `pinyin-pro`（`AdminApp.vue:16`） | `index.js` **315.9 KB** | 仅管理员生成学生账号 |
| `src/assets/fonts/*.woff2`（107 个子集） | **4724.4 KB**（Noto Sans SC 101 个 = 4410.5 KB；Fraunces 6 个 = 314.0 KB） | 全局（`main.ts:4`） |
| `src/assets/fonts.css` | **113.0 KB**（107 条 `@font-face`，全部带 `unicode-range` 与 `font-display: swap`） | 全局（5 个入口都引 `main.ts`） |
| `@fortawesome/fontawesome-free/css/all.min.css` | **72.2 KB** | 全局（`main.ts:6`） |
| FontAwesome `webfonts/`（4×woff2 + 4×ttf，CSS 同时引用两种格式） | **999.0 KB** | 全局（实际用到 81 个图标类名，均存在于 FA6 免费版） |
| 产物静态资源小计（字体+FA） | **≈ 5.9 MB** | 不含 JS/CSS |

### 5.4 类型检查结果

| 命令（均在 `frontend/` 下执行） | 结果 |
| --- | --- |
| `npm run typecheck`（= `vue-tsc --noEmit`，CI 门禁同款） | **exit 0，0 错误** |
| `npx vue-tsc --noEmit --noUnusedLocals --noUnusedParameters` | exit 2，**7 错误**（mock 6 + `TeacherApp.vue:664`） |
| `npx vue-tsc --noEmit --noUncheckedIndexedAccess` | **19 错误**（`AdminApp` 5 / `StudentApp` 6 / `mock` 6 / `parent` 1 / `teacher` 1） |
| 三项全开 | **26 错误**（mock 12 / student 6 / admin 5 / teacher 2 / parent 1） |
| `npm run build:no-typecheck` | **失败**：`Error: spawn EPERM`（沙箱阻断 esbuild 子进程，非代码问题） |
| `npm audit --omit=dev` | **失败**：registry `npmmirror.com` 未实现 advisories 接口（404 NOT_IMPLEMENTED） |
| 工具版本（`npm ci` 实际安装） | vue 3.5.40 / vite 5.4.21 / typescript 5.9.3 / vue-tsc 2.2.12 / rollup 4.62.3 / node 24.16.0 / npm 11.13.0 |

### 5.5 mock 模块 tree-shaking 实测矩阵（Rollup 4.62.3 进程内对照实验）

| 实验条件 | 产物字节 | 模块数 | `mock-csrf-token`/种子数据/`newpass123` |
| --- | --- | --- | --- |
| `VITE_USE_MOCK` 未定义替换（模拟无 `define`） | 37,556 | 4 | **HIT**（全部进入） |
| `VITE_USE_MOCK='false'`（define 生效） | **4,405** | **3** | **MISS（整模块剔除）** |
| `VITE_USE_MOCK='false'` + 空壳对照 | 4,405 | 3 | MISS（与上者字节数完全相同 → 剔除是完备的） |
| `VITE_USE_MOCK='true'` | 34,273 | 4 | **HIT（全部进入）** |

**判读**：
- 结论（推翻“假密码/假 token 一定进生产包”的假设）：**`release.yml` 的正式发布构建（未设 `VITE_USE_MOCK`）不含 mock 模块**；`mock-csrf-token`、`newpass123`、种子学生/班级数据均不会出现在该产物中。
- 但 **`ci.yml:44` 与 `pages.yml:45` 显式设 `VITE_USE_MOCK='true'`**，这两份产物（GitHub Pages 站点、以及被 package job 塞进 `server.exe` 的 `frontend-dist`）会完整包含 mock 层，体积代价约 **+29.9 KB（未压缩，仅 api 子图口径）** → 见 F1。
- 该实验的边界：以 `api.ts → mock/index.ts` 子图为准复现 Rollup 的 tree-shaking 决策，**不是 `vite build` 的完整产物**；若需要 100% 结论，必须在能 spawn 子进程的环境重跑 `npm run build`（见第 6 节）。

---

## 6. 未验证 / 存疑事项（如实标注）

1. **`dist` 产物未能生成**：`vite build` 与真实浏览器实测都被沙箱阻断（`spawn EPERM`；Chrome `mojo platform_channel 拒绝访问`）。因此 5.3 的体积是“依赖/资源源码口径”而非构建产物实测，mock 剔除结论来自由同版本 Rollup 复现的对照实验（5.5）。**需要在无沙箱环境复跑**：`cd frontend && npm ci && npm run build:no-typecheck`，然后
   `Select-String -Path dist/assets/*.js -Pattern 'mock-csrf-token|三年级一班|newpass123'`（预期 0 命中）与
   `VITE_USE_MOCK=true npm run build:no-typecheck`（预期命中），并记录 `dist/` 总体积。
2. **`Content-Length` 的浏览器端行为缺少端到端实测**：MDN/规范支持“静默忽略、无 TypeError、无警告”，Node/undici 24.16.0 实测反而不丢弃该头。建议在真实浏览器里确认一次：DevTools → Network → 触发一次“删除班级”（`AdminApp.vue:1003`）→ 查看该 DELETE 请求的 Request Headers 是否包含 `content-length`（预期不包含），同时观察 Console 是否出现任何提示（预期无）。
3. **后端是否真的依赖该头**：本次只读了 `routes_admin.cpp:130-147` 与 `routes_static.cpp:58-69`，没有全面核对 httplib 对无 `Content-Length` 的 DELETE 的处理。F24 的建议（删除该行）需与后端审计结论交叉确认；但即便保留，浏览器里也依然不生效。
4. **默认口令是否与后端种子一致**：F13 依据 `release.yml:164-167` 的 Release Notes（明示 `admin/admin123` 等）与 `models.cpp:16` 的“默认用户数据”注释判定，**未复算哈希**。后端审计报告（`BACKEND_FINDINGS.md` B2/B5 段）已独立得到同类结论，F13 以后端结论为准更稳妥。
5. **`/api/teacher/evaluation/dimensions` 是否还有其他消费者**：F4 只证明前端丢弃返回值，未核对后端是否有其他调用方（不影响前端删除该调用的建议）。
6. **`npm audit` 的环境限制**：registry 指向 `registry.npmmirror.com`，advisories 接口 404，因此本轮**没有**运行时依赖漏洞扫描结果，F17 来自公开漏洞库而非本机扫描。
7. **`node_modules` 是由 `--ignore-scripts` 安装的**：138 个包齐全、`vue-tsc` 与 `npm run typecheck` 正常，唯一潜在差异是跳过了各包的 postinstall（esbuild 的平台二进制已由 `@esbuild/win32-x64` 提供，`probe3` 确认 `esbuild.exe` 存在）。这可能与 `vite build` 失败有关，但 `probe4` 证明失败点是 `spawn`（沙箱）而非缺文件。

---

## 7. 建议的改进优先级与批次划分

### 批次 0（立即，合计约 1 人日，能消掉 1 blocker + 3 high/medium）

| # | 动作 | 对应发现 | 成本 |
| --- | --- | --- | --- |
| 1 | `ci.yml` 的 package job 改用非 mock 构建（或重新 build）；mock 构建只给 Pages | F1 | S |
| 2 | `api.ts` 增加 5xx 兜底分支 + 统一 `code !== 200` 提示，修正 `StudentApp` 的误导注释 | F2 | S |
| 3 | 删除 `api.ts:73-76` 的 `Content-Length` 无效代码 | F24 | S |
| 4 | 删除 `TeacherApp.vue:168-171` 硬编码假评价数据、`editEvaluation/deleteEvaluation` 改为禁用态、删掉空转的 `loadEvaluations()` 请求 | F4 | S |
| 5 | `release.yml:51` 改 `npm ci` | F16 | S |
| 6 | 清理 8 处零引用导出（F7）+ `Login.vue:13,15` 重复 import（F19） | F7/F19 | S |

### 批次 1（本迭代，约 3-5 人日，聚焦“看得见的正确性”）

| # | 动作 | 对应 | 成本 |
| --- | --- | --- | --- |
| 7 | `AdminApp.vue:1101-1115` 系统配置 4 个按钮改为禁用态或接线 | F10 | S |
| 8 | `AdminApp/TeacherApp/ParentApp` 的 `onMounted` 改 `Promise.all` | F9 | S |
| 9 | 抽 `callApi()` 包装，收敛 24 处错误样板（与 #2 合并做） | F8 | M |
| 10 | `ConfirmDialog` 键盘/焦点/dialog 语义（+ 顺带修 F23 的 overflow 竞态与监听器） | F12/F23 | S |
| 11 | 登录页演示账号块加 `import.meta.env` 开关 | F13 | S |
| 12 | `usePagination` 接回 3 个页面（并补首个单测） | F6 | S |
| 13 | `tsconfig.json` 打开 `noUnusedLocals`（修 7 处） | F5 | S |
| 14 | `TeacherApp.vue:578-587` 抽 `lib/download.ts` 并 revoke | F21 | S |
| 15 | `tsconfig.json` 清理 `include`/`paths` | F18 | S |

### 批次 2（下迭代，结构性改造，约 8-12 人日）

| # | 动作 | 对应 | 成本 |
| --- | --- | --- | --- |
| 16 | **AdminApp 拆分**：拆 7 个 tab 组件 + 2 个表单弹窗组件 + 3 个 composable，主文件降到 ≤ 350 行（按 F3 的 9 步分 4-6 个 PR） | F3 | L |
| 17 | **TeacherApp 拆分**：按同样方式拆 7 个 tab，主文件 ≤ 400 行 | F4 | L |
| 18 | 类型加固：`ApiResponse<T = unknown>`、去索引签名、补 34 个端点 DTO、`noUncheckedIndexedAccess`（19 处） | F5 | M |
| 19 | 引入 vitest + @vue/test-utils，先覆盖 `api`/`auth`/`usePagination` 三个纯逻辑模块（约 150 行测试） | F15 | M |
| 20 | 引入 eslint + prettier 并接进 CI | F15 | M |
| 21 | 包体优化：echarts 按需注册、xlsx/pinyin 动态 import、字体与 FA 精简 | F11 | M |
| 22 | tab→loader 映射表统一（4 个页面） | F22 | M |
| 23 | 依赖治理：xlsx 迁移或风险记录 + CI 依赖扫描 | F17 | M |

### 批次 3（可选，随部署环境决定）

| # | 动作 | 对应 |
| --- | --- | --- |
| 24 | `csrf_token` 改 sessionStorage / 后端支持 `SameSite=Strict` 后只依赖 cookie | F14 |
| 25 | mock 与真实后端语义对齐（未匹配路由的错误传播方式）+ 移除无谓延迟 | F20 |
| 26 | 弹窗/侧边栏的 a11y 补全（`role="dialog"`、焦点陷阱、`aria-*`） | F12 |

---

## 附：本报告未改动任何源码的证明

```
$ cd D:\邵敬文\comptation && git status --porcelain
?? .agent-teams/
?? docs/audit/
```
（`frontend/node_modules/` 与 `frontend/dist/` 均在 `.gitignore` 中；`package-lock.json` 无变更；`frontend/dist` 因构建失败未生成。）
