/**
 * 格式化工具 —— 迁移自 lib/common.js 的 formatDateTime
 *
 * 在 main.ts 注册为 app.config.globalProperties.$formatDateTime，
 * 模板中可直接用 $formatDateTime(row.created_at)，
 * 消除 teacher.html:1389 / parent.html:1057 的 method 包装。
 */

/**
 * 格式化日期时间字符串为 'YYYY-MM-DD HH:mm:ss'
 * @param dateStr 可被 Date 解析的日期时间；为空返回 ''
 * @returns 格式化字符串；解析失败时返回原值
 */
export function formatDateTime(dateStr: string | number | Date | null | undefined): string {
  if (!dateStr) return ''
  const date = new Date(dateStr)
  if (isNaN(date.getTime())) return String(dateStr)

  const pad = (n: number) => String(n).padStart(2, '0')
  const year = date.getFullYear()
  const month = pad(date.getMonth() + 1)
  const day = pad(date.getDate())
  const hours = pad(date.getHours())
  const minutes = pad(date.getMinutes())
  const seconds = pad(date.getSeconds())

  return `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`
}

/** 仅日期：YYYY-MM-DD */
export function formatDate(dateStr: string | number | Date | null | undefined): string {
  if (!dateStr) return ''
  const date = new Date(dateStr)
  if (isNaN(date.getTime())) return String(dateStr)
  const pad = (n: number) => String(n).padStart(2, '0')
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}`
}

/** 相对时间：刚刚 / N 分钟前 / N 小时前 / N 天前 / 超过 7 天回退到 formatDateTime */
export function formatRelative(dateStr: string | number | Date | null | undefined): string {
  if (!dateStr) return ''
  const date = new Date(dateStr)
  if (isNaN(date.getTime())) return String(dateStr)
  const diff = Date.now() - date.getTime()
  const sec = Math.floor(diff / 1000)
  if (sec < 60) return '刚刚'
  const min = Math.floor(sec / 60)
  if (min < 60) return `${min} 分钟前`
  const hr = Math.floor(min / 60)
  if (hr < 24) return `${hr} 小时前`
  const day = Math.floor(hr / 24)
  if (day < 7) return `${day} 天前`
  return formatDateTime(dateStr)
}
