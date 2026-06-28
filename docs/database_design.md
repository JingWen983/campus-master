# 校园管理系统数据库设计

## 1. 数据库表结构

### 1.1 用户表 (users)
| 字段名 | 数据类型 | 约束 | 描述 |
| :--- | :--- | :--- | :--- |
| `id` | `INT` | `PRIMARY KEY, AUTO_INCREMENT` | 用户ID |
| `username` | `VARCHAR(50)` | `UNIQUE, NOT NULL` | 用户名 |
| `password_hash` | `VARCHAR(255)` | `NOT NULL` | 密码哈希值 |
| `role_id` | `INT` | `FOREIGN KEY REFERENCES roles(id)` | 角色ID |
| `name` | `VARCHAR(100)` | `NOT NULL` | 姓名 |
| `email` | `VARCHAR(100)` | `UNIQUE` | 邮箱 |
| `phone` | `VARCHAR(20)` | | 电话号码 |
| `created_at` | `TIMESTAMP` | `DEFAULT CURRENT_TIMESTAMP` | 创建时间 |
| `updated_at` | `TIMESTAMP` | `DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP` | 更新时间 |

### 1.2 角色表 (roles)
| 字段名 | 数据类型 | 约束 | 描述 |
| :--- | :--- | :--- | :--- |
| `id` | `INT` | `PRIMARY KEY, AUTO_INCREMENT` | 角色ID |
| `name` | `VARCHAR(50)` | `UNIQUE, NOT NULL` | 角色名称 |
| `description` | `VARCHAR(255)` | | 角色描述 |
| `created_at` | `TIMESTAMP` | `DEFAULT CURRENT_TIMESTAMP` | 创建时间 |

### 1.3 权限表 (permissions)
| 字段名 | 数据类型 | 约束 | 描述 |
| :--- | :--- | :--- | :--- |
| `id` | `INT` | `PRIMARY KEY, AUTO_INCREMENT` | 权限ID |
| `name` | `VARCHAR(50)` | `UNIQUE, NOT NULL` | 权限名称 |
| `code` | `VARCHAR(50)` | `UNIQUE, NOT NULL` | 权限代码 |
| `description` | `VARCHAR(255)` | | 权限描述 |
| `created_at` | `TIMESTAMP` | `DEFAULT CURRENT_TIMESTAMP` | 创建时间 |

### 1.4 角色权限关联表 (role_permissions)
| 字段名 | 数据类型 | 约束 | 描述 |
| :--- | :--- | :--- | :--- |
| `id` | `INT` | `PRIMARY KEY, AUTO_INCREMENT` | 关联ID |
| `role_id` | `INT` | `FOREIGN KEY REFERENCES roles(id)` | 角色ID |
| `permission_id` | `INT` | `FOREIGN KEY REFERENCES permissions(id)` | 权限ID |
| `created_at` | `TIMESTAMP` | `DEFAULT CURRENT_TIMESTAMP` | 创建时间 |

### 1.5 学生信息表 (students)
| 字段名 | 数据类型 | 约束 | 描述 |
| :--- | :--- | :--- | :--- |
| `id` | `INT` | `PRIMARY KEY, AUTO_INCREMENT` | 学生ID |
| `user_id` | `INT` | `FOREIGN KEY REFERENCES users(id)` | 用户ID |
| `class_name` | `VARCHAR(50)` | `NOT NULL` | 班级名称 |
| `student_id` | `VARCHAR(20)` | `UNIQUE, NOT NULL` | 学号 |
| `gender` | `VARCHAR(10)` | | 性别 |
| `birthdate` | `DATE` | | 出生日期 |
| `total_points` | `INT` | `DEFAULT 0` | 总积分 |
| `created_at` | `TIMESTAMP` | `DEFAULT CURRENT_TIMESTAMP` | 创建时间 |
| `updated_at` | `TIMESTAMP` | `DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP` | 更新时间 |

### 1.6 积分记录表 (points_records)
| 字段名 | 数据类型 | 约束 | 描述 |
| :--- | :--- | :--- | :--- |
| `id` | `INT` | `PRIMARY KEY, AUTO_INCREMENT` | 记录ID |
| `student_id` | `INT` | `FOREIGN KEY REFERENCES students(id)` | 学生ID |
| `teacher_id` | `INT` | `FOREIGN KEY REFERENCES users(id)` | 操作教师ID |
| `points` | `INT` | `NOT NULL` | 积分变动值 |
| `reason` | `VARCHAR(255)` | `NOT NULL` | 变动原因 |
| `type` | `VARCHAR(20)` | `NOT NULL` | 变动类型 (add/deduct/exchange) |
| `created_at` | `TIMESTAMP` | `DEFAULT CURRENT_TIMESTAMP` | 创建时间 |

### 1.7 评价维度表 (evaluation_dimensions)
| 字段名 | 数据类型 | 约束 | 描述 |
| :--- | :--- | :--- | :--- |
| `id` | `INT` | `PRIMARY KEY, AUTO_INCREMENT` | 维度ID |
| `name` | `VARCHAR(50)` | `NOT NULL` | 维度名称 |
| `description` | `VARCHAR(255)` | | 维度描述 |
| `score_max` | `INT` | `DEFAULT 100` | 最高分数 |
| `created_at` | `TIMESTAMP` | `DEFAULT CURRENT_TIMESTAMP` | 创建时间 |

