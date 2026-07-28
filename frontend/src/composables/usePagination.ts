/**
 * 分页 composable —— 封装分页状态与计算属性
 *
 * 用法：
 *   const { currentPage, pageSize, total, totalPages, changePage } = usePagination({ pageSize: 10 })
 *   <Pagination v-bind="pagProps" @change="changePage" />
 */
import { reactive, computed } from 'vue'

export interface PaginationOptions {
  pageSize?: number
  currentPage?: number
  total?: number
}

export function usePagination(options: PaginationOptions = {}) {
  const state = reactive({
    currentPage: options.currentPage ?? 1,
    pageSize: options.pageSize ?? 10,
    total: options.total ?? 0,
  })

  const totalPages = computed(() =>
    state.pageSize > 0 ? Math.max(1, Math.ceil(state.total / state.pageSize)) : 1
  )

  const hasNext = computed(() => state.currentPage < totalPages.value)
  const hasPrev = computed(() => state.currentPage > 1)
  const startIndex = computed(() =>
    state.total === 0 ? 0 : (state.currentPage - 1) * state.pageSize + 1
  )
  const endIndex = computed(() => Math.min(state.currentPage * state.pageSize, state.total))

  function changePage(page: number) {
    if (page < 1 || page > totalPages.value || page === state.currentPage) return
    state.currentPage = page
  }

  function setTotal(total: number) {
    state.total = total
    // 越界回退（删除最后一页最后一条后自动回到上一页）
    if (state.currentPage > totalPages.value) state.currentPage = totalPages.value
  }

  function setPageSize(size: number) {
    state.pageSize = size
    state.currentPage = 1
  }

  function reset() {
    state.currentPage = 1
    state.total = 0
  }

  return {
    state,
    totalPages,
    hasNext,
    hasPrev,
    startIndex,
    endIndex,
    changePage,
    setTotal,
    setPageSize,
    reset,
  }
}
