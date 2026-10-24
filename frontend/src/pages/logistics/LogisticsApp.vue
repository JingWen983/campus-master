<script setup lang="ts">
/**
 * 后勤核销端 —— logistics.html 入口（role_id=5 后勤）
 *
 * 布局：
 *   上半区：摄像头扫码（jsQR 逐帧识别）+ 手输验证码 双通道 → 查询详情 → 确认核销
 *   下半区：待核销列表（5 秒轮询），点击任意一条直接进入确认
 *
 * 后端契约（routes_public.cpp）：
 *   GET  /api/logistics/lookup?code=XXXX-XXXX  查询单笔详情
 *   GET  /api/logistics/pending                待核销列表
 *   POST /api/logistics/redeem {code}          确认核销
 */
import { ref, onMounted, onUnmounted, nextTick } from 'vue'
import { api } from '../../lib/api'
import { checkAuth, logout } from '../../lib/auth'
import { scanVideoFrame, normalizeCode } from '../../lib/qrcode'
import { formatDateTime } from '../../lib/format'
import { toast } from '../../composables/useToast'
import { confirmDialog } from '../../composables/useConfirm'
import EmptyState from '../../components/EmptyState.vue'

interface RedeemDetail {
  id: number
  status: string
  code: string
  student_name: string
  className: string
  item_name: string
  cost: number
  time: string
}

interface PendingItem {
  id: number
  code: string
  student_name: string
  className: string
  item_name: string
  cost: number
  time: string
}

interface UserInfoShape { id: string; name: string; role_id: number }

const PENDING_POLL_MS = 5_000   // 需求确认：后勤端 5 秒轮询

const userInfo = ref<UserInfoShape>({ id: '', name: '', role_id: 5 })
const pendingList = ref<PendingItem[]>([])
const detail = ref<RedeemDetail | null>(null)
const manualCode = ref('')
const loading = ref(false)

// ===== 摄像头扫码状态 =====
const scanning = ref(false)
const scanError = ref('')
const videoEl = ref<HTMLVideoElement | null>(null)
const canvasEl = ref<HTMLCanvasElement | null>(null)
let mediaStream: MediaStream | null = null
let scanTimer: number | undefined
let pollTimer: number | undefined
let lastScannedCode = ''
let lastScannedAt = 0

async function fetchPending() {
  try {
    const res = await api.get<PendingItem[]>('/api/logistics/pending')
    if (res.code === 200 && res.data) pendingList.value = res.data
  } catch { /* api 层已提示 */ }
}

// ===== 查询详情（扫码 / 手输 / 列表点击 共用） =====
async function lookup(rawCode: string) {
  const code = normalizeCode(rawCode)
  if (!code) {
    toast.warning('验证码格式应为 XXXX-XXXX（8 位字母数字）')
    return
  }
  loading.value = true
  try {
    const res = await api.get<RedeemDetail>(`/api/logistics/lookup?code=${encodeURIComponent(code)}`)
    if (res.code === 200 && res.data) {
      detail.value = res.data
      if (res.data.status !== 'pending') {
        toast.warning(statusText(res.data.status))
      }
    } else {
      toast.error(res.msg || '查询失败')
    }
  } catch { /* api 层已提示 */ } finally {
    loading.value = false
  }
}

function statusText(status: string): string {
  if (status === 'pending') return '未兑换'
  if (status === 'redeemed') return '已兑换'
  if (status === 'expired') return '已过期退回'
  return '未知状态'
}

function statusClass(status: string): string {
  if (status === 'pending') return 'bg-amber-100 text-amber-700'
  if (status === 'redeemed') return 'bg-emerald-100 text-emerald-700'
  if (status === 'expired') return 'bg-stone-200 text-stone-500'
  return 'bg-stone-100 text-stone-600'
}

// ===== 确认核销 =====
async function confirmRedeem() {
  if (!detail.value || detail.value.status !== 'pending') return
  const d = detail.value
  const ok = await confirmDialog({
    message: `确认核销 ${d.student_name}（${d.className}）兑换的【${d.item_name}】？`,
    variant: 'primary',
    confirmText: '确认核销',
  })
  if (!ok) return
  const res = await api.post('/api/logistics/redeem', { code: d.code })
  if (res.code === 200) {
    toast.success(`核销成功：${d.student_name} · ${d.item_name}`)
    detail.value = null
    manualCode.value = ''
    fetchPending()
  } else {
    toast.error(res.msg || '核销失败')
  }
}

