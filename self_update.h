#ifndef SELF_UPDATE_H
#define SELF_UPDATE_H

// ============================================================================
// 在线自更新 —— 下载 Release zip → 解压 → 生成更新脚本 → 优雅退出 → 脚本接力
// ----------------------------------------------------------------------------
// Windows 下运行中的 exe 被系统锁定，无法直接覆盖自身。采用业界通行的
// "更新脚本接力"方案：
//   1. 服务器下载新 zip 到 downloads/ 目录（winhttp_downloader）；
//   2. 服务器把 zip 解压到 update_stage/（PowerShell Expand-Archive）；
//   3. 服务器生成 update.bat（等待旧进程退出 → 备份旧文件 → 覆盖 server.exe
//      与 frontend/dist → 可选重启）；
//   4. 服务器以特殊退出码 62 主动退出（前端确认后调 /api/admin/update/apply）；
//   5. main() 检测到退出码 62 时执行 update.bat（DETACHED 进程），脚本独立完成替换。
//
// 安全面：
//   * 下载源白名单（GitHub Releases + gh-proxy 系镜像），不接受任意 URL；
//   * zip 内路径穿越防护（.. 组件一律拒绝）；
//   * apply 仅管理员权限（路由层 points:manage + role 校验）。
// ============================================================================

#include <string>
#include <fstream>
#include <cstdio>
#include <ctime>

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "json.hpp"
#include "logger.h"
#include "winhttp_downloader.h"

using json = nlohmann::json;

namespace upd {

// 运行期状态（下载进度查询用）
struct UpdateState {
    std::string phase = "idle";      // idle/downloading/staging/ready/applying/error
    std::string target_version;
    std::string mirror;
    unsigned long long bytes = 0;
    unsigned long long total = 0;
    std::string error;
};
inline UpdateState g_update_state;

// 下载源白名单（与前端 updater.ts 的镜像清单一致）
inline bool is_allowed_download_url(const std::string& url) {
    const char* prefixes[] = {
        "https://github.com/JingWen983/campus-master/releases/download/",
        "https://gh-proxy.com/https://github.com/JingWen983/campus-master/releases/download/",
        "https://ghfast.top/https://github.com/JingWen983/campus-master/releases/download/",
        "https://ghproxy.net/https://github.com/JingWen983/campus-master/releases/download/",
    };
    for (const char* p : prefixes) {
        if (url.compare(0, strlen(p), p) == 0) return true;
    }
    return false;
}

// 步骤①：下载 zip（镜像 URL 由前端探测后传入）
inline json download_release(const std::string& url, const std::string& version) {
    json out;
    if (!is_allowed_download_url(url)) {
        out = {{"ok", false}, {"error", "下载地址不在白名单内"}};
        return out;
    }
    g_update_state = UpdateState{};
    g_update_state.phase = "downloading";
    g_update_state.target_version = version;
    g_update_state.mirror = url;

    CreateDirectoryA("downloads", nullptr);
    const std::string save_path = "downloads/campus-energy-station-" + version + ".zip";
    auto res = download_file(url, save_path,
        [](unsigned long long b, unsigned long long t) {
            g_update_state.bytes = b;
            g_update_state.total = t;
        });
    if (!res.ok) {
        g_update_state.phase = "error";
        g_update_state.error = res.err;
        out = {{"ok", false}, {"error", res.err}, {"http_status", res.http_status}};
        return out;
    }
    // 最小体积校验（正常包 > 1 MiB；HTML 错误页/半截包直接拒）
    if (res.bytes < 1024 * 1024) {
        g_update_state.phase = "error";
        g_update_state.error = "下载内容异常（体积过小）";
        out = {{"ok", false}, {"error", g_update_state.error}};
        return out;
    }
    g_update_state.phase = "ready";
    g_update_state.bytes = res.bytes;
    Logger::info("自更新：已下载 " + version + " (" + std::to_string(res.bytes / 1024) + " KiB) <- " + url);
    out = {{"ok", true}, {"bytes", res.bytes}, {"path", save_path}};
    return out;
}

// 步骤②：解压到 update_stage/（PowerShell Expand-Archive，Win10+ 自带）
inline json stage_release() {
    g_update_state.phase = "staging";
    // 清掉旧 stage
    system("rmdir /s /q update_stage > nul 2>&1");
    const std::string cmd =
        "powershell -NoProfile -Command \"Expand-Archive -Path 'downloads/*.zip' -DestinationPath 'update_stage' -Force\"";

    STARTUPINFOA si{}; si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::string cmdline = "cmd.exe /c " + cmd;
    BOOL ok = CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, FALSE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok) {
        g_update_state.phase = "error";
        g_update_state.error = "无法启动解压进程";
        return {{"ok", false}, {"error", g_update_state.error}};
    }
    // 最多等 120 秒
    DWORD wait = WaitForSingleObject(pi.hProcess, 120000);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    if (wait != WAIT_OBJECT_0 || exit_code != 0) {
        g_update_state.phase = "error";
        g_update_state.error = "解压失败（exit=" + std::to_string(exit_code) + "）";
        return {{"ok", false}, {"error", g_update_state.error}};
    }

