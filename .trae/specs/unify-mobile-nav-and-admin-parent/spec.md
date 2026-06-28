# 统一移动端导航与管理员家长角色 Spec

## Why
各端（学生/教师/管理员/家长）移动端顶部右上角显示内容不一致（积分/汉堡菜单/退出），且底部导航栏激活样式各异（颜色、位移、指示器不同，无图标填充）。同时管理员端用户管理和角色管理界面可能缺少家长角色的后端数据支持，导致前端硬编码的家长角色被 API 返回数据覆盖。

## What Changes
- 将学生端、教师端、管理员端移动端顶部右上角统一改为"退出登录"按钮（家长端已是退出，无需改动）
- 统一四端底部导航栏激活样式：选中项图标以各自主题色填充（背景色 + 文字色），移除位移和额外指示器
- 确保后端角色管理 API 返回家长角色（role_id=4），避免前端硬编码数据被覆盖
- 确保后端 role_permissions 包含家长角色的权限映射

## Impact
- Affected code: `student.html`, `teacher.html`, `admin.html`, `parent.html`（移动端导航部分）
- Affected code: `models.cpp`（角色权限数据）, `routes_admin.cpp`（角色管理 API）
- Affected specs: `add-parent-portal`（家长端基础功能已实现）

## ADDED Requirements

### Requirement: 统一移动端顶部右上角退出按钮
所有端（学生/教师/管理员/家长）的移动端顶部右上角 SHALL 显示"退出登录"按钮，点击后调用 `logout()` 方法。

#### Scenario: 学生端退出
- **WHEN** 学生在手机端点击右上角退出按钮
- **THEN** 清除登录状态并跳转到登录页

#### Scenario: 教师端退出
- **WHEN** 教师在手机端点击右上角退出按钮
- **THEN** 清除登录状态并跳转到登录页

#### Scenario: 管理员端退出
- **WHEN** 管理员在手机端点击右上角退出按钮
- **THEN** 清除登录状态并跳转到登录页

### Requirement: 统一底部导航栏激活样式
所有端底部导航栏的选中项 SHALL 以主题色背景填充图标区域，未选中项保持灰色。

#### Scenario: 切换标签页
- **WHEN** 用户点击底部导航栏的某个标签
- **THEN** 该标签图标获得主题色背景填充（圆角矩形背景），文字变为主题色
- **AND** 其他标签恢复灰色，无背景填充

### Requirement: 后端返回家长角色数据
角色管理 API（GET /api/admin/roles）SHALL 返回家长角色（id=4, name=家长）及其权限列表。

#### Scenario: 获取角色列表
- **WHEN** 管理员请求角色列表
- **THEN** 返回结果包含家长角色（id=4, name=家长, description=家长角色...）
- **AND** 家长角色包含权限映射（学生管理、商城管理）

## MODIFIED Requirements

### Requirement: 各端移动端顶部导航
学生端右上角从积分徽章改为退出按钮；教师端和管理员端右上角从汉堡菜单改为退出按钮。汉堡菜单原有的侧边栏切换功能由底部导航栏替代（移动端不显示侧边栏，仅通过底部导航切换）。

### Requirement: 各端底部导航栏激活样式
统一为：选中项图标使用主题色圆角背景填充（如 `bg-emerald-100 text-emerald-600`），移除 translateY 位移和 `::after` 伪元素指示器。各端主题色：
- 学生端：emerald（祖母绿）
- 教师端：teal（青绿）
- 管理员端：rose（玫瑰红）
- 家长端：indigo（靛蓝）

### Requirement: 后端角色权限映射
`models.cpp` 的 `role_permissions` 数组 SHALL 包含家长角色的权限映射 `{4, 3}` 和 `{4, 6}`（学生管理 + 商城管理）。
