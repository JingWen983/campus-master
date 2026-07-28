# 计划：将所有资源文件改为本地路由

## 摘要

将 4 个 HTML 文件中引用的 Google Fonts CDN（Fraunces + Noto Sans SC）改为本地加载，实现完全离线运行。

## 当前状态分析

### 已本地化的资源（无需改动）
以下资源已在 `lib/` 目录中，通过 `/lib/*` 路由本地加载：
- `vue.min.js`、`tailwind.min.js`、`echarts.min.js`、`common.js`
- `fontawesome.min.css` + `lib/webfonts/`（fa-solid-900.woff2, .ttf）

### 仍使用 CDN 的外部资源（需改）
4 个 HTML 文件均引用 Google Fonts CDN：

| 文件 | 行号 | 外部 URL |
|------|------|----------|
| `index.html` | 11 | `https://fonts.googleapis.com/css2?family=Fraunces:...&family=Noto+Sans+SC:...` |
| `admin.html` | 21 | 同上（权重不同） |
| `teacher.html` | 14-16 | 含 preconnect + Google Fonts CSS |
| `student.html` | 24-26 | 含 preconnect + Google Fonts CSS |

该 CSS 文件内部引用 `fonts.gstatic.com` 上的 woff2 字体文件。

### 后端静态文件服务
`routes_static.cpp` 已支持 `/lib/*` 路由，能根据扩展名设置 Content-Type（含 `.css`、`.woff2`、`.ttf`），**无需修改后端**。

## 实施步骤

### 步骤 1：下载 Google Fonts CSS 和字体文件
使用 Python 脚本完成：
1. 用现代浏览器 User-Agent 请求 Google Fonts CSS2 API，获取含 woff2 URL 的 CSS 内容
2. 解析 CSS 中所有 `@font-face` 的 `url(...)` 引用
3. 下载每个 woff2 字体文件到 `lib/fonts/` 目录
4. 将 CSS 中的远程 URL 替换为本地路径（`./fonts/xxx.woff2`）
5. 保存为 `lib/fonts.css`

涉及的字体：
- **Fraunces**（衬线展示字体）：权重 300-900，含 italic 变体
- **Noto Sans SC**（中文正文）：权重 300-900

### 步骤 2：更新 4 个 HTML 文件
将每个文件中的 Google Fonts `<link>` 替换为本地引用：

**index.html（第 11 行）：**
```html
<!-- 替换前 -->
<link href="https://fonts.googleapis.com/css2?family=Fraunces:opsz,wght@9..144,400;9..144,600;9..144,800;9..144,900&family=Noto+Sans+SC:wght@300;400;500;700&display=swap" rel="stylesheet">
<!-- 替换后 -->
<link href="/lib/fonts.css" rel="stylesheet">
```

**admin.html（第 21 行）：**
```html
<!-- 替换前 -->
<link href="https://fonts.googleapis.com/css2?family=Fraunces:opsz,wght@9..144,400;9..144,600;9..144,700;9..144,800;9..144,900&family=Noto+Sans+SC:wght@300;400;500;700;900&display=swap" rel="stylesheet">
<!-- 替换后 -->
<link href="/lib/fonts.css" rel="stylesheet">
```

**teacher.html（第 14-16 行）：**
```html
<!-- 替换前 -->
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Fraunces:opsz,wght@9..144,400;9..144,500;9..144,600;9..144,700;9..144,800;9..144,900&family=Noto+Sans+SC:wght@300;400;500;600;700;800&display=swap" rel="stylesheet">
<!-- 替换后 -->
<link href="/lib/fonts.css" rel="stylesheet">
```

**student.html（第 24-26 行）：**
```html
<!-- 替换前 -->
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Fraunces:opsz,wght@9..144,300;9..144,400;9..144,500;9..144,600;9..144,700;9..144,800;9..144,900&family=Noto+Sans+SC:wght@300;400;500;600;700;800;900&display=swap" rel="stylesheet">
<!-- 替换后 -->
<link href="/lib/fonts.css" rel="stylesheet">
```

### 步骤 3：验证
1. 检查 `lib/fonts/` 目录下有所有 woff2 文件
2. 检查 `lib/fonts.css` 中所有 `url()` 指向本地路径
3. 启动服务器，用 Playwright 验证页面渲染正常、字体加载成功
4. 确认浏览器 Network 面板中无任何外部 CDN 请求

## 假设与决策
- **字体格式**：仅下载 woff2 格式（现代浏览器均支持，体积最小）
- **字体权重范围**：下载所有 HTML 文件中引用的权重的并集（Fraunces 300-900 + italic，Noto Sans SC 300-900），统一供所有页面使用
- **CSS 文件位置**：放在 `lib/fonts.css`，字体文件放在 `lib/fonts/` 子目录
- **后端无需修改**：`/lib/*` 路由已支持 `.css` 和 `.woff2` 的 Content-Type
- **不删除 `fontawesome.zip`**：虽然无用但不影响功能，避免额外改动

## 验证步骤
1. `Grep` 确认 4 个 HTML 文件中不再有任何 `https://` 或 `//` 开头的资源引用
2. 启动 `server.exe`，运行 `test_frontend_redesign.py` 验证页面渲染
3. Playwright 检查 `document.fonts.size > 0` 确认字体加载成功
