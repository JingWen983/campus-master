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

- **用户管理**：管理员/教师/学生三类账号，支持 CRUD、批量导入、密码重置
- **积分管理**：教师对学生进行加分/扣分操作，完整记录历史流水
- **评价管理**：多维度学生评价（学习、纪律、品德等），支持增删改查
- **商城兑换**：积分兑换实物奖品，库存管理、兑换记录追溯
- **数据统计**：班级排名、学生排名、维度统计、可视化图表（ECharts）
- **班级管理**：班级信息维护、班主任绑定、年级分组

**三角色权限**：

| 角色 | role_id | 主要职责 |
|------|---------|----------|
| 管理员 | 1 | 系统配置、用户/角色/权限管理、商城管理、数据导入导出 |
| 教师 | 2 | 学生管理、积分操作、学生评价、数据统计查看 |
| 学生 | 3 | 查看个人信息、积分记录、评价结果、商城兑换 |

---

## 2. 技术栈

| 层级 | 技术选型 | 说明 |
|------|----------|------|
| **后端** | C++11 + cpp-httplib | 单头文件 HTTP 服务器，轻量高效 |
| **数据库** | SQLite3 | 持久化存储，数据落盘到 `campus_system.db` |
| **前端** | Vue 3（CDN 单文件应用） | 4 个独立 HTML 应用，无构建步骤 |
| **样式** | Tailwind CSS | 实用优先的 CSS 框架（本地化） |
| **图表** | ECharts | 数据可视化（班级排名、维度统计等） |
| **字体** | Fraunces + Noto Sans SC | 衬线展示字体 + 中文正文字体，已本地化 |
| **安全** | SHA256 + Token + RBAC | 密码哈希存储、Token 认证、基于角色的权限控制 |
| **JSON** | nlohmann/json | 单头文件 JSON 解析库 |

---

## 3. 架构设计

### 3.1 后端模块化结构

| 文件 | 职责 |
|------|------|
| `config.h` | `ServerConfig` 结构体定义 + `load_config()` 配置加载函数（从 `config.json` 读取） |
| `logger.h` / `logger.cpp` | `Logger` 类，支持日志级别（info/warning/error）+ 日志轮转（按大小/数量） |
| `sha256.h` | SHA256 哈希算法实现 + 密码工具函数（`sha256()` 哈希计算） |
| `models.h` / `models.cpp` | `User`/`Role`/`Permission`/`RolePermission`/`PointsRecord` 结构体 + 全局数据容器 + 索引优化（`find_user_by_id`/`find_user_by_username`/`check_permission_optimized`） |
| `auth.h` | CORS 头设置 + 请求/响应日志 + Token 生成/验证/解析 + `check_permission_middleware` 权限中间件 |
| `routes.h` | 5 个路由注册函数声明 |
| `routes_static.cpp` | 静态文件服务（HTML/CSS/JS/字体） |
| `routes_public.cpp` | 公共 API（登录/注册/用户信息/行为历史/商城/排名） |
| `routes_admin.cpp` | 管理员 API（30 个端点） |
| `routes_teacher.cpp` | 教师 API（14 个端点） |
| `routes_student.cpp` | 学生 API（5 个端点） |
| `main.cpp` | 入口文件：配置加载 → 日志初始化 → 数据库初始化 → 路由注册 → 索引初始化 → 线程池 → 启动服务器 |

### 3.2 前端架构

- **4 个单文件 HTML 应用**：
  - `index.html` — 统一登录入口（角色判断跳转）
  - `admin.html` — 管理员后台
  - `teacher.html` — 教师工作台
  - `student.html` — 学生个人中心
- **`lib/common.js`** — 公共 API 请求封装，提供：
  - `apiRequest(method, url, data)` — 统一 API 请求（自动注入 Authorization Token）
  - `checkAuth(roleId)` — 登录状态与角色校验（不通过则跳转登录页）
  - `logout()` — 清除本地存储并跳转登录页
  - `formatDateTime(dateStr)` — 日期时间格式化为 `YYYY-MM-DD HH:mm:ss`
- **资源本地化**：所有依赖（Vue/Tailwind/ECharts/字体）均托管在 `lib/` 和 `webfonts/` 目录，无外部 CDN 依赖

### 3.3 RBAC 权限模型

**3 个角色**：

| role_id | 角色名 | 描述 |
|---------|--------|------|
| 1 | 管理员 | 系统管理员，拥有所有权限 |
| 2 | 教师 | 教师角色，管理学生和积分 |
| 3 | 学生 | 学生角色，查看个人信息和兑换 |

**7 个权限**：

