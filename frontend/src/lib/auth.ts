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
  /**
   * 安全修复 V18（B2）：true = 该账号仍是首启随机初始口令，必须先改密。
   * 由 /api/auth/login 与 /api/auth/me 下发（routes_public.cpp:85 / :224）。
   */
  must_change_password?: boolean
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

/** 读取本地已存 userInfo 中的「必须先改密」标记（页面刷新后仍可识别） */
export function isPasswordChangeRequired(): boolean {
  const local = loadFromStorage()
  return local?.must_change_password === true
}

/** 清除本地 userInfo 中的「必须先改密」标记（改密成功后调用） */
export function clearPasswordChangeRequired(): void {
  const local = loadFromStorage()
  if (!local) return
  local.must_change_password = false
  saveUserInfo(local)
}

/**
 * 安全修复 V18（B2）：修改口令（首登强制改密与常规改密共用的唯一出口）。
 *
 * 后端契约（routes_public.cpp:438-536）：
 *   POST /api/auth/change-password  body: {"old_password": "...", "new_password": "..."}
 *   200 {"code":200,"msg":"密码修改成功","data":{"must_change_password":false}}
 *   400 原密码错误 / 新旧相同 / 强度不足（<8 位或不同时含字母与数字）/ 命中弱口令 / 与用户名相同
 *   这一端点是强制改密门禁的白名单端点（auth.h:325-326），未改密会话可调用。
 */
export async function changePassword(
  oldPassword: string,
  newPassword: string
): Promise<{ ok: boolean; msg: string }> {
  const res = await apiRequest<{ must_change_password?: boolean }>('POST', '/api/auth/change-password', {
    old_password: oldPassword,
    new_password: newPassword,
  })
  if (res.code === 200) {
    clearPasswordChangeRequired()
    return { ok: true, msg: res.msg || '密码修改成功' }
  }
  return { ok: false, msg: res.msg || '密码修改失败，请重试' }
}

/**
 * 登录：先尝试 /api/auth/login（已支持全部角色含家长 role_id=4），
 * 失败则回退 /api/parent/login（修复原 index.html 的 key bug：parent_password → password）。
 * 成功后存储 userInfo + csrf_token，并跳转角色主页。
 * @returns 成功返回 true，失败返回 false（错误 Toast 由 api 层处理）
 */
export async function login(username: string, password: string): Promise<boolean> {
  // 1. 普通登录（已支持家长 role_id=4，is_parent 会话）
  const res = await apiRequest<{ user: UserInfo; csrf_token: string; must_change_password?: boolean }>('POST', '/api/auth/login', {
    username,
    password,
  })
  if (res.code === 200 && res.data?.user) {
    saveUserInfo(res.data.user)
    setCsrfToken(res.data.csrf_token)
    // 安全修复 V18（B2）：后端在登录响应里明确告知需改密
    // （routes_public.cpp:85 的 user.must_change_password 与 :88 的 data.must_change_password）
    if (res.data.user.must_change_password === true || res.data.must_change_password === true) {
      // 把标记落到本地 userInfo，供角色主页的全局改密门禁在刷新后仍能识别
      saveUserInfo({ ...res.data.user, must_change_password: true })
    }
    location.href = getRoleHome(res.data.user.role_id)
    return true
  }

  // 2. 家长登录回退（修复 key：password 而非 parent_password；后端已下发 csrf_token）
  const pres = await apiRequest<{ user: UserInfo; csrf_token: string; must_change_password?: boolean }>(
    'POST',
    '/api/parent/login',
    { username, password }
  )
  if (pres.code === 200 && pres.data?.user) {
    saveUserInfo(pres.data.user)
    setCsrfToken(pres.data.csrf_token)
    if (pres.data.user.must_change_password === true || pres.data.must_change_password === true) {
      saveUserInfo({ ...pres.data.user, must_change_password: true })
    }
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
