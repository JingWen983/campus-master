# Tasks

- [x] Task 1: 修复教师端 formatDateTime 渲染崩溃
  - [x] SubTask 1.1: 在 `teacher.html` 的 Vue `methods` 中新增 `formatDateTime(dateStr)` 方法，返回 `formatDateTime(dateStr)`（调用 common.js 全局函数）

- [x] Task 2: 删除家长端主页面重复的标签横栏
  - [x] SubTask 2.1: 在 `parent.html` 中删除主页面内容区的 tab-btn 横栏 section（包含 4 个标签按钮：积分记录/教师评价/兑换记录/家校留言）
  - [x] SubTask 2.2: 确保删除横栏后，侧边栏导航和移动端底部导航仍能正常切换标签

- [x] Task 3: 验证修复效果
  - [x] SubTask 3.1: 使用 Playwright 登录教师账号，访问 teacher.html，验证所有导航项可点击切换，无控制台错误
  - [x] SubTask 3.2: 使用 Playwright 登录家长账号，访问 parent.html，验证主页面无重复横栏，侧边栏导航功能正常

# Task Dependencies
- Task 1 和 Task 2 相互独立，可并行
- Task 3 依赖 Task 1 和 Task 2 完成
