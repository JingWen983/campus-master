<script setup lang="ts">
/**
 * 更新检查面板（仅管理员）
 *
 * - 显示当前版本 / 远端最新版本 / 是否有更新；
 * - 4 条检查通道实时竞速状态（jsDelivr / GitHub Raw / 两个 gh 镜像）；
 * - 有更新时展示更新说明 + 按实测延迟排序的下载通道按钮（镜像优先）；
 * - "稍后再说"（本次会话不再弹）/ "跳过此版本"（localStorage 记忆）。
 */
import { ref, onMounted, onUnmounted } from 'vue'
import BaseModal from './BaseModal.vue'
import {
  CURRENT_VERSION, REPO_URL, RELEASES_URL,
  checkForUpdate, probeDownloadLinks,
  compareVersions, skipVersion, getSkipVersion,
  type ChannelResult, type UpdateCheckResult,
} from '../lib/updater'
import { toast } from '../composables/useToast'
import { formatDateTime } from '../lib/format'

/** v-model:show —— 由父组件（AdminApp）控制开合 */
const show = defineModel<boolean>('show', { default: false })

const checking = ref(false)
const hasResult = ref(false)
const result = ref<UpdateCheckResult | null>(null)
const channels = ref<ChannelResult[]>([])
const probing = ref(false)
/** 已探测的下载通道（探测完成后填充） */
const downloads = ref<{ label: string; url: string; ms: number }[]>([])

async function runCheck(silent = false) {
  if (checking.value) return
  checking.value = true
  hasResult.value = false
  downloads.value = []
  channels.value = []
  try {
    const r = await checkForUpdate(c => {
      const i = channels.value.findIndex(x => x.label === c.label)
      if (i >= 0) channels.value[i] = c
      else channels.value.push(c)
    })
    if (!r) {
      if (!silent) toast.error('检查失败：所有更新通道均不可达，请检查网络')
      hasResult.value = true
      result.value = null
      return
    }
    const latest = r.manifest.version
    const hasUpdate = CURRENT_VERSION === 'dev' ? false : compareVersions(latest, CURRENT_VERSION) > 0
    result.value = {
      hasUpdate,
      current: CURRENT_VERSION,
      latest,
      notes: r.manifest.notes || '',
      publishedAt: r.manifest.published_at || '',
      downloadLinks: [],
      channels: r.channels,
      checkedAt: new Date().toISOString(),
    }
    hasResult.value = true
    if (hasUpdate) {
      show.value = true   // 发现新版本弹出面板
      // 后台探测下载通道延迟（不阻塞面板展示）
      probing.value = true
      probeDownloadLinks('v' + latest.replace(/^v/, '')).then(links => {
        downloads.value = links
        if (result.value) result.value.downloadLinks = links
      }).finally(() => { probing.value = false })
    } else if (!silent) {
      toast.success(`已是最新版本（${CURRENT_VERSION}）`)
    }
  } finally {
    checking.value = false
  }
}

function openLatest() {
  window.open(RELEASES_URL, '_blank')
}

function goDownload(url: string) {
  window.open(url, '_blank')
}

function onSkip() {
  if (result.value) {
    skipVersion(result.value.latest)
    show.value = false
    toast.info(`已跳过 ${result.value.latest}，该版本不再提醒`)
  }
}

function onLater() {
  show.value = false
}

defineExpose({ runCheck })

onMounted(() => {
  // 面板挂载即静默检查一次（AdminApp 控制"跳过版本"后不弹窗）
  void (async () => {
    const r = await silentCheck()
    if (r?.hasUpdate && r.latest !== getSkipVersion()) show.value = true
  })()
})

async function silentCheck(): Promise<{ hasUpdate: boolean; latest: string } | null> {
  checking.value = true
  try {
    const r = await checkForUpdate()
    if (!r) return null
    const latest = r.manifest.version
    const hasUpdate = CURRENT_VERSION === 'dev' ? false : compareVersions(latest, CURRENT_VERSION) > 0
    return { hasUpdate, latest }
  } finally {
    checking.value = false
  }
}

onUnmounted(() => { /* 竞速 fetch 由 AbortController 自动清理 */ })
</script>

