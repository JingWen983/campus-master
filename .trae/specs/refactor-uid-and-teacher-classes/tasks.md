# Tasks

## 阶段一：独立 Bug 修复（可并行，不依赖 UID 重构）

- [x] Task 1: 修复管理员编辑用户对话框滚动问题
  - [x] 1.1: 在 admin.html 编辑用户对话框 modal-panel 增加 `max-h-[90vh] overflow-y-auto`
  - [x] 1.2: 验证小屏下编辑学生（含家长绑定字段）可滚动

- [x] Task 2: 修复三端操作记录显示
  - [x] 2.1: 修复 student.html 字段名：`record.score` → `record.points`（student.html:481,488,489）
  - [x] 2.2: 修复 routes_student.cpp:61 权限码 `mall:manage` → `student:manage`（或移除权限校验）
  - [x] 2.3: 修复 routes_admin.cpp dashboard 返回 recentActivities 数组（查询最近 10 条 points_records）
  - [x] 2.4: 验证三端记录正常显示

## 阶段二：UID 系统重构（ foundational，后续任务依赖）

- [x] Task 3: 数据库表结构重构（main.cpp）
  - [x] 3.1: users.id 改为 `TEXT PRIMARY KEY`，去掉 AUTOINCREMENT
  - [x] 3.2: classes 表新增 `grade_code TEXT`、`class_code TEXT` 字段
  - [x] 3.3: 新增 `teacher_classes` 表（teacher_id TEXT, class_id INTEGER, UNIQUE(teacher_id, class_id)）
  - [x] 3.4: 新增 `parent_students` 表（parent_id TEXT, student_id TEXT, UNIQUE(parent_id, student_id)）
  - [x] 3.5: points_records/evaluations/redemption_records/parent_messages/sessions 的 user_id/student_id/operator_id/evaluator_id 字段改为 TEXT
  - [x] 3.6: 数据迁移：检测旧 schema 自动删除重建（开发数据库方案）
  - [x] 3.7: 家长默认数据：parent-001 + parent_students 关联
  - [x] 3.8: 教师默认数据：teacher_classes 绑定记录
  - [x] 3.9: 默认数据改用新 ID 格式（admin-01, teacher-001, student-02-01-01, parent-001）

- [x] Task 4: C++ 数据结构与函数签名重构（models.h, models.cpp）
  - [x] 4.1: User.id 改为 string；PointsRecord.student_id/operator_id 改为 string
  - [x] 4.2: user_id_map 改为 unordered_map<string, User>
  - [x] 4.3: find_user_by_id/delete_user_from_db/update_user_points_in_db/remove_user_index/check_permission_optimized 签名改 string
  - [x] 4.4: load_users_from_db/save_user_to_db SQL 适配 TEXT id
  - [x] 4.5: 新增 ID 生成函数 `generate_user_id(role, grade_code, class_code)` 

- [x] Task 5: 认证模块重构（auth.h）
  - [x] 5.1: create_session/verify_session/get_session_info 签名改 string（verify_session 返回空串表示失败）
  - [x] 5.2: Auth::check_permission/check_role/check_permission_middleware/check_parent_auth_middleware 改 string
  - [x] 5.3: sessions 表 SQL 拼接 %d → '%s'

- [x] Task 6: 路由层重构（routes_*.cpp）
  - [x] 6.1: routes_admin.cpp：stoi(req.matches[1]) → 直接字符串；req_json.value("id",0) → value("id","")；URL 正则 \d+ → [^/]+；SQL %d → '%s'
  - [x] 6.2: routes_teacher.cpp：同上适配
  - [x] 6.3: routes_student.cpp：verify_session 返回值改 string
  - [x] 6.4: routes_parent.cpp：家长登录改用 parent-xxx + password_hash；check_same_parent 改 string
  - [x] 6.5: routes_public.cpp：注册 API 返回字符串 id；登录响应适配

- [x] Task 7: 编译验证 UID 重构
  - [x] 7.1: 编译通过无错误
  - [x] 7.2: 服务器启动，默认账号登录正常（admin-01/admin123, teacher-001/teacher123, student-02-01-01/student123, parent-001/parent123）
  - [x] 7.3: 数据库迁移成功（旧 schema 检测后自动重建）

## 阶段三：新功能（依赖 UID 重构）

- [x] Task 8: 教师班级绑定后端（routes_admin.cpp, routes_teacher.cpp）
  - [x] 8.1: routes_admin.cpp 编辑教师 API：保存时同步更新 teacher_classes（先删后插）
  - [x] 8.2: routes_admin.cpp GET /api/admin/users 返回教师时附带 bound_class_ids 数组
  - [x] 8.3: routes_teacher.cpp 新增 GET /api/teacher/my-classes 返回当前教师绑定班级
  - [x] 8.4: routes_teacher.cpp /api/teacher/students 按绑定班级过滤
  - [x] 8.5: routes_teacher.cpp /api/teacher/points/records、/evaluations、/dashboard、/statistics、/parent-messages 按绑定班级过滤

- [x] Task 9: 家长真实用户后端（routes_admin.cpp, routes_parent.cpp）
  - [x] 9.1: routes_admin.cpp 家长 CRUD：创建/编辑/删除家长真实用户；编辑时可多选绑定子女
  - [x] 9.2: routes_parent.cpp 登录已用 parent-xxx + password_hash（Task 6 完成）
  - [x] 9.3: routes_parent.cpp 子女查询通过 parent_students（Task 6 完成）

- [x] Task 10: 前端适配（admin.html, teacher.html, parent.html, index.html）
  - [x] 10.1: admin.html 用户管理表格 ID 列直接显示字符串 id（移除家长 "—" 逻辑）
  - [x] 10.2: admin.html 编辑教师对话框新增"绑定班级"多选组件
  - [x] 10.3: admin.html 编辑家长对话框新增"绑定子女"多选组件
  - [x] 10.4: admin.html 班级管理新增 grade_code/class_code 字段输入
  - [x] 10.5: teacher.html 班级筛选下拉改为动态加载（从 /api/teacher/my-classes）
  - [x] 10.6: teacher.html 移除硬编码的 3 个班级 option
  - [x] 10.7: index.html 家长登录演示账号改为 parent-001/parent123
  - [x] 10.8: parent.html 无需修改（走标准 /api/auth/login + session）

- [x] Task 11: 端到端验证
  - [x] 11.1: 管理员创建/编辑/删除各角色用户，ID 格式正确
  - [x] 11.2: 教师绑定多班级后仅看到绑定班级学生
  - [x] 11.3: 家长登录查看多子女数据
  - [x] 11.4: 三端操作记录正常显示
  - [x] 11.5: 编辑对话框可滚动
  - [x] 11.6: 控制台无报错

# Task Dependencies
- Task 1, Task 2 独立，可并行
- Task 3→4→5→6→7 串行（UID 重构链）
- Task 8, 9 依赖 Task 7 完成
- Task 10 依赖 Task 8, 9
- Task 11 依赖 Task 10
