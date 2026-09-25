/**
 * 统一 API 请求封装 —— 迁移自 lib/common.js 的 apiRequest
 *
 * 关键修复（CSRF 回归）：
 * - 登录响应中的 csrf_token 由 auth.ts 存入 localStorage（key: 'csrf_token'）
 * - 写方法（POST/PUT/DELETE）自动从 localStorage 读取并注入 X-CSRF-Token 头
 *   对应后端 auth.h csrf_check()：校验该头与 HttpOnly cookie csrf_token 相等
 * - 401 自动跳登录页；403/网络错误/非 JSON 通过 useToast 报错
 *
 * Mock 模式（GitHub Pages Demo）：
 * - 当 VITE_USE_MOCK=true 时，所有请求走 mockRequest()，返回 localStorage 数据
 * - 用于纯前端部署（无 C++ 后端环境）
 */
import { toast } from '../composables/useToast'

const API_BASE = ''

/**
 * 安全修复 V18（B2）：后端「必须先改初始口令」的信号。
 * 事件名与文案必须与后端一致：auth.h:327-340 的 403 响应体
 * {"code":403,"must_change_password":true,"msg":"首次登录必须修改初始口令后才能使用其他功能"}。
 */
export const MUST_CHANGE_PASSWORD_EVENT = 'auth:must-change-password'
export const PASSWORD_CHANGE_MSG = '首次登录必须修改初始口令后才能使用其他功能'

export interface ApiResponse<T = any> {
  code: number
  msg?: string
  data?: T
  [key: string]: any
}

/** 读取 CSRF token（登录时由 auth.ts 写入） */
function getCsrfToken(): string {
  return localStorage.getItem('csrf_token') || ''
}

/** 登录成功后由 auth.ts 调用，存储 CSRF token */
export function setCsrfToken(token: string) {
  if (token) localStorage.setItem('csrf_token', token)
}

/** 退出时清除 */
export function clearCsrfToken() {
  localStorage.removeItem('csrf_token')
}

function isWriteMethod(method: string): boolean {
  return method === 'POST' || method === 'PUT' || method === 'DELETE' || method === 'PATCH'
}

/**
 * 统一 API 请求
 * @param method HTTP 方法
 * @param url 相对路径，如 '/api/admin/users'
 * @param data 请求体（POST/PUT/DELETE 带体时）
 * @returns 解析后的 JSON 响应
 */
export async function apiRequest<T = any>(
  method: string,
  url: string,
  data?: any
): Promise<ApiResponse<T>> {
  // Mock 模式：GitHub Pages 纯前端 Demo，无 C++ 后端。
  //
  // 为什么这里用**内联常量判断 + 动态 import**，而不是调用 mock 模块里的 isMockEnabled()：
  //   * vite.config.ts 的 define 只做「字面量替换」，不做代码内联 —— 在**另一个模块**里的
  //     `import.meta.env.VITE_USE_MOCK === 'true'` 会被替换成 `'false' === 'true'` 并折叠为
  //     false，但 mock 模块自身的导出会被继续保留；
  //   * 原本 `import { isMockEnabled, mockRequest } from '../mock'` 是无条件静态导入，
  //     Rollup 无法据此摇掉该模块 → mock 的 localStorage 键、提示语等字符串全部进入
  //     非 mock 产物。CI 的 F1 守卫（ci.yml 的 `grep campus_mock_db_v1`）因此**必然失败**，
  //     与「是否真的启用了 mock」无关（batch 0 起 CI 的 package job 一直红）。
  //   * 现在：条件内联后 'false' === 'true' → false，分支成为静态死代码；
  //     `import('../mock')` 只存在于该死分支内 → Rollup 直接移除该分支与整个 mock chunk。
  if (import.meta.env.VITE_USE_MOCK === 'true') {
    const { mockRequest } = await import('../mock')
    return mockRequest<T>(method, url, data)
  }

  const options: RequestInit = {
    method,
    credentials: 'include',
    headers: {} as Record<string, string>,
  }

  const headers = options.headers as Record<string, string>

  if (data !== undefined && data !== null) {
    headers['Content-Type'] = 'application/json'
    options.body = JSON.stringify(data)
  }
  // F24：不再为无体 DELETE 手动设置 Content-Length。
  // 依据（MDN Forbidden request header）：Content-Length 属 fetch 规范禁止的请求头，
  // 浏览器对 Headers.set 静默忽略 —— 该行从未生效。无请求体时由 fetch 自动置 0，
  // 后端 httplib 对无 Content-Length 的 DELETE 正常路由（路由层按方法+路径匹配）。

  // CSRF：写方法注入 X-CSRF-Token（后端 csrf_check 校验与 cookie 相等）
  if (isWriteMethod(method)) {
    const csrf = getCsrfToken()
    if (csrf) headers['X-CSRF-Token'] = csrf
  }

  let response: Response
  try {
    response = await fetch(API_BASE + url, options)
  } catch (e) {
    // 网络错误 / 服务器不可达
    toast.error('网络异常，请检查连接后重试')
    throw e
  }

  // 尝试解析 JSON
  let json: ApiResponse<T>
  try {
    json = await response.json()
  } catch (e) {
    toast.error('服务器响应格式错误')
    throw e
  }

  // HTTP 状态码处理
  if (response.status === 401) {
    // 会话失效：清本地态并跳登录
    localStorage.removeItem('userInfo')
    clearCsrfToken()
    if (!location.pathname.endsWith('/index.html') && location.pathname !== '/' && location.pathname !== import.meta.env.BASE_URL) {
      location.href = import.meta.env.BASE_URL || '/'
    }
    return json
  }

  if (response.status === 403) {
    // 安全修复 V18（B2）：未改初始口令的会话访问业务接口 → 后端返回
    // {"code":403,"must_change_password":true,...}（auth.h:327-340）。
    // 广播事件让全局改密门禁接管，不再只弹一条 toast。
    if (json.must_change_password === true || json.msg === PASSWORD_CHANGE_MSG) {
      window.dispatchEvent(new CustomEvent(MUST_CHANGE_PASSWORD_EVENT))
      return json
    }
    // CSRF 校验失败 / 权限不足
    const msg = json.msg || (isWriteMethod(method) ? 'CSRF 校验失败，请刷新页面重试' : '权限不足')
    toast.error(msg)
    return json
  }

  if (response.status === 429) {
    toast.warning(json.msg || '请求过于频繁，请稍后再试')
    return json
  }

  // 业务码非 200 但 HTTP 200：交由调用方处理，不打断
  return json
}

/** 便捷方法 */
export const api = {
  get: <T = any>(url: string) => apiRequest<T>('GET', url),
  post: <T = any>(url: string, data?: any) => apiRequest<T>('POST', url, data),
  put: <T = any>(url: string, data?: any) => apiRequest<T>('PUT', url, data),
  delete: <T = any>(url: string, data?: any) => apiRequest<T>('DELETE', url, data),
}
