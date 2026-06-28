# Checklist

## 移动端顶部右上角统一退出按钮
- [x] `student.html` 顶部右上角积分徽章已替换为退出登录按钮，点击调用 `logout()`
- [x] `teacher.html` 顶部右上角汉堡菜单已替换为退出登录按钮，点击调用 `logout()`
- [x] `admin.html` 顶部右上角汉堡菜单已替换为退出登录按钮，点击调用 `logout()`
- [x] `parent.html` 顶部右上角已是退出按钮，无需修改

## 底部导航栏统一激活样式
- [x] `student.html` 底部导航选中项使用 emerald 主题色圆角背景填充，nav-dot 圆点已移除
- [x] `teacher.html` 底部导航选中项使用 teal 主题色圆角背景填充，`::after` 伪元素已移除
- [x] `admin.html` 底部导航选中项使用 rose 主题色圆角背景填充，translateY 位移已移除，类切换方式统一为叠加 active 类
- [x] `parent.html` 底部导航选中项使用 indigo 主题色圆角背景填充，translateY 位移已移除
- [x] 四端未选中项保持灰色（text-stone-400），无背景填充

## 后端家长角色数据
- [x] `models.cpp` 的 `roles` 数组包含家长角色（id=4, name=家长）
- [x] `models.cpp` 的 `role_permissions` 数组包含 `{4, 3}` 和 `{4, 6}` 权限映射
- [x] `main.cpp` init_sql 中 roles 表和 role_permissions 表包含家长角色数据
- [x] GET /api/admin/roles 返回结果包含家长角色

## 管理员端家长角色显示
- [x] 角色管理界面显示家长角色卡片
- [x] 用户管理界面角色筛选下拉框包含家长选项
- [x] 用户列表中家长用户显示紫色"家长"标签
- [x] 添加用户对话框角色选择包含家长选项
- [x] 编辑用户对话框角色选择包含家长选项
