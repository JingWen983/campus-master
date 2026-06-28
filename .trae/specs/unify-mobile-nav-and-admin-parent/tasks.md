# Tasks

- [x] Task 1: 统一学生端移动端导航（student.html）
  - [x] SubTask 1.1: 将顶部右上角的积分徽章替换为退出登录按钮（约 line 419-429），调用 `logout()` 方法，样式参考家长端退出按钮
  - [x] SubTask 1.2: 修改底部导航栏激活样式，选中项图标使用 emerald 主题色圆角背景填充（`bg-emerald-100 text-emerald-600`），移除 nav-dot 圆点指示器（约 line 284-294, 729-750）

- [x] Task 2: 统一教师端移动端导航（teacher.html）
  - [x] SubTask 2.1: 将顶部右上角的汉堡菜单按钮替换为退出登录按钮（约 line 368-378），调用 `logout()` 方法
  - [x] SubTask 2.2: 修改底部导航栏激活样式，选中项图标使用 teal 主题色圆角背景填充（`bg-teal-100 text-teal-600`），移除 `::after` 伪元素指示器（约 line 270-279, 836-856）

- [x] Task 3: 统一管理员端移动端导航（admin.html）
  - [x] SubTask 3.1: 将顶部右上角的汉堡菜单按钮替换为退出登录按钮（约 line 380-390），调用 `logout()` 方法
  - [x] SubTask 3.2: 修改底部导航栏激活样式，选中项图标使用 rose 主题色圆角背景填充（`bg-rose-100 text-rose-600`），移除 translateY 位移（约 line 284-287, 1048-1067），统一类切换方式为叠加 active 类

- [x] Task 4: 统一家长端底部导航样式（parent.html）
  - [x] SubTask 4.1: 修改底部导航栏激活样式，选中项图标使用 indigo 主题色圆角背景填充（`bg-indigo-100 text-indigo-600`），移除 translateY 位移（约 line 376-378, 828-845）

- [x] Task 5: 后端家长角色数据补全（models.cpp）
  - [x] SubTask 5.1: 在 `models.cpp` 的 `roles` 数组中添加家长角色（id=4, name=家长, description=家长角色，查看孩子学习成绩和积分情况）
  - [x] SubTask 5.2: 在 `models.cpp` 的 `role_permissions` 数组中添加家长角色权限映射 `{4, 3}` 和 `{4, 6}`（学生管理 + 商城管理）
  - [x] SubTask 5.3: 在 `main.cpp` 的 init_sql 中为 roles 表插入家长角色记录（INSERT OR IGNORE），为 role_permissions 表插入家长权限映射

- [x] Task 6: 验证与测试
  - [x] SubTask 6.1: 验证四端移动端顶部右上角均显示退出按钮，点击可正常退出
  - [x] SubTask 6.2: 验证四端底部导航栏选中项以主题色背景填充，切换标签时样式正确
  - [x] SubTask 6.3: 验证管理员端角色管理界面显示家长角色卡片
  - [x] SubTask 6.4: 验证管理员端用户管理界面可筛选、添加、编辑家长角色用户

# Task Dependencies
- Task 1-4 相互独立，可并行
- Task 5 独立于前端任务，可并行
- Task 6 依赖 Task 1-5 全部完成
