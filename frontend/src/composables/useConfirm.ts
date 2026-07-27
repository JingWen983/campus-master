/**
 * 全局 Confirm 系统 —— Promise 化替换原生 confirm()
 *
 * 设计：模块级响应式状态 + 单一 ConfirmDialog 组件。
 * 调用 const ok = await confirmDialog({ title, message }) 等待用户点击。
 */
import { reactive } from 'vue'

export interface ConfirmOptions {
  title?: string
  message: string
  /** 确认按钮文字 */
  confirmText?: string
  /** 取消按钮文字 */
  cancelText?: string
  /** 确认按钮主题：danger(红) / primary(主) / warning(黄) */
  variant?: 'danger' | 'primary' | 'warning'
}

interface ConfirmState extends Required<Omit<ConfirmOptions, 'title'>> {
  visible: boolean
  title: string
  resolve: ((value: boolean) => void) | null
}

const state = reactive<ConfirmState>({
  visible: false,
  title: '',
  message: '',
  confirmText: '确认',
  cancelText: '取消',
  variant: 'primary',
  resolve: null,
})

/**
 * 弹出确认框，返回 Promise<boolean>：true=确认 / false=取消
 * 替换原生 confirm() 的等价写法：const ok = await confirmDialog({message:'确定吗？'})
 */
export function confirmDialog(options: ConfirmOptions): Promise<boolean> {
  return new Promise(resolve => {
    state.title = options.title ?? '请确认'
    state.message = options.message
    state.confirmText = options.confirmText ?? '确认'
    state.cancelText = options.cancelText ?? '取消'
    state.variant = options.variant ?? 'primary'
    state.visible = true
    state.resolve = resolve
  })
}

/** ConfirmDialog 组件内部调用 */
export function useConfirm() {
  function answer(ok: boolean) {
    if (state.resolve) {
      state.resolve(ok)
      state.resolve = null
    }
    state.visible = false
  }
  return { state, answer }
}
