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
import { isMockEnabled, mockRequest } from '../mock'

const API_BASE = ''

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
  // Mock 模式：GitHub Pages 纯前端 Demo，无 C++ 后端
  if (isMockEnabled()) {
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
  } else if (method === 'DELETE') {
    // 无请求体的 DELETE 需 Content-Length:0，兼容部分反代
    headers['Content-Length'] = '0'
  }

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