| permission_id | 权限名称 | 权限代码 | 描述 |
|---------------|----------|----------|------|
| 1 | 系统管理 | `system:manage` | 系统配置管理 |
| 2 | 用户管理 | `user:manage` | 用户和角色管理 |
| 3 | 学生管理 | `student:manage` | 学生信息管理 |
| 4 | 积分管理 | `points:manage` | 积分操作管理 |
| 5 | 评价管理 | `evaluation:manage` | 学生评价管理 |
| 6 | 商城管理 | `mall:manage` | 兑换商城管理 |
| 7 | 数据统计 | `statistics:view` | 数据统计查看 |

**权限分配表**（角色 × 权限）：

| 权限代码 | 管理员(1) | 教师(2) | 学生(3) |
|----------|:---------:|:-------:|:-------:|
| `system:manage` | ✅ | — | — |
| `user:manage` | ✅ | — | — |
| `student:manage` | ✅ | ✅ | — |
| `points:manage` | ✅ | ✅ | — |
| `evaluation:manage` | ✅ | ✅ | — |
| `mall:manage` | ✅ | — | ✅ |
| `statistics:view` | ✅ | ✅ | — |

> 管理员拥有全部 7 项权限；教师拥有 4 项（学生/积分/评价/统计）；学生仅拥有 1 项（商城，用于兑换）。

---

## 4. 目录结构

```
comptation/
├── main.cpp                  # 服务器入口文件（配置加载/数据库初始化/路由注册/启动）
├── config.h                  # ServerConfig 结构体 + load_config() 配置加载
├── config.json               # 运行时配置文件（端口/数据库/日志/安全/HTTPS）
├── logger.h                  # Logger 类声明
├── logger.cpp                # Logger 类实现（含日志轮转）
├── sha256.h                  # SHA256 哈希算法 + 密码工具函数
├── models.h                  # 数据模型结构体声明（User/Role/Permission 等）
├── models.cpp                # 全局数据容器 + 索引优化实现
├── auth.h                    # CORS + Token 认证 + 权限中间件
├── routes.h                  # 路由注册函数声明
├── routes_static.cpp         # 静态文件路由（HTML/CSS/JS/字体）
├── routes_public.cpp         # 公共 API 路由（8 个端点）
├── routes_admin.cpp          # 管理员 API 路由（30 个端点）
├── routes_teacher.cpp        # 教师 API 路由（14 个端点）
├── routes_student.cpp        # 学生 API 路由（5 个端点）
├── httplib.h                 # cpp-httplib 单头文件 HTTP 服务器库
├── json.hpp                  # nlohmann/json 单头文件 JSON 库
├── sqlite3.c                 # SQLite3 源码（amalgamation）
├── sqlite3.h                 # SQLite3 头文件
├── sqlite3ext.h              # SQLite3 扩展头文件
├── sqlite3.dll               # SQLite3 动态链接库（运行时依赖）
├── sqlite3.def               # SQLite3 模块定义文件
├── sqlite3.lib               # SQLite3 静态库
├── sqlite3.exp               # SQLite3 导出文件
├── sqlite_wrapper.h          # SQLite3 C++ 封装类（SqliteDb）
├── database.h                # 数据库辅助定义
├── build.bat                 # 构建脚本（SQLite 模式）
├── start_server.bat          # 服务器启动脚本（崩溃后 5 秒自动重启）
├── install_autostart.bat     # 开机自启注册脚本（Windows 任务计划）
├── campus_system.db          # SQLite 数据库文件（运行时生成）
├── server.exe                # 编译生成的可执行文件
├── server.log                # 运行时日志文件
├── index.html                # 前端 - 登录入口页
├── admin.html                # 前端 - 管理员后台
├── teacher.html              # 前端 - 教师工作台
├── student.html              # 前端 - 学生个人中心
├── lib/                      # 前端公共资源目录
│   ├── common.js             # 公共 API 请求封装（apiRequest/checkAuth/logout）
│   ├── vue.min.js            # Vue 3 框架（本地化）
│   ├── tailwind.min.js       # Tailwind CSS（本地化）
│   ├── echarts.min.js        # ECharts 图表库（本地化）
│   ├── fonts.css             # 字体样式定义
│   └── fontawesome.zip       # Font Awesome 图标包
├── test_production.py        # 生产环境综合测试（75 项）
├── test_frontend_redesign.py # 前端重构验证测试（28 项）
├── test_permissions.py       # 权限漏洞专项测试
└── docs/
    └── user_manual.md        # 用户使用手册
```

---

## 5. 构建指南

### 5.1 环境要求

- **编译器**：MinGW-w64 / g++（支持 C++11 标准）
- **SQLite3 源码**：`sqlite3.c` / `sqlite3.h` / `sqlite3.dll`
- **操作系统**：Windows（脚本基于 `.bat`，代码含 Windows 控制台 UTF-8 设置）

### 5.2 方式一：使用 build.bat

