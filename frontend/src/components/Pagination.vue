<script setup lang="ts">
/**
 * 分页控件 —— 配合 usePagination composable 使用
 *
 * 用法：
 *   const { state, totalPages, changePage } = usePagination()
 *   <Pagination :current-page="state.currentPage" :total-pages="totalPages" :total="state.total"
 *               :start-index="startIndex" :end-index="endIndex"
 *               @change="changePage" />
 */
import { computed } from 'vue'

const props = defineProps<{
  currentPage: number
  totalPages: number
  total: number
  startIndex?: number
  endIndex?: number
}>()

const emit = defineEmits<{
  (e: 'change', page: number): void
}>()

/** 生成页码按钮列表（含 ... 占位），最多显示 7 个 */
const pages = computed<(number | string)[]>(() => {
  const total = props.totalPages
  const cur = props.currentPage
  if (total <= 7) {
    return Array.from({ length: total }, (_, i) => i + 1)
  }
  const result: (number | string)[] = [1]
  if (cur > 4) result.push('…')
  const start = Math.max(2, cur - 1)
  const end = Math.min(total - 1, cur + 1)
  for (let i = start; i <= end; i++) result.push(i)
  if (cur < total - 3) result.push('…')
  result.push(total)
  return result
})

function go(page: number) {
  if (page >= 1 && page <= props.totalPages && page !== props.currentPage) {
    emit('change', page)
  }
}
</script>

<template>
  <div class="flex flex-col sm:flex-row items-center justify-between gap-3 mt-4">
    <p class="text-xs text-stone-500">
      <template v-if="total > 0">
        显示第 {{ startIndex }} - {{ endIndex }} 条，共 {{ total }} 条
      </template>
      <template v-else>暂无数据</template>
    </p>
    <div v-if="totalPages > 1" class="flex items-center gap-1">
      <button
        class="page-btn"
        :disabled="currentPage <= 1"
        @click="go(currentPage - 1)"
        aria-label="上一页"
      >
        <i class="fa-solid fa-chevron-left text-xs"></i>
      </button>
      <template v-for="(p, i) in pages" :key="i">
        <span v-if="p === '…'" class="page-ellipsis">…</span>
        <button
          v-else
          class="page-btn"
          :class="{ active: p === currentPage }"
          @click="go(p as number)"
        >{{ p }}</button>
      </template>
      <button
        class="page-btn"
        :disabled="currentPage >= totalPages"
        @click="go(currentPage + 1)"
        aria-label="下一页"
      >
        <i class="fa-solid fa-chevron-right text-xs"></i>
      </button>
    </div>
  </div>
</template>

<style scoped>
.page-btn {
  min-width: 2rem;
  height: 2rem;
  padding: 0 0.5rem;
  border-radius: 0.5rem;
  font-size: 0.85rem;
  color: #57534e;
  background: rgba(255, 255, 255, 0.6);
  border: 1px solid rgba(255, 255, 255, 0.5);
  transition: all 0.2s;
  cursor: pointer;
}
.page-btn:hover:not(:disabled):not(.active) {
  background: rgba(255, 255, 255, 0.9);
}
.page-btn.active {
  background: #d97706;
  color: #fff;
  border-color: #d97706;
  font-weight: 600;
}
.page-btn:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}
.page-ellipsis {
  padding: 0 0.25rem;
  color: #a8a29e;
}
</style>
