/**
 * 更新检查 —— 自动检查新版本 + 自动选择合适的镜像通道
 *
 * 设计要点：
 * - 检查源（update.json 元数据）与下载源（zip 附件）分离，各自竞速；
 * - 检查源 4 条通道并发"竞速"：最先返回**有效 JSON**者胜出（失败的通道自然被淘汰）；
 * - 下载直链 github.com 在部分网络不可达，故 zip 下载按钮按延迟排序提供镜像：
 *   gh-proxy.com / ghfast.top / gh-proxy.net 等 gh-proxy 系镜像拼接 Release 附件地址；
 * - 仅管理员使用（在 AdminApp 系统配置 tab 中挂载，并做登录后静默自动检查 + 导航徽章）。
 *
 * 元数据文件 update.json 随仓库根提交（README 更新清单说明），内容形如：
 * {
 *   "version": "0.0.9",
 *   "notes": "- 积分卡……\n- 检查更新……",
 *   "published_at": "2026-10-24",
 *   "min_version": "0.0.1"
 * }
 */

/** 本前端构建的版本号：发布时由构建环境 VITE_APP_VERSION 注入，本地开发显示 dev */
export const CURRENT_VERSION: string = import.meta.env.VITE_APP_VERSION || 'dev'

export const REPO = 'JingWen983/campus-master'
export const REPO_URL = `https://github.com/${REPO}`
export const RELEASES_URL = `${REPO_URL}/releases`
/** 下载文件名必须与 release.yml 的 Create zip package 步骤一致 */
export const ZIP_NAME_PATTERN = 'campus-energy-station'

/** 检查源：多通道竞速（按网络实测延迟与 CORS 支持选定，均可浏览器直连） */
const MANIFEST_SOURCES: string[] = [
  `https://cdn.jsdelivr.net/gh/${REPO}@main/update.json`,
  `https://raw.githubusercontent.com/${REPO}/main/update.json`,
  `https://gh-proxy.com/https://raw.githubusercontent.com/${REPO}/main/update.json`,
  `https://ghfast.top/https://raw.githubusercontent.com/${REPO}/main/update.json`,
]

/** zip 下载镜像模板（{zip} 替换为完整附件 URL）；github 直链放最后 */
const ZIP_MIRRORS: { label: string; url: string }[] = [
  { label: 'gh-proxy.com 镜像', url: `https://gh-proxy.com/https://github.com/${REPO}/releases/download/{tag}/${ZIP_NAME_PATTERN}-{tag}.zip` },
  { label: 'ghfast.top 镜像', url: `https://ghfast.top/https://github.com/${REPO}/releases/download/{tag}/${ZIP_NAME_PATTERN}-{tag}.zip` },
  { label: 'GitHub 官方直链', url: `https://github.com/${REPO}/releases/download/{tag}/${ZIP_NAME_PATTERN}-{tag}.zip` },
]

export interface UpdateManifest {
  version: string
  notes?: string
  published_at?: string
  min_version?: string
}

export type ChannelStatus = 'pending' | 'ok' | 'fail' | 'slow'

export interface ChannelResult {
  label: string
  status: ChannelStatus
  ms?: number
}

export interface UpdateCheckResult {
  hasUpdate: boolean
  current: string
  latest: string
  notes: string
  publishedAt: string
  /** 按实测延迟排序的下载通道（标签 + 已填充 tag 的 URL） */
  downloadLinks: { label: string; url: string; ms: number }[]
  channels: ChannelResult[]
  checkedAt: string
}

/** 语义化版本比较：返回 1 / 0 / -1（支持 v 前缀与三段数字） */
export function compareVersions(a: string, b: string): number {
  const pa = a.replace(/^v/i, '').split('.').map(n => parseInt(n, 10) || 0)
  const pb = b.replace(/^v/i, '').split('.').map(n => parseInt(n, 10) || 0)
  for (let i = 0; i < 3; i++) {
    const d = (pa[i] || 0) - (pb[i] || 0)
    if (d !== 0) return d > 0 ? 1 : -1
  }
  return 0
}

