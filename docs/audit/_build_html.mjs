// 将 docs/audit/*.md 导出为独立 HTML
// 自包含 Markdown 转换器：不 spawn 子进程，避免沙箱 piped-stdio 限制
import { readFileSync, writeFileSync, readdirSync } from 'node:fs';
import { join, basename, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

// fileURLToPath 会正确解码非 ASCII 路径（本项目路径含中文）
const DIR = dirname(fileURLToPath(import.meta.url));

// ---------- 行内转换 ----------
const esc = (s) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');

function inline(text) {
  const codes = [];
  // 保护行内代码
  let s = text.replace(/`([^`]+)`/g, (_, c) => {
    codes.push(c);
    return `\u0000C${codes.length - 1}\u0000`;
  });

  s = esc(s);
  s = s.replace(/\*\*\*([^*]+)\*\*\*/g, '<strong><em>$1</em></strong>');
  s = s.replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');
  s = s.replace(/(^|[^*])\*([^*\s][^*]*)\*/g, '$1<em>$2</em>');
  // 链接
  s = s.replace(/\[([^\]]+)\]\(([^)]+)\)/g, '<a href="$2">$1</a>');

  // 还原行内代码
  s = s.replace(/\u0000C(\d+)\u0000/g, (_, i) => `<code>${esc(codes[+i])}</code>`);
  return s;
}

// ---------- 表格 ----------
const isTableSep = (l) => /^\|[\s:|-]+\|$/.test(l.trim());
const splitRow = (l) =>
  l.trim().replace(/^\|/, '').replace(/\|$/, '').split('|').map((c) => c.trim());

// ---------- 块级转换 ----------
// 返回 HTML 字符串；标题收集进传入的 toc 数组（保证嵌套调用可以安全复用）
function md2html(md, toc = []) {
  const lines = md.replace(/\r\n/g, '\n').split('\n');
  const out = [];
  let i = 0;
  let inCode = false;
  let codeBuf = [];
  let codeLang = '';
  let listStack = []; // 'ul' | 'ol'

  const closeLists = () => {
    while (listStack.length) out.push(`</${listStack.pop()}>`);
  };

  const slug = (t) =>
    'h-' + t.toLowerCase().replace(/<[^>]+>/g, '').replace(/[^\w\u4e00-\u9fa5]+/g, '-').replace(/^-|-$/g, '').slice(0, 60);

  while (i < lines.length) {
    const raw = lines[i];
    const line = raw.trimEnd();

    // 代码块
    if (/^```/.test(line.trim())) {
      if (!inCode) {
        closeLists();
        inCode = true;
        codeLang = line.trim().slice(3).trim();
        codeBuf = [];
      } else {
        inCode = false;
        out.push(
          `<pre class="code"${codeLang ? ` data-lang="${esc(codeLang)}"` : ''}><code>${esc(codeBuf.join('\n'))}</code></pre>`
        );
      }
      i++;
      continue;
    }
    if (inCode) {
      codeBuf.push(raw);
      i++;
      continue;
    }

    // 空行
    if (!line.trim()) {
      closeLists();
      i++;
      continue;
    }

    // 分隔线
    if (/^(-{3,}|\*{3,}|_{3,})$/.test(line.trim())) {
      closeLists();
      out.push('<hr>');
      i++;
      continue;
    }

    // 标题
    const h = line.match(/^(#{1,6})\s+(.*)$/);
    if (h) {
      closeLists();
      const lvl = h[1].length;
      const txt = h[2].trim();
      const id = slug(txt);
      if (lvl >= 2 && lvl <= 3) toc.push({ lvl, id, txt: txt.replace(/<[^>]+>/g, '') });
      out.push(`<h${lvl} id="${id}">${inline(txt)}</h${lvl}>`);
      i++;
      continue;
    }

    // 引用块（支持连续行 / 内含标题如 "> # xxx"）
    if (/^>\s?/.test(line)) {
      closeLists();
      const buf = [];
      while (i < lines.length && /^>\s?/.test(lines[i])) {
        buf.push(lines[i].replace(/^>\s?/, ''));
        i++;
      }
      const inner = buf.join('\n');
      const bigH = inner.match(/^#\s+(.*)$/);
      if (bigH) {
        out.push(`<blockquote class="callout"><p class="callout-big">${inline(bigH[1])}</p></blockquote>`);
      } else {
        out.push(`<blockquote>${md2html(inner)}</blockquote>`);
      }      continue;
    }

    // 表格
    if (/^\|/.test(line.trim()) && i + 1 < lines.length && isTableSep(lines[i + 1])) {
      closeLists();
      const head = splitRow(line);
      i += 2;
      const rows = [];
      while (i < lines.length && /^\|/.test(lines[i].trim())) {
        rows.push(splitRow(lines[i]));
        i++;
      }
      out.push('<div class="tw"><table>');
      out.push('<thead><tr>' + head.map((c) => `<th>${inline(c)}</th>`).join('') + '</tr></thead>');
      out.push('<tbody>');
      for (const r of rows) {
        out.push('<tr>' + r.map((c) => `<td>${inline(c)}</td>`).join('') + '</tr>');
      }
      out.push('</tbody></table></div>');
      continue;
    }

    // 列表
    const ul = line.match(/^[-*+]\s+(.*)$/);
    const ol = line.match(/^(\d+)[.)]\s+(.*)$/);
    if (ul || ol) {
      const want = ul ? 'ul' : 'ol';
      if (listStack[listStack.length - 1] !== want) {
        closeLists();
        listStack.push(want);
        out.push(`<${want}>`);
      }
      out.push(`<li>${inline(ul ? ul[1] : ol[2])}</li>`);
      i++;
      continue;
    }

    // 普通段落
    closeLists();
    const para = [line.trim()];
    i++;
    while (
      i < lines.length &&
      lines[i].trim() &&
      !/^(#{1,6}\s|>|\||```|[-*+]\s|\d+[.)]\s)/.test(lines[i].trim()) &&
      !/^(-{3,}|\*{3,}|_{3,})$/.test(lines[i].trim())
    ) {
      para.push(lines[i].trim());
      i++;
    }
    out.push(`<p>${inline(para.join(' '))}</p>`);
  }
  closeLists();
  return out.join('\n');
}

// ---------- 页面模板 ----------
const TITLES = {
  'CODE_QUALITY_SUMMARY': '代码质量评估总览',
  'CODE_QUALITY_REVIEW': '代码质量改进建议书（主交付物）',
  'VERIFICATION': '独立复核报告',
  'BACKEND_FINDINGS': '后端 C++ 审计',
  'FRONTEND_FINDINGS': '前端 Vue3/TS 审计',
};

const ORDER = ['CODE_QUALITY_SUMMARY', 'CODE_QUALITY_REVIEW', 'VERIFICATION', 'BACKEND_FINDINGS', 'FRONTEND_FINDINGS'];

const NAV = (cur) =>
  ORDER.map((k) => {
    const cls = k === cur ? ' class="active"' : '';
    return `<a href="${k}.html"${cls}>${TITLES[k]}</a>`;
  }).join('\n      ');

function page(key, bodyHtml, toc, meta) {
  const title = TITLES[key] || '总览与导航';
  const tocHtml = toc
    .filter((t) => t.lvl === 2)
    .map((t) => `<a class="l2" href="#${t.id}">${esc(t.txt)}</a>`)
    .join('\n        ');
  return `<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>${title} — 校园能量站代码质量评估</title>
<style>
:root{--bg:#f7f8fa;--panel:#fff;--ink:#1f2328;--muted:#5a6572;--line:#e3e6ea;--accent:#0f766e;
--blocker:#b91c1c;--high:#c2410c;--med:#a16207;--low:#4b5563;--code-bg:#f3f4f6;}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);
font:16px/1.75 -apple-system,BlinkMacSystemFont,"Segoe UI","PingFang SC","Hiragino Sans GB","Microsoft YaHei",sans-serif;}
.wrap{display:flex;max-width:1500px;margin:0 auto;gap:0;align-items:flex-start}
aside{position:sticky;top:0;height:100vh;overflow-y:auto;flex:0 0 280px;background:#0f172a;color:#cbd5e1;padding:24px 18px}
aside h1{font-size:15px;color:#fff;margin:0 0 4px;letter-spacing:.02em}
aside .sub{font-size:12px;color:#94a3b8;margin-bottom:20px;line-height:1.5}
aside nav a{display:block;color:#cbd5e1;text-decoration:none;font-size:13.5px;padding:7px 10px;border-radius:7px;margin-bottom:3px}
aside nav a:hover{background:#1e293b;color:#fff}
aside nav a.active{background:var(--accent);color:#fff;font-weight:600}
aside .toc{border-top:1px solid #1e293b;margin-top:18px;padding-top:14px}
aside .toc .h{font-size:11px;text-transform:uppercase;letter-spacing:.08em;color:#64748b;margin-bottom:8px}
aside .toc a{display:block;color:#94a3b8;text-decoration:none;font-size:12.5px;padding:4px 10px;border-left:2px solid #1e293b;line-height:1.45}
aside .toc a:hover{color:#fff;border-left-color:var(--accent)}
main{flex:1 1 auto;min-width:0;background:var(--panel);padding:48px 60px 96px;min-height:100vh}
.meta{font-size:12.5px;color:var(--muted);border-bottom:1px solid var(--line);padding-bottom:16px;margin-bottom:32px}
h1{font-size:30px;margin:0 0 10px;letter-spacing:-.01em}
h2{font-size:22px;margin:44px 0 14px;padding-top:14px;border-top:1px solid var(--line)}
h2:first-of-type{border-top:none}
h3{font-size:17.5px;margin:28px 0 10px}
h4{font-size:15.5px;margin:20px 0 8px;color:var(--muted)}
p{margin:12px 0}
a{color:#0e7490}
ul,ol{margin:12px 0;padding-left:26px}
li{margin:5px 0}
code{background:var(--code-bg);padding:2px 6px;border-radius:5px;font-size:13.5px;
font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,"Cascadia Code",monospace;color:#0f172a}
pre.code{background:#0f172a;color:#e2e8f0;padding:16px 18px;border-radius:10px;overflow-x:auto;margin:16px 0;
font-size:13.5px;line-height:1.6}
pre.code code{background:none;color:inherit;padding:0;font-size:inherit}
blockquote{margin:18px 0;padding:14px 20px;background:#f0fdfa;border-left:4px solid var(--accent);border-radius:0 8px 8px 0;color:#134e4a}
blockquote blockquote{margin:8px 0}
blockquote.callout{background:#fef2f2;border-left-color:var(--blocker)}
blockquote.callout .callout-big{font-size:18px;font-weight:700;color:var(--blocker);margin:0;line-height:1.5}
hr{border:none;border-top:1px solid var(--line);margin:36px 0}
.tw{overflow-x:auto;margin:18px 0}
table{border-collapse:collapse;width:100%;font-size:14px}
th,td{border:1px solid var(--line);padding:9px 12px;text-align:left;vertical-align:top}
th{background:#f1f5f9;font-weight:600;white-space:nowrap}
tr:nth-child(even) td{background:#fafbfc}
strong{color:#0b1220}
@media print{
  aside{display:none}
  body{background:#fff}
  main{padding:0;max-width:100%}
  pre.code{background:#f5f5f5;color:#111;border:1px solid #ddd}
  h2{page-break-after:avoid}
  .tw,table,pre.code,blockquote{page-break-inside:avoid}
}
@media(max-width:900px){.wrap{flex-direction:column}aside{position:static;height:auto;width:100%;flex:none}main{padding:28px 20px 64px}}
</style>
</head>
<body>
<div class="wrap">
  <aside>
    <h1>校园能量站 · 代码质量评估</h1>
    <div class="sub">只读审计 · 未改动任何源码<br>2026-09-12</div>
    <nav>
      ${NAV(key)}
    </nav>
    <div class="toc">
      <div class="h">本页目录</div>
      ${tocHtml}
    </div>
  </aside>
  <main>
    <div class="meta">${meta}</div>
    ${bodyHtml}
  </main>
</div>
</body>
</html>`;
}

// ---------- 执行 ----------
const files = readdirSync(DIR).filter((f) => f.endsWith('.md'));
const done = [];
for (const f of files) {
  const key = basename(f, '.md');
  if (!TITLES[key]) continue;
  const md = readFileSync(join(DIR, f), 'utf8');
  const toc = [];
  const html = md2html(md, toc);
  const lines = md.split('\n').length;
  const bytes = Buffer.byteLength(md, 'utf8');
  const meta = `源文件 <code>${f}</code> · ${lines} 行 · ${bytes.toLocaleString('en-US')} 字节 · 共 ${toc.filter((t) => t.lvl === 2).length} 个章节`;
  writeFileSync(join(DIR, key + '.html'), page(key, html, toc, meta), 'utf8');
  done.push(`${key}.html  (${lines} 行 md / ${toc.filter((t) => t.lvl === 2).length} 章节)`);
}

// 索引页
const idxBody = `<h1>校园能量站 — 代码质量评估</h1>
<div class="meta">只读审计 · 未改动任何源码、配置或构建产物 · 评估日期 2026-09-12</div>
<blockquote class="callout"><p class="callout-big">不得用 F1 的存在降低 B1 的紧急度；修复顺序 B1 先于 F1。</p></blockquote>
<p>本次评估共 <strong>60 条</strong>发现：blocker <strong>2</strong> / high <strong>14</strong> / medium <strong>23</strong> / low <strong>21</strong>。覆盖后端自研 18 文件 6,084 行、前端 src 34 文件 9,513 行，约 15,600 行自研代码。</p>
${ORDER.map((k, n) => {
  const desc = {
    CODE_QUALITY_SUMMARY: '阅读入口。13 节，从头读到尾即可掌握全局：结论、批次 0、两个 blocker 详解、勘误表、局限声明。',
    CODE_QUALITY_REVIEW: '完整改进建议书。七节结构，含发现清单总表、分主题详述、四批路线图与勘误表。',
    VERIFICATION: '独立复核。逐条裁定 CONFIRMED 35 / PARTIAL 3 / REFUTED 0，含 14 行勘误表与 7 条存疑事项判定。',
    BACKEND_FINDINGS: '后端原始审计。30 条发现，每条含文件:行号与源码原文证据。',
    FRONTEND_FINDINGS: '前端原始审计。24 条发现，含巨型组件量化指标与构建产物分析。',
  }[k];
  return `<h3><a href="${k}.html">${n + 1}. ${TITLES[k]}</a></h3><p>${desc}</p>`;
}).join('\n')}
<hr>
<p style="color:#5a6572;font-size:14px">审计期间未运行 <code>server.exe</code>、未发起 HTTP 请求、未 commit。报告中所有修复动作均为<strong>建议，尚未执行</strong>。</p>`;

writeFileSync(
  join(DIR, 'index.html'),
  page('__index', idxBody, [], '总览与导航'),
  'utf8'
);
done.push('index.html  (索引页)');
console.log('导出完成:\n' + done.map((d) => '  - ' + d).join('\n'));
