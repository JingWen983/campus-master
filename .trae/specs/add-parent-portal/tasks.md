# Tasks

- [x] Task 1: 数据库与认证基础改造
  - [x] SubTask 1.1: 在 `main.cpp` 的 init_sql 中为 users 表新增 `parent_password_hash` TEXT 和 `parent_phone` TEXT 字段（使用 ALTER TABLE ... ADD COLUMN 兼容已存在数据库）
  - [x] SubTask 1.2: 在 init_sql 中创建 `parent_messages` 表（含 student_id/sender_type/sender_id/content/reply_to/read_status/created_at 字段）
  - [x] SubTask 1.3: 在 init_sql 中为默认 student 账号设置 parent_password_hash（parent123 的 SHA256）和 parent_phone（13800000001）
  - [x] SubTask 1.4: 在 `auth.h` 新增 `generate_parent_token`、`verify_parent_token`、`get_student_id_from_parent_token`、`check_parent_auth_middleware` 函数

- [x] Task 2: 后端家长端 API 实现
  - [x] SubTask 2.1: 创建 `routes_parent.cpp` 文件，实现 `register_parent_routes` 函数
  - [x] SubTask 2.2: 实现 `POST /api/parent/login`（验证家长密码，返回 ptoken_ + children 列表）
  - [x] SubTask 2.3: 实现 `GET /api/parent/children`（返回同 parent_phone 的所有学生）
  - [x] SubTask 2.4: 实现 `GET /api/parent/student/{id}/info`（含越权校验：{id} 须与登录子女同 parent_phone）
  - [x] SubTask 2.5: 实现 `GET /api/parent/student/{id}/points`（积分记录，复用 routes_student.cpp 的查询逻辑）
  - [x] SubTask 2.6: 实现 `GET /api/parent/student/{id}/evaluation`（评价记录）
  - [x] SubTask 2.7: 实现 `GET /api/parent/student/{id}/redemptions`（兑换记录）
  - [x] SubTask 2.8: 实现 `GET /api/parent/student/{id}/messages` 和 `POST /api/parent/student/{id}/messages`（留言查询与发送）

- [x] Task 3: 后端教师端留言 API 实现
  - [x] SubTask 3.1: 在 `routes_teacher.cpp` 新增 `GET /api/teacher/parent-messages`（查询教师所辖班级学生的留言，按未读优先+时间倒序）
  - [x] SubTask 3.2: 新增 `POST /api/teacher/parent-messages/{id}/reply`（教师回复留言，原留言标记已读）
  - [x] SubTask 3.3: 新增 `PUT /api/teacher/parent-messages/{id}/read`（标记留言为已读）

- [x] Task 4: 后端模块集成
  - [x] SubTask 4.1: 在 `routes.h` 新增 `register_parent_routes` 函数声明
  - [x] SubTask 4.2: 在 `main.cpp` 调用 `register_parent_routes(svr)`
  - [x] SubTask 4.3: 在 `routes_static.cpp` 新增 `GET /parent.html` 静态路由
  - [x] SubTask 4.4: 更新 `build.bat` 编译列表新增 `routes_parent.cpp`

- [x] Task 5: 前端家长端页面实现
  - [x] SubTask 5.1: 创建 `parent.html`，使用 Vue 3 + Tailwind（本地化），延续 Vibrant Campus 设计风格（建议使用 indigo/violet 主色调区分家长端）
  - [x] SubTask 5.2: 实现子女切换栏（顶部横向卡片列表，点击切换）
  - [x] SubTask 5.3: 实现子女信息卡（姓名/班级/积分/班级排名）
  - [x] SubTask 5.4: 实现积分记录表格（时间/积分/类型/原因）
  - [x] SubTask 5.5: 实现教师评价卡片列表（维度/分数/评语/评价人/时间）
  - [x] SubTask 5.6: 实现兑换记录表格（时间/商品/消耗积分）
  - [x] SubTask 5.7: 实现留言对话区（消息列表 + 底部输入框，发送调用 POST API）

- [x] Task 6: 前端登录页与公共库更新
  - [x] SubTask 6.1: 在 `index.html` 增加"家长登录"按钮，点击切换为子女学号+家长密码表单，登录成功跳转 `/parent.html`
  - [x] SubTask 6.2: 在 `lib/common.js` 的 `checkAuth` 函数支持 role_id=4（家长），roleNames 增加 `{ 4: '家长' }`

- [x] Task 7: 前端教师工作台留言管理
  - [x] SubTask 7.1: 在 `teacher.html` 新增"家长留言"标签页
  - [x] SubTask 7.2: 实现留言列表加载（未读优先+时间倒序），支持点击回复（弹窗输入内容）

- [x] Task 8: 编译验证与测试
  - [x] SubTask 8.1: 运行 build.bat 编译项目，确保无编译错误
  - [x] SubTask 8.2: 启动 server.exe，手动测试家长登录、子女切换、各数据查看、留言发送
  - [x] SubTask 8.3: 手动测试教师端留言查看与回复功能

# Task Dependencies
- Task 2 依赖 Task 1（需要认证中间件和数据库字段）
- Task 3 依赖 Task 1（需要 parent_messages 表）
- Task 4 依赖 Task 2（需要 routes_parent.cpp 已实现）
- Task 5 依赖 Task 4（后端 API 可用）
- Task 6 可与 Task 5 并行
- Task 7 依赖 Task 3（教师端留言 API 可用）
- Task 8 依赖 Task 4、5、6、7 全部完成