/** 带 CORS 超时的 fetch；成功返回 JSON，失败抛错 */
async function fetchJson(url: string, timeoutMs: number): Promise<UpdateManifest> {
  const ctrl = new AbortController()
  const timer = setTimeout(() => ctrl.abort(), timeoutMs)
  try {
    const res = await fetch(url, { signal: ctrl.signal, cache: 'no-store' })
    if (!res.ok) throw new Error(`HTTP ${res.status}`)
    const j = await res.json()
    if (!j || typeof j.version !== 'string') throw new Error('无效清单')
    return j as UpdateManifest
  } finally {
    clearTimeout(timer)
  }
}

/**
 * 并发竞速检查所有通道，返回首个成功通道的清单 + 各通道状态。
 * @param onChannel 逐通道回调（UI 实时显示各通道状态）
 */
export async function checkForUpdate(
  onChannel?: (r: ChannelResult) => void
): Promise<{ manifest: UpdateManifest; channels: ChannelResult[] } | null> {
  const channels: ChannelResult[] = MANIFEST_SOURCES.map((u, i) => ({
    label: sourceLabel(u, i),
    status: 'pending' as ChannelStatus,
  }))
  channels.forEach(c => onChannel?.({ ...c }))

  const tasks = MANIFEST_SOURCES.map(async (url, i) => {
    const t0 = performance.now()
    try {
      const m = await fetchJson(url, 8000)
      const ms = Math.round(performance.now() - t0)
      channels[i].status = 'ok'
      channels[i].ms = ms
      onChannel?.({ ...channels[i] })
      return { manifest: m, order: i }
    } catch {
      channels[i].status = 'fail'
      onChannel?.({ ...channels[i] })
      return null
    }
  })

  // 全部跑完后按成功者中"最先发起即最快"取第一个非空（保持数组顺序优先级）
  const results = await Promise.all(tasks)
  const success = results.filter((r): r is { manifest: UpdateManifest; order: number } => r !== null)
  if (success.length === 0) return null

  // 把未完成的 pending 置为 slow（竞速已结束仍未返回）
  channels.forEach(c => { if (c.status === 'pending') c.status = 'slow' })
  return { manifest: success[0].manifest, channels }
}

function sourceLabel(url: string, i: number): string {
  const names = ['jsDelivr CDN', 'GitHub Raw', 'gh-proxy.com 镜像', 'ghfast.top 镜像']
  return names[i] || url
}

/**
 * 探测各下载镜像对指定 tag 的可用性与延迟（HEAD 请求）。
 * 返回按延迟升序的可用通道；全部失败时仍返回官方直链兜底。
 */
export async function probeDownloadLinks(tag: string): Promise<{ label: string; url: string; ms: number }[]> {
  const links = ZIP_MIRRORS.map(m => ({
    label: m.label,
    url: m.url.replace('{tag}', tag),
    ms: 0,
  }))
  await Promise.all(links.map(async link => {
    const t0 = performance.now()
    try {
      const ctrl = new AbortController()
      const timer = setTimeout(() => ctrl.abort(), 6000)
      const res = await fetch(link.url, { method: 'HEAD', signal: ctrl.signal, mode: 'cors' })
      clearTimeout(timer)
      link.ms = res.ok || res.status === 302 ? Math.round(performance.now() - t0) : -1
    } catch {
      link.ms = -1
    }
  }))
  // 可用（ms>=0）按延迟升序在前；不可用保持原顺序跟在后面（用户仍可手动尝试）
  const ok = links.filter(l => l.ms >= 0).sort((a, b) => a.ms - b.ms)
  const rest = links.filter(l => l.ms < 0)
  return [...ok, ...rest]
}

/** localStorage 键：跳过该版本的提醒 */
const SKIP_KEY = 'update_skip_version'
export function skipVersion(v: string) {
  try { localStorage.setItem(SKIP_KEY, v) } catch { /* 忽略 */ }
}
export function getSkipVersion(): string {
  try { return localStorage.getItem(SKIP_KEY) || '' } catch { return '' }
}

/** localStorage 键：上次检查时间（ISO 字符串） */
const LAST_CHECK_KEY = 'update_last_check'
export function setLastCheck(iso: string) {
  try { localStorage.setItem(LAST_CHECK_KEY, iso) } catch { /* 忽略 */ }
}
export function getLastCheck(): string {
  try { return localStorage.getItem(LAST_CHECK_KEY) || '' } catch { return '' }
}