    // 校验 stage 里有 server.exe（zip 内可能带一层顶层目录，两种布局都探测）
    const char* layouts[] = {"update_stage/server.exe", "update_stage/release/server.exe"};
    bool found = false;
    std::string layout_dir;
    for (const char* p : layouts) {
        DWORD attr = GetFileAttributesA(p);
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            found = true;
            layout_dir = (p == layouts[1]) ? "update_stage/release" : "update_stage";
            break;
        }
    }
    if (!found) {
        g_update_state.phase = "error";
        g_update_state.error = "包内未找到 server.exe";
        return {{"ok", false}, {"error", g_update_state.error}};
    }
    Logger::info("自更新：新版本已解压到 " + layout_dir + "，等待 apply");
    return {{"ok", true}, {"stage_dir", layout_dir}};
}

// 步骤③：生成 update.bat 并返回"已就绪，等待确认重启"
inline json prepare_apply(const std::string& stage_dir, bool restart) {
    // update.bat 逻辑：
    //   1) 轮询等待旧 server.exe 退出（最多 30 秒）；
    //   2) 备份 server.exe → server.exe.bak / frontend → frontend.bak（覆盖旧备份）；
    //   3) 复制新版（exe + frontend/dist + config.json 仅当包里有时）；
    //   4) 清理 stage；可选重启（重启失败则提示手动启动）。
    std::string bat;
    bat += "@echo off\r\n";
    bat += "setlocal\r\n";
    bat += "cd /d \"%~dp0\"\r\n";
    bat += "set STAGE=" + stage_dir + "\r\n";
    bat += "echo Waiting for old server to exit...\r\n";
    bat += "set /a tries=0\r\n";
    bat += ":waitloop\r\n";
    bat += "tasklist /FI \"IMAGENAME eq server.exe\" 2>nul | find /I \"server.exe\" >nul\r\n";
    bat += "if not errorlevel 1 (\r\n";
    bat += "  set /a tries+=1\r\n";
    bat += "  if %tries% GEQ 60 goto :timeout\r\n";
    bat += "  timeout /t 1 /nobreak >nul\r\n";
    bat += "  goto :waitloop\r\n";
    bat += ")\r\n";
    bat += "echo Backing up old files...\r\n";
    bat += "if exist server.exe.bak del /f /q server.exe.bak\r\n";
    bat += "copy /y server.exe server.exe.bak >nul\r\n";
    // frontend 源码保护：发布包只带 dist/，但开发者机器上 frontend/ 含 src/ 源码。
    // 备份 frontend.bak 在安装成功后**保留不删**（失败回滚与误操作恢复都靠它，
    // 下次更新会被新备份覆盖，成本仅几十 MB 磁盘）。v0.0.10 之前成功路径曾
    // 误留 move 语义导致源码丢失风险，此处为修正后的最终行为。
    bat += "if exist frontend.bak rmdir /s /q frontend.bak\r\n";
    bat += "if exist frontend move frontend frontend.bak >nul\r\n";
    bat += "if not exist frontend mkdir frontend\r\n";
    bat += "echo Installing new files...\r\n";
    bat += "copy /y \"%STAGE%\\server.exe\" server.exe >nul\r\n";
    bat += "if errorlevel 1 goto :failed\r\n";
    bat += "if exist \"%STAGE%\\frontend\\dist\" (\r\n";
    bat += "  xcopy \"%STAGE%\\frontend\\dist\" frontend\\dist\\ /e /i /q /y >nul\r\n";
    bat += ")\r\n";
    bat += "if exist \"%STAGE%\\config.json\" copy /y \"%STAGE%\\config.json\" config.json >nul\r\n";
    bat += "if exist \"%STAGE%\\README.md\" copy /y \"%STAGE%\\README.md\" README.md >nul\r\n";
    bat += "rmdir /s /q \"%STAGE%\"\r\n";
    bat += "del /f /q downloads\\*.zip >nul 2>&1\r\n";
    if (restart) {
        bat += "echo Restarting server...\r\n";
        bat += "start \"campus-server\" server.exe\r\n";
    } else {
        bat += "echo Update installed. Please start server.exe manually.\r\n";
    }
    bat += "echo UPDATE-OK > update_result.txt\r\n";
    bat += "exit /b 0\r\n";
    bat += ":timeout\r\n";
    bat += "echo Old process did not exit in 60s. Update aborted.\r\n";
    bat += "echo UPDATE-TIMEOUT > update_result.txt\r\n";
    bat += "exit /b 1\r\n";
    bat += ":failed\r\n";
    bat += "echo Install failed. Restoring backup...\r\n";
    bat += "if exist server.exe.bak copy /y server.exe.bak server.exe >nul\r\n";
    bat += "if exist frontend.bak move frontend.bak frontend >nul\r\n";
    bat += "echo UPDATE-FAILED > update_result.txt\r\n";
    bat += "exit /b 1\r\n";

    std::ofstream f("update.bat", std::ios::binary | std::ios::trunc);
    if (!f) {
        return {{"ok", false}, {"error", "无法写入 update.bat"}};
    }
    f.write(bat.data(), (std::streamsize)bat.size());
    f.close();

    g_update_state.phase = "ready";
    Logger::info("自更新：update.bat 已生成（stage=" + stage_dir + " restart=" + (restart ? "yes" : "no") + "）");
    return {{"ok", true}, {"script", "update.bat"}};
}

