<script setup lang="ts">
/**
 * 应用布局外壳 —— 替换 4 角色重复的 #app flex h-screen 结构
 * 组合 Sidebar + 顶栏（手机端）+ 主内容槽 + 背景装饰
 *
 * 用法：
 *   <AppLayout :theme="theme" :brand="brand" :nav-items="nav" :current-tab="tab"
 *              :user-info="userInfo" @switch-tab="tab = $event">
 *     <div v-show="tab === 'home'">...</div>
 *   </AppLayout>
 */
import type { NavItem } from '../lib/navConfig'
import type { ThemeConfig } from '../lib/theme'
import Sidebar from './Sidebar.vue'

interface UserInfo {
  id: string
  username: string
  name: string
  role_id: number
  className?: string
  points?: number
}

withDefaults(defineProps<{
  theme: ThemeConfig
  brand: string
  brandEn?: string
  brandIcon?: string
  navItems: NavItem[]
  currentTab: string
  userInfo: UserInfo
  sidebarWidth?: string
}>(), {
  brandEn: '',
  brandIcon: 'fa-leaf',
  sidebarWidth: 'w-64',
})

const emit = defineEmits<{
  (e: 'switch-tab', key: string): void
}>()
</script>

<template>
  <div class="flex h-screen w-full overflow-hidden font-body relative" style="z-index: 2;">
    <!-- 背景装饰：浮动光斑 + 颗粒纹理 -->
    <div class="blob blob-1"></div>
    <div class="blob blob-2"></div>
    <div class="blob blob-3"></div>
    <div class="grain-overlay"></div>

    <!-- 侧边栏 -->
    <Sidebar
      :nav-items="navItems"
      :current-tab="currentTab"
      :user-info="userInfo"
      :theme="theme"
      :brand="brand"
      :brand-en="brandEn"
      :brand-icon="brandIcon"
      :width="sidebarWidth"
      @switch-tab="emit('switch-tab', $event)"
    >
      <template v-if="$slots['extra-section']" #extra-section>
        <slot name="extra-section" />
      </template>
    </Sidebar>

    <!-- 右侧主体 -->
    <div class="flex-1 flex flex-col h-full relative overflow-hidden">
      <!-- 手机端顶栏 -->
      <header class="md:hidden glass-strong h-14 flex items-center justify-between px-4 z-10 shrink-0">
        <div class="flex items-center gap-2">
          <div
            class="w-8 h-8 rounded-xl flex items-center justify-center text-white"
            :class="theme.logoGradient"
          >
            <i class="fa-solid text-sm" :class="brandIcon"></i>
          </div>
          <span class="font-display font-bold text-stone-800">{{ brand }}</span>
        </div>
        <div class="flex items-center gap-2">
          <span class="text-xs text-stone-600 hidden sm:inline">{{ userInfo.name }}</span>
          <button
            type="button"
            class="w-9 h-9 rounded-lg flex items-center justify-center text-stone-500"
            :class="theme.soft"
            title="退出登录"
            @click="$emit('switch-tab', '__logout__')"
          >
            <i class="fa-solid fa-arrow-right-from-bracket"></i>
          </button>
        </div>
      </header>

      <!-- 主内容区 -->
      <main class="flex-1 overflow-y-auto p-4 md:p-8 pb-24 md:pb-8 no-scrollbar scroll-smooth">
        <div class="max-w-6xl mx-auto">
          <slot />
        </div>
      </main>

      <!-- 手机端底部导航 -->
      <nav
        v-if="navItems.length"
        class="md:hidden fixed bottom-0 inset-x-0 glass-strong border-t border-white/40 flex items-center justify-around py-2 z-30"
      >
        <button
          v-for="item in navItems.slice(0, 6)"
          :key="item.key"
          type="button"
          class="flex flex-col items-center gap-0.5 px-2 py-1 transition-colors"
          :class="currentTab === item.key ? theme.text : 'text-stone-400'"
          @click="emit('switch-tab', item.key)"
        >
          <i class="fa-solid text-base" :class="item.icon"></i>
          <span class="text-[10px]">{{ item.label.substring(0, 2) }}</span>
        </button>
      </nav>
    </div>
  </div>
</template>