// ===== 摄像头扫码 =====
async function startScan() {
  scanError.value = ''
  try {
    mediaStream = await navigator.mediaDevices.getUserMedia({
      video: { facingMode: 'environment', width: { ideal: 1280 }, height: { ideal: 720 } },
    })
    await nextTick()
    if (videoEl.value) {
      videoEl.value.srcObject = mediaStream
      await videoEl.value.play()
    }
    scanning.value = true
    scanTimer = window.setInterval(() => {
      if (!videoEl.value || !canvasEl.value) return
      const text = scanVideoFrame(videoEl.value, canvasEl.value)
      if (!text) return
      // 防同一张二维码连续触发：1.5 秒内同一码不重复处理
      const now = Date.now()
      if (text === lastScannedCode && now - lastScannedAt < 1500) return
      lastScannedCode = text
      lastScannedAt = now
      const code = normalizeCode(text)
      if (code) {
        stopScan()
        lookup(code)
      }
    }, 220)
  } catch (e) {
    scanError.value = '无法访问摄像头，请检查浏览器权限（需 HTTPS 或 localhost），或改用下方手动输入'
    scanning.value = false
  }
}

function stopScan() {
  if (scanTimer) { clearInterval(scanTimer); scanTimer = undefined }
  if (mediaStream) {
    mediaStream.getTracks().forEach(t => t.stop())
    mediaStream = null
  }
  scanning.value = false
}

function onManualSubmit() {
  if (!manualCode.value.trim()) {
    toast.warning('请输入 8 位验证码')
    return
  }
  lookup(manualCode.value)
}

function pickFromPending(item: PendingItem) {
  lookup(item.code)
}

onMounted(async () => {
  const user = await checkAuth(5)
  if (!user) return
  userInfo.value = { id: user.id, name: user.name, role_id: user.role_id }
  fetchPending()
  pollTimer = window.setInterval(fetchPending, PENDING_POLL_MS)
})

onUnmounted(() => {
  stopScan()
  if (pollTimer) clearInterval(pollTimer)
})
</script>

