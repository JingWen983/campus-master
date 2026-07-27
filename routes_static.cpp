#include "routes.h"
#include "auth.h"
#include "logger.h"
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

// 前端构建产物目录（Vite 输出：5 个 HTML + assets/）
static const std::string FRONTEND_DIR = "frontend/dist";

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

// 按文件扩展名设置 Content-Type（精确后缀匹配，避免 .js 误匹配 .json）
static std::string content_type_for(const std::string& path) {
    auto pos = path.find_last_of('.');
    if (pos == std::string::npos) return "application/octet-stream";
    std::string ext = path.substr(pos);
    if (ext == ".js" || ext == ".mjs")  return "application/javascript";
    if (ext == ".css")                   return "text/css";
    if (ext == ".json")                  return "application/json";
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".woff2")                 return "font/woff2";
    if (ext == ".woff")                  return "font/woff";
    if (ext == ".ttf")                   return "font/ttf";
    if (ext == ".svg")                   return "image/svg+xml";
    if (ext == ".png")                   return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif")                   return "image/gif";
    if (ext == ".ico")                   return "image/x-icon";
    if (ext == ".map")                   return "application/json";
    return "application/octet-stream";
}

// 从 FRONTEND_DIR 读取指定相对路径文件，写入 content 并返回 true；失败返回 false
static bool read_frontend_file(const std::string& rel_path,
                               std::string& content,
                               std::string& content_type_out) {
    if (!is_path_safe(rel_path)) return false;
    std::string full = FRONTEND_DIR + "/" + rel_path;
    std::ifstream ifs(full, std::ios::binary);
    if (!ifs) return false;
    content.assign((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    content_type_out = content_type_for(rel_path);
    return true;
}

// 便捷：直接向 res 输出 FRONTEND_DIR 下的文件，失败返回 404
static void serve_frontend_file(httplib::Response& res, const std::string& rel_path) {
    std::string content, ctype;
    if (!read_frontend_file(rel_path, content, ctype)) {
        res.status = 404;
        res.set_content("Not found", "text/plain; charset=utf-8");
        return;
    }
    res.set_content(content, ctype.c_str());
}

void register_static_routes(httplib::Server& svr) {
    // 1. 处理前端 POST 请求前的 OPTIONS 预检请求 (解决跨域拦截)
    svr.Options(R"(.*)", [](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        res.status = 200;
    });

    // 2. HTML 页面路由（表驱动，替代 5 段重复 ifstream handler）
    // URL → FRONTEND_DIR 下的文件名
    static const std::vector<std::pair<std::string, std::string>> html_routes = {
        {"/",            "index.html"},
        {"/index.html",  "index.html"},
        {"/admin.html",  "admin.html"},
        {"/teacher.html","teacher.html"},
        {"/student.html","student.html"},
        {"/parent.html", "parent.html"},
    };
    for (const auto& route : html_routes) {
        svr.Get(route.first, [file = route.second](const httplib::Request& req, httplib::Response& res) {
            serve_frontend_file(res, file);
        });
    }

    // 3. Vite 构建产物 /assets/* （hashed JS/CSS/字体，由 Vite 生成）
    svr.Get("/assets/.*", [](const httplib::Request& req, httplib::Response& res) {
        // req.path 形如 /assets/index-AbCd.js，去掉前导 '/' 即相对 FRONTEND_DIR 的路径
        std::string rel = req.path.substr(1); // "assets/index-AbCd.js"
        serve_frontend_file(res, rel);
    });
}
