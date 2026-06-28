# Cookie 认证改造计划

## Summary
将当前明文 token + localStorage 的认证方案改为基于 SQLite 会话表的 HttpOnly Cookie 认证方案。后端生成随机会话 ID 存入 SQLite sessions 表并通过 Set-Cookie 下发，前端不再手动管理 token。暂不启用 HTTPS（后续可追加）。

## Current State Analysis

### 当前认证流程
1. **登录**：`POST /api/auth/login` → 验证密码 → 生成明文 token `token_<id>_<timestamp>` → JSON body 返回
2. **前端存储**：`localStorage.setItem('token', token)` + `localStorage.setItem('userInfo', ...)`
3. **请求认证**：前端 `apiRequest()` 从 localStorage 取 token 放入 `Authorization` 头
4. **后端验证**：`check_permission_middleware()` 从 `Authorization` 头取 token → 解析格式 → 检查过期
5. **退出**：`logout()` 仅清除 localStorage，服务端无感知

### 安全问题
- token 为明文拼接 `token_<id>_<timestamp>`，无签名无加密，可伪造
- localStorage 可被 XSS 读取
- 服务端无会话管理，无法主动使 token 失效
- 退出登录仅前端清除，服务端无记录

### 关键文件
- `auth.h`：token 生成/验证/权限检查中间件（全量改造）
- `lib/common.js`：apiRequest/checkAuth/logout（全量改造）
- `index.html`：登录页，fetch 调用 + localStorage 存储 + 跳转（改造）
- `routes_public.cpp`：登录 API、/api/auth/me 等（改造）
- `routes_parent.cpp`：家长登录 API、家长认证中间件（改造）
- `routes_admin.cpp` / `routes_teacher.cpp` / `routes_student.cpp`：各端调用认证中间件（适配）
- `main.cpp`：数据库初始化（新增 sessions 表）
- `admin.html` / `teacher.html` / `student.html` / `parent.html`：checkAuth 调用（适配）
- `config.h` / `config.json`：cookie 相关配置（新增字段）

## Proposed Changes

### 1. 数据库：新增 sessions 表（main.cpp）

在 init_sql 中添加建表语句：
```sql
CREATE TABLE IF NOT EXISTS sessions (
    session_id TEXT PRIMARY KEY,
    user_id INTEGER NOT NULL,
    role_id INTEGER NOT NULL,
    created_at INTEGER NOT NULL,
    expires_at INTEGER NOT NULL,
    is_parent INTEGER DEFAULT 0,
    student_id INTEGER DEFAULT NULL
);
```
- `session_id`：64 字符随机十六进制字符串（32 字节随机数）
- `is_parent`：0=普通用户，1=家长
- `student_id`：家长会话关联的子女 ID（家长专用）

### 2. auth.h 改造：Cookie + Session 认证

#### 2.1 新增 Session 管理函数
```cpp
// 生成随机会话 ID（64 字符十六进制）
std::string generate_session_id();

// 创建会话并存入 SQLite
bool create_session(const std::string& session_id, int user_id, int role_id, 
                    int expiry_hours, bool is_parent = false, int student_id = -1);

// 验证会话有效性（存在且未过期），返回 user_id（成功）或 -1（失败）
int verify_session(const std::string& session_id);

// 获取会话信息（role_id, is_parent, student_id）
bool get_session_info(const std::string& session_id, int& user_id, int& role_id, 
                      bool& is_parent, int& student_id);

// 删除会话（退出登录）
bool delete_session(const std::string& session_id);

// 清理过期会话
void cleanup_expired_sessions();
```

#### 2.2 改造认证中间件
- `check_permission_middleware()`：从 `req.get_header_value("Cookie")` 中提取 `session_id` cookie → `verify_session()` → `get_session_info()` → 权限检查
- `check_parent_auth_middleware()`：同样从 Cookie 提取 session_id → 验证 → 确认 `is_parent=1` → 返回 `student_id`

#### 2.3 移除旧 token 代码
- 移除 `generate_token()` / `verify_token()` / `get_user_id_from_token()`
- 移除 `generate_parent_token()` / `verify_parent_token()` / `get_student_id_from_parent_token()`

#### 2.4 新增 Cookie 解析辅助函数
```cpp
// 从 Cookie 头中提取指定 cookie 的值
std::string get_cookie_value(const httplib::Request& req, const std::string& name);

// 设置认证 cookie 的辅助函数
void set_session_cookie(httplib::Response& res, const std::string& session_id, int max_age);
```

