<script setup lang="ts">
/**
 * 通用模态框 —— 替换 admin 11 个 + teacher 5 个重复模态框
 * 三段插槽：#header / 默认 / #footer
 *
 * 用法：
 *   <BaseModal :show="showEdit" title="编辑用户" icon="fa-pen-to-square" @close="showEdit = false">
 *     <表单内容>
 *     <template #footer>
 *       <button @click="showEdit = false">取消</button>
 *       <button @click="save">保存</button>
 *     </template>
 *   </BaseModal>
 */
import { watch, onMounted, onBeforeUnmount } from 'vue'

const props = withDefaults(defineProps<{
  show: boolean
  title?: string
  icon?: string
  /** 图标渐变 from 颜色 */
  iconFrom?: string
  /** 图标渐变 to 颜色 */
  iconTo?: string
  /** 最大宽度：max-w-md / max-w-lg / max-w-2xl / max-w-4xl */
  maxWidth?: string
  /** 是否可点击遮罩关闭（默认 true） */
  closeOnBackdrop?: boolean
}>(), {
  title: '',
  icon: '',
  iconFrom: '#38bdf8',
  iconTo: '#0284c7',
  maxWidth: 'max-w-md',
  closeOnBackdrop: true,
})

const emit = defineEmits<{
  (e: 'close'): void
}>()

function close() {
  emit('close')
}

function onBackdrop() {
  if (props.closeOnBackdrop) close()
}

function onKey(e: KeyboardEvent) {
  if (props.show && e.key === 'Escape') close()
}

// 锁定背景滚动
watch(
  () => props.show,
  (v) => {
    document.body.style.overflow = v ? 'hidden' : ''
  }
)

onMounted(() => window.addEventListener('keydown', onKey))
onBeforeUnmount(() => {
  window.removeEventListener('keydown', onKey)
  document.body.style.overflow = ''
})

const iconStyle = {
  background: `linear-gradient(135deg, var(--modal-icon-from), var(--modal-icon-to))`,
}
</script>

<template>
  <Teleport to="body">
    <Transition name="modal">
      <div
        v-if="show"
        class="modal-backdrop fixed inset-0 flex items-center justify-center z-50 p-4"
        @click.self="onBackdrop"
      >
        <div
          class="modal-panel bg-white/95 backdrop-blur-xl rounded-3xl p-7 w-full max-h-[90vh] overflow-y-auto shadow-2xl"
          :class="maxWidth"
          style="--modal-icon-from: transparent; --modal-icon-to: transparent"
          :style="{ '--modal-icon-from': iconFrom, '--modal-icon-to': iconTo } as Record<string, string>"
        >
          <!-- 标题区（有 title 或 icon 时显示，否则交给 #header 插槽） -->
          <div v-if="title || icon || $slots.header" class="flex items-center gap-3 mb-5">
            <slot name="header">
              <div
                v-if="icon"
                class="w-10 h-10 rounded-xl text-white flex items-center justify-center shrink-0"
                :style="iconStyle"
              >
                <i class="fa-solid" :class="icon"></i>
              </div>
              <h3 v-if="title" class="font-display text-2xl font-bold text-stone-900">{{ title }}</h3>
            </slot>
          </div>

          <!-- 默认内容区 -->
          <slot />

          <!-- 操作区 -->
          <div v-if="$slots.footer" class="flex justify-end space-x-3 mt-6">
            <slot name="footer" />
          </div>
        </div>
      </div>
    </Transition>
  </Teleport>
</template>

<style scoped>
.modal-backdrop {
  background: rgba(28, 20, 16, 0.45);
  backdrop-filter: blur(8px);
  -webkit-backdrop-filter: blur(8px);
}

/* 入场 / 出场 */
.modal-enter-active,
.modal-leave-active {
  transition: opacity 0.25s ease;
}
.modal-enter-active .modal-panel,
.modal-leave-active .modal-panel {
  transition: transform 0.35s cubic-bezier(0.16, 1, 0.3, 1), opacity 0.3s ease;
}
.modal-enter-from,
.modal-leave-to {
  opacity: 0;
}
.modal-enter-from .modal-panel,
.modal-leave-to .modal-panel {
  transform: translateY(20px) scale(0.96);
  opacity: 0;
}
</style>
