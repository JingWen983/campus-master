<script setup lang="ts">
/**
 * 全局确认对话框 —— 替换原生 confirm()
 * 由 main.ts 挂载，状态来自 useConfirm 模块。
 */
import { useConfirm } from '../composables/useConfirm'

const { state, answer } = useConfirm()

const variantClass: Record<string, string> = {
  danger: 'bg-rose-600 hover:bg-rose-700',
  primary: 'bg-amber-600 hover:bg-amber-700',
  warning: 'bg-amber-500 hover:bg-amber-600',
}

const iconClass: Record<string, string> = {
  danger: 'fa-circle-exclamation text-rose-500',
  primary: 'fa-circle-question text-amber-500',
  warning: 'fa-triangle-exclamation text-amber-500',
}

function onKey(e: KeyboardEvent) {
  if (!state.visible) return
  if (e.key === 'Escape') answer(false)
  if (e.key === 'Enter') answer(true)
}
</script>

<template>
  <Teleport to="body">
    <Transition name="confirm">
      <div v-if="state.visible" class="modal-backdrop" @click.self="answer(false)" @keydown="onKey" tabindex="0">
        <div class="modal-panel confirm-panel">
          <div class="confirm-icon">
            <i class="fa-solid" :class="iconClass[state.variant]"></i>
          </div>
          <h3 class="confirm-title">{{ state.title }}</h3>
          <p class="confirm-msg">{{ state.message }}</p>
          <div class="confirm-actions">
            <button class="btn-cancel" @click="answer(false)">
              {{ state.cancelText }}
            </button>
            <button class="btn-confirm" :class="variantClass[state.variant]" @click="answer(true)">
              {{ state.confirmText }}
            </button>
          </div>
        </div>
      </div>
    </Transition>
  </Teleport>
</template>

<style scoped>
.confirm-panel {
  max-width: 420px;
  text-align: center;
  padding: 2rem 1.75rem 1.5rem;
}
.confirm-icon {
  font-size: 2.5rem;
  margin-bottom: 0.75rem;
}
.confirm-title {
  font-size: 1.15rem;
  font-weight: 600;
  color: #1a1410;
  margin-bottom: 0.5rem;
}
.confirm-msg {
  color: #57534e;
  font-size: 0.95rem;
  line-height: 1.5;
  margin-bottom: 1.5rem;
  white-space: pre-wrap;
}
.confirm-actions {
  display: flex;
  gap: 0.75rem;
  justify-content: center;
}
.confirm-actions button {
  padding: 0.5rem 1.5rem;
  border-radius: 0.5rem;
  font-size: 0.9rem;
  font-weight: 500;
  transition: all 0.2s;
  cursor: pointer;
}
.btn-cancel {
  background: #f5f5f4;
  color: #57534e;
}
.btn-cancel:hover {
  background: #e7e5e4;
}
.btn-confirm {
  color: white;
  box-shadow: 0 4px 10px -2px rgba(0, 0, 0, 0.15);
}

/* 过渡动画 */
.confirm-enter-active,
.confirm-leave-active {
  transition: opacity 0.25s ease;
}
.confirm-enter-active .modal-panel,
.confirm-leave-active .modal-panel {
  transition: transform 0.25s cubic-bezier(0.16, 1, 0.3, 1), opacity 0.25s ease;
}
.confirm-enter-from,
.confirm-leave-to {
  opacity: 0;
}
.confirm-enter-from .modal-panel,
.confirm-leave-to .modal-panel {
  transform: scale(0.92) translateY(-8px);
  opacity: 0;
}
</style>
