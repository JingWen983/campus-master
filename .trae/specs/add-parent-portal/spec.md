# 家长端页面 Spec

## Why
当前系统仅有管理员/教师/学生三类角色，家长无法查看子女在校的积分、评价、兑换记录，也无法与教师沟通。新增家长端页面，让家长通过子女学号+家长独立密码登录，查看多个子女的在校情况并与教师留言互动，形成"学校-家庭"协同教育闭环。

## What Changes
- **数据库**：`users` 表新增 `parent_password_hash`（家长密码哈希）和 `parent_phone`（家长手机号，用于关联多个子女）两个字段；新增 `parent_messages` 表存储家长-教师留言
- **后端**：新增 `routes_parent.cpp` 路由文件，提供家长登录、子女列表、子女信息/积分/评价/兑换记录查询、留言发送等 API；在 `routes_teacher.cpp` 新增教师回复留言的 API
- **前端**：新增 `parent.html` 家长端页面；更新 `index.html` 增加家长登录入口；更新 `common.js` 支持家长角色
- **构建**：更新 `routes.h`、`main.cpp`、`build.bat` 以纳入新模块
- **认证**：家长 Token 使用 `ptoken_` 前缀区别于学生 `token_`，自定义 `check_parent_auth_middleware` 中间件

## Impact
- Affected specs: 无（首个家长端 spec）
- Affected code:
  - `main.cpp` — 数据库初始化 SQL（新增字段+新表）、注册家长路由
  - `routes.h` — 新增 `register_parent_routes` 声明
  - `routes_parent.cpp` — 新建文件，家长端全部 API
  - `routes_teacher.cpp` — 新增教师查看/回复家长留言的 API
  - `routes_static.cpp` — 新增 `/parent.html` 静态路由
  - `auth.h` — 新增 `check_parent_auth_middleware`、`generate_parent_token`、`verify_parent_token`
  - `index.html` — 登录页增加"家长登录"入口
  - `parent.html` — 新建文件，家长端完整页面
  - `lib/common.js` — `checkAuth` 支持 role_id=4（家长）
  - `build.bat` — 编译列表新增 `routes_parent.cpp`

## ADDED Requirements

### Requirement: 家长登录认证
系统 SHALL 提供家长登录接口 `POST /api/parent/login`，接受 `username`（子女学号/用户名）和 `parent_password`（家长密码），验证通过后返回 `ptoken_` 前缀的 Token 和关联的子女列表。

#### Scenario: 家长登录成功
- **WHEN** 家长输入子女的用户名和正确的家长密码
- **THEN** 返回 code=200，data 包含 token（ptoken_ 前缀）和 children 数组（同 parent_phone 的所有学生）

#### Scenario: 家长密码错误
- **WHEN** 家长输入的家长密码与 `parent_password_hash` 不匹配
- **THEN** 返回 code=401，msg="家长密码错误"

#### Scenario: 学生未设置家长密码
- **WHEN** 该学生账号的 `parent_password_hash` 为空
- **THEN** 返回 code=403，msg="该学生尚未开通家长账号，请联系教师设置"

### Requirement: 家长 Token 认证中间件
系统 SHALL 提供 `check_parent_auth_middleware` 中间件，验证 `ptoken_` 前缀的 Token，并从 Token 中提取登录时使用的子女 student_id。

#### Scenario: 有效家长 Token
- **WHEN** 请求头携带有效的 ptoken_ Token
- **THEN** 中间件放行，后续处理可获取 parent 的 primary_student_id

#### Scenario: 无效或过期的家长 Token
- **WHEN** Token 为空、非 ptoken_ 前缀、或已过期（超过 24 小时）
- **THEN** 返回 401，msg="token 无效或已过期"

### Requirement: 家长查看子女列表
系统 SHALL 提供 `GET /api/parent/children` 接口，返回与当前登录子女共享同一 `parent_phone` 的所有学生列表。

#### Scenario: 查询子女列表
- **WHEN** 家长携带有效 ptoken_ 请求 `/api/parent/children`
- **THEN** 返回 code=200，data 为子女数组，每项含 id/name/className/points

### Requirement: 家长查看单个子女信息
系统 SHALL 提供以下接口供家长查看指定子女（须属于同一 parent_phone 关联组）的数据：

| 方法 | 路径 | 功能 |
|------|------|------|
| GET | `/api/parent/student/{id}/info` | 子女基本信息+积分+班级排名 |
| GET | `/api/parent/student/{id}/points` | 子女积分变动记录 |
| GET | `/api/parent/student/{id}/evaluation` | 子女教师评价 |
| GET | `/api/parent/student/{id}/redemptions` | 子女兑换记录 |

#### Scenario: 查看关联子女信息
- **WHEN** 家长请求路径中 {id} 对应的学生与登录子女共享同一 parent_phone
- **THEN** 返回 code=200 和对应数据

#### Scenario: 越权查看非关联子女
- **WHEN** 家长请求路径中 {id} 对应的学生与登录子女的 parent_phone 不同
- **THEN** 返回 code=403，msg="无权查看该学生信息"

