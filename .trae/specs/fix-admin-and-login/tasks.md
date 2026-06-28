# Tasks

- [x] Task 1: 修复角色管理API返回权限列表（routes_admin.cpp）
  - [x] SubTask 1.1: 修改 `GET /api/admin/roles` 接口，为每个角色查询关联的 permissions 并组装到返回的 JSON 中（含 id, name, code）

- [x] Task 2: 管理员用户列表显示家长（routes_admin.cpp + admin.html）
  - [x] SubTask 2.1: 修改 `GET /api/admin/users` 接口，在返回用户列表时额外查询有 `parent_password_hash IS NOT NULL` 的学生记录，构建虚拟家长用户条目（role_id=4, name=学生姓名+"家长", username=家长手机号）
  - [x] SubTask 2.2: 确认 admin.html 用户列表能正确显示 role_id=4 的家长用户（紫色标签），无需额外前端修改

- [x] Task 3: 登录页自动跳转（index.html + routes_static.cpp）
  - [x] SubTask 3.1: 在 index.html 的 Vue app 中添加 `mounted()` 生命周期钩子
  - [x] SubTask 3.2: mounted 中调用 `/api/auth/me`（使用 fetch + credentials: 'include'），若返回 code=200 则根据 role_id 跳转对应页面
  - [x] SubTask 3.3: 在 routes_static.cpp 中添加 `svr.Get("/index.html", ...)` 路由，解决访问 /index.html 返回 404 的问题

- [x] Task 4: 统一登录表单（index.html）
  - [x] SubTask 4.1: 移除 `loginMode` 数据属性和 `switchMode` 方法，移除模式切换按钮和条件渲染
  - [x] SubTask 4.2: 改为单一登录表单：用户名/密码输入框，登录逻辑先调用 `/api/auth/login`，失败后自动调用 `/api/parent/login`
  - [x] SubTask 4.3: 保留密码显示切换和演示账号快捷填充功能

- [x] Task 5: 修复学生端退出按钮图标可见性（student.html）
  - [x] SubTask 5.1: 调整退出按钮图标颜色或背景，确保在浅色半透明背景上清晰可见（改用 emerald 主题色：bg-emerald-50/80 + text-emerald-600）

- [x] Task 6: 修复家长会话 role_id（routes_parent.cpp + routes_public.cpp）
  - [x] SubTask 6.1: routes_parent.cpp 家长登录时 create_session 传入 role_id=4（而非 user->role_id=3）
  - [x] SubTask 6.2: routes_public.cpp `/api/auth/me` 使用 get_session_info 检测家长会话，返回 role_id=4

- [x] Task 7: 验证
  - [x] SubTask 7.1: 使用 webapp-testing 验证管理员角色管理权限列表正常显示
  - [x] SubTask 7.2: 使用 webapp-testing 验证用户列表包含家长用户
  - [x] SubTask 7.3: 使用 webapp-testing 验证登录页已登录时自动跳转
  - [x] SubTask 7.4: 使用 webapp-testing 验证统一登录表单功能正常
  - [x] SubTask 7.5: 使用 webapp-testing 验证家长登录后会话 role_id=4
  - [x] SubTask 7.6: 使用 webapp-testing 验证学生端退出按钮使用 emerald 主题色
  - [x] SubTask 7.7: 编译验证无错误

# Task Dependencies
- Task 1 和 Task 2 可并行（都是 routes_admin.cpp 但不同接口）
- Task 3 和 Task 4 可并行（都是 index.html 但不同功能）
- Task 5 独立
- Task 6 独立（修复家长 role_id 问题）
- Task 7 依赖 Task 1-6 全部完成
