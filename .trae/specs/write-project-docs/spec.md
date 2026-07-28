# 编写完整项目说明文档 Spec

## Why
项目已有完整的后端 API（63 个路由端点）、前端页面（4 个 HTML）、数据库模型和构建系统，但缺乏一份完整的技术说明文档（README.md），导致新开发者难以快速理解项目架构、构建方式和 API 接口。现有的 `docs/user_manual.md` 仅面向最终用户，缺少面向开发者的技术文档。

## What Changes
- 在项目根目录创建 `README.md`，包含完整的项目技术说明文档
- 文档涵盖：项目简介、技术栈、架构设计、目录结构、构建指南、API 接口文档、数据库设计、部署指南、默认账号、测试说明

## Impact
- Affected code: `README.md`（新建）
- 不影响任何现有代码逻辑，纯文档新增

## ADDED Requirements

### Requirement: 项目技术说明文档
系统 SHALL 提供一份完整的 `README.md` 文档，包含以下章节：

#### Scenario: 新开发者阅读文档
- **WHEN** 新开发者打开项目根目录
- **THEN** 能通过 `README.md` 快速理解项目用途、技术栈、架构设计、构建方式和 API 接口

### Requirement: 文档章节完整性
文档 SHALL 包含以下章节，每个章节内容基于实际代码：

1. **项目简介** — 系统名称、用途、核心功能
2. **技术栈** — C++ httplib、SQLite、Vue 3、Tailwind CSS、ECharts、SHA256
3. **架构设计** — 后端模块化结构（config/logger/sha256/models/auth/routes）、前端单文件应用、RBAC 权限模型
4. **目录结构** — 所有源文件和资源文件的说明
5. **构建指南** — build.bat 的两种模式（SQLite/内存）、编译依赖、手动编译步骤
6. **配置说明** — config.json 各字段含义
7. **API 接口文档** — 按模块分组的 63 个端点（方法、路径、权限、说明）
8. **数据库设计** — 8 张表的结构（users/roles/permissions/role_permissions/points_records/evaluations/mall_items/redemption_records/classes）
9. **默认账号** — 三个角色的用户名/密码
10. **部署指南** — start_server.bat 自动重启、install_autostart.bat 开机自启
11. **测试说明** — test_production.py 和 test_frontend_redesign.py 的用途

#### Scenario: 查阅 API 接口
- **WHEN** 开发者需要查看某个 API 端点的信息
- **THEN** 能在文档的 API 接口章节找到该方法、路径、所需权限和功能说明

#### Scenario: 构建项目
- **WHEN** 开发者按照文档构建项目
- **THEN** 能通过 build.bat 或手动 g++ 命令成功编译出 server.exe
