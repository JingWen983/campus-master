<script setup lang="ts">
/**
 * 空状态占位 —— 替换各页 v-if="xxx.length === 0" 的重复块
 * 三种 variant：inline(内联) / card(卡片) / table-row(表格跨列空行)
 */
withDefaults(defineProps<{
  icon?: string
  text?: string
  variant?: 'inline' | 'card' | 'table-row'
  colspan?: number
}>(), {
  icon: 'fa-folder-open',
  text: '暂无数据',
  variant: 'inline',
  colspan: 4,
})
</script>

<template>
  <div v-if="variant === 'inline'" class="text-center py-12 text-stone-400">
    <i class="fa-solid mb-3 opacity-50" :class="[icon, 'text-4xl']"></i>
    <p class="text-sm">{{ text }}</p>
  </div>

  <div v-else-if="variant === 'card'" class="glass-card text-center py-16 enter-up">
    <i class="fa-solid mb-3 text-stone-300" :class="[icon, 'text-5xl']"></i>
    <p class="text-stone-400">{{ text }}</p>
  </div>

  <tr v-else>
    <td :colspan="colspan" class="px-6 py-12 text-center text-stone-400">
      <i class="fa-solid mb-2 opacity-40" :class="[icon, 'text-3xl']"></i>
      <p class="text-sm">{{ text }}</p>
    </td>
  </tr>
</template>
