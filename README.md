# 校园文明能量站

> 基于 C++ + Vue 3 的校园文明积分管理系统，通过积分激励机制促进学生文明行为。

---

## 目录

1. [项目简介](#1-项目简介)
2. [技术栈](#2-技术栈)
3. [架构设计](#3-架构设计)
4. [目录结构](#4-目录结构)
5. [构建指南](#5-构建指南)
6. [配置说明](#6-配置说明)
7. [API 接口文档](#7-api-接口文档)
8. [数据库设计](#8-数据库设计)
9. [默认账号](#9-默认账号)
10. [部署指南](#10-部署指南)
11. [测试说明](#11-测试说明)

---

## 1. 项目简介

**系统名称**：校园文明能量站

**用途**：基于 Web 的校园管理系统，通过积分激励机制促进学生文明行为。系统将学生的日常行为（学习、纪律、卫生、活动等）量化为积分，配合教师评价、商城兑换等闭环设计，形成"行为→积分→激励"的正向反馈。

**核心功能**：

- **用户管理**：管理员/教师/学生/家长四类账号，支持 CRUD、批量导入、密码重置
- **积分管理**：教师对学生进行加分/扣分操作，完整记录历史流水
- **评价管理**：多维度学生评价（学习、纪律、品德等），支持增删改查
- **商城兑换**：积分兑换实物奖品，库存管理、兑换记录追溯
- **家长端**：绑定孩子，查看积分/评价/兑换记录，家校留言沟通
- **数据统计**：班级排名、学生排名、维度统计、可视化图表（ECharts）
- **班级管理**：班级信息维护、班主任绑定、年级分组

**四角色权限**：

| 角色 | role_id | 主要职责 |
|------|---------|----------|
| 管理员 | 1 | 系统配置、用户/角色/权限管理、商城管理、数据导入导出 |
| 教师 | 2 | 学生管理、积分操作、学生评价、班级管理、家校留言、数据统计查看 |
| 学生 | 3 | 查看个人信息、积分记录、评价结果、商城兑换 |
| 家长 | 4 | 绑定孩子，查看孩子积分/评价/兑换记录，商城兑换，家校留言 |

---

## 2. 技术栈

| 层级 | 技术选型 | 说明 |
|------|----------|------|
| **后端** | C++11 + cpp-httplib | 单头文件 HTTP 服务器，轻量高效 |
| **数据库** | SQLite3 | 持久化存储，数据落盘到 `campus_system.db` |
| **前端** | Vue 3 SFC + TypeScript | Vite 多入口构建，5 个独立 SPA（登录/管理员/教师/学生/家长） |
| **样式** | Tailwind CSS | 实用优先的 CSS 框架（npm + PostCSS 构建） |
| **图表** | ECharts | 数据可视化（班级排名、维度统计等） |
| **字体** | Fraunces + Noto Sans SC | 衬线展示字体 + 中文正文字体，已本地化 |
| **安全** | PBKDF2 + Cookie 会话 + CSRF + RBAC | 密码哈希（10 万次迭代加盐）、HttpOnly Cookie 会话、CSRF 双提交校验、登录锁定、基于角色的权限控制 |
| **JSON** | nlohmann/json | 单头文件 JSON 解析库 |

---

## 3. 架构设计

### 3.1 后端模块化结构

| 文件 | 职责 |
|------|------|
| `config.h` | `ServerConfig` 结构体定义 + `load_config()` 配置加载函数（从 `config.json` 读取） |
| `logger.h` / `logger.cpp` | `Logger` 类，支持日志级别（info/warning/error）+ 日志轮转（按大小/数量） |
| `sha256.h` | SHA256 哈希算法实现 + 密码哈希工具（`hash_password()`/`verify_password()`：PBKDF2-SHA256 十万次迭代 + 随机盐，自动识别兼容旧版 SHA-256 哈希） |
| `models.h` / `models.cpp` | `User`/`Role`/`Permission`/`RolePermission`/`PointsRecord` 结构体 + 全局数据容器 + 索引优化（`find_user_by_id`/`find_user_by_username`/`check_permission_optimized`） |
| `auth.h` | CORS 头设置 + 请求/响应日志 + Cookie 会话管理（HttpOnly/SameSite/Secure）+ CSRF 双提交校验 + `check_permission_middleware` 权限中间件 |
| `routes.h` | 6 个路由注册函数声明 |
| `routes_static.cpp` | 静态文件服务（frontend/dist/ 的 HTML 与 hashed 资源） |
| `routes_public.cpp` | 公共 API（登录/注册/登出/用户信息/行为历史/商城/排名） |
| `routes_admin.cpp` | 管理员 API（31 个端点） |
| `routes_teacher.cpp` | 教师 API（19 个端点） |
| `routes_student.cpp` | 学生 API（5 个端点） |
| `routes_parent.cpp` | 家长 API（9 个端点） |
| `sqlite_wrapper.h` / `database.h` | SQLite3 C API 的 C++ 封装（`SqliteDb`）+ 数据库辅助定义 |
| `main.cpp` | 入口文件：配置加载 → 日志初始化 → 数据库初始化 → 路由注册 → 索引初始化 → 线程池 → 启动服务器 |

### 3.2 前端架构

基于 **Vite + Vue 3 SFC + TypeScript** 工程化架构，源码位于 `frontend/`，构建产物输出到 `frontend/dist/`，由 C++ 后端静态服务托管。

- **5 个独立入口 HTML**（多入口构建，各角色一个 SPA）：
  - `index.html` → 登录页（`src/pages/Login.vue`）
  - `admin.html` → 管理员后台（`src/pages/admin/AdminApp.vue`，7 tab）
  - `teacher.html` → 教师工作台（`src/pages/teacher/TeacherApp.vue`，7 tab）
  - `student.html` → 学生个人中心（`src/pages/student/StudentApp.vue`）
  - `parent.html` → 家长端（`src/pages/parent/ParentApp.vue`）
- **公共基础设施**（`src/lib/`）：
  - `api.ts` — `apiRequest(method, url, data)`，统一注入 CSRF Token（`X-CSRF-Token` 头）+ 错误 Toast + 401 跳登录
  - `auth.ts` — `checkAuth(roleId)` 调 `/api/auth/me` 验证会话；`logout()` 带 CSRF 头登出
  - `format.ts` — `formatDateTime` 等格式化工具，注册为 `app.config.globalProperties.$formatDateTime`
  - `theme.ts` / `navConfig.ts` — 角色主题色与侧边栏导航配置
- **Composables**（`src/composables/`）：`useToast`（替换 `alert`）、`useConfirm`（替换 `confirm`）、`usePagination`、`useChart`（ECharts 生命周期）
- **可复用组件**（`src/components/`）：`AppLayout`、`Sidebar`、`BaseModal`、`StatCard`、`RoleBadge`、`BaseChart`、`EmptyState`、`Pagination`、`ToastContainer`、`ConfirmDialog`
- **依赖**：Vue 3、ECharts、XLSX、pinyin-pro、Tailwind CSS（PostCSS 构建）、FontAwesome —— 全部经 npm 管理，Vite 打包 hashed 产物到 `dist/assets/`

### 3.3 RBAC 权限模型

**4 个角色**：

| role_id | 角色名 | 描述 |
|---------|--------|------|
| 1 | 管理员 | 系统管理员，拥有所有权限 |
| 2 | 教师 | 教师角色，管理学生和积分 |
| 3 | 学生 | 学生角色，查看个人信息和兑换 |
| 4 | 家长 | 家长角色，查看孩子学习成绩和积分情况 |

**12 个权限**：

| permission_id | 权限名称 | 权限代码 | 描述 |
|---------------|----------|----------|------|
| 1 | 系统管理 | `system:manage` | 系统配置管理 |
| 2 | 用户管理 | `user:manage` | 用户和角色管理 |
| 3 | 学生管理 | `student:manage` | 学生信息管理 |
| 4 | 积分管理 | `points:manage` | 积分操作管理 |
| 5 | 评价管理 | `evaluation:manage` | 学生评价管理 |
| 6 | 商城管理 | `mall:manage` | 兑换商城管理 |
| 7 | 数据统计 | `statistics:view` | 数据统计查看 |
| 8 | 家长管理 | `parent:manage` | 家长账号与绑定管理 |
| 9 | 留言管理 | `message:manage` | 家校留言管理 |
| 10 | 班级管理 | `class:manage` | 班级信息管理 |
| 11 | 兑换管理 | `redemption:manage` | 兑换记录管理 |
| 12 | 数据导出 | `data:export` | 数据导出与备份 |

**权限分配表**（角色 × 权限）：

| 权限代码 | 管理员(1) | 教师(2) | 学生(3) | 家长(4) |
|----------|:---------:|:-------:|:-------:|:-------:|
| `system:manage` | ✅ | — | — | — |
| `user:manage` | ✅ | — | — | — |
| `student:manage` | ✅ | ✅ | — | — |
| `points:manage` | ✅ | ✅ | — | — |
| `evaluation:manage` | ✅ | ✅ | — | — |
| `mall:manage` | ✅ | — | ✅ | ✅ |
| `statistics:view` | ✅ | ✅ | — | — |
| `parent:manage` | ✅ | — | — | — |
| `message:manage` | ✅ | ✅ | — | ✅ |
| `class:manage` | ✅ | ✅ | — | — |
| `redemption:manage` | ✅ | ✅ | — | ✅ |
| `data:export` | ✅ | — | — | — |

> 管理员拥有全部 12 项权限；教师拥有 7 项（学生/积分/评价/统计/留言/班级/兑换）；学生仅拥有 1 项（商城，用于兑换）；家长拥有 3 项（商城/留言/兑换）。

---

## 4. 目录结构

```
comptation/
├── .github/
│   └── workflows/
│       ├── ci.yml           # PR 检查（前端 typecheck/build + 后端编译冒烟）
│       ├── pages.yml        # 纯前端 Mock Demo 自动部署到 GitHub Pages
│       └── release.yml      # 推送 v*.*.* 标签时编译打包并发布 GitHub Release
├── main.cpp                 # 服务器入口文件（配置加载/数据库初始化/路由注册/启动）
├── config.h                 # ServerConfig 结构体 + load_config() 配置加载
├── config.json              # 运行时配置文件（端口/数据库/日志/安全/CORS/HTTPS）
├── logger.h / logger.cpp    # Logger 类（日志级别 + 按大小/数量轮转）
├── sha256.h                 # SHA256 算法 + PBKDF2 密码哈希/校验工具函数
├── models.h / models.cpp    # 数据模型结构体 + 全局数据容器 + 索引优化
├── auth.h                   # CORS + Cookie 会话认证 + CSRF 校验 + 权限中间件
├── routes.h                 # 路由注册函数声明
├── routes_static.cpp        # 静态文件路由（frontend/dist/ 构建产物）
├── routes_public.cpp        # 公共 API 路由（9 个端点）
├── routes_admin.cpp         # 管理员 API 路由（31 个端点）
├── routes_teacher.cpp       # 教师 API 路由（19 个端点）
├── routes_student.cpp       # 学生 API 路由（5 个端点）
├── routes_parent.cpp        # 家长 API 路由（9 个端点）
├── httplib.h                # cpp-httplib 单头文件 HTTP 服务器库
├── json.hpp                 # nlohmann/json 单头文件 JSON 库
├── sqlite3.c / sqlite3.h / sqlite3ext.h  # SQLite3 amalgamation 源码（直接静态编译）
├── sqlite3.def              # SQLite3 模块定义文件
├── sqlite_wrapper.h         # SQLite3 C++ 封装类（SqliteDb）
├── database.h               # 数据库辅助定义
├── frontend/                # 前端工程（Vite + Vue 3 SFC + TypeScript + Tailwind）
│   ├── package.json         # 依赖与构建脚本（vue/echarts/xlsx/pinyin-pro/font-awesome）
│   ├── vite.config.ts       # 多入口构建配置（5 个 HTML 入口）
│   ├── tailwind.config.js / postcss.config.js / tsconfig.json
│   ├── index.html           # Vite 入口 → src/pages/Login.vue（登录页）
│   ├── admin.html / teacher.html / student.html / parent.html  # 各角色入口
│   └── src/
│       ├── main.ts          # createApp 工厂 + globalProperties + Toast/Confirm 挂载
│       ├── style.css        # 主题：:root 变量、@font-face、.glass*、动画
│       ├── lib/             # api.ts / auth.ts / format.ts / theme.ts / navConfig.ts
│       ├── mock/            # 纯前端 Mock 数据层（仅 GitHub Pages 演示用）
│       ├── entries/         # 各 HTML 入口引导（login/admin/teacher/student/parent）
│       ├── composables/     # useToast / useConfirm / usePagination / useChart
│       ├── components/      # AppLayout / Sidebar / BaseModal / StatCard 等 10 个
│       ├── assets/          # 本地化字体（Fraunces / Noto Sans SC）
│       └── pages/           # Login.vue + admin/ + teacher/ + student/ + parent/
└── docs/                    # 项目文档
    ├── design_document.md   # 系统设计文档
    ├── database_design.md   # 数据库设计说明
    ├── user_manual.md       # 用户使用手册
    ├── maintenance_manual.md# 维护手册
    ├── sqlite_setup.md      # SQLite 构建说明
    ├── arch_diagram.jpg / flow_diagram.jpg  # 架构/流程图
    └── shots/               # 功能截图
```

> 运行时在本地生成的 `campus_system.db`（数据库）、`server.log`（日志）、`cookies*.txt`（调试会话）等文件已被 `.gitignore` 排除，不入库。

---

## 5. 构建指南

### 5.1 环境要求

- **编译器**：MinGW-w64 g++（支持 C++11，CI 使用 gcc 14.2.0 验证通过）
- **Node.js**：20+（含 npm，仅前端构建需要）
- **SQLite3**：无需安装，仓库内自带 amalgamation 源码（`sqlite3.c`），直接静态编译
- **操作系统**：Windows（使用 WinSock 网络库；CI 与 Release 均在 windows-latest 上构建）

### 5.2 前端构建（必须先于后端）

C++ 后端通过 `routes_static.cpp` 托管 `frontend/dist/` 构建产物，因此首次构建必须先完成前端步骤，否则页面 404。

```bash
cd frontend
npm ci        # 安装依赖（也可用 npm install）
npm run build # vue-tsc 类型检查 + vite build → dist/
```

构建产物：`frontend/dist/`（5 个 HTML 入口 + `assets/` hashed 资源）。

**构建参数**（与 `.github/workflows/pages.yml` 一致）：

| 环境变量 | 示例值 | 说明 |
|----------|--------|------|
| `VITE_USE_MOCK` | `true` | 启用纯前端 Mock 数据模式（无需 C++ 后端，GitHub Pages Demo 即此形态） |
| `VITE_BASE` | `/campus-master/` | 部署子路径（GitHub Pages 项目站点需要；独立部署可省略） |

开发调试：`npm run dev` 启动 Vite 开发服务器。

### 5.3 后端编译

以下命令与 `.github/workflows/release.yml` 完全同源：

```bash
# 1. 编译 SQLite3 C 库
gcc -c sqlite3.c -o sqlite3.o -O2

# 2. 编译 9 个 C++ 源文件
g++ -c main.cpp          -o main.o          -std=c++11 -O2 -I.
g++ -c models.cpp        -o models.o        -std=c++11 -O2 -I.
g++ -c logger.cpp        -o logger.o        -std=c++11 -O2 -I.
g++ -c routes_static.cpp -o routes_static.o -std=c++11 -O2 -I.
g++ -c routes_public.cpp -o routes_public.o -std=c++11 -O2 -I.
g++ -c routes_admin.cpp  -o routes_admin.o  -std=c++11 -O2 -I.
g++ -c routes_teacher.cpp -o routes_teacher.o -std=c++11 -O2 -I.
g++ -c routes_student.cpp -o routes_student.o -std=c++11 -O2 -I.
g++ -c routes_parent.cpp -o routes_parent.o -std=c++11 -O2 -I.

# 3. 链接生成 server.exe（静态链接 MinGW 运行时，无外部 DLL 依赖）
g++ -o server.exe main.o models.o logger.o routes_static.o routes_public.o routes_admin.o routes_teacher.o routes_student.o routes_parent.o sqlite3.o -lws2_32 -lwsock32 -std=c++11 -O2 -static -static-libgcc -static-libstdc++ -lwinpthread
```

> **说明**：
> - `-lws2_32 -lwsock32` 为 Windows 套接字库，cpp-httplib 依赖其进行网络通信
> - `-static -static-libgcc -static-libstdc++ -lwinpthread` 确保 `server.exe` 可独立分发，目标机器无需安装 MinGW 运行时
> - 启动后数据自动持久化到 `campus_system.db`（路径可在 `config.json` 配置）

### 5.4 一键获取成品（免编译）

推送 `v*.*.*` 格式的 tag（或在 Actions 页面手动触发 Release 工作流），GitHub Actions 会自动编译并发布带 `server.exe` + 前端产物的 zip 包，可直接从 [Releases](https://github.com/JingWen983/campus-master/releases) 下载解压运行。

---

## 6. 配置说明

配置文件 `config.json` 字段说明：

| 字段路径 | 类型 | 默认值 | 说明 |
|----------|------|--------|------|
| `server.host` | string | `"0.0.0.0"` | 服务器监听地址（`0.0.0.0` 表示监听所有网卡） |
| `server.port` | int | `8080` | 服务器监听端口 |
| `server.thread_count` | int | `8` | 线程池大小（并发处理请求数） |
| `database.path` | string | `"campus_system.db"` | SQLite 数据库文件路径 |
| `log.file` | string | `"server.log"` | 日志文件路径 |
| `log.max_size_mb` | int | `10` | 单个日志文件最大大小（MB），超过后触发轮转 |
| `log.max_files` | int | `5` | 保留的日志文件最大数量 |
| `security.token_expiry_hours` | int | `24` | Token 过期时间（小时），默认 24 小时 |
| `security.session_expiry_hours` | int | `24` | 会话过期时间（小时） |
| `security.cookie_name` | string | `"sid"` | 会话 Cookie 名称 |
| `security.csrf_enabled` | bool | `true` | 是否启用 CSRF 双提交校验 |
| `security.max_login_attempts` | int | `5` | 最大登录失败尝试次数（配置项，防暴力破解） |
| `security.lockout_minutes` | int | `30` | 账户锁定时长（分钟） |
| `cors.allowed_origins` | list | `[]` | 允许的 CORS 来源列表（空 = 不额外放行跨域来源） |
| `https.enabled` | bool | `false` | 是否启用 HTTPS（当前编译版本不支持，需 OpenSSL 版本） |
| `https.cert_path` | string | `""` | HTTPS 证书文件路径 |
| `https.key_path` | string | `""` | HTTPS 私钥文件路径 |

> **注意**：当前编译版本不支持 SSLServer。若 `https.enabled` 为 `true`，服务器会输出警告并回退到 HTTP 模式。如需 HTTPS，需使用 OpenSSL 版本重新编译 cpp-httplib。

---

## 7. API 接口文档

系统共提供 **75 个路由端点**（静态 2 + 公共 9 + 管理员 31 + 教师 19 + 学生 5 + 家长 9），按模块分组如下。

### 7.1 静态文件路由（7 个）

由 `routes_static.cpp` 从 `frontend/dist/` 提供前端构建产物。

| 方法 | 路径 | 权限 | 功能说明 |
|------|------|------|----------|
| OPTIONS | `.*` | 公开 | CORS 预检请求 |
| GET | `/` | 公开 | 返回 `index.html` 登录入口页 |
| GET | `/index.html` | 公开 | 返回登录入口页 |
| GET | `/admin.html` | 公开 | 返回管理员后台页面 |
| GET | `/teacher.html` | 公开 | 返回教师工作台页面 |
| GET | `/student.html` | 公开 | 返回学生个人中心页面 |
| GET | `/parent.html` | 公开 | 返回家长端页面 |
| GET | `/assets/.*` | 公开 | 提供 Vite 构建的 hashed JS/CSS/字体资源 |

### 7.2 公共 API（9 个）

无需登录即可访问的基础接口。

| 方法 | 路径 | 权限 | 功能说明 |
|------|------|------|----------|
| POST | `/api/auth/login` | 公开 | 用户登录，写入会话 Cookie |
| POST | `/api/auth/logout` | 登录 | 退出登录，清除会话 |
| POST | `/api/auth/register` | 公开 | 用户注册（默认学生角色） |
| GET | `/api/auth/me` | 会话 | 获取当前登录用户信息 |
| GET | `/api/user/info` | 会话 | 获取用户基本信息 |
| GET | `/api/behavior/history` | 会话 | 获取行为历史记录 |
| GET | `/api/mall/items` | 公开 | 获取商城商品列表 |
| POST | `/api/mall/redeem` | 会话 | 兑换商城商品（扣减积分） |
| GET | `/api/rank/class` | 公开 | 获取班级积分排名 |

### 7.3 管理员 API（31 个）

所有接口均需会话 Cookie + 对应权限。

#### 系统管理（`system:manage` 权限）

| 方法 | 路径 | 权限 | 功能说明 |
|------|------|------|----------|
| GET | `/api/admin/system` | `system:manage` | 获取系统信息（版本/运行状态） |
| GET | `/api/admin/system/config` | `system:manage` | 获取系统配置 |
| PUT | `/api/admin/system/config` | `system:manage` | 更新系统配置 |
| POST | `/api/admin/system/backup` | `system:manage` | 备份数据库 |
| GET | `/api/admin/dashboard` | `system:manage` | 管理员仪表盘数据汇总 |
| GET | `/api/admin/export` | `system:manage` | 导出数据（用户/积分/评价等） |
| POST | `/api/admin/import` | `system:manage` | 导入数据（批量用户/积分等） |

#### 商城管理（`system:manage` 权限）

| 方法 | 路径 | 权限 | 功能说明 |
|------|------|------|----------|
| GET | `/api/admin/mall` | `system:manage` | 获取商城商品列表（管理视图） |
| POST | `/api/admin/mall` | `system:manage` | 新增商城商品 |
| PUT | `/api/admin/mall/{id}` | `system:manage` | 更新指定商品信息 |
| DELETE | `/api/admin/mall/{id}` | `system:manage` | 删除指定商品 |
| GET | `/api/admin/redemptions` | `system:manage` | 获取所有兑换记录列表 |

#### 用户/角色/权限管理（`user:manage` 权限）

| 方法 | 路径 | 权限 | 功能说明 |
|------|------|------|----------|
| GET | `/api/admin/users` | `user:manage` | 获取用户列表（支持筛选） |
| POST | `/api/admin/users` | `user:manage` | 新增用户 |
| POST | `/api/admin/students/batch-import` | `user:manage` | 批量导入学生 |
| PUT | `/api/admin/users/{id}` | `user:manage` | 更新用户信息 |
| DELETE | `/api/admin/users` | `user:manage` | 删除用户（批量） |
| POST | `/api/admin/users/reset-password` | `user:manage` | 重置用户密码 |
| GET | `/api/admin/roles` | `user:manage` | 获取角色列表 |
| POST | `/api/admin/roles` | `user:manage` | 新增角色 |
| PUT | `/api/admin/roles/{id}` | `user:manage` | 更新角色信息 |
| DELETE | `/api/admin/roles` | `user:manage` | 删除角色 |
| GET | `/api/admin/permissions` | `user:manage` | 获取权限列表 |
| POST | `/api/admin/permissions` | `user:manage` | 新增权限 |
| PUT | `/api/admin/permissions/{id}` | `user:manage` | 更新权限信息 |
| DELETE | `/api/admin/permissions` | `user:manage` | 删除权限 |

#### 班级管理（`user:manage` 权限）

| 方法 | 路径 | 权限 | 功能说明 |
|------|------|------|----------|
| GET | `/api/admin/classes` | `user:manage` | 获取班级列表 |
| POST | `/api/admin/classes` | `user:manage` | 新增班级 |
| PUT | `/api/admin/classes/{id}` | `user:manage` | 更新班级信息 |
| DELETE | `/api/admin/classes/{id}` | `user:manage` | 删除班级 |

#### 数据统计（`statistics:view` 权限）

| 方法 | 路径 | 权限 | 功能说明 |
|------|------|------|----------|
| GET | `/api/admin/statistics` | `statistics:view` | 获取综合统计数据（用户/积分/班级维度） |

### 7.4 教师 API（19 个）

| 方法 | 路径 | 权限 | 功能说明 |
|------|------|------|----------|
| GET | `/api/teacher/my-classes` | `student:manage` | 获取所教班级列表 |
| GET | `/api/teacher/students` | `student:manage` | 获取学生列表 |
| POST | `/api/teacher/students` | `student:manage` | 新增学生 |
| PUT | `/api/teacher/students/{id}` | `student:manage` | 更新学生信息 |
| DELETE | `/api/teacher/students` | `student:manage` | 删除学生 |
| POST | `/api/teacher/students/import` | `student:manage` | 批量导入学生 |
| POST | `/api/teacher/points` | `points:manage` | 学生积分操作（加分/扣分） |
| GET | `/api/teacher/points/records` | `points:manage` | 获取积分操作记录列表 |
| GET | `/api/teacher/evaluation/dimensions` | `evaluation:manage` | 获取评价维度列表 |
| POST | `/api/teacher/evaluation` | `evaluation:manage` | 提交学生评价 |
| GET | `/api/teacher/evaluations` | `evaluation:manage` | 获取评价列表 |
| PUT | `/api/teacher/evaluation/{id}` | `evaluation:manage` | 更新评价 |
| DELETE | `/api/teacher/evaluation/{id}` | `evaluation:manage` | 删除评价 |
| GET | `/api/teacher/dashboard` | `student:manage` | 教师仪表盘数据汇总 |
| GET | `/api/teacher/statistics` | `statistics:view` | 获取教师维度统计数据 |
| GET | `/api/teacher/parent-messages` | `student:manage` | 获取家校留言列表 |
| POST | `/api/teacher/parent-messages/{id}/reply` | `student:manage` | 回复家长留言 |
| PUT | `/api/teacher/parent-messages/{id}/read` | `student:manage` | 标记留言已读 |
| GET | `/api/teacher/redemptions` | `student:manage` | 获取兑换记录列表 |

### 7.5 学生 API（5 个）

| 方法 | 路径 | 权限 | 功能说明 |
|------|------|------|----------|
| GET | `/api/student/info` | `mall:manage` | 获取学生个人信息（含积分） |
| GET | `/api/student/points/records` | `mall:manage` | 获取自己的积分记录列表 |
| GET | `/api/student/evaluation` | `mall:manage` | 获取自己的评价结果 |
| GET | `/api/student/mall` | `mall:manage` | 获取可兑换商品列表 |
| GET | `/api/student/redemptions` | `mall:manage` | 获取自己的兑换记录 |

> **说明**：学生端接口统一使用 `mall:manage` 权限作为访问控制（学生角色唯一拥有的权限），用于验证学生身份。

### 7.6 家长 API（9 个）

由 `routes_parent.cpp` 提供，除登录/登出外均通过家长会话校验（`is_parent`），且只能访问已绑定的孩子数据。

| 方法 | 路径 | 权限 | 功能说明 |
|------|------|------|----------|
| POST | `/api/parent/login` | 公开 | 家长登录（家长账号 + 家长密码） |
| POST | `/api/parent/logout` | 家长会话 | 退出登录 |
| GET | `/api/parent/children` | 家长会话 | 获取绑定的孩子列表 |
| GET | `/api/parent/student/{id}/info` | 家长会话 | 查看孩子的个人信息与积分 |
| GET | `/api/parent/student/{id}/points` | 家长会话 | 查看孩子的积分记录 |
| GET | `/api/parent/student/{id}/evaluation` | 家长会话 | 查看孩子的评价结果 |
| GET | `/api/parent/student/{id}/redemptions` | 家长会话 | 查看孩子的兑换记录 |
| GET | `/api/parent/student/{id}/messages` | 家长会话 | 获取家校留言（收件箱） |
| POST | `/api/parent/student/{id}/messages` | 家长会话 | 发送家校留言 |

---

## 8. 数据库设计

数据库初始化 SQL 位于 `main.cpp` 的 `init_sql` 字符串中，共创建 **13 张表** + 7 个索引。

### 8.1 `users` — 用户表

| 字段名 | 类型 | 说明 |
|--------|------|------|
| `id` | INTEGER PK AUTO | 主键，自增 |
| `username` | TEXT UNIQUE NOT NULL | 用户名，唯一 |
| `password_hash` | TEXT NOT NULL | 密码哈希（新版 `pbkdf2$迭代次数$盐$摘要` 格式；初始种子账号为旧版 SHA-256，登录校验时自动识别） |
| `role_id` | INTEGER NOT NULL | 角色 ID（1=管理员/2=教师/3=学生/4=家长） |
| `name` | TEXT NOT NULL | 用户姓名 |
| `className` | TEXT | 班级名称 |
| `points` | INTEGER DEFAULT 0 | 当前积分 |
| `created_at` | DATETIME DEFAULT CURRENT_TIMESTAMP | 创建时间 |
| `updated_at` | DATETIME DEFAULT CURRENT_TIMESTAMP | 更新时间 |

### 8.2 `roles` — 角色表

| 字段名 | 类型 | 说明 |
|--------|------|------|
| `id` | INTEGER PK AUTO | 主键，自增 |
| `name` | TEXT UNIQUE NOT NULL | 角色名称 |
| `description` | TEXT | 角色描述 |

### 8.3 `permissions` — 权限表

| 字段名 | 类型 | 说明 |
|--------|------|------|
| `id` | INTEGER PK AUTO | 主键，自增 |
| `name` | TEXT NOT NULL | 权限名称 |
| `code` | TEXT UNIQUE NOT NULL | 权限代码（如 `system:manage`） |
| `description` | TEXT | 权限描述 |

### 8.4 `role_permissions` — 角色权限关联表

| 字段名 | 类型 | 说明 |
|--------|------|------|
| `id` | INTEGER PK AUTO | 主键，自增 |
| `role_id` | INTEGER NOT NULL | 角色 ID（外键 → `roles.id`） |
| `permission_id` | INTEGER NOT NULL | 权限 ID（外键 → `permissions.id`） |
| — | UNIQUE(role_id, permission_id) | 联合唯一约束 |

### 8.5 `points_records` — 积分记录表

| 字段名 | 类型 | 说明 |
|--------|------|------|
| `id` | INTEGER PK AUTO | 主键，自增 |
| `student_id` | INTEGER NOT NULL | 学生 ID（外键 → `users.id`） |
| `points` | INTEGER NOT NULL | 积分变动值（正为加/负为扣） |
| `reason` | TEXT | 积分变动原因 |
| `operator_id` | INTEGER NOT NULL | 操作人 ID（外键 → `users.id`） |
| `created_at` | DATETIME DEFAULT CURRENT_TIMESTAMP | 创建时间 |

### 8.6 `evaluations` — 评价表

| 字段名 | 类型 | 说明 |
|--------|------|------|
| `id` | INTEGER PK AUTO | 主键，自增 |
| `student_id` | INTEGER NOT NULL | 学生 ID（外键 → `users.id`） |
| `dimension_id` | INTEGER NOT NULL | 评价维度 ID |
| `score` | INTEGER NOT NULL | 评分 |
| `comment` | TEXT | 评语 |
| `evaluator_id` | INTEGER NOT NULL | 评价人 ID（外键 → `users.id`） |
| `created_at` | DATETIME DEFAULT CURRENT_TIMESTAMP | 创建时间 |
| `updated_at` | DATETIME DEFAULT CURRENT_TIMESTAMP | 更新时间 |

### 8.7 `mall_items` — 商城商品表

| 字段名 | 类型 | 说明 |
|--------|------|------|
| `id` | INTEGER PK AUTO | 主键，自增 |
| `name` | TEXT NOT NULL | 商品名称 |
| `description` | TEXT | 商品描述 |
| `cost` | INTEGER NOT NULL | 兑换所需积分 |
| `stock` | INTEGER DEFAULT -1 | 库存（-1 表示不限） |
| `image_url` | TEXT | 商品图片 URL |
| `status` | INTEGER DEFAULT 1 | 状态（1=上架/0=下架） |
| `created_at` | DATETIME DEFAULT CURRENT_TIMESTAMP | 创建时间 |

### 8.8 `redemption_records` — 兑换记录表

| 字段名 | 类型 | 说明 |
|--------|------|------|
| `id` | INTEGER PK AUTO | 主键，自增 |
| `student_id` | INTEGER NOT NULL | 学生 ID（外键 → `users.id`） |
| `item_id` | INTEGER NOT NULL | 商品 ID（外键 → `mall_items.id`） |
| `cost` | INTEGER NOT NULL | 兑换时消耗的积分 |
| `created_at` | DATETIME DEFAULT CURRENT_TIMESTAMP | 兑换时间 |

### 8.9 `classes` — 班级表

| 字段名 | 类型 | 说明 |
|--------|------|------|
| `id` | INTEGER PK AUTO | 主键，自增 |
| `name` | TEXT UNIQUE NOT NULL | 班级名称，唯一 |
| `grade` | TEXT | 年级 |
| `grade_code` | TEXT | 年级编码（如 `02` 表示高二） |
| `class_code` | TEXT | 班级编码（如 `01`） |
| `head_teacher` | TEXT | 班主任姓名 |
| `description` | TEXT | 班级描述 |
| `created_at` | DATETIME DEFAULT CURRENT_TIMESTAMP | 创建时间 |

### 8.10 `teacher_classes` — 教师-班级关联表

| 字段名 | 类型 | 说明 |
|--------|------|------|
| `id` | INTEGER PK AUTO | 主键，自增 |
| `teacher_id` | TEXT NOT NULL | 教师 ID（外键 → `users.id`） |
| `class_id` | INTEGER NOT NULL | 班级 ID（外键 → `classes.id`） |
| — | UNIQUE(teacher_id, class_id) | 联合唯一约束 |

### 8.11 `parent_students` — 家长-学生关联表

| 字段名 | 类型 | 说明 |
|--------|------|------|
| `id` | INTEGER PK AUTO | 主键，自增 |
| `parent_id` | TEXT NOT NULL | 家长 ID（外键 → `users.id`） |
| `student_id` | TEXT NOT NULL | 学生 ID（外键 → `users.id`） |
| — | UNIQUE(parent_id, student_id) | 联合唯一约束 |

### 8.12 `parent_messages` — 家校留言表

| 字段名 | 类型 | 说明 |
|--------|------|------|
| `id` | INTEGER PK AUTO | 主键，自增 |
| `student_id` | TEXT NOT NULL | 关联学生 ID |
| `sender_type` | TEXT NOT NULL | 发送方类型（家长/教师） |
| `sender_id` | TEXT | 发送人 ID |
| `content` | TEXT NOT NULL | 留言内容 |
| `reply_to` | INTEGER | 回复的目标留言 ID |
| `read_status` | INTEGER DEFAULT 0 | 已读状态 |
| `created_at` | DATETIME DEFAULT CURRENT_TIMESTAMP | 创建时间 |

### 8.13 `sessions` — 会话表（Cookie 认证）

| 字段名 | 类型 | 说明 |
|--------|------|------|
| `session_id` | TEXT PK | 会话 ID（64 位随机 hex，存于 HttpOnly Cookie） |
| `user_id` | TEXT NOT NULL | 用户 ID |
| `role_id` | INTEGER NOT NULL | 角色 ID |
| `created_at` | INTEGER NOT NULL | 创建时间戳 |
| `expires_at` | INTEGER NOT NULL | 过期时间戳 |
| `is_parent` | INTEGER DEFAULT 0 | 是否为家长会话 |
| `student_id` | TEXT | 家长会话当前查看的学生 ID |

### 8.14 索引

| 索引名 | 表 | 字段 | 用途 |
|--------|----|------|------|
| `idx_users_username` | users | username | 加速用户名查询 |
| `idx_users_role_id` | users | role_id | 加速按角色筛选 |
| `idx_points_records_student_id` | points_records | student_id | 加速按学生查积分记录 |
| `idx_evaluations_student_id` | evaluations | student_id | 加速按学生查评价 |
| `idx_parent_messages_student_id` | parent_messages | student_id | 加速按学生查留言 |
| `idx_sessions_user_id` | sessions | user_id | 加速按用户查会话 |
| `idx_sessions_expires_at` | sessions | expires_at | 加速会话过期清理 |

---

## 9. 默认账号

系统初始化时通过 `INSERT OR IGNORE` 写入 4 个默认账号：

| 角色 | 用户名 | 密码 | 姓名 | 班级 | 初始积分 |
|------|--------|------|------|------|----------|
| 管理员 | `admin` | `admin123` | 管理员 | 系统管理 | 0 |
| 教师 | `teacher` | `teacher123` | 王老师 | 高二(1)班 | 0 |
| 学生 | `student` | `student123` | 张同学 | 高二(1)班 | 150 |
| 家长 | `parent` | `parent123` | 张同学家长 | — | 0 |

> **安全提示**：生产环境部署后请立即修改默认密码。种子账号的密码以旧版 SHA-256 哈希存储（如 `admin123` → `240be518...`）；通过接口新设置/修改的密码自动使用 PBKDF2-SHA256（10 万次迭代 + 随机盐），登录校验时两种格式均自动识别，旧账号修改一次密码即完成升级。

---

## 10. 部署指南

### 10.0 免编译部署（推荐）

从 [Releases](https://github.com/JingWen983/campus-master/releases) 下载最新 zip 包，内含已编译的 `server.exe`（静态链接，无需额外 DLL）、前端构建产物 `frontend/dist/` 和 `config.json`，解压后直接运行即可。

### 10.1 启动服务器

```bash
server.exe
```

- 启动前确保 `frontend/dist/` 与 `server.exe` 同级存在（Release 包已包含）
- 默认监听 `http://0.0.0.0:8080`

**崩溃自动重启（可选）**：如需无人值守运行，可自建重启脚本（此类脚本含机器路径，不入库）：

```bat
@echo off
cd /d "%~dp0"
:loop
start /b /wait server.exe
echo 服务器已停止，5秒后自动重启...
timeout /t 5 /nobreak >nul
goto loop
```

### 10.2 开机自启（可选）

使用 Windows 任务计划注册（按实际路径调整）：

```bat
schtasks /create /tn "CampusManagementServer" /tr "\"C:\path\to\start_server.bat\"" /sc onlogon /rl highest /f
```

**卸载自启命令**：

```bat
schtasks /delete /tn "CampusManagementServer" /f
```

### 10.3 配置监听地址

修改 `config.json` 中的 `server.host` 和 `server.port` 字段：

```json
{
    "server": {
        "host": "0.0.0.0",
        "port": 8080,
        "thread_count": 8
    }
}
```

- `host` 设为 `0.0.0.0` 监听所有网卡（局域网可访问）
- `host` 设为 `127.0.0.1` 仅本机可访问
- `port` 修改后需重启服务器生效

### 10.4 访问系统

服务器启动后，浏览器访问：

```
http://<服务器IP>:8080/
```

登录后根据角色自动跳转到对应工作台。

---

## 11. 测试说明

### 11.1 持续集成检查（CI）

推送到 `main` 或提交 Pull Request 时，GitHub Actions（`.github/workflows/ci.yml`）自动运行：

- **前端**：`npm ci` → `vue-tsc` 类型检查 → Vite 生产构建（含 Mock 模式参数）
- **后端**：Windows 环境下按 Release 同源命令完整编译 9 个 C++ 源文件并链接 `server.exe`（编译冒烟）

### 11.2 本地验证

```bash
# 前端类型检查 + 构建
cd frontend && npm ci && npm run build

# 后端完整编译（命令见第 5.3 节）
```

### 11.3 历史测试脚本

早期版本曾提供 Python E2E 测试脚本（`test_production.py` 等，覆盖认证/CRUD/权限/并发），后随仓库安全清理移出版本库。如需参考，可从历史提交 `aa5ca6a` 中查看：

```bash
git show aa5ca6a:test_production.py
```

---

## 附录

### 相关文档

- `docs/design_document.md` — 系统设计文档
- `docs/database_design.md` — 数据库设计说明
- `docs/user_manual.md` — 用户使用手册
- `docs/maintenance_manual.md` — 维护手册
- `docs/sqlite_setup.md` — SQLite 构建说明
- 在线 Demo（纯前端 Mock）：https://jingwen983.github.io/campus-master/

### 关键依赖版本

| 依赖 | 版本 | 说明 |
|------|------|------|
| cpp-httplib | 单头文件版 | HTTP 服务器 |
| nlohmann/json | 单头文件版 | JSON 解析 |
| SQLite3 | amalgamation 版 | 嵌入式数据库（仓库内静态编译） |
| Vue 3 | ^3.4 | 前端框架（npm + Vite 构建） |
| TypeScript | ^5.4 | 前端类型系统 |
| Vite | ^5.2 | 前端构建工具 |
| Tailwind CSS | ^3.4 | CSS 框架（npm + PostCSS 构建） |
| ECharts | ^5.5 | 图表库（npm） |
| FontAwesome Free | ^6.5 | 图标库（npm） |

> 注：依赖版本以 `frontend/package.json` 为准。

### 许可声明

本项目为校园内部管理系统，所有第三方库遵循其各自开源协议。
