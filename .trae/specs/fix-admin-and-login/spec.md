# 管理员端修复与登录页改进 Spec

## Why
管理员端用户列表无法显示家长（设计缺陷：家长非独立用户），角色管理权限列表为空（API未返回permissions）。登录页缺少已登录自动跳转功能。学生端退出按钮图标对比度低。

## What Changes
- 修复 `GET /api/admin/roles` API，使其返回每个角色的 permissions 数组
- 管理员端用户列表支持显示家长身份（通过查询有 parent_password_hash 的学生记录来展示家长用户）
- 登录页（index.html）添加 mounted 钩子，调用 `/api/auth/me` 检查已登录状态，自动跳转对应角色页面
- 在 routes_static.cpp 中注册 `/index.html` 路由，解决访问 /index.html 返回 404 导致 mounted 钩子无法触发的问题
- 学生端退出按钮图标增加可见性（调整颜色/对比度，改用 emerald 主题色）
- 统一登录页：将家长登录与普通登录合并为单表单，不再需要模式切换
- 修复家长会话 role_id：routes_parent.cpp 家长登录时 create_session 传入 role_id=4；routes_public.cpp `/api/auth/me` 使用 get_session_info 检测家长会话并返回 role_id=4

## Impact
- Affected code: `routes_admin.cpp`（角色API+用户API）、`admin.html`（前端显示）、`index.html`（登录页自动跳转+统一表单）、`student.html`（按钮图标）、`lib/common.js`

## ADDED Requirements

### Requirement: 登录页自动跳转
index.html SHALL 在页面加载时调用 `/api/auth/me` 检查会话有效性，若已登录则自动跳转到对应角色页面。

#### Scenario: 已登录用户访问登录页
- **WHEN** 已登录用户访问 `/`（index.html）
- **THEN** 页面自动跳转到 `roleRoutes[role_id]` 对应的仪表盘页面
- **AND** 未登录用户正常显示登录表单

### Requirement: 统一登录表单
index.html SHALL 使用单一登录表单，根据输入的用户名自动判断角色（普通用户/家长），不再需要模式切换按钮。

#### Scenario: 家长登录
- **WHEN** 家长在统一表单中输入子女学号和家长密码
- **THEN** 系统先尝试普通登录，失败后自动尝试家长登录
- **AND** 成功后跳转到 parent.html

#### Scenario: 普通用户登录
- **WHEN** 用户输入用户名和密码
- **THEN** 系统执行普通登录
- **AND** 成功后跳转到对应角色页面

## MODIFIED Requirements

### Requirement: 角色管理API返回权限列表
`GET /api/admin/roles` SHALL 返回每个角色对象包含 `permissions` 数组（含 permission 的 id, name, code）。

#### Scenario: 获取角色列表
- **WHEN** 管理员请求角色列表
- **THEN** 每个角色对象包含 `permissions` 数组
- **AND** 权限数组包含该角色关联的所有权限的 id/name/code

### Requirement: 管理员用户列表显示家长
管理员端用户列表 SHALL 显示家长用户。家长用户通过查询 users 表中 `parent_password_hash IS NOT NULL` 的学生记录来构建虚拟家长用户条目。

#### Scenario: 查看用户列表
- **WHEN** 管理员查看用户列表
- **THEN** 列表中包含家长用户条目（role_id=4）
- **AND** 家长用户显示关联的学生姓名和家长手机号

### Requirement: 学生端退出按钮图标可见性
student.html 退出按钮图标 SHALL 有足够的视觉对比度，确保在浅色背景上清晰可见。

## REMOVED Requirements

### Requirement: 家长登录模式切换
**Reason**: 统一为单表单登录，不再需要 normal/parent 模式切换
**Migration**: 移除 `loginMode` 数据属性和 `switchMode` 方法，改为单表单自动判断