双击运行 `build.bat`，脚本会自动完成以下步骤：

1. 检查 `sqlite3.h` 与 `sqlite3.dll` 是否存在
2. 编译 SQLite C 库（`sqlite3.c` → `sqlite3.o`）
3. 逐个编译所有 `.cpp` 源文件
4. 链接生成 `server.exe`（包含 `sqlite3.o` + `-lws2_32 -lwsock32`）

构建过程中数据持久化到 `campus_system.db`。

### 5.3 方式二：手动编译

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

# 3. 链接生成可执行文件
g++ -o server.exe main.o models.o logger.o routes_static.o routes_public.o routes_admin.o routes_teacher.o routes_student.o routes_parent.o sqlite3.o -lws2_32 -lwsock32 -std=c++11 -O2
```

> **说明**：`-lws2_32 -lwsock32` 为 Windows 套接字库，cpp-httplib 依赖其进行网络通信。

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
| `security.max_login_attempts` | int | `5` | 最大登录失败尝试次数（配置项，防暴力破解） |
| `security.lockout_minutes` | int | `30` | 账户锁定时长（分钟） |
| `https.enabled` | bool | `false` | 是否启用 HTTPS（当前编译版本不支持，需 OpenSSL 版本） |
| `https.cert_path` | string | `""` | HTTPS 证书文件路径 |
| `https.key_path` | string | `""` | HTTPS 私钥文件路径 |

> **注意**：当前编译版本不支持 SSLServer。若 `https.enabled` 为 `true`，服务器会输出警告并回退到 HTTP 模式。如需 HTTPS，需使用 OpenSSL 版本重新编译 cpp-httplib。

---

## 7. API 接口文档

系统共提供 **63 个路由端点**，按模块分组如下。

### 7.1 静态文件路由（6 个）

| 方法 | 路径 | 权限 | 功能说明 |
|------|------|------|----------|
| GET | `/` | 公开 | 返回 `index.html` 登录入口页 |
| GET | `/admin.html` | 公开 | 返回管理员后台页面 |
| GET | `/teacher.html` | 公开 | 返回教师工作台页面 |
| GET | `/student.html` | 公开 | 返回学生个人中心页面 |
| GET | `/lib/.*` | 公开 | 提供 `lib/` 目录下的 JS/CSS 静态资源 |
| GET | `/webfonts/.*` | 公开 | 提供 `webfonts/` 目录下的字体文件 |

### 7.2 公共 API（8 个）

无需登录即可访问的基础接口。

| 方法 | 路径 | 权限 | 功能说明 |
|------|------|------|----------|
| POST | `/api/auth/login` | 公开 | 用户登录，返回 Token 和用户信息 |
| POST | `/api/auth/register` | 公开 | 用户注册（默认学生角色） |
| GET | `/api/auth/me` | Token | 获取当前登录用户信息 |
| GET | `/api/user/info` | Token | 获取用户基本信息 |
| GET | `/api/behavior/history` | Token | 获取行为历史记录 |
| GET | `/api/mall/items` | 公开 | 获取商城商品列表 |
| POST | `/api/mall/redeem` | Token | 兑换商城商品（扣减积分） |
| GET | `/api/rank/class` | 公开 | 获取班级积分排名 |

### 7.3 管理员 API（30 个）

所有接口均需 `Authorization` Token + 对应权限。

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

### 7.4 教师 API（14 个）

| 方法 | 路径 | 权限 | 功能说明 |
|------|------|------|----------|
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

### 7.5 学生 API（5 个）

| 方法 | 路径 | 权限 | 功能说明 |
|------|------|------|----------|
| GET | `/api/student/info` | `mall:manage` | 获取学生个人信息（含积分） |
| GET | `/api/student/points/records` | `mall:manage` | 获取自己的积分记录列表 |
| GET | `/api/student/evaluation` | `mall:manage` | 获取自己的评价结果 |
| GET | `/api/student/mall` | `mall:manage` | 获取可兑换商品列表 |
| GET | `/api/student/redemptions` | `mall:manage` | 获取自己的兑换记录 |

> **说明**：学生端接口统一使用 `mall:manage` 权限作为访问控制（学生角色唯一拥有的权限），用于验证学生身份。

---

## 8. 数据库设计

数据库初始化 SQL 位于 `main.cpp` 的 `init_sql` 字符串中，共创建 **9 张表** + 4 个索引。

### 8.1 `users` — 用户表

| 字段名 | 类型 | 说明 |
|--------|------|------|
| `id` | INTEGER PK AUTO | 主键，自增 |
| `username` | TEXT UNIQUE NOT NULL | 用户名，唯一 |
| `password_hash` | TEXT NOT NULL | 密码的 SHA256 哈希值 |
| `role_id` | INTEGER NOT NULL | 角色 ID（1=管理员/2=教师/3=学生） |
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
| `head_teacher` | TEXT | 班主任姓名 |
| `description` | TEXT | 班级描述 |
| `created_at` | DATETIME DEFAULT CURRENT_TIMESTAMP | 创建时间 |

### 8.10 索引

| 索引名 | 表 | 字段 | 用途 |
|--------|----|------|------|
| `idx_users_username` | users | username | 加速用户名查询 |
| `idx_users_role_id` | users | role_id | 加速按角色筛选 |
| `idx_points_records_student_id` | points_records | student_id | 加速按学生查积分记录 |
| `idx_evaluations_student_id` | evaluations | student_id | 加速按学生查评价 |

---

## 9. 默认账号

系统初始化时通过 `INSERT OR IGNORE` 写入 3 个默认账号，密码以 SHA256 哈希存储：

| 角色 | 用户名 | 密码 | 姓名 | 班级 | 初始积分 |
|------|--------|------|------|------|----------|
| 管理员 | `admin` | `admin123` | 管理员 | 系统管理 | 0 |
| 教师 | `teacher` | `teacher123` | 王老师 | 高二(1)班 | 0 |
| 学生 | `student` | `student123` | 张同学 | 高二(1)班 | 150 |

> **安全提示**：生产环境部署后请立即修改默认密码。密码在数据库中以 SHA256 哈希存储，例如 `admin123` 的哈希值为 `240be518fabd2724ddb6f04eeb1da5967448d7e831c08c8fa822809f74c720a9`。

---

## 10. 部署指南

### 10.1 启动服务器

#### 直接启动

```bash
server.exe
```

#### 使用 `start_server.bat`（推荐）

`start_server.bat` 提供崩溃自动重启能力：

```bat
@echo off
cd /d "%~dp0"
:loop
start /b /wait server.exe
echo 服务器已停止，5秒后自动重启...
timeout /t 5 /nobreak >nul
goto loop
```

- 服务器异常退出后，等待 **5 秒**自动重启
- 适合长期无人值守运行

### 10.2 开机自启

使用 `install_autostart.bat` 注册 Windows 任务计划：

```bat
schtasks /create /tn "CampusManagementServer" /tr "\"%SCRIPT_DIR%\start_server.bat\"" /sc onlogon /rl highest /f
```

- **任务名称**：`CampusManagementServer`
- **触发条件**：用户登录时（`/sc onlogon`）
- **运行权限**：最高权限（`/rl highest`）
- **需要管理员权限运行**此脚本

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

项目提供 3 个 Python 测试脚本，覆盖功能、前端、权限三个维度。

### 11.1 `test_production.py` — 生产环境综合测试

**测试范围**：75 项测试用例，覆盖：

- **健康检查**：服务器连通性、基础接口可用性
- **认证模块**：登录/注册/Token 验证/登出
- **前端渲染**：4 个 HTML 页面加载与基础结构
- **API CRUD**：用户/积分/评价/商城/班级的增删改查
- **权限控制**：跨角色访问拦截
- **数据持久化**：SQLite 数据落盘验证
- **并发测试**：多线程请求稳定性
- **错误处理**：异常输入、边界条件

### 11.2 `test_frontend_redesign.py` — 前端重构验证测试

**测试范围**：28 项测试用例，覆盖：

- **页面渲染**：HTML 结构、样式加载、脚本执行
- **交互功能**：登录跳转、菜单切换、表单提交
- **移动端适配**：响应式布局、触摸操作
- **未登录重定向**：无 Token 访问自动跳转登录页

### 11.3 `test_permissions.py` — 权限漏洞专项测试

**测试范围**：权限安全专项验证，覆盖：

- 越权访问检测（学生访问管理员接口）
- Token 伪造/篡改检测
- 权限提升检测
- 跨角色数据访问检测

### 11.4 运行测试

```bash
# 确保服务器已启动
python test_production.py
python test_frontend_redesign.py
python test_permissions.py
```

> **前置条件**：测试脚本依赖 Python 3 + `requests` 库，运行前需确保服务器已启动并可访问。

---

## 附录

### 相关文档

- `docs/user_manual.md` — 用户使用手册

### 关键依赖版本

| 依赖 | 版本 | 说明 |
|------|------|------|
| cpp-httplib | 单头文件版 | HTTP 服务器 |
| nlohmann/json | 单头文件版 | JSON 解析 |
| SQLite3 | amalgamation 版 | 嵌入式数据库 |
| Vue 3 | CDN 版（本地化） | 前端框架 |
| Tailwind CSS | CDN 版（本地化） | CSS 框架 |
| ECharts | CDN 版（本地化） | 图表库 |

### 许可声明

本项目为校园内部管理系统，所有第三方库遵循其各自开源协议。
