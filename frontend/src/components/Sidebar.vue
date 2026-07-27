<script setup lang="ts">
/**
 * 配置驱动侧边栏 —— 替换 4 角色重复的 aside 结构
 *
 * 功能：
 * - 品牌头部（logo 渐变 + 标题 + 英文副标）
 * - nav 菜单（navItems prop，active 态高亮，可选 badge）
 * - 用户卡片（头像 + 姓名 + 角色 + 退出按钮）
 * - #extra-section 插槽（parent 用于子女列表）
 *
 * 不含布局外壳，由 AppLayout 组合。
 */
import type { NavItem } from '../lib/navConfig'
import type { ThemeConfig } from '../lib/theme'
import { ROLE_NAMES } from '../lib/auth'
import { logout as doLogout } from '../lib/auth'

interface UserInfo {
  id: string
  username: string
  name: string
  role_id: number
  className?: string
  points?: number
}

const props = withDefaults(defineProps<{
  navItems: NavItem[]
  currentTab: string
  userInfo: UserInfo
  theme: ThemeConfig
  brand: string
  brandEn?: string
  brandIcon?: string
  width?: string
}>(), {
  brandEn: '',
  brandIcon: 'fa-leaf',
  width: 'w-64',
})

const emit = defineEmits<{
  (e: 'switch-tab', key: string): void
}>()

function onSwitch(key: string) {
  if (key !== props.currentTab) emit('switch-tab', key)
}

function onLogout() {
  doLogout()
}
</script>

<template>
  <aside
    class="hidden md:flex flex-col glass-side z-20 relative shrink-0"
    :class="width"
  >
    <!-- ① 品牌头部 -->
    <div class="px-6 pt-8 pb-6">
      <div class="flex items-center gap-3">
        <div
          class="w-11 h-11 rounded-2xl flex items-center justify-center text-white shadow-lg"
          :class="theme.logoGradient"
        >
          <i class="fa-solid text-xl" :class="brandIcon"></i>
        </div>
        <div>
          <h1 class="font-display font-bold text-stone-900 leading-tight">{{ brand }}</h1>
          <p v-if="brandEn" class="text-[10px] tracking-[0.2em] text-stone-400 uppercase">{{ brandEn }}</p>
        </div>
      </div>
    </div>

    <!-- 可选：额外分区插槽（parent 子女列表） -->
    <slot name="extra-section" />

    <!-- ② nav 菜单 -->
    <nav class="flex-1 px-4 space-y-1 overflow-y-auto no-scrollbar">
      <p class="px-3 pt-2 pb-2 text-[10px] font-bold tracking-[0.2em] text-stone-400 uppercase">导航</p>
      <button
        v-for="item in navItems"
        :key="item.key"
        type="button"
        class="nav-item w-full flex items-center px-4 py-3 rounded-2xl transition-all"
        :class="currentTab === item.key
          ? 'active ' + theme.soft + ' ' + theme.text + ' font-semibold'
          : 'text-stone-600 hover:bg-white/60'"
        @click="onSwitch(item.key)"
      >
        <i class="fa-solid w-5 text-center" :class="item.icon"></i>
        <span class="ml-3 font-medium text-sm">{{ item.label }}</span>
        <span
          v-if="item.badge && item.badge > 0"
          class="ml-auto chip"
          style="background: #ef4444; color: #fff; min-width: 20px;"
        >{{ item.badge }}</span>
      </button>
    </nav>

    <!-- ③ 用户卡片 -->
    <div class="p-3 shrink-0">
      <div class="glass rounded-2xl p-3 flex items-center gap-3">
        <div
          class="w-10 h-10 rounded-xl flex items-center justify-center text-white font-bold shrink-0"
          :class="theme.logoGradient"
        >
          {{ userInfo.name?.charAt(0) || 'U' }}
        </div>
        <div class="flex-1 min-w-0">
          <p class="text-sm font-semibold text-stone-800 truncate">{{ userInfo.name }}</p>
          <p class="text-xs text-stone-500 truncate">
            {{ ROLE_NAMES[userInfo.role_id] || '用户' }}
            <span v-if="userInfo.className"> · {{ userInfo.className }}</span>
          </p>
        </div>
        <button
          type="button"
          class="w-8 h-8 rounded-lg flex items-center justify-center text-stone-400 hover:text-rose-600 hover:bg-rose-50 transition-colors"
          title="退出登录"
          @click="onLogout"
        >
          <i class="fa-solid fa-arrow-right-from-bracket text-sm"></i>
        </button>
      </div>
    </div>
  </aside>
</template>

<style scoped>
.nav-item.active {
  font-weight: 600;
}
.chip {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  padding: 0 0.4rem;
  height: 1.25rem;
  border-radius: 9999px;
  font-size: 0.68rem;
  font-weight: 600;
}
</style>