<template>
  <div class="min-h-screen bg-stone-100">
    <!-- 顶栏 -->
    <header class="bg-white border-b border-stone-200 sticky top-0 z-10 shadow-sm">
      <div class="max-w-7xl mx-auto px-6 py-4 flex items-center justify-between">
        <div class="flex items-center gap-3">
          <div class="w-11 h-11 rounded-2xl bg-gradient-to-br from-cyan-500 to-sky-700 flex items-center justify-center shadow-lg shadow-cyan-500/30">
            <i class="fa-solid fa-box-open text-white text-lg"></i>
          </div>
          <div>
            <h1 class="font-display text-xl font-bold text-stone-800">后勤核销台</h1>
            <p class="text-xs text-stone-400">扫描学生二维码或输入验证码完成兑换核销</p>
          </div>
        </div>
        <div class="flex items-center gap-3">
          <span class="text-sm text-stone-600">
            <i class="fa-solid fa-user-shield mr-1 text-cyan-600"></i>{{ userInfo.name }}
          </span>
          <span class="inline-flex items-center gap-1.5 bg-cyan-50 text-cyan-700 px-3 py-1.5 rounded-full text-xs font-semibold">
            <span class="w-2 h-2 rounded-full bg-cyan-500 animate-pulse"></span>后勤
          </span>
          <button class="text-stone-400 hover:text-rose-500 transition" title="退出登录" @click="logout()">
            <i class="fa-solid fa-arrow-right-from-bracket"></i>
          </button>
        </div>
      </div>
    </header>

    <main class="max-w-7xl mx-auto px-6 py-6 space-y-6">
      <!-- 上半区：扫码 + 手输 -->
      <div class="grid md:grid-cols-2 gap-6">
        <!-- 摄像头扫码 -->
        <div class="card-base p-5">
          <h2 class="font-bold text-stone-800 mb-4 flex items-center gap-2">
            <i class="fa-solid fa-camera-retro text-cyan-600"></i>扫码核销
          </h2>
          <div class="relative rounded-2xl overflow-hidden bg-stone-900 aspect-video flex items-center justify-center">
            <video v-show="scanning" ref="videoEl" class="w-full h-full object-cover" muted playsinline></video>
            <canvas ref="canvasEl" class="hidden"></canvas>
            <div v-if="!scanning" class="text-center text-stone-400 py-16">
              <i class="fa-solid fa-video text-4xl mb-3 opacity-50"></i>
              <p class="text-sm">点击下方按钮开启摄像头</p>
            </div>
            <!-- 扫描取景框 -->
            <div v-if="scanning" class="scan-frame"></div>
          </div>
          <p v-if="scanError" class="text-xs text-rose-500 mt-2">{{ scanError }}</p>
          <div class="flex gap-3 mt-4">
            <button v-if="!scanning" class="btn-primary flex-1 py-2.5 rounded-2xl text-sm font-medium" @click="startScan">
              <i class="fa-solid fa-play mr-2"></i>开启摄像头
            </button>
            <button v-else class="btn-danger flex-1 py-2.5 rounded-2xl text-sm font-medium" @click="stopScan">
              <i class="fa-solid fa-stop mr-2"></i>停止扫码
            </button>
          </div>
        </div>

        <!-- 手动输入 -->
        <div class="card-base p-5 flex flex-col">
          <h2 class="font-bold text-stone-800 mb-4 flex items-center gap-2">
            <i class="fa-solid fa-keyboard text-cyan-600"></i>输入验证码
          </h2>
          <div class="flex-1 flex flex-col justify-center">
            <input
              v-model.trim="manualCode"
              type="text"
              placeholder="XXXX-XXXX"
              maxlength="12"
              class="field-input w-full px-5 py-4 text-center font-mono text-2xl tracking-[0.25em] uppercase rounded-2xl border-2 border-stone-200 focus:border-cyan-400 focus:outline-none"
              @keyup.enter="onManualSubmit"
            >
            <p class="text-xs text-stone-400 mt-3 text-center">8 位大写字母 + 数字（支持扫码枪输入后自动提交）</p>
            <button class="btn-primary mt-5 py-3 rounded-2xl text-sm font-semibold" :disabled="loading" @click="onManualSubmit">
              <i class="fa-solid fa-magnifying-glass mr-2"></i>{{ loading ? '查询中…' : '查询兑换单' }}
            </button>
          </div>
        </div>
      </div>

      <!-- 核销详情卡 -->
      <div v-if="detail" class="card-base p-6 border-2" :class="detail.status === 'pending' ? 'border-cyan-300' : 'border-stone-200'">
        <div class="flex items-start justify-between mb-4">
          <h2 class="font-bold text-stone-800 text-lg flex items-center gap-2">
            <i class="fa-solid fa-receipt text-cyan-600"></i>兑换单详情
          </h2>
          <span class="px-3 py-1 rounded-full text-xs font-bold" :class="statusClass(detail.status)">
            {{ statusText(detail.status) }}
          </span>
        </div>
        <div class="grid grid-cols-2 md:grid-cols-3 gap-4 mb-5">
          <div><p class="text-xs text-stone-400 mb-1">学生姓名</p><p class="text-lg font-bold text-stone-800">{{ detail.student_name }}</p></div>
          <div><p class="text-xs text-stone-400 mb-1">班级</p><p class="text-lg font-bold text-stone-800">{{ detail.className || '—' }}</p></div>
          <div><p class="text-xs text-stone-400 mb-1">兑换物品</p><p class="text-lg font-bold text-stone-800">{{ detail.item_name }}</p></div>
          <div><p class="text-xs text-stone-400 mb-1">消耗积分</p><p class="text-lg font-bold text-orange-600">{{ detail.cost }}</p></div>
          <div><p class="text-xs text-stone-400 mb-1">验证码</p><p class="text-lg font-mono font-bold tracking-widest text-stone-800">{{ detail.code }}</p></div>
          <div><p class="text-xs text-stone-400 mb-1">兑换时间</p><p class="text-sm font-medium text-stone-600 pt-1.5">{{ formatDateTime(detail.time) }}</p></div>
        </div>
        <div class="flex gap-3">
          <button v-if="detail.status === 'pending'" class="btn-primary flex-1 py-3.5 rounded-2xl font-bold" @click="confirmRedeem">
            <i class="fa-solid fa-circle-check mr-2"></i>确认核销无误
          </button>
          <p v-else class="flex-1 text-center text-sm text-stone-400 py-3.5 bg-stone-50 rounded-2xl">
            {{ detail.status === 'redeemed' ? '该单已完成核销，请勿重复发放' : '该单已过期并自动退回积分，无法核销' }}
          </p>
          <button class="btn-ghost px-6 py-3.5 rounded-2xl text-sm" @click="detail = null">关闭</button>
        </div>
      </div>

      <!-- 待核销列表 -->
      <div class="card-base p-5">
        <div class="flex items-center justify-between mb-4">
          <h2 class="font-bold text-stone-800 flex items-center gap-2">
            <i class="fa-solid fa-hourglass-half text-amber-500"></i>待核销列表
            <span class="bg-amber-100 text-amber-700 px-2.5 py-0.5 rounded-full text-xs font-bold">{{ pendingList.length }}</span>
          </h2>
          <span class="text-xs text-stone-400 flex items-center gap-1.5">
            <span class="w-1.5 h-1.5 rounded-full bg-emerald-500 animate-pulse"></span>每 5 秒自动刷新
          </span>
        </div>
        <EmptyState v-if="pendingList.length === 0" icon="fa-box-open" text="当前没有待核销的兑换单" variant="card" />
        <div v-else class="overflow-x-auto">
          <table class="w-full text-sm">
            <thead>
              <tr class="text-left text-xs text-stone-400 uppercase tracking-wider border-b border-stone-100">
                <th class="py-3 pr-4">学生</th>
                <th class="py-3 pr-4">班级</th>
                <th class="py-3 pr-4">兑换物</th>
                <th class="py-3 pr-4">验证码</th>
                <th class="py-3 pr-4">积分</th>
                <th class="py-3 pr-4">兑换时间</th>
                <th class="py-3"></th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="item in pendingList" :key="item.id" class="border-b border-stone-50 hover:bg-stone-50/60 transition">
                <td class="py-3 pr-4 font-semibold text-stone-800">{{ item.student_name }}</td>
                <td class="py-3 pr-4 text-stone-600">{{ item.className || '—' }}</td>
                <td class="py-3 pr-4 text-stone-600">{{ item.item_name }}</td>
                <td class="py-3 pr-4 font-mono font-bold tracking-wider text-stone-700">{{ item.code }}</td>
                <td class="py-3 pr-4 text-orange-600 font-bold">{{ item.cost }}</td>
                <td class="py-3 pr-4 text-stone-400 text-xs">{{ formatDateTime(item.time) }}</td>
                <td class="py-3">
                  <button class="text-cyan-600 hover:text-cyan-800 font-medium text-xs whitespace-nowrap" @click="pickFromPending(item)">
                    核销 <i class="fa-solid fa-arrow-right ml-0.5"></i>
                  </button>
                </td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>
    </main>
  </div>
</template>

<style scoped>
.card-base {
  background: white;
  border-radius: 1.25rem;
  box-shadow: 0 4px 24px -8px rgba(28, 25, 23, 0.12);
}
.btn-primary {
  background: linear-gradient(135deg, #06b6d4, #0284c7);
  color: white;
  transition: opacity 0.15s;
}
.btn-primary:hover { opacity: 0.9; }
.btn-primary:disabled { opacity: 0.5; cursor: not-allowed; }
.btn-danger {
  background: linear-gradient(135deg, #f43f5e, #e11d48);
  color: white;
}
.btn-ghost {
  background: #f5f5f4;
  color: #57534e;
}
.btn-ghost:hover { background: #e7e5e4; }
.field-input {
  background: #fafaf9;
}
.scan-frame {
  position: absolute;
  inset: 22%;
  border: 2px solid rgba(34, 211, 238, 0.9);
  border-radius: 1rem;
  box-shadow: 0 0 0 9999px rgba(8, 15, 31, 0.35);
  pointer-events: none;
}
</style>