<template>
  <BaseModal
    :show="show"
    title="软件更新"
    icon="fa-arrows-rotate"
    icon-from="#38bdf8"
    icon-to="#0284c7"
    max-width="max-w-lg"
    @close="show = false"
  >
    <div class="space-y-4">
      <!-- 版本对比 -->
      <div class="flex items-center justify-center gap-4 py-2">
        <div class="text-center">
          <p class="text-xs text-stone-400 mb-1">当前版本</p>
          <p class="font-display text-2xl font-black text-stone-700">{{ result?.current || CURRENT_VERSION }}</p>
        </div>
        <i class="fa-solid fa-arrow-right text-stone-300 text-xl" :class="result?.hasUpdate ? 'animate-pulse text-sky-500' : ''"></i>
        <div class="text-center">
          <p class="text-xs text-stone-400 mb-1">最新版本</p>
          <p class="font-display text-2xl font-black" :class="result?.hasUpdate ? 'text-sky-600' : 'text-stone-700'">
            {{ result?.latest || '—' }}
          </p>
        </div>
      </div>

      <!-- 结论条 -->
      <div v-if="hasResult && result" class="rounded-2xl p-4 text-sm leading-relaxed"
        :class="result.hasUpdate ? 'bg-sky-50 text-sky-800' : 'bg-emerald-50 text-emerald-700'">
        <template v-if="result.hasUpdate">
          <p class="font-bold mb-1"><i class="fa-solid fa-party-horn mr-1.5"></i>发现新版本 v{{ result.latest }}！</p>
          <p v-if="result.publishedAt" class="text-xs opacity-70">发布时间：{{ result.publishedAt }}</p>
        </template>
        <template v-else>
          <p class="font-bold"><i class="fa-solid fa-circle-check mr-1.5"></i>已是最新版本</p>
        </template>
      </div>
      <div v-else-if="hasResult" class="rounded-2xl p-4 text-sm bg-rose-50 text-rose-700">
        <p class="font-bold"><i class="fa-solid fa-triangle-exclamation mr-1.5"></i>所有更新通道均不可达</p>
        <p class="text-xs mt-1 opacity-75">请检查服务器网络后重试，或前往 <a :href="RELEASES_URL" target="_blank" class="underline font-medium">GitHub Releases</a> 手动查看</p>
      </div>

      <!-- 更新说明 -->
      <div v-if="result?.hasUpdate && result.notes" class="rounded-2xl bg-stone-50 p-4">
        <p class="text-xs font-bold text-stone-400 uppercase tracking-wider mb-2">更新内容</p>
        <pre class="text-sm text-stone-700 whitespace-pre-wrap font-body leading-relaxed max-h-40 overflow-y-auto">{{ result.notes }}</pre>
      </div>

      <!-- 通道状态 -->
      <div v-if="channels.length" class="rounded-2xl bg-stone-50 p-4">
        <p class="text-xs font-bold text-stone-400 uppercase tracking-wider mb-2">检查通道</p>
        <div class="space-y-1.5">
          <div v-for="c in channels" :key="c.label" class="flex items-center justify-between text-sm">
            <span class="text-stone-600">{{ c.label }}</span>
            <span class="text-xs font-medium"
              :class="{
                'text-stone-300': c.status === 'pending',
                'text-emerald-600': c.status === 'ok',
                'text-rose-400': c.status === 'fail',
                'text-stone-400': c.status === 'slow',
              }">
              {{ c.status === 'pending' ? '检查中…' : c.status === 'ok' ? `✓ ${c.ms}ms` : c.status === 'fail' ? '不可达' : '超时' }}
            </span>
          </div>
        </div>
      </div>

      <!-- 下载通道（探测后按延迟排序） -->
      <div v-if="result?.hasUpdate" class="rounded-2xl border border-sky-200 bg-sky-50/50 p-4">
        <p class="text-xs font-bold text-sky-500 uppercase tracking-wider mb-2">
          下载新版本 {{ probing ? '· 正在为您挑选最快的通道…' : '· 按速度排序' }}
        </p>
        <div class="space-y-2">
          <button v-if="!probing" v-for="(d, i) in downloads" :key="d.url" type="button"
            class="w-full flex items-center justify-between px-4 py-3 rounded-xl text-sm font-medium transition text-left"
            :class="i === 0 ? 'bg-sky-500 text-white hover:bg-sky-600' : 'bg-white text-stone-600 hover:bg-sky-50 border border-sky-100'"
            @click="goDownload(d.url)">
            <span>
              <span v-if="i === 0" class="mr-1.5 text-xs font-bold">推荐</span>{{ d.label }}
            </span>
            <span class="text-xs" :class="i === 0 ? 'text-sky-100' : 'text-stone-400'">
              {{ d.ms >= 0 ? d.ms + 'ms' : '未响应' }}
            </span>
          </button>
          <p v-if="probing" class="text-sm text-sky-600 py-2"><i class="fa-solid fa-spinner fa-spin mr-2"></i>正在探测镜像延迟…</p>
          <button type="button" class="w-full px-4 py-2.5 rounded-xl text-xs text-stone-500 hover:text-sky-600"
            @click="openLatest">
            <i class="fa-solid fa-up-right-from-square mr-1"></i>前往 GitHub Releases 页面
          </button>
        </div>
      </div>
    </div>

    <template #footer>
      <button v-if="result?.hasUpdate" type="button" class="px-4 py-2.5 rounded-2xl text-sm text-stone-500 hover:text-stone-700" @click="onSkip">
        跳过此版本
      </button>
      <button type="button" class="px-4 py-2.5 rounded-2xl text-sm text-stone-500 hover:text-stone-700" @click="onLater">
        {{ result?.hasUpdate ? '稍后再说' : '关闭' }}
      </button>
      <button type="button" class="px-5 py-2.5 rounded-2xl text-sm font-medium bg-sky-500 text-white hover:bg-sky-600" :disabled="checking" @click="runCheck(false)">
        <i class="fa-solid fa-arrows-rotate mr-2" :class="checking ? 'fa-spin' : ''"></i>{{ checking ? '检查中…' : '重新检查' }}
      </button>
    </template>
  </BaseModal>
</template>