### 1.8 评价表 (evaluations)
| 字段名 | 数据类型 | 约束 | 描述 |
| :--- | :--- | :--- | :--- |
| `id` | `INT` | `PRIMARY KEY, AUTO_INCREMENT` | 评价ID |
| `student_id` | `INT` | `FOREIGN KEY REFERENCES students(id)` | 学生ID |
| `teacher_id` | `INT` | `FOREIGN KEY REFERENCES users(id)` | 教师ID |
| `dimension_id` | `INT` | `FOREIGN KEY REFERENCES evaluation_dimensions(id)` | 评价维度ID |
| `score` | `INT` | `NOT NULL` | 评价分数 |
| `comment` | `TEXT` | | 评价内容 |
| `evaluation_date` | `DATE` | `NOT NULL` | 评价日期 |
| `created_at` | `TIMESTAMP` | `DEFAULT CURRENT_TIMESTAMP` | 创建时间 |
| `updated_at` | `TIMESTAMP` | `DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP` | 更新时间 |

## 2. 数据库索引设计

### 2.1 主键索引
- 所有表的 `id` 字段

### 2.2 唯一索引
- `users.username`
- `users.email`
- `students.student_id`
- `roles.name`
- `permissions.code`

### 2.3 普通索引
- `users.role_id`
- `students.user_id`
- `points_records.student_id`
- `points_records.teacher_id`
- `evaluations.student_id`
- `evaluations.teacher_id`
- `evaluations.dimension_id`

## 3. 数据库关系图

```
+-------------+        +-------------+        +-------------------+
|   users     |        |   roles     |        |  permissions      |
+-------------+        +-------------+        +-------------------+
| id (PK)     |<-------| id (PK)     |<-------| id (PK)          |
| username    |        | name        |        | name             |
| password_hash |       | description |        | code             |
| role_id (FK) |       +-------------+        | description      |
| name        |                                +-------------------+
| email       |                                      |
| phone       |                                      |
+-------------+                                      |
        |                                           |
        |                                           |
+-------------+        +-------------------+         |
|  students   |        | role_permissions |<--------+
+-------------+        +-------------------+
| id (PK)     |        | id (PK)         |
| user_id (FK) |------>| role_id (FK)     |
| class_name  |        | permission_id (FK)|----+   
| student_id  |        +-------------------+    |   
| gender      |                                |   
| birthdate   |                                |   
| total_points|                                |   
+-------------+                                |   
        |                                     |   
        |                                     |   
+-------------------+        +-------------------+
| points_records    |        | evaluation_dimensions |
+-------------------+        +-------------------+
| id (PK)           |        | id (PK)           |
| student_id (FK)   |        | name              |
| teacher_id (FK)   |        | description       |
| points            |        | score_max         |
| reason            |        +-------------------+
| type              |                   |
+-------------------+                   |
                                        |
+-------------------+
|  evaluations      |
+-------------------+
| id (PK)           |
| student_id (FK)   |
| teacher_id (FK)   |
| dimension_id (FK) |<------------------+
| score             |
| comment           |
| evaluation_date   |
+-------------------+
```

## 4. 数据初始化

### 4.1 初始角色
| id | name | description |
| :--- | :--- | :--- |
| 1 | 管理员 | 系统管理员，拥有所有权限 |
| 2 | 教师 | 教师角色，管理学生和积分 |
| 3 | 学生 | 学生角色，查看个人信息和兑换 |

### 4.2 初始权限
| id | name | code | description |
| :--- | :--- | :--- | :--- |
| 1 | 系统管理 | system:manage | 系统配置管理 |
| 2 | 用户管理 | user:manage | 用户和角色管理 |
| 3 | 学生管理 | student:manage | 学生信息管理 |
| 4 | 积分管理 | points:manage | 积分操作管理 |
| 5 | 评价管理 | evaluation:manage | 学生评价管理 |
| 6 | 商城管理 | mall:manage | 兑换商城管理 |
| 7 | 数据统计 | statistics:view | 数据统计查看 |

### 4.3 初始评价维度
| id | name | description | score_max |
| :--- | :--- | :--- | :--- |
| 1 | 德育 | 思想品德和道德素养 | 100 |
| 2 | 智育 | 学习成绩和学习能力 | 100 |
| 3 | 体育 | 体育锻炼和健康状况 | 100 |
| 4 | 美育 | 艺术素养和审美能力 | 100 |
| 5 | 劳育 | 劳动技能和实践能力 | 100 |

## 5. 数据库优化策略

1. **索引优化**：为频繁查询的字段创建索引，如用户登录、学生查询等
2. **查询优化**：使用合理的查询语句，避免全表扫描
3. **事务管理**：对积分操作等关键业务使用事务确保数据一致性
4. **分表策略**：对于积分记录等可能增长迅速的表，考虑分表存储
5. **缓存策略**：对热点数据使用缓存，减少数据库查询

## 6. 安全考虑

1. **密码加密**：使用 bcrypt 等算法存储密码
2. **SQL注入防护**：使用参数化查询
3. **数据验证**：在应用层对输入数据进行验证
4. **权限控制**：严格的权限检查，防止越权访问
5. **数据备份**：定期备份数据库，确保数据安全