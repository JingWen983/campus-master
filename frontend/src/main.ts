import { createApp, h, type Component } from 'vue'
import './style.css'
// @font-face 声明（Noto Sans SC + Fraunces，迁移自原 /lib/fonts.css）
import './assets/fonts.css'
// FontAwesome 全局样式（webfonts 由 Vite 从 node_modules 自动 bundle）
import '@fortawesome/fontawesome-free/css/all.min.css'

// 全局 Toast / Confirm 容器（替换 121 处原生 alert/confirm）
import ToastContainer from './components/ToastContainer.vue'
import ConfirmDialog from './components/ConfirmDialog.vue'
// 安全修复 V18（B2）：全局强制首登改密门禁 —— 5 个入口页共用，避免逐页重复接入
import ChangePasswordGate from './components/ChangePasswordGate.vue'

// 全局格式化工具（注册为 globalProperties，模板中用 $formatDateTime）
import { formatDateTime, formatDate, formatRelative } from './lib/format'

/**
 * 通用挂载工厂：每个入口页调用 mountApp(RootComponent)
 * - 注册 globalProperties：$formatDateTime / $formatDate / $formatRelative
 * - 挂载全局 Toast 容器与 Confirm 对话框（teleport 到 body，独立于 #app）
 */
export function mountApp(root: Component) {
  const app = createApp(root)

  // globalProperties：模板中可直接使用 $formatDateTime(row.created_at)
  app.config.globalProperties.$formatDateTime = formatDateTime
  app.config.globalProperties.$formatDate = formatDate
  app.config.globalProperties.$formatRelative = formatRelative

  // 全局 Toast / Confirm 容器：以独立子 app 挂载到 body，
  // 避免与各页根组件的状态相互影响（两个 app 不共享响应式上下文，
  // 但 composables/useToast.ts 与 useConfirm.ts 使用模块级状态，跨 app 共享）
  // 同一个独立 app 里同时挂载强制改密门禁（B2）：它自身的状态来自
  // lib/passwordGate.ts 的模块级 ref 与 lib/auth.ts 的 localStorage，与根组件解耦。
  const host = document.createElement('div')
  host.id = '__global_overlays__'
  document.body.appendChild(host)
  createApp({
    render: () => h('div', null, [h(ToastContainer), h(ConfirmDialog), h(ChangePasswordGate)]),
  }).mount(host)

  app.mount('#app')
}
