#ifndef WINHTTP_DOWNLOADER_H
#define WINHTTP_DOWNLOADER_H

// ============================================================================
// WinHTTP 下载器 —— 供"在线自更新"下载 Release zip 使用
// ----------------------------------------------------------------------------
// 设计要点：
//   * 纯 Windows API（winhttp），无第三方依赖；MinGW 与 MSVC 均可编译；
//   * 自动跟随重定向（镜像站 302 → GitHub 实际文件）；镜像 URL 直接作起点；
//   * 分块拉取（64 KiB），进度回调按已下载字节数通知（前端轮询显示进度条）；
//   * 同步阻塞式：在 handler 线程内直接下载（zip 约 6 MB，内网/镜像一般 < 1 分钟）；
//   * UA 标识 campus-energy-station-updater，部分镜像对空 UA 拒绝。
// ============================================================================

#include <string>
#include <functional>
#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")   // MSVC；MinGW 走链接命令行 -lwinhttp

namespace upd {

struct DownloadResult {
    bool ok = false;
    int http_status = 0;
    unsigned long long bytes = 0;
    std::string err;         // 失败原因（给人看，不回显到前端）
    std::string saved_path;
};

// 同步下载 url → save_path。on_progress(bytes, total) 可为空。
// total 可能为 0（服务器未给 Content-Length），此时 UI 显示"已下载字节"。
inline DownloadResult download_file(const std::string& url,
                                    const std::string& save_path,
                                    const std::function<void(unsigned long long, unsigned long long)>& on_progress = nullptr) {
    DownloadResult r;
    r.saved_path = save_path;

    // ---- 拆解 URL（仅支持 http/https）----
    // URL_COMPONENTSA 在部分 MinGW winhttp 头里未导出（仅 W 版），统一用 wide API。
    URL_COMPONENTSW uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t whost[256] = {0};
    wchar_t wpath[2048] = {0};
    uc.lpszHostName = whost;     uc.dwHostNameLength = 255;
    uc.lpszUrlPath = wpath;      uc.dwUrlPathLength = 2047;
    // 先转 UTF-16（URL 含中文场景无，但镜像 URL 纯 ASCII，等宽转换即可）
    std::wstring wurl(url.begin(), url.end());
    if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc)) {
        r.err = "URL 解析失败";
        return r;
    }
    const bool https = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    std::string host(uc.lpszHostName, uc.lpszHostName + uc.dwHostNameLength);
    std::string path(uc.lpszUrlPath, uc.lpszUrlPath + uc.dwUrlPathLength);

    HINTERNET hSession = WinHttpOpen(L"campus-energy-station-updater/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { r.err = "WinHttpOpen 失败"; return r; }

    // 30 秒连接 / 60 秒收发超时（镜像偶发慢启动）
    WinHttpSetTimeouts(hSession, 30000, 30000, 60000, 60000);

    HINTERNET hConnect = WinHttpConnect(hSession,
                                        std::wstring(host.begin(), host.end()).c_str(),
                                        https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); r.err = "WinHttpConnect 失败"; return r; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
                                            std::wstring(path.begin(), path.end()).c_str(),
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            https ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        r.err = "WinHttpOpenRequest 失败";
        return r;
    }

    // 自动重定向（镜像 302 常见）
    DWORD flags = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &flags, sizeof(flags));

    bool done = false;
    for (int attempt = 0; attempt < 2 && !done; attempt++) {   // 一次内部重试
        if (attempt > 0) {   // 重试前小等
            Sleep(1500);
        }
        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            r.err = "发送请求失败（网络不可达？）";
            continue;
        }
        if (!WinHttpReceiveResponse(hRequest, nullptr)) {
            r.err = "接收响应失败";
            continue;
        }
        DWORD status = 0, size = sizeof(status);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
        r.http_status = (int)status;
        if (status != 200) {
            r.err = "HTTP " + std::to_string(status);
            continue;
        }

        // Content-Length（可选）
        unsigned long long total = 0;
        DWORD len64 = sizeof(total);
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER64,
                                WINHTTP_HEADER_NAME_BY_INDEX, &total, &len64, WINHTTP_NO_HEADER_INDEX)) {
            // total 已取得
        }

        FILE* fp = nullptr;
        if (fopen_s(&fp, save_path.c_str(), "wb") != 0 || !fp) {
            r.err = "无法写入文件 " + save_path;
            break;
        }
        char buf[65536];
        DWORD bytes_read = 0;
        bool io_err = false;
        while (true) {
            if (!WinHttpReadData(hRequest, buf, sizeof(buf), &bytes_read)) {
                r.err = "读取数据失败"; io_err = true; break;
            }
            if (bytes_read == 0) break;   // EOF
            if (fwrite(buf, 1, bytes_read, fp) != bytes_read) {
                r.err = "写盘失败（磁盘满？）"; io_err = true; break;
            }
            r.bytes += bytes_read;
            if (on_progress) on_progress(r.bytes, total);
        }
        fclose(fp);
        if (!io_err) {
            r.ok = true;
            done = true;
        } else {
            remove(save_path.c_str());   // 半截文件不留
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return r;
}

} // namespace upd

#endif // WINHTTP_DOWNLOADER_H
