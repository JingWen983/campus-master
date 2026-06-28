#include "routes.h"
#include "auth.h"
#include "logger.h"
#include <fstream>
#include <iterator>
#include <system_error>

// 安全修复 V3：检查路径是否包含 .. 等穿越组件
// 已 URL 解码后的路径若包含 ".." 段，则视为非法
static bool is_path_safe(const std::string& path) {
    if (path.find("..") != std::string::npos) return false;
    // 拒绝绝对路径与盘符
    if (!path.empty() && path[0] == '/') {
        // 仅允许单斜杠开头（合法的 URL path），但不能是 //
        if (path.size() >= 2 && path[1] == '/') return false;
    }
    if (path.size() >= 2 && path[1] == ':') return false; // Windows 盘符
    return true;
}

// 安全修复 V3：在指定白名单目录内解析文件，越界返回 false
static bool read_file_in_base(const std::string& url_path,
                              const std::string& base_prefix,
                              std::string& content,
                              std::string& content_type_out) {
    // 仅接受形如 /base/xxx 的请求，并强制拼接为相对工作目录路径
    if (url_path.size() <= base_prefix.size()) return false;
    if (url_path.compare(0, base_prefix.size(), base_prefix) != 0) return false;

    std::string rel = url_path.substr(1); // 去掉开头的 '/'
    if (!is_path_safe(rel)) return false;

    std::ifstream ifs(rel, std::ios::binary);
    if (!ifs) return false;
    content.assign((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    // 根据文件扩展名设置 Content-Type（白名单）
    content_type_out = "application/octet-stream";
    if (rel.find(".js") != std::string::npos)        content_type_out = "application/javascript";
    else if (rel.find(".css") != std::string::npos)  content_type_out = "text/css";
    else if (rel.find(".json") != std::string::npos) content_type_out = "application/json";
    else if (rel.find(".html") != std::string::npos) content_type_out = "text/html";
    else if (rel.find(".woff2") != std::string::npos) content_type_out = "font/woff2";
    else if (rel.find(".woff") != std::string::npos) content_type_out = "font/woff";
    else if (rel.find(".ttf") != std::string::npos)  content_type_out = "font/ttf";
    else if (rel.find(".svg") != std::string::npos)  content_type_out = "image/svg+xml";
    else if (rel.find(".png") != std::string::npos)  content_type_out = "image/png";
    else if (rel.find(".jpg") != std::string::npos || rel.find(".jpeg") != std::string::npos)
        content_type_out = "image/jpeg";
    return true;
}

void register_static_routes(httplib::Server& svr) {
    // 1. 处理前端发送 POST 请求前的 OPTIONS 预检请求 (解决跨域拦截)
    svr.Options(R"(.*)", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        res.status = 200;
    });

    // 0. 根路径返回 index.html (登录页面)
    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream ifs("index.html", std::ios::binary);
        if (!ifs) {
            res.status = 404;
            res.set_content("index.html not found", "text/plain");
            return;
        }

        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        res.set_content(content, "text/html");
    });

    // 0.0 /index.html 也返回登录页面
    svr.Get("/index.html", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream ifs("index.html", std::ios::binary);
        if (!ifs) {
            res.status = 404;
            res.set_content("index.html not found", "text/plain");
            return;
        }

        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        res.set_content(content, "text/html");
    });

    // 0.1 返回 admin.html
    svr.Get("/admin.html", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream ifs("admin.html", std::ios::binary);
        if (!ifs) {
            res.status = 404;
            res.set_content("admin.html not found", "text/plain");
            return;
        }

        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        res.set_content(content, "text/html");
    });

    // 0.2 返回 teacher.html
    svr.Get("/teacher.html", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream ifs("teacher.html", std::ios::binary);
        if (!ifs) {
            res.status = 404;
            res.set_content("teacher.html not found", "text/plain");
            return;
        }

        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        res.set_content(content, "text/html");
    });

    // 0.3 返回 student.html
    svr.Get("/student.html", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream ifs("student.html", std::ios::binary);
        if (!ifs) {
            res.status = 404;
            res.set_content("student.html not found", "text/plain");
            return;
        }

        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        res.set_content(content, "text/html");
    });

    // 0.3.1 返回 parent.html
    svr.Get("/parent.html", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream ifs("parent.html", std::ios::binary);
        if (!ifs) {
            res.status = 404;
            res.set_content("parent.html not found", "text/plain");
            return;
        }

        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        res.set_content(content, "text/html");
    });

    // 0.4 返回 lib 目录下的静态文件
    svr.Get("/lib/.*", [](const httplib::Request& req, httplib::Response& res) {
        std::string content, ctype;
        // 安全修复 V3：拒绝路径穿越，统一走白名单读取函数
        if (!read_file_in_base(req.path, "/lib/", content, ctype)) {
            res.status = 404;
            res.set_content("Not found", "text/plain");
            return;
        }
        res.set_content(content, ctype.c_str());
    });

    // 0.5 返回 webfonts 目录下的字体文件
    svr.Get("/webfonts/.*", [](const httplib::Request& req, httplib::Response& res) {
        std::string content, ctype;
        if (!read_file_in_base(req.path, "/webfonts/", content, ctype)) {
            res.status = 404;
            res.set_content("Not found", "text/plain");
            return;
        }
        res.set_content(content, ctype.c_str());
    });
}
