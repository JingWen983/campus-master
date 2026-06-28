# 删除内存存储模式计划

## Summary
移除项目中所有 `#ifdef USE_SQLITE` / `#else` / `#endif` 条件编译分支，只保留 SQLite 数据库模式代码。删除 build.bat 中的内存模式编译选项。简化代码结构，消除内存模式下的功能缺陷和安全隐患。

## Current State Analysis
项目通过 `#ifdef USE_SQLITE` 条件编译支持两种存储模式：SQLite 数据库模式（默认）和内存模式（降级版）。共 9 个文件、约 69 处条件编译块。内存模式是 SQLite 模式的简化子集，存在以下问题：
- 家长登录跳过密码验证（安全隐患）
- 评价、留言、兑换记录等功能不实际存储
- 商城商品等使用硬编码 mock 数据
- 班级管理不实际存储

内存数据结构（`vector<User> users` 等）在两种模式下都被用作数据库缓存，**不能删除**，只需移除条件编译包裹。

## Proposed Changes

### 1. sqlite_wrapper.h — 移除 9 处条件编译
- 移除 `#ifdef USE_SQLITE` / `#else` / `#endif` 包裹
- 保留 SQLite 实现（`sqlite3*` 相关代码）
- 移除 `#else` 分支中的空实现和 `void* db_` 成员
- 保留 `escapeString` 方法（无条件编译包裹，不受影响）

### 2. models.h — 移除 2 处条件编译
- 行 11-14：移除 `#ifdef` 包裹，直接 `#include "sqlite_wrapper.h"` 和 `extern SqliteDb db;`
- 行 75-80：移除 `#ifdef` 包裹，直接声明 4 个数据库辅助函数原型

### 3. models.cpp — 移除 1 处条件编译
- 行 49-88：移除 `#ifdef` / `#endif` 包裹，保留 `load_users_from_db`、`save_user_to_db`、`delete_user_from_db`、`update_user_points_in_db` 函数实现
- **保留** `vector<User> users` 等全局数据结构定义和初始数据（作为内存缓存和兜底数据）

### 4. main.cpp — 移除 2 处条件编译
- 行 17-20：移除 `#ifdef` 包裹，直接定义 `SqliteDb db;` 全局实例
- 行 36-204：移除 `#ifdef` / `#endif` 包裹，保留数据库初始化逻辑（建表、索引、默认数据）

### 5. routes_admin.cpp — 移除 21 处条件编译
- 逐个移除每个 `#ifdef USE_SQLITE` / `#else` / `#endif` 块
- 保留 `#ifdef` 分支的 SQLite 代码，删除 `#else` 分支的内存模式代码
- 涉及：商城CRUD、用户CRUD、数据导入导出、班级CRUD、仪表盘统计、系统备份等

### 6. routes_teacher.cpp — 移除 15 处条件编译
- 同上，保留 SQLite 分支，删除内存模式分支
- 涉及：学生管理、积分操作、评价管理、数据统计、家长留言等

### 7. routes_parent.cpp — 移除 9 处条件编译
- 同上，保留 SQLite 分支
- 涉及：越权校验、家长登录、子女信息、积分记录、评价、兑换、留言等

### 8. routes_public.cpp — 移除 6 处条件编译
- 同上，保留 SQLite 分支
- 涉及：注册、积分记录、商城列表、兑换、风采榜等

### 9. routes_student.cpp — 移除 4 处条件编译
- 同上，保留 SQLite 分支
- 涉及：积分记录、评价信息、商城列表、兑换记录

### 10. build.bat — 简化构建脚本
- 移除内存模式编译选项（选项2，约行 129-200）
- 移除菜单选择逻辑，直接执行 SQLite 模式编译
- 保留 sqlite3.c 编译和 `-DUSE_SQLITE` 标志（或移除 `-DUSE_SQLITE`，因为不再需要条件编译）

### 11. README.md — 更新文档
- 移除内存模式编译说明（约行 211-243）
- 仅保留 SQLite 模式说明

## Assumptions & Decisions
1. **保留内存数据结构**：`vector<User> users`、`vector<Role> roles` 等全局变量在 SQLite 模式下仍作为内存缓存使用，不删除
2. **保留 models.cpp 中的初始数据**：`roles`、`permissions`、`role_permissions` 的硬编码数据在 SQLite 模式下也被使用（不从数据库重新加载），保留作为兜底
3. **移除 `-DUSE_SQLITE` 宏**：删除条件编译后不再需要此宏定义，build.bat 中移除该编译标志
4. **不修改 routes_static.cpp 和 routes.h**：这两个文件无条件编译指令

## Verification Steps
1. 编译验证：使用 build.bat 编译项目，确保无编译错误
2. 启动服务器：运行 server.exe，确认数据库初始化正常
3. 功能验证：
   - 管理员登录，用户管理/班级管理/商城管理正常
   - 教师登录，学生管理/积分操作/评价/留言正常
   - 学生登录，查看积分/评价/商城/兑换正常
   - 家长登录，密码验证生效，查看子女数据正常
4. 代码检查：全文搜索 `USE_SQLITE`、`#ifdef`、`#else`、`#endif`，确认无遗留条件编译