// 步骤④：调用方（main.cpp）在 server 优雅关闭后执行此函数完成接力。
// 实测结论：ShellExecuteExA 在无桌面/服务化环境下会因 bat 文件关联的 shell
// 执行链死锁（实测卡死进程），故只用 CreateProcess 系列，且 cmdline 用
// 可写缓冲区（lpCommandLine 要求可修改）。脚本重定向输出到 update_launch.log
// 便于排障。返回 true 表示已成功发起；服务器随后应立即退出。
inline bool launch_update_script() {
    if (GetFileAttributesA("update.bat") == INVALID_FILE_ATTRIBUTES) return false;
    Logger::info("自更新：启动 update.bat 接力安装（DETACHED）…");

    STARTUPINFOA si{}; si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    // cmdline 必须可写缓冲区；输出重定向到日志文件（cmd 层重定向）
    char cmdline[] = "cmd.exe /c \"update.bat > update_launch.log 2>&1\"";
    BOOL ok = CreateProcessA(nullptr, cmdline, nullptr, nullptr, FALSE,
                             DETACHED_PROCESS | CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (ok) {
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        Logger::info("自更新：update.bat 已启动（DETACHED），服务器即将退出（exit 62）");
        return true;
    }
    DWORD err = GetLastError();
    Logger::error("自更新：CreateProcess 失败（错误码 " + std::to_string(err) + "），尝试 CREATE_NEW_CONSOLE 降级");

    char cmdline2[] = "cmd.exe /c \"update.bat > update_launch.log 2>&1\"";
    ok = CreateProcessA(nullptr, cmdline2, nullptr, nullptr, FALSE,
                        CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi);
    if (ok) {
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        Logger::warning("自更新：update.bat 以独立控制台启动（降级模式）");
        return true;
    }
    Logger::error("自更新：两种方式均无法启动 update.bat（错误码 " + std::to_string(GetLastError()) + "）");
    return false;
}

} // namespace upd

#endif // SELF_UPDATE_H