### Requirement: 家长-教师留言功能
系统 SHALL 提供留言 API，家长可向教师发送留言，教师可查看并回复。

#### 数据表 `parent_messages` 结构
| 字段 | 类型 | 说明 |
|------|------|------|
| id | INTEGER PK AUTO | 主键 |
| student_id | INTEGER NOT NULL | 关联学生 ID（外键→users.id） |
| sender_type | TEXT NOT NULL | 发送者类型：'parent' 或 'teacher' |
| sender_id | INTEGER | 发送者 ID（teacher 时为 users.id；parent 时为登录子女的 student_id） |
| content | TEXT NOT NULL | 留言内容 |
| reply_to | INTEGER | 回复的消息 ID（可空，表示首条留言） |
| created_at | DATETIME DEFAULT CURRENT_TIMESTAMP | 创建时间 |
| read_status | INTEGER DEFAULT 0 | 已读状态：0=未读，1=已读 |

#### 家长端 API
| 方法 | 路径 | 功能 |
|------|------|------|
| GET | `/api/parent/student/{id}/messages` | 获取该子女的留言列表（按时间正序） |
| POST | `/api/parent/student/{id}/messages` | 家长发送留言（body: content） |

#### 教师端 API（新增到 routes_teacher.cpp）
| 方法 | 路径 | 权限 | 功能 |
|------|------|------|------|
| GET | `/api/teacher/parent-messages` | `student:manage` | 获取教师所辖班级学生的留言列表 |
| POST | `/api/teacher/parent-messages/{id}/reply` | `student:manage` | 教师回复指定留言（body: content） |
| PUT | `/api/teacher/parent-messages/{id}/read` | `student:manage` | 标记留言为已读 |

#### Scenario: 家长发送留言
- **WHEN** 家长 POST 留言内容到 `/api/parent/student/{id}/messages`，且 {id} 为关联子女
- **THEN** 创建 sender_type='parent' 的记录，返回 code=200

#### Scenario: 教师回复留言
- **WHEN** 教师 POST 回复内容到 `/api/teacher/parent-messages/{id}/reply`
- **THEN** 创建 sender_type='teacher'、reply_to={id} 的记录，原留言标记为已读，返回 code=200

### Requirement: 家长端前端页面
系统 SHALL 提供 `parent.html` 页面，包含以下功能区：

1. **子女切换栏** — 顶部显示所有关联子女，可点击切换查看不同子女数据
2. **子女信息卡** — 显示当前选中子女的姓名、班级、积分、班级排名
3. **积分记录列表** — 表格展示积分变动（时间/积分/类型/原因）
4. **教师评价列表** — 卡片展示各维度评价（维度/分数/评语/评价人/时间）
5. **兑换记录列表** — 表格展示兑换历史（时间/商品/消耗积分）
6. **留言区** — 与教师的留言对话列表 + 底部留言输入框

#### Scenario: 家长访问页面
- **WHEN** 家长登录后跳转到 parent.html
- **THEN** 页面自动加载子女列表，默认显示第一个子女的完整信息

#### Scenario: 未登录访问
- **WHEN** 未携带有效 ptoken_ 直接访问 parent.html
- **THEN** common.js 的 checkAuth(4) 检测后跳转回登录页

### Requirement: 登录页家长入口
`index.html` 登录页 SHALL 增加"家长登录"按钮/选项，点击后切换到家长登录模式（输入子女学号+家长密码），登录成功后跳转 `/parent.html`。

#### Scenario: 选择家长登录
- **WHEN** 用户在登录页点击"家长登录"
- **THEN** 表单切换为"子女学号"+"家长密码"两个字段，提交调用 `/api/parent/login`

### Requirement: 数据库字段与默认数据
系统 SHALL 在数据库初始化 SQL 中：
1. 为 `users` 表新增 `parent_password_hash` TEXT（可空）和 `parent_phone` TEXT（可空）字段
2. 创建 `parent_messages` 表
3. 为默认学生账号 `student` 设置家长密码（默认 `parent123`）和 parent_phone（默认 `13800000001`），便于测试

#### Scenario: 全新部署
- **WHEN** 首次启动服务器，数据库初始化
- **THEN** users 表含新字段，parent_messages 表已创建，student 账号可使用 parent123 进行家长登录

## MODIFIED Requirements

### Requirement: 教师工作台新增留言管理
教师工作台 `teacher.html` SHALL 新增"家长留言"标签页，展示未读留言列表，支持点击回复。

#### Scenario: 教师查看家长留言
- **WHEN** 教师点击"家长留言"标签
- **THEN** 加载 `/api/teacher/parent-messages`，按未读优先、时间倒序展示

#### Scenario: 教师回复留言
- **WHEN** 教师在留言项点击回复，输入内容并提交
- **THEN** 调用 `/api/teacher/parent-messages/{id}/reply`，成功后刷新列表