### 3. 后端登录 API 改造

#### 3.1 普通登录（routes_public.cpp）
- `POST /api/auth/login`：验证密码后 → `generate_session_id()` → `create_session()` → `set_session_cookie()` 设置 HttpOnly cookie → 返回 JSON（不含 token，仅含用户信息）

#### 3.2 家长登录（routes_parent.cpp）
- `POST /api/parent/login`：验证密码后 → `generate_session_id()` → `create_session(is_parent=true, student_id=...)` → `set_session_cookie()` → 返回 JSON

#### 3.3 退出登录 API（新增）
- `POST /api/auth/logout`：从 Cookie 提取 session_id → `delete_session()` → 清除 cookie → 返回成功
- `POST /api/parent/logout`：同上（家长退出）

#### 3.4 /api/auth/me 改造
- 从 Cookie 提取 session_id → `verify_session()` → 返回用户信息

### 4. 前端改造

#### 4.1 lib/common.js
- `apiRequest()`：移除 token 从 localStorage 读取和 Authorization 头设置逻辑，改为 `credentials: 'include'`（让浏览器自动携带 cookie）
- `checkAuth(roleId)`：移除 localStorage token 检查，改为调用 `/api/auth/me` 验证会话有效性，失败则跳转 `/`
- `logout()`：改为调用 `POST /api/auth/logout` API，成功后跳转 `/`
- 移除所有 `localStorage.getItem('token')` / `localStorage.setItem('token', ...)`

#### 4.2 index.html（登录页）
- 登录成功后移除 `localStorage.setItem('token', ...)`，仅存储 userInfo（可选，用于前端显示）
- fetch 请求添加 `credentials: 'include'`

#### 4.3 admin.html / teacher.html / student.html / parent.html
- 适配 `checkAuth` 新签名（内部逻辑变化，调用方式不变）
- 移除直接读取 localStorage token 的代码
- 退出按钮调用 `logout()`（已改为 API 调用）

### 5. 配置新增（config.h / config.json）
- `session_expiry_hours`：会话过期时间（默认 24 小时）
- `cookie_name`：cookie 名称（默认 `"sid"`）
- `cookie_secure`：是否启用 Secure 标记（默认 false，因暂不启用 HTTPS）

### 6. Cookie 属性设置
```
Set-Cookie: sid=<session_id>; HttpOnly; Path=/; Max-Age=86400; SameSite=Lax
```
- `HttpOnly`：禁止 JS 读取，防 XSS
- `Path=/`：全站有效
- `SameSite=Lax`：防 CSRF（允许顶层导航携带）
- 不设 `Secure`（因暂无 HTTPS），后续启用 HTTPS 时追加

## Assumptions & Decisions
1. **暂不启用 HTTPS**：OpenSSL 未安装，cookie 暂不设 Secure 标记，后续启用 HTTPS 时追加
2. **SQLite 会话表**：在现有数据库中新增 sessions 表，不引入 Redis 等外部依赖
3. **移除旧 token 体系**：彻底替换，不保留兼容（前后端同时改造）
4. **userInfo 可选保留**：前端可继续用 localStorage 存 userInfo 做显示用途，但认证完全依赖 cookie
5. **会话清理**：在每次登录时顺便清理过期会话（`cleanup_expired_sessions()`）
6. **双体系统一**：普通用户和家长用户均使用同一 session 表，通过 `is_parent` 字段区分

## Verification Steps
1. **编译验证**：使用 build.bat 编译，确保无编译错误
2. **登录验证**：
   - 普通用户登录（admin/teacher/student），检查响应头有 Set-Cookie
   - 家长登录，检查响应头有 Set-Cookie
   - 检查 cookie 属性包含 HttpOnly
3. **会话验证**：
   - 登录后访问各端页面，checkAuth 正常通过
   - 直接访问受保护页面（未登录），跳转到登录页
   - 角色不匹配（如学生访问 admin.html），跳转到登录页
4. **请求认证**：
   - 登录后 API 请求自动携带 cookie，后端正常验证
   - 清除 cookie 后 API 请求返回 401
5. **退出登录**：
   - 调用退出 API，检查 cookie 被清除
   - 退出后访问受保护页面跳转登录页
   - 检查 SQLite sessions 表中对应记录已删除
6. **安全验证**：
   - 浏览器 DevTools 中无法通过 `document.cookie` 读取 sid（HttpOnly 生效）
   - 伪造的 session_id 无法通过验证
