<script setup lang="ts">
/**
 * 积分卡批量打印弹窗
 *
 * 卡片网格：面额 + 卡号 + 二维码（内容为卡号），供打印后裁切发放。
 * - 二维码用 makeQrDataUrl 分批生成（每批 25 张，避免一次阻塞 UI）；
 * - 打印走**隐藏 iframe 独立文档**：把卡片 HTML 写入 iframe 后调 iframe 的
 *   contentWindow.print()。相比在主文档用 @media print + visibility，
 *   不受弹窗容器 overflow/max-height 裁剪（此前只能打出视口内一屏 ≈ 5 张），
 *   任意数量（上限 200 张）都能完整分页打印。
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

/** 卡片 HTML（预览与打印共用同一结构） */
function cardHtml(c: PrintableCard): string {
  const qr = qrs.value[c.code]
  return `<div class="ticket-card">
    <div class="ticket-top"><span class="ticket-brand">⚡ 能量站</span><span class="ticket-value">${c.face_value} 分</span></div>
    ${qr ? `<img class="ticket-qr" src="${qr}" alt="">` : '<div class="ticket-qr ticket-qr-placeholder"></div>'}
    <div class="ticket-code">${c.code}</div>
    ${c.bound_student_name ? `<div class="ticket-bound">专属：${c.bound_student_name}</div>` : ''}
  </div>`
}

const PAGE_STYLE = `
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body { font-family: 'Noto Sans SC', sans-serif; }
  .card-grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(150px, 1fr));
    gap: 12px;
    padding: 8px;
  }
  .ticket-card {
    background: #fff;
    border: 1.5px dashed #a8a29e;
    border-radius: 12px;
    padding: 10px 12px;
    text-align: center;
    break-inside: avoid;
    page-break-inside: avoid;
  }
  .ticket-top { display: flex; justify-content: space-between; align-items: center; margin-bottom: 6px; }
  .ticket-brand { font-size: 10px; color: #a8a29e; font-weight: 600; }
  .ticket-value { font-size: 13px; font-weight: 800; color: #d97706; }
  .ticket-qr { width: 108px; height: 108px; margin: 0 auto 6px; border-radius: 8px; }
  .ticket-qr-placeholder { background: #f5f5f4; }
  .ticket-code { font-family: ui-monospace, Menlo, monospace; font-size: 11px; font-weight: 700; color: #1c1917; }
  .ticket-bound { font-size: 10px; color: #059669; margin-top: 3px; font-weight: 600; }
  @page { margin: 8mm; }
`

/** 写入隐藏 iframe 并打印（全部卡片，分页自然分） */
function print() {
  if (generating.value) return
  const html = `<!DOCTYPE html><html><head><meta charset="utf-8"><style>${PAGE_STYLE}</style></head>
    <body><div class="card-grid">${props.cards.map(cardHtml).join('')}</div></body></html>`
  // 复用一个隐藏 iframe
  let iframe = document.getElementById('__card_print_frame__') as HTMLIFrameElement | null
  if (!iframe) {
    iframe = document.createElement('iframe')
    iframe.id = '__card_print_frame__'
    iframe.style.cssText = 'position:fixed;right:0;bottom:0;width:0;height:0;border:0;visibility:hidden;'
    document.body.appendChild(iframe)
  }
  const doc = iframe.contentWindow?.document
  if (!doc) return
  doc.open()
  doc.write(html)
  doc.close()
  // 等图片解码完成后再触发打印
  iframe.onload = () => {
    iframe!.contentWindow?.focus()
    iframe!.contentWindow?.print()
  }
  // 某些浏览器 doc.close 不触发 onload，双保险延迟触发
  setTimeout(() => {
    try { iframe!.contentWindow?.print() } catch { /* 已打印过则忽略 */ }
  }, 400)
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
      {{ generating ? '二维码生成中…' : '点击"打印"后选择打印机或另存为 PDF（全部卡片自动分页）' }}
    </p>

    <!-- 屏幕预览（打印内容以 iframe 为准，这里只是看一眼） -->
    <div class="max-h-[60vh] overflow-y-auto bg-stone-50 rounded-2xl p-4">
      <div class="preview-grid">
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
        <i class="fa-solid fa-print mr-2"></i>打印全部 {{ cards.length }} 张
      </button>
    </template>
  </BaseModal>
</template>

<style scoped>
.preview-grid {
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
