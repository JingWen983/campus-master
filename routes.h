#ifndef ROUTES_H
#define ROUTES_H

#include "httplib.h"

// 路由注册函数声明
// 每个函数负责注册一组相关的 API 路由

// 静态文件路由（index.html, admin.html, teacher.html, student.html, lib/）
void register_static_routes(httplib::Server& svr);

// 认证路由（/api/auth/*, /api/user/*, /api/behavior/*, /api/mall/*, /api/rank/*）
void register_public_routes(httplib::Server& svr);

// 管理员路由（/api/admin/*）
void register_admin_routes(httplib::Server& svr);

// 教师路由（/api/teacher/*）
void register_teacher_routes(httplib::Server& svr);

// 学生路由（/api/student/*）
void register_student_routes(httplib::Server& svr);

// 家长路由（/api/parent/*）
void register_parent_routes(httplib::Server& svr);

#endif
