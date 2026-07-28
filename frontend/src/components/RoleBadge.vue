<script setup lang="ts">
/**
 * 角色徽章 —— 替换 admin.html L579-581 的三层三元嵌套
 * roleId: 1=管理员 / 2=教师 / 3=学生 / 4=家长
 */
import { computed } from 'vue'
import { ROLE_NAMES } from '../lib/auth'

const props = defineProps<{
  roleId: number
  /** 自定义文字，默认按 roleId 显示角色名 */
  label?: string
}>()

const config = computed(() => {
  switch (props.roleId) {
    case 1: return { bg: 'bg-rose-100', text: 'text-rose-700' }
    case 2: return { bg: 'bg-amber-100', text: 'text-amber-700' }
    case 3: return { bg: 'bg-emerald-100', text: 'text-emerald-700' }
    case 4: return { bg: 'bg-violet-100', text: 'text-violet-700' }
    default: return { bg: 'bg-stone-100', text: 'text-stone-700' }
  }
})

const text = computed(() => props.label || ROLE_NAMES[props.roleId] || '未知')
</script>

<template>
  <span class="badge" :class="[config.bg, config.text]">{{ text }}</span>
</template>

<style scoped>
.badge {
  display: inline-flex;
  align-items: center;
  padding: 0.25rem 0.7rem;
  border-radius: 9999px;
  font-size: 0.72rem;
  font-weight: 600;
  white-space: nowrap;
}
</style>
