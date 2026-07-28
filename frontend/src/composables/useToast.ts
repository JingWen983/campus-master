/**
 * 全局 Toast 系统 —— 替换原 121 处原生 alert()
 *
 * 设计：模块级响应式状态 + 单一 ToastContainer 组件挂载在 #app 外层。
 * 任意组件调用 showToast(msg) 即可触发，无需 props 传递。
 */
import { reactive, readonly } from 'vue'

export type ToastType = 'success' | 'error' | 'warning' | 'info'

export interface ToastItem {
  id: number
  message: string
  type: ToastType
  /** 自动关闭延时（ms），0 表示不自动关闭 */
  duration: number
}

interface ToastState {
  list: ToastItem[]
}

const state = reactive<ToastState>({ list: [] })
let seq = 0

function push(message: string, type: ToastType, duration = 3000) {
  const id = ++seq
  state.list.push({ id, message, type, duration })
  if (duration > 0) {
    setTimeout(() => remove(id), duration)
  }
  return id
}

function remove(id: number) {
  const idx = state.list.findIndex(t => t.id === id)
  if (idx !== -1) state.list.splice(idx, 1)
}

/** 便捷 API：在任意组件 / 模块中直接调用 */
export const toast = {
  success: (msg: string, duration?: number) => push(msg, 'success', duration),
  error: (msg: string, duration?: number) => push(msg, 'error', duration ?? 4000),
  warning: (msg: string, duration?: number) => push(msg, 'warning', duration),
  info: (msg: string, duration?: number) => push(msg, 'info', duration),
  remove,
}

/** 兼容别名：showToast(msg, type) —— 替换 alert() 的最小改动写法 */
export function showToast(message: string, type: ToastType = 'info', duration?: number) {
  push(message, type, duration)
}

/** ToastContainer 组件读取的只读状态 */
export function useToast() {
  return {
    toasts: readonly(state).list,
    remove,
  }
}
