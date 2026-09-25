/**
 * 安全修复 V18（B2）：强制首登改密 —— 前端交互状态（composable）
 *
 * 后端契约出处：
 * - 登录响应携带 `must_change_password`：routes_public.cpp:85（user 内）与 :88（data 内）
 * - /api/auth/me 同样下发：routes_public.cpp:224（刷新页面后仍能识别）
 * - 未改密的会话访问业务接口 → 403 且响应体含 must_change_password:true：auth.h:327-340
 *   （经 check_permission_middleware(auth.h:346) 与 check_parent_auth_middleware(auth.h:395) 双门禁）
 * - 改密端点：POST /api/auth/change-password，body {old_password,new_password}：routes_public.cpp:438-536
 * - 白名单：改密 / 登出 / me 不被门禁拦截（auth.h:325-326）
 *
 * api.ts 在收到该 403 时广播 MUST_CHANGE_PASSWORD_EVENT，本模块监听并置位，
 * 由全局挂载的 ChangePasswordGate.vue 渲染阻断式改密表单。
 */
import { ref } from 'vue'
import { MUST_CHANGE_PASSWORD_EVENT } from './api'
import { isPasswordChangeRequired } from './auth'

/** true = 必须改密，主界面被门禁阻断 */
export const mustChangePassword = ref(isPasswordChangeRequired())

let listening = false
/** 幂等注册监听（全局门禁组件 mount 时调用） */
export function watchPasswordChangeRequirement(): void {
  if (listening) return
  listening = true
  window.addEventListener(MUST_CHANGE_PASSWORD_EVENT, () => {
    mustChangePassword.value = true
  })
}

/** 从本地 userInfo 重新同步一次（登录跳转后进入新页面时调用） */
export function syncPasswordChangeRequirement(): void {
  mustChangePassword.value = isPasswordChangeRequired()
}

/** 改密成功：放行主界面 */
export function releasePasswordGate(): void {
  mustChangePassword.value = false
}
