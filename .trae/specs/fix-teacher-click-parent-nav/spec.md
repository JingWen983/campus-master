# 修复教师端点击与家长端重复导航 Spec

## Why
上一轮新增家长留言功能后，教师端页面因 Vue 渲染崩溃导致所有导航项无法点击；家长端页面在侧边栏布局重构后，主页面仍保留了原有的标签横栏，与侧边栏导航内容完全重复，造成视觉冗余和交互混乱。

## What Changes
- **教师端修复**：在 `teacher.html` 的 Vue `methods` 中新增 `formatDateTime` 方法，包装 common.js 的全局函数，修复 `formatDateTime is not a function` 渲染崩溃
- **家长端修复**：删除 `parent.html` 主页面中的 tab-btn 横栏（4 个标签按钮），仅保留侧边栏导航和移动端底部导航

## Impact
- Affected specs: add-parent-portal（家长端 spec 的后续修复）
- Affected code:
  - `teacher.html` — Vue methods 新增 `formatDateTime` 方法
  - `parent.html` — 删除主页面中的标签横栏 section

## MODIFIED Requirements

### Requirement: 教师端 Vue 实例方法完整性
教师端 `teacher.html` 的 Vue 实例 SHALL 在 `methods` 中定义所有模板引用的方法。当模板中使用 `formatDateTime` 时，methods 中 SHALL 提供该方法（包装 common.js 全局函数）。

#### Scenario: 教师端页面渲染
- **WHEN** 教师登录后访问 teacher.html
- **THEN** Vue 应用正常挂载，无控制台错误，所有导航项可点击切换

### Requirement: 家长端导航唯一性
家长端 `parent.html` 的功能导航 SHALL 仅出现在侧边栏（PC 端）和底部导航（移动端），主页面内容区 SHALL 不再重复显示标签横栏。

#### Scenario: 家长端 PC 端导航
- **WHEN** 家长在 PC 端访问 parent.html
- **THEN** 功能导航仅显示在左侧侧边栏，主页面内容区直接展示当前标签内容，无重复横栏

#### Scenario: 家长端移动端导航
- **WHEN** 家长在移动端访问 parent.html
- **THEN** 功能导航仅显示在底部导航栏，主页面内容区直接展示当前标签内容，无重复横栏
