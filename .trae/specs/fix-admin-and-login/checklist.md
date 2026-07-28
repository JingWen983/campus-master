# Checklist

## 角色管理API返回权限列表
- [x] `GET /api/admin/roles` 返回的每个角色对象包含 `permissions` 数组
- [x] permissions 数组中每项包含 id, name, code 字段
- [x] admin.html 角色管理界面的权限列表徽章正常显示

## 管理员用户列表显示家长
- [x] `GET /api/admin/users` 返回的列表包含 role_id=4 的家长用户
- [x] 家长用户显示关联学生信息
- [x] admin.html 用户列表正确显示家长紫色标签
- [x] 用户筛选下拉框选择"家长"时能筛选出家长用户

## 登录页自动跳转
- [x] index.html 有 mounted 生命周期钩子
- [x] mounted 中调用 /api/auth/me 检查登录状态
- [x] 已登录用户访问登录页时自动跳转对应角色页面
- [x] 未登录用户正常显示登录表单
- [x] routes_static.cpp 已注册 /index.html 路由（不再 404）

## 统一登录表单
- [x] 移除 loginMode 和 switchMode 相关代码
- [x] 单一登录表单支持普通用户和家长登录
- [x] 普通用户登录成功后跳转对应角色页面
- [x] 家长登录成功后跳转 parent.html
- [x] 演示账号快捷填充功能正常

## 家长会话 role_id
- [x] routes_parent.cpp 家长登录时 create_session 传入 role_id=4
- [x] /api/auth/me 对家长会话返回 role_id=4
- [x] 家长会话能正确触发 mounted 钩子跳转到 /parent.html

## 学生端退出按钮图标
- [x] 退出按钮图标在浅色背景上清晰可见
- [x] 图标颜色/对比度与整体UI协调（emerald 主题色）

## 编译验证
- [x] 所有 .cpp 文件编译无错误
- [x] server.exe 链接成功
