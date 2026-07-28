# 用户ID重构与教师班级绑定 Spec

## Why
当前系统存在三类问题：(1) 三端操作记录无法正常显示（字段名不匹配、后端未返回数据）；(2) 家长是虚拟用户无法直接编辑；(3) 教师只能隐式关联单个班级、可查看全部学生无权限隔离。同时现有整数自增 ID 缺乏业务语义，无法直观识别角色/年级/班级。本变更通过重构用户 ID 为业务编码、将家长升级为真实用户、新增教师-班级多对多绑定，一次性解决上述问题并建立清晰的权限边界。

## What Changes

### Bug 修复
- 修复 admin/teacher/student 三端操作记录无法显示（字段名不匹配、后端缺数据、权限码错误）
- 修复管理员编辑用户对话框内容超出视口无法滚动（增加 max-h + overflow-y-auto）
- 修复管理员端家长用户可直接编辑（家长升级为真实用户后走标准编辑流程）

### 新功能
- 教师绑定多班级：新增 teacher_classes 关联表，教师端按绑定班级筛选学生/记录，未绑定班级的学生不可见
- 管理员端可配置教师绑定的班级（编辑教师时多选班级）

### **BREAKING**：用户 ID 系统重构
- `users.id` 由 `INTEGER AUTOINCREMENT` 改为 `TEXT PRIMARY KEY`，格式：
  - 管理员：`admin-01`（角色-编号，2位序号）
  - 教师：`teacher-001`（角色-编号，3位序号）
  - 家长：`parent-001`（角色-编号，3位序号）
  - 学生：`student-01-02-05`（角色-年级-班级-编号，各2位）
- 家长从"虚拟用户"升级为真实用户（独立 id、密码、role_id=4）
- 新增 `parent_students` 关联表（家长-学生多对多）
- `classes` 表新增 `grade_code`/`class_code` 字段用于生成学生 ID
- 6 张关联表的外键字段全部由 INTEGER 改为 TEXT
- C++ `User.id`、`PointsRecord.student_id/operator_id` 由 int 改为 string
- 所有 `find_user_by_id`、`create_session`、`verify_session` 等函数签名改为 string
- URL 路由正则由 `\d+` 改为 `[^/]+`

## Impact
- **Affected specs**: add-parent-portal（家长模型变更）、fix-admin-and-login（用户管理变更）
- **Affected code**:
  - 数据库: main.cpp（表结构/迁移）、models.h/cpp（结构体/索引/函数）
  - 认证: auth.h（session/权限中间件）
  - 路由: routes_admin.cpp、routes_teacher.cpp、routes_student.cpp、routes_parent.cpp、routes_public.cpp
  - 前端: admin.html、teacher.html、student.html、parent.html、index.html
  - 登录: 家长登录改用 parent-001 + 家长密码（不再用学生学号+家长密码）

## ADDED Requirements

### Requirement: 用户 ID 业务编码
系统 SHALL 为每个用户生成符合 `角色-编号` 或 `角色-年级-班级-编号` 格式的字符串主键。

#### Scenario: 创建学生
- **WHEN** 管理员创建一个高二(1)班的学生，且该班级已有 4 名学生
- **THEN** 新学生 id 为 `student-02-01-05`（02=高二, 01=1班, 05=序号）

#### Scenario: 创建教师
- **WHEN** 管理员创建第 3 个教师
- **THEN** 新教师 id 为 `teacher-003`

#### Scenario: 创建家长
- **WHEN** 管理员创建第 2 个家长
- **THEN** 新家长 id 为 `parent-002`

#### Scenario: 创建管理员
- **WHEN** 创建第 2 个管理员
- **THEN** 新管理员 id 为 `admin-02`

### Requirement: 家长真实用户模型
系统 SHALL 将家长存储为 users 表中 role_id=4 的真实用户，拥有独立 id、密码、姓名。

#### Scenario: 家长登录
- **WHEN** 家长用 id `parent-001` 和家长密码登录
- **THEN** 登录成功，跳转 parent.html，可查看通过 parent_students 关联的所有子女

#### Scenario: 家长绑定子女
- **WHEN** 管理员编辑家长时勾选若干学生
- **THEN** parent_students 表插入对应 (parent_id, student_id) 记录，家长端可见这些学生

### Requirement: 教师多班级绑定
系统 SHALL 通过 teacher_classes 关联表支持教师绑定多个班级。

#### Scenario: 教师查看学生
- **WHEN** 教师绑定高二(1)班和高二(2)班，查看学生列表
- **THEN** 仅返回这两个班级的学生，其他班级学生不可见

#### Scenario: 管理员配置教师班级
- **WHEN** 管理员编辑教师时勾选多个班级并保存
- **THEN** teacher_classes 表更新为所选班级集合

#### Scenario: 教师筛选班级
- **WHEN** 教师在学生列表选择"高二(2)班"筛选
- **THEN** 仅显示高二(2)班学生（且必须是该教师绑定的班级）

### Requirement: 班级编号字段
系统 SHALL 在 classes 表新增 grade_code 和 class_code 字段用于 ID 生成和展示。

#### Scenario: 年级编码映射
- 高一→01, 高二→02, 高三→03, 初一→07, 初二→08, 初三→09

#### Scenario: 班级编码
- "高二(1)班" → grade_code=02, class_code=01

### Requirement: 操作记录正确显示
系统 SHALL 在三端正确显示操作/行为记录。

#### Scenario: 学生查看行为记录
- **WHEN** 学生进入行为记录页面
- **THEN** 显示积分数值（正数绿色+向上箭头，负数橙色+向下箭头）、原因、时间

#### Scenario: 教师查看积分记录
- **WHEN** 教师进入积分管理页面
- **THEN** 显示学生姓名、班级、积分、原因、时间、操作人

#### Scenario: 管理员查看最近活动
- **WHEN** 管理员进入仪表盘
- **THEN** 显示最近 N 条活动记录（含描述和时间）

### Requirement: 编辑用户对话框可滚动
系统 SHALL 保证编辑用户对话框在内容超出视口时可滚动查看。

#### Scenario: 小屏编辑学生
- **WHEN** 在窗口高度不足时打开编辑学生对话框（含家长绑定字段）
- **THEN** 对话框内部可垂直滚动，能看到顶部字段和底部保存按钮

## MODIFIED Requirements

### Requirement: 用户管理
管理员编辑用户时：家长走标准编辑流程（真实用户）；教师编辑时可多选绑定班级；学生编辑时可绑定家长（通过 parent_students）；对话框可滚动。

### Requirement: 登录认证
登录使用字符串 user_id；家长用 parent-xxx + 家长密码登录（不再用学生学号+家长密码）；session 表 user_id/student_id 改为 TEXT。

### Requirement: 教师端数据权限
教师端所有学生相关数据（学生列表、积分记录、评价、留言、统计）均按 teacher_classes 绑定的班级范围过滤。

## REMOVED Requirements

### Requirement: 虚拟家长用户
**Reason**: 家长升级为真实用户，不再通过学生记录的 parent_password_hash/parent_phone 构建。
**Migration**: 
- 将现有 parent_phone 相同的学生分组，每组创建一个 parent-xxx 真实用户
- 在 parent_students 插入 (parent_id, student_id) 关联
- 家长密码沿用原 parent_password_hash
- users 表保留 parent_password_hash/parent_phone 字段一段时间用于迁移，后续删除

### Requirement: 教师单班级 className 隐式关联
**Reason**: 改用 teacher_classes 显式多对多关系。
**Migration**: 现有教师 className 值（如"高二(1)班"）迁移为 teacher_classes 中一条绑定记录。
