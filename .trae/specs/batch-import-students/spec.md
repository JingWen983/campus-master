# 批量导入学生 Spec

## Why
当前管理员只能通过"添加用户"对话框逐个录入学生，面对新学期整班建制录入场景效率极低。需要支持通过表格文件（CSV/Excel）批量导入学生，并自动解决班级名称到学生 ID 编码的匹配问题。

## What Changes
- 新增管理员端"批量导入学生"功能入口（用户管理 tab 顶部按钮）
- 新增 CSV / Excel 模板下载（前端生成，含表头：姓名、班级名称）
- 新增前端文件解析与预览（引入 SheetJS 同时支持 .xlsx 和 .csv，无需双库）
- 新增前端姓名拼音转换（引入 pinyin-pro，用于自动生成用户名和密码）
- 新增后端批量导入 API `POST /api/admin/students/batch-import`
- 班级匹配：按 `classes.name` 精确匹配，反查 `grade_code`/`class_code` 生成学生 ID
- 用户名/密码按规则自动生成，无需老师填写
- 详细的导入结果报告（成功/失败/跳过 + 每行原因）

## Impact
- Affected specs: 无（新能力）
- Affected code:
  - `d:\邵敬文\comptation\admin.html`（新增对话框、按钮、解析/预览/提交逻辑）
  - `d:\邵敬文\comptation\routes_admin.cpp`（新增批量导入 API）
  - `d:\邵敬文\comptation\lib\`（新增 `xlsx.min.js`、`pinyin-pro.umd.min.js` 两个库文件）
  - 复用：`generate_user_id`（models.cpp）、`save_user_to_db`（models.cpp）、`find_user_by_username`（models.cpp）

## ADDED Requirements

### Requirement: 批量导入学生 API
系统 SHALL 提供后端接口 `POST /api/admin/students/batch-import`，接收学生数组（每项含 `name`、`className`），逐条处理：
1. 校验 name、className 非空
2. 按 className 查 `classes` 表获取 `grade_code`、`class_code`；未匹配则该行失败
3. 检查 username 是否已存在（username 由前端生成传入，后端兜底再查一次）
4. 调用 `generate_user_id(3, grade_code, class_code)` 生成学生 ID
5. 调用 `save_user_to_db` 持久化，并 push 到内存 `users` + `update_user_index`
6. 每处理完一条立即更新内存，保证同班下一条序号递增

#### Scenario: 全部成功
- **WHEN** 管理员上传含 3 行学生（姓名+班级名均合法且班级存在）
- **THEN** 返回 code=200，data.success=3, data.failed=0, data.records 含 3 条成功记录（含 username、password、student_id）

#### Scenario: 部分失败
- **WHEN** 上传 5 行，其中 1 行班级名拼写错误、1 行姓名为空
- **THEN** 返回 code=200，data.success=3, data.failed=2，failed 记录含行号和失败原因

#### Scenario: 班级未匹配
- **WHEN** 某行 className 在 classes 表中不存在
- **THEN** 该行标记失败，原因"班级不存在：xxx"，其余行不受影响

### Requirement: 前端文件解析与预览
系统 SHALL 在管理员"用户管理"tab 提供批量导入入口，支持 .xlsx 和 .csv 两种格式：
- 使用 SheetJS（xlsx.js）统一解析两种格式
- 解析后展示预览表格（姓名、班级名称、匹配状态），匹配失败的行红色标注
- 提供"下载模板"按钮，前端生成含表头和示例行的 .xlsx 文件

#### Scenario: 上传 CSV
- **WHEN** 管理员上传 .csv 文件（UTF-8，含姓名/班级名称两列）
- **THEN** 预览表展示解析结果，每行显示班级是否匹配

#### Scenario: 下载模板
- **WHEN** 管理员点击"下载模板"
- **THEN** 浏览器下载 `学生批量导入模板.xlsx`，含表头"姓名|班级名称"及示例行"张三|高二(1)班"

### Requirement: 用户名与密码自动生成
系统 SHALL 按以下规则为每个导入学生自动生成用户名和初始密码（使用 pinyin-pro 转换姓名拼音）：
- **用户名** = 姓名拼音首字母（小写）+ 班级 class_code + 班级内序号
  - 示例：张三在 `高二(1)班`（class_code=01）第 1 号 → `zs0101`
  - 拼音首字母重复时序号自然递增去重（zs0101、zs0102）
- **密码** = `Stu@` + class_code + 班级内序号
  - 示例：`Stu@0101`
- 班级内序号由 `generate_user_id` 在同 grade_code+class_code 前缀下累计得到

#### Scenario: 同名同班
- **WHEN** 同一班级导入两个"张三"
- **THEN** 第一个 username=`zs0101`，第二个 username=`zs0102`，两者密码分别为 `Stu@0101`、`Stu@0102`

#### Scenario: 多音字/生僻字
- **WHEN** 姓名含多音字（如"乐"）
- **THEN** pinyin-pro 取默认读音首字母，正常生成

### Requirement: 导入结果报告
系统 SHALL 在导入完成后展示详细报告：
- 成功数 / 失败数 / 总数
- 成功记录列表：行号、姓名、班级、用户名、初始密码、学生 ID
- 失败记录列表：行号、姓名、失败原因
- 提供"复制全部账号密码"按钮（便于老师下发给学生）

#### Scenario: 报告展示
- **WHEN** 导入完成（无论是否有失败）
- **THEN** 对话框切换为结果视图，展示统计与明细，可一键复制账号密码

### Requirement: 模板格式约定
系统 SHALL 约定批量导入模板格式：
- 列 A：姓名（必填）
- 列 B：班级名称（必填，须与 `classes.name` 完全一致，如"高二(1)班"）
- 第一行为表头，不参与导入
- 文件编码 UTF-8（CSV）或标准 xlsx

## 设计决策

1. **文件解析在前端**：前端用 SheetJS 解析为 JSON 数组，POST 给后端。后端 C++ 不引入表格解析库，降低复杂度。
2. **拼音在前端**：pinyin-pro 在前端生成 username/password，随每行一起 POST 给后端。后端不引入拼音库。后端对 username 仍做去重校验（兜底）。
3. **班级匹配在后端**：前端预览时用已加载的 `classes` 数组做初步匹配提示，但最终以后端查 DB 为准（防止前端 classes 过期）。
4. **序号递增**：后端每生成一个学生立即 push 到 `users` + `update_user_index` + `save_user_to_db`，确保同班下一条 `generate_user_id` 序号递增。整个批量导入不做事务回滚（部分成功是预期行为）。
5. **不修改现有 `/api/admin/import`**：现有接口是系统级全量备份恢复，与本功能（学生批量录入）用途不同，互不影响。
