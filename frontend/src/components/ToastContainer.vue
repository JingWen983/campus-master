<script setup lang="ts">
/**
 * 全局 Toast 容器 —— 挂载在 #app 外层（由 main.ts 通过 teleport 渲染）。
 * 读取 useToast 模块状态并渲染队列。
 */
import { useToast } from '../composables/useToast'

const { toasts, remove } = useToast()

const iconMap: Record<string, string> = {
  success: 'fa-circle-check',
  error: 'fa-circle-xmark',
  warning: 'fa-triangle-exclamation',
  info: 'fa-circle-info',
}

const colorMap: Record<string, string> = {
  success: 'text-emerald-600',
  error: 'text-rose-600',
  warning: 'text-amber-600',
  info: 'text-teal-600',
}
</script>

<template>
  <Teleport to="body">
    <div class="toast-container" aria-live="polite">
      <TransitionGroup name="toast">
        <div
          v-for="t in toasts"
          :key="t.id"
          class="toast-item glass-strong"
          :class="t.type"
          role="alert"
          @click="remove(t.id)"
        >
          <i class="fa-solid" :class="[iconMap[t.type], colorMap[t.type]]"></i>
          <span class="toast-msg">{{ t.message }}</span>
          <i class="fa-solid fa-xmark toast-close"></i>
        </div>
      </TransitionGroup>
    </div>
  </Teleport>
</template>

<style scoped>
.toast-container {
  position: fixed;
  top: 1.5rem;
  left: 50%;
  transform: translateX(-50%);
  z-index: 9999;
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
  pointer-events: none;
}
.toast-item {
  pointer-events: auto;
  display: flex;
  align-items: center;
  gap: 0.75rem;
  padding: 0.75rem 1.25rem;
  border-radius: 0.75rem;
  box-shadow: 0 10px 25px -8px rgba(0, 0, 0, 0.18);
  min-width: 280px;
  max-width: 480px;
  cursor: pointer;
  font-size: 0.9rem;
}
.toast-msg {
  flex: 1;
  color: #1a1410;
}
.toast-close {
  color: #a8a29e;
  font-size: 0.8rem;
}

/* 入场 / 出场动画 */
.toast-enter-active,
.toast-leave-active {
  transition: all 0.35s cubic-bezier(0.16, 1, 0.3, 1);
}
.toast-enter-from {
  opacity: 0;
  transform: translateY(-16px) scale(0.95);
}
.toast-leave-to {
  opacity: 0;
  transform: translateY(-8px) scale(0.95);
}
.toast-move {
  transition: transform 0.35s cubic-bezier(0.16, 1, 0.3, 1);
}
</style>
