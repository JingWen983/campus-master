# Tasks

- [x] Task 1: 引入前端依赖库（xlsx.js + pinyin-pro）
  - [x] SubTask 1.1: 下载 SheetJS UMD 版到 `lib/xlsx.min.js`（用于解析 .xlsx 和 .csv）
  - [x] SubTask 1.2: 下载 pinyin-pro UMD 版到 `lib/pinyin-pro.umd.min.js`（用于姓名转拼音首字母）
  - [x] SubTask 1.3: 在 `admin.html` 顶部 `<head>` 添加两个 `<script>` 引用

- [x] Task 2: 后端实现批量导入 API
  - [x] SubTask 2.1: 在 `routes_admin.cpp` 新增 `POST /api/admin/students/batch-import`，权限校验 `user:manage`
  - [x] SubTask 2.2: 解析请求体 `students` 数组（每项 name、className、username、password），逐条处理
  - [x] SubTask 2.3: 班级匹配：按 className 查 `classes` 表得 grade_code/class_code，未匹配记失败
  - [x] SubTask 2.4: username 兜底去重：`find_user_by_username` 已存在则记失败
  - [x] SubTask 2.5: 调 `generate_user_id(3, grade_code, class_code)` 生成 ID → 构造 User → push 到 users + update_user_index + save_user_to_db（逐条立即生效保证序号递增）
  - [x] SubTask 2.6: 返回 `data.success`、`data.failed`、`data.records`（成功含 username/password/student_id，失败含 reason）
  - [x] SubTask 2.7: 编译 `routes_admin.cpp` 并链接 `server.exe`，启动验证接口可调通

- [x] Task 3: 前端实现批量导入对话框与流程
  - [x] SubTask 3.1: 在用户管理 tab 顶部"添加用户"按钮旁新增"批量导入学生"按钮
  - [x] SubTask 3.2: 新增 data 字段：`showBatchImportDialog`、`batchImportStep`（select/preview/result）、`batchImportRows`、`batchImportResult`
  - [x] SubTask 3.3: 实现模板下载方法 `downloadStudentTemplate()`：用 xlsx.js 生成含表头"姓名|班级名称"+示例行的 .xlsx 并触发下载
  - [x] SubTask 3.4: 实现文件选择与解析方法 `handleBatchFileSelect()`：用 xlsx.js 解析为行数组，跳过表头，每行填入 `{name, className}`，并用 classes 数组初步标注匹配状态
  - [x] SubTask 3.5: 实现预览视图：表格展示行号/姓名/班级名/匹配状态，未匹配行红色标注
  - [x] SubTask 3.6: 实现拼音生成方法 `genStudentAccount(name, classCode, seq)`：用 pinyin-pro 取姓名拼音首字母生成 username 和 password（NFD 规范化去声调）
  - [x] SubTask 3.7: 实现 `submitBatchImport()`：为每行预生成 username/password（含同班序号累计去重），未匹配行计入前端失败列表，POST 到 `/api/admin/students/batch-import`，切到结果视图
  - [x] SubTask 3.8: 实现结果视图：统计 + 成功明细（含复制按钮）+ 失败明细
  - [x] SubTask 3.9: 对话框 HTML：三个 step 用 `v-if` 切换，含关闭/返回按钮

- [x] Task 4: 端到端验证
  - [x] SubTask 4.1: 下载模板 → 填入 3 行（含 1 行故意写错班级名）→ 上传 → 预览显示 2 匹配 1 不匹配
  - [x] SubTask 4.2: 提交导入 → 结果显示 2 成功 1 失败 → 成功记录含 username/password/student_id
  - [x] SubTask 4.3: 验证生成的学生 ID 格式为 `student-<grade_code>-<class_code>-<seq>`（如 student-02-01-02）
  - [x] SubTask 4.4: 用生成的 username/password 登录学生端验证账号可用
  - [x] SubTask 4.5: 验证同班两个学生 username 序号递增（zd0101 / qe0102）

# Task Dependencies
- Task 3 依赖 Task 1（前端库）和 Task 2（后端 API）
- Task 1 和 Task 2 可并行
- Task 4 依赖 Task 1、2、3 全部完成
