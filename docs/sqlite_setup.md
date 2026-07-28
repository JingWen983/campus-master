# SQLite 数据库配置说明

## 概述

校园文明能量站系统支持两种数据存储模式：
1. **内存存储模式**（默认）：数据存储在内存中，服务器重启后数据会丢失
2. **SQLite 数据库模式**：数据持久化存储在 SQLite 数据库文件中

## SQLite 配置步骤

### 方法一：使用 vcpkg 安装（推荐）

1. 安装 vcpkg（如果尚未安装）：
```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```

2. 安装 SQLite：
```bash
.\vcpkg install sqlite3:x64-windows
```

3. 编译项目：
```bash
g++ -o server.exe main.cpp -lws2_32 -lwsock32 -std=c++11 -O2 -DUSE_SQLITE -I<vcpkg路径>/installed/x64-windows/include -L<vcpkg路径>/installed/x64-windows/lib -lsqlite3
```

### 方法二：手动下载 SQLite

1. 访问 SQLite 官网下载页面：https://www.sqlite.org/download.html

2. 下载以下文件：
   - `sqlite-amalgamation-*.zip`（源代码）
   - `sqlite-dll-win64-*.zip`（DLL 文件）

3. 解压后将以下文件复制到项目目录：
   - `sqlite3.h`
   - `sqlite3ext.h`
   - `sqlite3.dll`

4. 生成静态库：
```bash
# 使用 Visual Studio 开发者命令提示符
lib /def:sqlite3.def /out:sqlite3.lib
```

5. 编译项目：
```bash
g++ -o server.exe main.cpp sqlite3.lib -lws2_32 -lwsock32 -std=c++11 -O2 -DUSE_SQLITE
```

### 方法三：使用预编译的 SQLite

1. 从以下地址下载预编译的 SQLite：
   - https://github.com/niclasr/sqlite3-prebuilt/releases

2. 将 `sqlite3.h`、`sqlite3.lib`、`sqlite3.dll` 复制到项目目录

3. 编译项目：
```bash
g++ -o server.exe main.cpp sqlite3.lib -lws2_32 -lwsock32 -std=c++11 -O2 -DUSE_SQLITE
```

## 数据库初始化

首次运行时，系统会自动创建数据库文件 `campus_system.db` 并初始化以下表结构：

### 用户表 (users)
- id: 用户ID（主键）
- username: 用户名（唯一）
- password_hash: 密码哈希
- role_id: 角色ID
- name: 姓名
- className: 班级
- points: 积分
- created_at: 创建时间
- updated_at: 更新时间

### 角色表 (roles)
- id: 角色ID（主键）
- name: 角色名称
- description: 角色描述

### 权限表 (permissions)
- id: 权限ID（主键）
- name: 权限名称
- code: 权限代码
- description: 权限描述

### 角色权限关联表 (role_permissions)
- id: 关联ID（主键）
- role_id: 角色ID
- permission_id: 权限ID

### 积分记录表 (points_records)
- id: 记录ID（主键）
- student_id: 学生ID
- points: 积分变动
- reason: 原因
- operator_id: 操作人ID
- created_at: 创建时间

### 评价表 (evaluations)
- id: 评价ID（主键）
- student_id: 学生ID
- dimension_id: 评价维度ID
- score: 分数
- comment: 评价内容
- evaluator_id: 评价人ID
- created_at: 创建时间
- updated_at: 更新时间

### 商城商品表 (mall_items)
- id: 商品ID（主键）
- name: 商品名称
- description: 商品描述
- cost: 所需积分
- stock: 库存
- image_url: 图片链接
- status: 状态
- created_at: 创建时间

### 兑换记录表 (redemption_records)
- id: 记录ID（主键）
- student_id: 学生ID
- item_id: 商品ID
- cost: 消耗积分
- created_at: 创建时间

## 默认账号

系统初始化后会创建以下默认账号：

| 角色 | 用户名 | 密码 |
|------|--------|------|
| 管理员 | admin | admin123 |
| 教师 | teacher | teacher123 |
| 学生 | student | student123 |

## 数据备份

### 手动备份
直接复制 `campus_system.db` 文件即可。

### 使用 SQLite 命令备份
```bash
sqlite3 campus_system.db ".backup backup.db"
```

### 导出为 SQL 文件
```bash
sqlite3 campus_system.db .dump > backup.sql
```

## 数据恢复

### 从备份文件恢复
```bash
sqlite3 campus_system.db ".read backup.sql"
```

### 从 SQL 文件恢复
```bash
sqlite3 campus_system.db < backup.sql
```

## 数据库管理工具

推荐使用以下工具管理 SQLite 数据库：
- **DB Browser for SQLite**（免费开源）：https://sqlitebrowser.org/
- **SQLite Expert**（免费版）：http://www.sqliteexpert.com/
- **DBeaver**（免费开源）：https://dbeaver.io/

## 常见问题

### Q: 编译时提示找不到 sqlite3.h
A: 确保 SQLite 头文件在编译器的包含路径中，或使用 `-I` 参数指定路径。

### Q: 运行时提示找不到 sqlite3.dll
A: 确保 `sqlite3.dll` 文件在可执行文件同一目录下，或将其添加到系统 PATH 环境变量中。

### Q: 数据库文件损坏怎么办
A: 尝试使用 SQLite 的恢复功能：
```bash
sqlite3 campus_system.db ".recover" > recover.sql
sqlite3 new.db < recover.sql
```

### Q: 如何查看数据库内容
A: 使用 SQLite 命令行工具：
```bash
sqlite3 campus_system.db
.tables
SELECT * FROM users;
```

## 性能优化建议

1. **定期清理日志**：SQLite 会产生 WAL 文件，定期清理可以减少磁盘占用
2. **创建索引**：系统已自动创建常用索引，如需额外索引可手动添加
3. **定期备份**：建议每天备份一次数据库文件
4. **监控数据库大小**：定期检查数据库文件大小，必要时进行清理

## 注意事项

1. SQLite 不适合高并发场景，如需支持大量并发用户，建议迁移到 MySQL 或 PostgreSQL
2. 数据库文件应定期备份，避免数据丢失
3. 修改数据库结构前请先备份
4. 生产环境建议修改默认密码