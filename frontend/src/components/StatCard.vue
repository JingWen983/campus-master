<script setup lang="ts">
/**
 * 仪表盘统计卡片 —— 替换 admin/teacher 的 stat card 网格项
 * 支持 glass-card (admin 风格) 与 lift-card (teacher 风格) 两种 variant
 */
import { computed } from 'vue'

const props = withDefaults(defineProps<{
  label: string
  value: string | number
  icon: string
  /** 图标背景渐变 from */
  gradientFrom?: string
  /** 图标背景渐变 to */
  gradientTo?: string
  /** 英文副标 / 说明小字 */
  sublabel?: string
  /** 入场动画延时序号 1-4 */
  delay?: number
  variant?: 'glass-card' | 'lift-card'
}>(), {
  gradientFrom: '#38bdf8',
  gradientTo: '#0284c7',
  delay: 1,
  variant: 'glass-card',
})

const delayClass = computed(() => `enter-delay-${props.delay}`)
const iconStyle = computed(() => ({
  background: `linear-gradient(135deg, ${props.gradientFrom}, ${props.gradientTo})`,
  color: '#fff',
}))
</script>

<template>
  <div
    class="rounded-3xl p-6 enter-up"
    :class="[delayClass, variant === 'lift-card' ? 'glass lift-card' : 'glass-card']"
  >
    <div class="flex items-start justify-between">
      <div>
        <p class="text-xs font-semibold tracking-wider text-stone-500 uppercase">{{ label }}</p>
        <h3 class="font-display text-5xl font-black text-stone-900 mt-2 leading-none">{{ value }}</h3>
        <p v-if="sublabel" class="text-[11px] text-stone-400 mt-2">{{ sublabel }}</p>
      </div>
      <div
        class="stat-icon w-12 h-12 rounded-xl flex items-center justify-center shrink-0"
        :style="iconStyle"
      >
        <i class="fa-solid text-lg" :class="icon"></i>
      </div>
    </div>
  </div>
</template>

<style scoped>
.stat-icon {
  box-shadow: 0 8px 20px -6px rgba(0, 0, 0, 0.2);
}
.lift-card {
  transition: transform 0.3s cubic-bezier(0.16, 1, 0.3, 1);
}
.lift-card:hover {
  transform: translateY(-4px);
}
</style>
