/// <reference types="vite/client" />

declare module '*.vue' {
  import type { DefineComponent } from 'vue'
  const component: DefineComponent<{}, {}, any>
  export default component
}

// 全局属性类型声明（main.ts 注册）
import type { formatDateTime as _fmt } from './lib/format'
declare module 'vue' {
  interface ComponentCustomProperties {
    $formatDateTime: typeof _fmt
    $formatDate: (dateStr: string | number | Date | null | undefined) => string
    $formatRelative: (dateStr: string | number | Date | null | undefined) => string
  }
}
