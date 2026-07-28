# Checklist

## 阶段一：Bug 修复
- [x] admin.html 编辑用户对话框 modal-panel 含 `max-h-[90vh] overflow-y-auto`
- [x] 小屏下编辑学生对话框可垂直滚动到底部保存按钮
- [x] student.html 行为记录显示积分数值（正数绿色↑/负数橙色↓）
- [x] routes_student.cpp 积分记录权限码正确（不再误用 mall:manage）
- [x] routes_admin.cpp dashboard 返回 recentActivities 数组（含 description 和 time）
- [x] admin.html 仪表盘显示最近活动记录
- [x] teacher.html 积分记录正常显示（权限正确时）

## 阶段二：UID 重构
- [x] users.id 为 TEXT PRIMARY KEY，格式符合 admin-01/teacher-001/parent-001/student-01-02-05
- [x] classes 表含 grade_code、class_code 字段
- [x] teacher_classes 表存在且含 UNIQUE(teacher_id, class_id)
- [x] parent_students 表存在且含 UNIQUE(parent_id, student_id)
- [x] points_records.student_id/operator_id 为 TEXT
- [x] evaluations.student_id/evaluator_id 为 TEXT
- [x] redemption_records.student_id 为 TEXT
- [x] parent_messages.student_id/sender_id 为 TEXT
- [x] sessions.user_id/student_id 为 TEXT
- [x] User.id 为 string 类型
- [x] PointsRecord.student_id/operator_id 为 string 类型
- [x] user_id_map 为 unordered_map<string, User>
- [x] find_user_by_id 签名为 string
- [x] create_session/verify_session/get_session_info 签名为 string
- [x] verify_session 失败返回空串（非 -1）
- [x] URL 路由正则为 [^/]+（非 \d+）
- [x] 所有 SQL 拼接 user_id 用 '%s'（非 %d）
- [x] 新增 generate_user_id 函数
- [x] 数据迁移：检测旧 schema 自动删除重建（开发数据库方案）
- [x] 数据迁移：家长默认数据 parent-001 + parent_students 关联
- [x] 数据迁移：教师默认数据 teacher_classes 绑定记录
- [x] 编译通过无错误
- [x] admin/admin123 登录成功（id=admin-01）
- [x] teacher/teacher123 登录成功（id=teacher-001）
- [x] student/student123 登录成功（id=student-02-01-01）
- [x] parent/parent123 登录成功（id=parent-001）

## 阶段三：新功能
- [x] routes_admin.cpp 编辑教师 API 同步更新 teacher_classes
- [x] GET /api/admin/users 教师记录含 bound_class_ids
- [x] GET /api/teacher/my-classes 返回绑定班级列表
- [x] /api/teacher/students 仅返回绑定班级的学生
- [x] /api/teacher/points/records 按绑定班级过滤
- [x] /api/teacher/evaluations 按绑定班级过滤
- [x] /api/teacher/dashboard 统计限定绑定班级
- [x] /api/teacher/statistics 限定绑定班级
- [x] /api/teacher/parent-messages 限定绑定班级
- [x] routes_admin.cpp 家长 CRUD 支持 parent_students 绑定子女
- [x] routes_parent.cpp 登录用 parent-xxx + password_hash
- [x] routes_parent.cpp 子女查询通过 parent_students
- [x] admin.html 用户表格 ID 列显示字符串 id
- [x] admin.html 编辑教师对话框含绑定班级多选
- [x] admin.html 编辑家长对话框含绑定子女多选
- [x] admin.html 班级管理含 grade_code/class_code 输入
- [x] teacher.html 班级筛选动态加载（无硬编码 option）
- [x] index.html 家长演示账号为 parent/parent123
- [x] parent.html 适配新登录响应（走标准 /api/auth/login + session）
- [x] routes_public.cpp /api/auth/login 家长会话标记 is_parent=true

## 端到端验证
- [x] 创建各角色用户 ID 格式正确（admin-01/teacher-001/parent-001/student-02-01-01）
- [x] 教师仅看到绑定班级学生（王老师仅见高二(1)班张同学）
- [x] 家长登录查看多子女（parent-001 查看张同学）
- [x] 三端操作记录显示正常（teacher/admin/student/parent 均返回积分记录）
- [x] 编辑对话框可滚动（modal-panel 含 max-h-[90vh] overflow-y-auto）
- [x] 四端控制台无报错（admin/teacher/student/parent 均 0 错误）
