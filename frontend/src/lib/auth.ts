/**
 * 认证 composable —— 迁移自 lib/common.js 的 checkAuth / logout
 *
 * 关键修复：
 * - checkAuth 不再只读 localStorage，而是先调 /api/auth/me 验证会话有效性
 *   （修复 common.js:42-63 cookie 过期前端无感知的问题）
 * - logout 带 CSRF 头（修复 V10 回归导致的 403）
 * - userInfo 改为响应式，跨组件共享
 */
import { reactive } from 'vue'
import { apiRequest, setCsrfToken, clearCsrfToken } from './api'

export interface UserInfo {
  id: string
  username: string
  name: string
  role_id: number
  className?: string
  points?: number
}

export const ROLE_NAMES: Record<number, string> = {
  1: '管理员',
  2: '教师',
  3: '学生',
  4: '家长',
}

/** 跳转到对应角色主页（适配 GitHub Pages 子路径） */
export function getRoleHome(roleId: number): string {
  // import.meta.env.BASE_URL 由 Vite 注入，值为 vite.config.ts 的 base
  // 本地开发为 '/'，GitHub Pages 为 '/campus-master/'
  const base = import.meta.env.BASE_URL || '/'
  const page = (() => {
    switch (roleId) {
      case 1: return 'admin.html'
      case 2: return 'teacher.html'
      case 3: return 'student.html'
      case 4: return 'parent.html'
      default: return ''
    }
  })()
  return base + page
}

/** 模块级响应式 userInfo，全应用共享（登录前为空对象） */
const userInfo = reactive<Partial<UserInfo>>({})

function loadFromStorage(): UserInfo | null {
  const str = localStorage.getItem('userInfo')
  if (!str) return null
  try {
    return JSON.parse(str) as UserInfo
  } catch {
    return null
  }
}

function saveUserInfo(info: UserInfo) {
  Object.assign(userInfo, info)
  localStorage.setItem('userInfo', JSON.stringify(info))
}

function clearUserInfo() {
  Object.keys(userInfo).forEach(k => delete (userInfo as Record<string, unknown>)[k])
  localStorage.removeItem('userInfo')
}

/**
 * 登录：先尝试 /api/auth/login（已支持全部角色含家长 role_id=4），
 * 失败则回退 /api/parent/login（修复原 index.html 的 key bug：parent_password → password）。
 * 成功后存储 userInfo + csrf_token，并跳转角色主页。
 * @returns 成功返回 true，失败返回 false（错误 Toast 由 api 层处理）
 */
export async function login(username: string, password: string): Promise<boolean> {
  // 1. 普通登录（已支持家长 role_id=4，is_parent 会话）
  const res = await apiRequest<{ user: UserInfo; csrf_token: string }>('POST', '/api/auth/login', {
    username,
    password,
  })
  if (res.code === 200 && res.data?.user) {
    saveUserInfo(res.data.user)
    setCsrfToken(res.data.csrf_token)
    location.href = getRoleHome(res.data.user.role_id)
    return true
  }

  // 2. 家长登录回退（修复 key：password 而非 parent_password；后端已下发 csrf_token）
  const pres = await apiRequest<{ user: UserInfo; csrf_token: string }>(
    'POST',
    '/api/parent/login',
    { username, password }
  )
  if (pres.code === 200 && pres.data?.user) {
    saveUserInfo(pres.data.user)
    setCsrfToken(pres.data.csrf_token)
    location.href = getRoleHome(pres.data.user.role_id)
    return true
  }

  return false
}

/**
 * 退出登录：调用 /api/auth/logout（带 CSRF 头），清本地态并跳登录页
 */
export async function logout() {
  try {
    await apiRequest('POST', '/api/auth/logout')
  } catch {
    // 忽略错误，继续清本地态
  }
  clearUserInfo()
  clearCsrfToken()
  location.href = import.meta.env.BASE_URL || '/'
}

/**
 * 校验登录状态与角色权限
 * 修复：先调 /api/auth/me 验证会话，避免只读 localStorage 导致 cookie 过期无感知
 *
 * @param roleId 期望角色 ID：1=管理员 / 2=教师 / 3=学生 / 4=家长
 * @returns 校验通过返回 UserInfo，否则跳转登录页并返回 null
 */
export async function checkAuth(roleId: number): Promise<UserInfo | null> {
  // 1. 先用 localStorage 快速短路（无本地态直接跳）
  const local = loadFromStorage()
  if (!local) {
    location.href = import.meta.env.BASE_URL || '/'
    return null
  }

  // 2. 调 /api/auth/me 验证会话有效性
  const res = await apiRequest<UserInfo>('GET', '/api/auth/me')
  if (res.code !== 200 || !res.data) {
    clearUserInfo()
    clearCsrfToken()
    location.href = import.meta.env.BASE_URL || '/'
    return null
  }

  // 3. 用服务端数据刷新本地（防止本地被篡改）
  saveUserInfo(res.data)

  // 4. 角色校验
  if (res.data.role_id !== roleId) {
    const { toast } = await import('../composables/useToast')
    toast.error(`您不是${ROLE_NAMES[roleId] || '该角色'}，无法访问此页面`)
    location.href = getRoleHome(res.data.role_id)
    return null
  }

  return res.data
}

/** 响应式 userInfo，组件中直接 import 使用 */
export function useAuth() {
  return { userInfo, login, logout, checkAuth }
}
