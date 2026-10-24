<script setup lang="ts">
/**
 * 积分卡批量打印弹窗
 *
 * 卡片网格：面额 + 卡号 + 二维码（内容为卡号），供打印后裁切发放。
 * - 二维码用 makeQrDataUrl 分批生成（每批 25 张，避免一次阻塞 UI）；
 * - window.print() 打印；打印样式在 style.css 的 @media print 中
 *   （只显示 .card-print-root，隐藏页面其余部分）。
 */
import { ref, watch } from 'vue'
import BaseModal from './BaseModal.vue'
import { makeQrDataUrl } from '../lib/qrcode'

export interface PrintableCard {
  code: string
  face_value: number
  bound_student_name?: string
}

const props = defineProps<{
  show: boolean
  cards: PrintableCard[]
  batchNo?: string
}>()

const emit = defineEmits<{ (e: 'close'): void }>()

const qrs = ref<Record<string, string>>({})
const generating = ref(false)

// 打开弹窗时分批生成二维码
watch(() => props.show, async (show) => {
  if (!show) { qrs.value = {}; return }
  generating.value = true
  const map: Record<string, string> = {}
  const list = props.cards
  for (let i = 0; i < list.length; i += 25) {
    const batch = list.slice(i, i + 25)
    // 让出主线程：每批之间渲染一帧
    await new Promise(r => requestAnimationFrame(() => r(null)))
    await Promise.all(batch.map(async c => {
      if (!map[c.code]) {
        try { map[c.code] = await makeQrDataUrl(c.code, 150) } catch { /* 单张失败不阻塞 */ }
      }
    }))
    qrs.value = { ...map }
  }
  generating.value = false
})

function print() {
  window.print()
}
</script>

<template>
  <BaseModal
    :show="show"
    title="打印积分卡"
    icon="fa-print"
    icon-from="#fcd34d"
    icon-to="#d97706"
    max-width="max-w-5xl"
    @close="emit('close')"
  >
    <p class="text-xs text-stone-400 mb-3">
      <i class="fa-solid fa-circle-info mr-1"></i>
      共 {{ cards.length }} 张{{ batchNo ? `（批次 ${batchNo}）` : '' }}；
      {{ generating ? '二维码生成中…' : '点击"打印"后选择打印机或另存为 PDF' }}
    </p>

    <!-- 屏幕预览 + 打印内容（同一节点） -->
    <div class="card-print-root max-h-[60vh] overflow-y-auto bg-stone-50 rounded-2xl p-4">
      <div class="card-grid">
        <div v-for="c in cards" :key="c.code" class="ticket-card">
          <div class="ticket-top">
            <span class="ticket-brand"><i class="fa-solid fa-bolt"></i> 能量站</span>
            <span class="ticket-value">{{ c.face_value }} 分</span>
          </div>
          <img v-if="qrs[c.code]" :src="qrs[c.code]" alt="" class="ticket-qr">
          <div v-else class="ticket-qr ticket-qr-placeholder"></div>
          <div class="ticket-code">{{ c.code }}</div>
          <div v-if="c.bound_student_name" class="ticket-bound">专属：{{ c.bound_student_name }}</div>
        </div>
      </div>
    </div>

    <template #footer>
      <button type="button" class="px-5 py-2.5 rounded-2xl text-sm font-medium bg-stone-100 text-stone-600 hover:bg-stone-200" @click="emit('close')">关闭</button>
      <button type="button" class="px-6 py-2.5 rounded-2xl text-sm font-semibold text-white bg-gradient-to-r from-amber-500 to-amber-600 hover:opacity-90 disabled:opacity-50" :disabled="generating" @click="print">
        <i class="fa-solid fa-print mr-2"></i>打印
      </button>
    </template>
  </BaseModal>
</template>

<style scoped>
.card-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(150px, 1fr));
  gap: 12px;
}
.ticket-card {
  background: white;
  border: 1.5px dashed #d6d3d1;
  border-radius: 12px;
  padding: 10px 12px;
  text-align: center;
}
.ticket-top {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 6px;
}
.ticket-brand { font-size: 10px; color: #a8a29e; font-weight: 600; }
.ticket-value { font-size: 13px; font-weight: 800; color: #d97706; }
.ticket-qr {
  width: 108px;
  height: 108px;
  margin: 0 auto 6px;
  border-radius: 8px;
  border: 3px solid #fafaf9;
  box-shadow: 0 1px 4px rgba(0,0,0,0.08);
}
.ticket-qr-placeholder { background: #f5f5f4; }
.ticket-code {
  font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
  font-size: 11px;
  font-weight: 700;
  color: #1c1917;
  letter-spacing: 0.02em;
}
.ticket-bound { font-size: 10px; color: #059669; margin-top: 3px; font-weight: 600; }
</style>
