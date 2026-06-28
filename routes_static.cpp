#include "routes.h"
#include "auth.h"
#include "logger.h"
#include <fstream>
#include <iterator>

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
        std::string path = req.path;
        // 移除开头的 /
        if (path.substr(0, 1) == "/") {
            path = path.substr(1);
        }

        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) {
            res.status = 404;
            res.set_content("File not found: " + path, "text/plain");
            return;
        }

        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        // 根据文件扩展名设置 Content-Type
        const char* content_type = "application/octet-stream";
        if (path.find(".js") != std::string::npos) {
            content_type = "application/javascript";
        } else if (path.find(".css") != std::string::npos) {
            content_type = "text/css";
        } else if (path.find(".json") != std::string::npos) {
            content_type = "application/json";
        } else if (path.find(".html") != std::string::npos) {
            content_type = "text/html";
        } else if (path.find(".woff2") != std::string::npos) {
            content_type = "font/woff2";
        } else if (path.find(".woff") != std::string::npos) {
            content_type = "font/woff";
        } else if (path.find(".ttf") != std::string::npos) {
            content_type = "font/ttf";
        } else if (path.find(".svg") != std::string::npos) {
            content_type = "image/svg+xml";
        } else if (path.find(".png") != std::string::npos) {
            content_type = "image/png";
        } else if (path.find(".jpg") != std::string::npos || path.find(".jpeg") != std::string::npos) {
            content_type = "image/jpeg";
        }

        res.set_content(content, content_type);
    });

    // 0.5 返回 webfonts 目录下的字体文件
    svr.Get("/webfonts/.*", [](const httplib::Request& req, httplib::Response& res) {
        std::string path = req.path;
        // 移除开头的 /
        if (path.substr(0, 1) == "/") {
            path = path.substr(1);
        }

        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) {
            res.status = 404;
            res.set_content("File not found: " + path, "text/plain");
            return;
        }

        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        // 根据文件扩展名设置 Content-Type
        const char* content_type = "application/octet-stream";
        if (path.find(".woff2") != std::string::npos) {
            content_type = "font/woff2";
        } else if (path.find(".woff") != std::string::npos) {
            content_type = "font/woff";
        } else if (path.find(".ttf") != std::string::npos) {
            content_type = "font/ttf";
        } else if (path.find(".eot") != std::string::npos) {
            content_type = "application/vnd.ms-fontobject";
        }

        res.set_content(content, content_type);
    });
}
